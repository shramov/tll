/*
 * Copyright (c) 2019-2021 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#include "gtest/gtest.h"

#include "tll/logger.h"
#include "tll/logger/impl.h"
#include "tll/logger/prefix.h"

#include <list>

struct log_map : public tll_logger_impl_t
{
	using log_entry_t = std::pair<tll::Logger::level_t, std::string>;

	std::map<std::string, std::list<log_entry_t>> map;

	log_map()
	{
		*(tll_logger_impl_t *) this = {};
		log = _log;
		log_new = _new;
		log_free = _free;
	}

	~log_map() { tll_logger_register(nullptr); }

	static int _log(long long ts, const char * category, tll_logger_level_t level, const char * data, size_t size, void * obj)
	{
		fmt::print(stderr, "Log: {} {} {}\n", tll::Logger::level_name(level), category, std::string_view(data, size));
		auto list = static_cast<std::list<log_entry_t> *>(obj);
		list->push_back({level, std::string(data, size)});
		return 0;
	}

	static void * _new(tll_logger_impl_t * impl, const char * category)
	{
		fmt::print(stderr, "Create new logger {}\n", category);
		auto self = static_cast<log_map *>(impl);
		auto r = self->map.insert({category, {}});
		return &r.first->second;
	}

	static void _free(tll_logger_impl_t * impl, const char * category, void * user)
	{
		fmt::print(stderr, "Drop logger {}\n", category);
		auto self = static_cast<log_map *>(impl);
		self->map.erase(category);
	}
};

TEST(Logger, New)
{
	log_map impl;
	auto & map = impl.map;

	{
		tll::Logger l0 { "l0" };
		ASSERT_EQ(map.size(), 0u);

		tll_logger_register(&impl);

		ASSERT_EQ(map.size(), 1u);
		ASSERT_EQ(map.begin()->first, "l0");
	}

	ASSERT_EQ(map.size(), 0u);

	{
		tll::Logger l0 { "l0" };

		ASSERT_EQ(map.size(), 1u);
		ASSERT_EQ(map.begin()->first, "l0");
	}

	tll_logger_register(nullptr);
	ASSERT_EQ(map.size(), 0u);
}

TEST(Logger, Set)
{
	log_map impl;
	using log_entry_t = log_map::log_entry_t;
	tll_logger_register(&impl);

	{
		tll::Logger l0 { "l0" };
		ASSERT_EQ(l0.level(), tll::Logger::Debug);
	}

	tll::Logger::set("l0", tll::Logger::Info);
	tll::Logger l0 { "l0" };

	ASSERT_EQ(impl.map.size(), 1u);
	auto & list = impl.map["l0"];

	ASSERT_EQ(l0.level(), tll::Logger::Info);
	l0.debug("Debug");
	ASSERT_EQ(list.size(), 0u);

	l0.info("Info");
	ASSERT_EQ(list.size(), 1u);
	ASSERT_EQ(list.back(), log_entry_t(tll::Logger::Info, "Info"));

	tll::Logger l1 { "l0" };
	l0.warning("Second");

	ASSERT_EQ(list.size(), 2u);
	ASSERT_EQ(list.back(), log_entry_t(tll::Logger::Warning, "Second"));

	l1.level() = tll::Logger::Debug;

	ASSERT_EQ(l0.level(), tll::Logger::Debug);
	l0.debug("Debug");

	ASSERT_EQ(list.size(), 3u);
	ASSERT_EQ(list.back(), log_entry_t(tll::Logger::Debug, "Debug"));
}

TEST(Logger, SetPrefix)
{
	tll::Logger::set("prefix.l0/*", tll::Logger::Info);
	tll::Logger::set("prefix.l0/child/*", tll::Logger::Trace);
	tll::Logger::set("prefix.l0/child", tll::Logger::Warning);
	tll::Logger::set("prefix.l0/child/a", tll::Logger::Error);

	ASSERT_EQ(tll::Logger("prefix").level(), tll::Logger::Debug);
	ASSERT_EQ(tll::Logger("prefix.l0/child").level(), tll::Logger::Warning);
	ASSERT_EQ(tll::Logger("prefix.l0/child/a").level(), tll::Logger::Error);
	ASSERT_EQ(tll::Logger("prefix.l0/child/a.b").level(), tll::Logger::Error);
	ASSERT_EQ(tll::Logger("prefix.l0/child/b").level(), tll::Logger::Trace);
	ASSERT_EQ(tll::Logger("prefix.l0/child/c").level(), tll::Logger::Trace);
	ASSERT_EQ(tll::Logger("prefix.l0/c").level(), tll::Logger::Info);
}

struct NonCopyConstrutable
{
	NonCopyConstrutable() {}
	NonCopyConstrutable(const NonCopyConstrutable &) = delete;
	constexpr operator std::string_view () const noexcept { return "NonCopyConstrutable"; }
};

constexpr auto format_as(const NonCopyConstrutable &v) noexcept { return static_cast<std::string_view>(v); }

TEST(Logger, NonCopyConstrutable)
{
	log_map impl;
	tll_logger_register(&impl);

	tll::Logger l { "l0" };
	NonCopyConstrutable ncc;
	l.log(l.Debug, "{}", ncc);
	l.info("{}", ncc);
	(void) l.fail(0, "{}", ncc);
}

struct Convertible
{
	Convertible() {}
	Convertible(const NonCopyConstrutable &) = delete;
};

template <>
struct tll::conv::dump<Convertible> : public to_string_from_string_buf<Convertible>
{
	template <typename Buf>
	static std::string_view to_string_buf(const Convertible &v, Buf &buf)
	{
		return "Convertible";
	}
};

TEST(Logger, Conv)
{
	log_map impl;
	tll_logger_register(&impl);

	tll::Logger l { "l0" };
	Convertible c;
	l.log(l.Debug, "{}", c);
	l.info("{}", c);
	(void) l.fail(0, "{}", c);
}

TEST(Logger, Prefix)
{
	log_map impl;
	tll_logger_register(&impl);

	std::string_view str = "str";
	tll::Logger l { "l0" };
	auto p0 = l.prefix("f0");
	auto p1 = l.prefix("f1 {}", 10);
	auto p2 = l.prefix("f2 {} {}", "char", str);
	auto p3 = l.prefix("f3 {} {} {}", "char", str, str.data());
	auto p4 = l.prefix("f4 {} {} {} {}", 1, 2, 3, 4);
	auto p5 = l.prefix("f5 {} {} {} {} {}", 1, "char", str, str.data(), 1.1);

	int called = 0;
	auto pf = p0.prefix([&called](){ called++; return "func"; });

	ASSERT_EQ(l.level(), p0.level());

	ASSERT_EQ(impl.map.size(), 1u);
	auto & list = impl.map["l0"];
	l.info("l0");
	ASSERT_EQ(list.back().second, "l0");

	p0.info("p0"); ASSERT_EQ(list.back().second, "f0 p0");
	p1.info("p1"); ASSERT_EQ(list.back().second, "f1 10 p1");
	p2.info("p2"); ASSERT_EQ(list.back().second, "f2 char str p2");
	p3.info("p3"); ASSERT_EQ(list.back().second, "f3 char str str p3");
	p4.info("p4"); ASSERT_EQ(list.back().second, "f4 1 2 3 4 p4");
	p5.info("p5"); ASSERT_EQ(list.back().second, "f5 1 char str str 1.1 p5");

	list.clear();
	pf.trace("trace");
	ASSERT_EQ(list.size(), 0u);
	ASSERT_EQ(called, 0);

	pf.info("pf"); ASSERT_EQ(list.back().second, "f0 func pf");
	ASSERT_EQ(called, 1);
	pf.info("second");
	ASSERT_EQ(called, 1);

	// Check for unhandled exceptions
	auto pinv = l.prefix("{:d}", "str");
	auto pfinv = l.prefix([](){ return fmt::format("{:d}", "str"); });

	pinv.info("pinv");
	pfinv.info("pfinv");
}
