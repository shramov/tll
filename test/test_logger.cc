/*
 * Copyright (c) 2019 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#include "gtest/gtest.h"

#include "tll/logger.h"

#include <list>

struct log_map : public tll_logger_impl_t
{
	using log_entry_t = std::pair<tll::Logger::level_t, std::string>;

	std::map<std::string, std::list<log_entry_t>> map;

	log_map()
	{
		log = _log;
		log_new = _new;
		log_free = _free;
	}

	~log_map() { tll_logger_register(nullptr); }

	static int _log(long long ts, const char * category, tll_logger_level_t level, const char * data, size_t size, void * obj)
	{
		fmt::print("Log: {} {} {}\n", tll::Logger::level_name(level), category, std::string_view(data, size));
		auto list = static_cast<std::list<log_entry_t> *>(obj);
		list->push_back({level, std::string(data, size)});
		return 0;
	}

	static void * _new(const char * category, tll_logger_impl_t * impl)
	{
		fmt::print("Create new logger {}\n", category);
		auto self = static_cast<log_map *>(impl);
		auto r = self->map.insert({category, {}});
		return &r.first->second;
	}

	static void _free(const char * category, void * user, tll_logger_impl_t * impl)
	{
		fmt::print("Drop logger {}\n", category);
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

struct NonCopyConstrutable
{
	NonCopyConstrutable() {}
	NonCopyConstrutable(const NonCopyConstrutable &) = delete;
	operator std::string_view () const { return "NonCopyConstrutable"; }
};

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
