/*
 * Copyright (c) 2018-2021 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#ifndef _TLL_IMPL_CHANNEL_PUB_H
#define _TLL_IMPL_CHANNEL_PUB_H

#include "tll/channel/frame.h"
#include "tll/channel/lastseq.h"
#include "tll/channel/tcp.h"
#include "tll/util/cppring.h"

#include <list>

class ChPubServer;

class ChPubSocket : public tll::channel::TcpSocket<ChPubSocket>
{
	using container_type = tll::util::DataRing<tll_frame_t>;
	const container_type * _ring;
	long long _seq = -1;
	const unsigned char * _ptr = nullptr;
	container_type::const_iterator _iter = {};
	bool _hello = true;

 public:
	static constexpr auto open_policy() { return OpenPolicy::Manual; }

	static constexpr std::string_view channel_protocol() { return "pub"; }
	static constexpr std::string_view impl_protocol() { return "pub-socket"; } // Only visible in logs

	int _init(const tll::Channel::Url &, tll::Channel *master);
	int _open(const tll::ConstConfig &);
	int _close();

	int _process(long timeout, int flags);
	int _post(const tll_msg_t *msg, int flags) { return ENOTSUP; }

	int _process_data(bool pollout = false);
 private:

	int _process_open();
	int _on_active();
};

class ChPubServer : public tll::channel::LastSeqTx<ChPubServer, tll::channel::TcpServer<ChPubServer, ChPubSocket>>
{
	size_t _size;
	tll::util::DataRing<tll_frame_t> _ring;
	bool _hello = true;

 public:
	static constexpr std::string_view channel_protocol() { return "pub+tcp"; }
	static constexpr std::string_view param_prefix() { return "pub"; }

	ChPubServer() {}

	int _init(const tll::Channel::Url &url, tll::Channel *master);
	std::optional<const tll_channel_impl_t *> _init_replace(const tll::Channel::Url &url, tll::Channel *master);

	int _close();

	int _post(const tll_msg_t *msg, int flags);

	bool hello() const { return _hello; }
	const tll::util::DataRing<tll_frame_t> * ring() const { return &_ring; }

	void _on_child_error(ChPubSocket * s) { tll_channel_close(*s, 1); }

 private:
	static int _cb(const tll_channel_t *c, const tll_msg_t *msg, void * user)
	{
		return static_cast<ChPubServer *>(user)->_cb(c, msg);
	}

	static int _cb_data(const tll_channel_t *c, const tll_msg_t *msg, void * user)
	{
		return static_cast<ChPubServer *>(user)->_callback_data(msg);
	}

	int _cb(const tll_channel_t *c, const tll_msg_t *msg);

	void _shift();
};

//extern template class tll::channel::Base<ChPubServer>;

#endif//_TLL_IMPL_CHANNEL_PUB_H
