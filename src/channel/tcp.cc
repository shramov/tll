/*
 * Copyright (c) 2018-2020 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#include "channel/tcp.h"

#include "tll/channel/tcp.hpp"

#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>

using namespace tll;

TLL_DEFINE_IMPL(ChTcp);
TLL_DEFINE_IMPL(ChTcpServer);
TLL_DEFINE_IMPL(ChTcpSocket);
TLL_DEFINE_IMPL(tll::channel::TcpServerSocket<ChTcpServer>);

tll_channel_impl_t * ChTcp::_init_replace(const tll::UrlView &url)
{
	auto reader = channel_props_reader(url);
	auto client = reader.getT("mode", true, {{"client", true}, {"server", false}});
	if (!reader)
		return _log.fail(nullptr, "Invalid url: {}", reader.error());
	if (!client)
		return &ChTcpServer::impl;
	return 0;
}
