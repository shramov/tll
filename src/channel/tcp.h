/*
 * Copyright (c) 2018-2020 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#ifndef _TLL_IMPL_CHANNEL_TCP_H
#define _TLL_IMPL_CHANNEL_TCP_H

#include "tll/channel/tcp.h"

class ChTcp : public tll::channel::TcpClient<ChTcp>
{
 public:
	static constexpr std::string_view param_prefix() { return "tcp"; }

	tll_channel_impl_t * _init_replace(const tll::UrlView &url);
};

class ChTcpSocket : public tll::channel::TcpSocket<ChTcpSocket>
{
 public:
	static constexpr std::string_view param_prefix() { return "tcp"; }
	static constexpr std::string_view impl_protocol() { return "tcp-socket"; } // Only visible in logs
};

class ChTcpServer : public tll::channel::TcpServer<ChTcpServer, ChTcpSocket>
{
 public:
	static constexpr std::string_view param_prefix() { return "tcp"; }
	static constexpr std::string_view impl_protocol() { return "tcp-server"; } // Only visible in logs
};

#endif//_TLL_IMPL_CHANNEL_TCP_H
