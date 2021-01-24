/*
 * Copyright (c) 2020 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#include "tll/channel/logic.h"
#include "tll/stat.h"

class Stat : public tll::LogicBase<Stat>
{
	tll_stat_list_t * _stat = nullptr;
	tll::Channel * _timer = nullptr;
 public:
	static constexpr std::string_view param_prefix() { return "stat"; }

	int _init(const tll::Channel::Url &, tll::Channel *master);

	int logic(const tll::Channel * c, const tll_msg_t *msg);
	int _dump(tll_stat_iter_t * i);
	std::string _dump(tll_stat_field_t & v);
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
	auto page = tll_stat_iter_swap(iter);
	while (page == nullptr) {
		if (tll_stat_iter_empty(iter)) return 0;
		page = tll_stat_iter_swap(iter);
	}

	std::string r = "";
	for (auto i = 0u; i < page->size; i++) {
		if (r.size())
			r += ", ";
		r += _dump(page->fields[i]);
	}
	_log.info("Page {}: {}", tll_stat_iter_name(iter), r);
	return 0;
}

std::string Stat::_dump(tll_stat_field_t & v)
{
	std::string_view name = {v.name, strnlen(v.name, 7)};

	std::string_view suffix = "";
	switch (v.unit) {
	case tll::stat::Bytes: suffix = "b"; break;
	case tll::stat::Ns: suffix = "ns"; break;
	default: break;
	}

	switch (v.type) {
	case TLL_STAT_INT: return fmt::format("{}: {}{}", name, v.value, suffix);
	case TLL_STAT_FLOAT: return fmt::format("{}: {}{}", name, v.fvalue, suffix);
	}
	return fmt::format("{}: unknown type {}", name, (unsigned char) v.type);
}

TLL_DEFINE_IMPL(Stat);

auto channel_module = tll::make_channel_module<Stat>();
