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
	if (_json.init(reader))
		return _log.fail(EINVAL, "Failed to init JSON encoder");

	return Base::_init(url, master);
}
