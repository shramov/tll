/*
 * Copyright (c) 2021 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#include "channel/timeit.h"

#include "tll/util/time.h"

using namespace tll;

TLL_DEFINE_IMPL(ChTimeIt);

int ChTimeIt::_post(const tll_msg_t *msg, int flags)
{
	if (!_stat_enable)
		return Base::_post(msg, flags);
	auto start = tll::time::now();
	auto r = Base::_post(msg, flags);
	auto dt = tll::time::now() - start;

	auto page = stat()->acquire();
	if (page) {
		page->tx = dt.count();
		stat()->release(page);
	}
	return r;
}

int ChTimeIt::_on_data(const tll_msg_t *msg)
{
	if (!_stat_enable)
		return Base::_on_data(msg);
	auto start = tll::time::now();
	auto r = Base::_on_data(msg);
	auto dt = tll::time::now() - start;

	auto page = stat()->acquire();
	if (page) {
		page->rx = dt.count();
		stat()->release(page);
	}
	return r;
}
