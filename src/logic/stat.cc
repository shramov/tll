/*
 * Copyright (c)2020-2021 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#include "tll/channel/logic.h"
#include "tll/channel/module.h"
#include "tll/stat.h"
#include "tll/util/conv.h"
#include "tll/util/conv-fmt.h"

#include "tll/scheme/logic/stat.h"

#include "logic/quantile.h"

#include <regex>

class Stat : public tll::LogicBase<Stat>
{
	tll_stat_list_t * _stat = nullptr;
	tll::Channel * _timer = nullptr;
	bool _secondary;

	struct page_rule_t
	{
		std::regex re;
		tll::Logger log;
		bool skip = false;
	};

	std::list<page_rule_t> _rules;

	std::vector<char> _buf;

 public:
	static constexpr std::string_view channel_protocol() { return "stat"; }

	static constexpr auto scheme_policy() { return SchemePolicy::Manual; }

	int _init(const tll::Channel::Url &, tll::Channel *master);

	int logic(const tll::Channel * c, const tll_msg_t *msg);
	int _dump(tll_stat_iter_t * i);
	std::string _dump(const tll::stat::Field & v);

	template <typename T>
	std::string _group(std::string_view name, tll_stat_unit_t unit, int64_t count, T sum, T min, T max);
};

int Stat::_init(const tll::Channel::Url &url, tll::Channel *)
{
	auto i = _channels.find("timer");
	if (i == _channels.end()) return _log.fail(EINVAL, "No timer channel");
	if (i->second.size() != 1) return _log.fail(EINVAL, "Need exactly one input, got {}", i->second.size());
	_timer = i->second.front();
	_stat = context().stat_list();
	if (!_stat)
		return _log.fail(EINVAL, "Context does not have stat list");

	auto reader = channel_props_reader(url);
	_secondary = reader.getT("secondary", false);
	if (!reader)
		return _log.fail(EINVAL, "Invalid url: {}", reader.error());

	_scheme.reset(context().scheme_load(stat_scheme::scheme_string));
	if (!_scheme.get())
		return _log.fail(EINVAL, "Failed to load timer scheme");

	for (auto &[_, c] : url.browse("page.*", true)) {
		auto m = c.get("match");
		if (!m || m->empty()) continue;
		try {
			auto l = c.get("logger");
			tll::Logger log = _log;
			if (l && l->size()) {
				std::string lname(*l);
				if (lname[0] == '.')
					lname = _log.name() + lname;
				log = tll::Logger(lname);
			}

			auto skip = c.getT<bool>("skip", false);
			if (!skip)
				return _log.fail(EINVAL, "Invalid 'skip' value: {}", skip.error()); 

			_log.info("Pages '{}' via logger {}", *m, log.name());
			_rules.emplace_back(page_rule_t { std::regex(std::string(*m)), std::move(log), *skip });
		} catch (std::regex_error &e) {
			return _log.fail(EINVAL, "Invalid regex {}: {}", *m, e.what());
		}
	}
	return 0;
}

int Stat::logic(const tll::Channel * c, const tll_msg_t *msg)
{
	if (msg->type != TLL_MESSAGE_DATA)
		return 0;
	if (c != _timer)
		return 0;
	auto i = tll_stat_list_begin(_stat);
	for (; i != nullptr; i = tll_stat_iter_next(i))
		_dump(i);

	return 0;
}

int Stat::_dump(tll_stat_iter_t * iter)
{
	if (tll_stat_iter_empty(iter)) return 0;
	std::string name(tll_stat_iter_name(iter));

	tll::Logger * log = nullptr;
	for (auto & r : _rules) {
		if (std::regex_match(name, r.re)) {
			if (r.skip) {
				_log.debug("Skip page {}", name);
				return 0;
			}
			log = &r.log;
			break;
		}
	}

	if (!log) {
		if (_secondary) {
			_log.debug("Skip page {}", name);
			return 0;
		}
		log = &_log;
	}

	auto page = tll_stat_iter_swap(iter);
	while (page == nullptr) {
		if (tll_stat_iter_empty(iter)) return 0;
		page = tll_stat_iter_swap(iter);
	}

	auto data = stat_scheme::Page::bind(_buf);
	_buf.resize(0);
	_buf.resize(data.meta_size());
	data.set_name(name);
	auto fields = data.get_fields();
	fields.resize(page->size);
	auto size = 0;

	std::string r = "";
	auto ptr = static_cast<const tll::stat::Field *>(page->fields);
	auto end = ptr + page->size;
	for (; ptr != end; ptr++) {
		auto field = fields[size++];
		if (r.size())
			r += ", ";
		if (ptr + 3 < end && ptr->name() == "_tllgrp") {
			auto count = ptr;
			auto sum = ++ptr;
			auto min = ++ptr;
			auto max = ++ptr;
			field.set_name(sum->name());
			field.set_unit(static_cast<stat_scheme::Unit>(sum->unit()));
			if (sum->type() == TLL_STAT_FLOAT) {
				r += _group(sum->name(), sum->unit(), count->value, sum->fvalue, min->fvalue, max->fvalue);
				auto group = field.get_value().set_fgroup();
				group.set_count(count->value);
				group.set_min(min->fvalue);
				group.set_max(max->fvalue);
				if (count->value)
					group.set_avg(sum->fvalue / count->value);
			} else {
				r += _group(sum->name(), sum->unit(), count->value, sum->value, min->value, max->value);
				auto group = field.get_value().set_igroup();
				group.set_count(count->value);
				group.set_min(min->value);
				group.set_max(max->value);
				if (count->value)
					group.set_avg(sum->value / count->value);
			}
			continue;
		}
		field.set_name(ptr->name());
		field.set_unit(static_cast<stat_scheme::Unit>(ptr->unit()));
		if (ptr->type() == TLL_STAT_FLOAT) {
			auto value = field.get_value().set_fvalue();
			value.set_method(static_cast<stat_scheme::Method>(ptr->method()));
			value.set_value(ptr->fvalue);
		} else {
			auto value = field.get_value().set_ivalue();
			value.set_method(static_cast<stat_scheme::Method>(ptr->method()));
			value.set_value(ptr->value);
		}
		r += _dump(*ptr);
	}
	fields.resize(size);

	log->info("Page {}: {}", name, r);
	tll_msg_t msg = { TLL_MESSAGE_DATA };
	msg.msgid = data.meta_id();
	msg.data = data.view().data();
	msg.size = data.view().size();
	_callback(&msg);
	return 0;
}

namespace tll::conv {
template <typename T>
struct dump<std::pair<T, std::string_view>> : public to_string_from_string_buf<std::pair<T, std::string_view>>
{
	using value_type = std::pair<T, std::string_view>;

	template <typename Buf>
	static inline std::string_view to_string_buf(const value_type &v, Buf &buf)
	{
		auto r = tll::conv::to_string_buf(v.first, buf);
		return tll::conv::append(buf, r, v.second);
	}
};
}

namespace {
template <typename T>
std::pair<T, std::string_view> shorten_bytes(T v)
{
	if (v > 1024ll * 1024 * 1024 * 1000)
		return {v / (1024 * 1024 * 1000), "gb"};
	else if (v > 1024 * 1024 * 1000)
		return {v / (1024 * 1024), "mb"};
	else if (v > 1024 * 1000)
		return {v / (1024), "kb"};
	return {v, "b"};
}

template <typename T>
std::pair<T, std::string_view> shorten_time(T v)
{
	if (v > 1000ll * 1000 * 1000 * 100)
		return {v / (1000ll * 1000 * 1000) , "s"};
	else if (v > 1000 * 1000 * 1000)
		return {v / (1000 * 1000), "ms"};
	else if (v > 1000 * 1000)
		return {v / (1000), "us"};
	return {v, "ns"};
}

template <typename T>
std::string format_field(std::string_view name, std::string_view suffix, std::pair<T, std::string_view> v)
{
	return fmt::format("{}/{}: {}{}", name, suffix, v.first, v.second);
}
}

std::string Stat::_dump(const tll::stat::Field & v)
{
	std::string_view name = v.name();

	std::string_view suffix = "";
	switch (v.unit()) {
	case tll::stat::Bytes:
		if (v.type() == TLL_STAT_INT)
			return format_field(name, "b", shorten_bytes(v.value));
		else
			return format_field(name, "b", shorten_bytes(v.fvalue));
	case tll::stat::Ns:
		if (v.type() == TLL_STAT_INT)
			return format_field(name, "ns", shorten_time(v.value));
		else
			return format_field(name, "ns", shorten_time(v.fvalue));
	default: break;
	}

	switch (v.type()) {
	case TLL_STAT_INT: return fmt::format("{}: {}{}", name, v.value, suffix);
	case TLL_STAT_FLOAT: return fmt::format("{}: {}{}", name, v.fvalue, suffix);
	}
	return fmt::format("{}: unknown type {}", name, v.type());
}

template <typename T>
std::string Stat::_group(std::string_view name, tll_stat_unit_t unit, int64_t count, T sum, T min, T max)
{
	if (count == 0)
		return fmt::format("{}: -/-/-", name);
	double avg = ((double) sum) / count;

	switch (unit) {
	case tll::stat::Bytes:
		return fmt::format("{}: {}/{}/{}", name, shorten_bytes(min), shorten_bytes(avg), shorten_bytes(max));
	case tll::stat::Ns:
		return fmt::format("{}: {:.3f}us/{:.3f}us/{}us", name, min / 1000., avg /1000., max /1000.);
	default:
		return fmt::format("{}: {}/{:.3f}/{}", name, min, avg, max);
	}
}

TLL_DEFINE_IMPL(Quantile);
TLL_DEFINE_IMPL(Stat);

TLL_DEFINE_MODULE(Quantile, Stat);
