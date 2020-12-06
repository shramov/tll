/*
 * Copyright (c) 2018-2019 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#include "channel/json.h"
#include "tll/util/json.h"

using namespace tll;

TLL_DEFINE_IMPL(ChJSON);

int ChJSON::_init(const Channel::Url &url, Channel *master)
{
	auto reader = channel_props_reader(url);
	_inverted = reader.getT("inverted", false);
	if (!reader)
		return _log.fail(EINVAL, "Invalid url: {}", reader.error());
	if (_json.init(reader))
		return _log.fail(EINVAL, "Failed to init JSON encoder");

	return Base::_init(url, master);
}

int ChJSON::_post(const tll_msg_t *msg, int flags)
{
	if (!msg->size)
		return _child->post(msg, flags);
	tll_msg_t m;
	tll_msg_copy_info(&m, msg);
	auto r = _inverted?_json.decode(msg, &m):_json.encode(msg, &m);
	if (!r)
		return _log.fail(EINVAL, "Failed to encode JSON");
	m.data = r->data;
	m.size = r->size;
	return _child->post(&m, flags);
}

int ChJSON::_on_data(const tll_msg_t *msg)
{
	if (!msg->size)
		return _callback_data(msg);
	tll_msg_t m;
	tll_msg_copy_info(&m, msg);
	auto r = _inverted?_json.encode(msg, &m):_json.decode(msg, &m);
	if (!r)
		return _log.fail(EINVAL, "Failed to decode JSON");
	m.data = r->data;
	m.size = r->size;
	return _callback_data(&m);
}
