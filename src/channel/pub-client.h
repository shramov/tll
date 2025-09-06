/*
 * Copyright (c) 2018-2021 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#ifndef _TLL_IMPL_CHANNEL_PUB_CLIENT_H
#define _TLL_IMPL_CHANNEL_PUB_CLIENT_H

#include "tll/channel/frame.h"
#include "tll/channel/lastseq.h"
#include "tll/channel/tcp.h"

class ChPubClient : public tll::channel::LastSeqRx<ChPubClient, tll::channel::TcpClient<ChPubClient>>
{
	using Base = tll::channel::LastSeqRx<ChPubClient, tll::channel::TcpClient<ChPubClient>>;
	long long _seq = -1;
	bool _hello = true;
	size_t _size = 0;
	std::string _peer;

	enum { Closed, Connected, Active } _state = Closed;

 public:
	static constexpr std::string_view channel_protocol() { return "pub"; }

	int _init(const tll::Channel::Url &url, tll::Channel *master);

	int _open(const tll::ConstConfig &url);
	int _close()
	{
		config_info().setT("seq", _seq);
		return Base::_close();
	}

	int _post(const tll_msg_t *msg, int flags) { return ENOTSUP; }
	int _process(long timeout, int flags);

	int _on_connect()
	{
		_state = Connected;
		return _post_hello();
	}

	void _on_close()
	{
		_log.error("Server dropped connection");
		state(tll::state::Error);
	}
 private:
	int _post_hello();
	int _process_open();
	int _process_data();
	int _process_pending();
};

//extern template class tll::channel::Base<ChPubClient>;

#endif//_TLL_IMPL_CHANNEL_PUB_CLIENT_H
