/*
 * Copyright (c) 2022 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#ifndef _TLL_IMPL_CHANNEL_STREAM_SERVER_H
#define _TLL_IMPL_CHANNEL_STREAM_SERVER_H

#include "tll/channel/frame.h"
#include "tll/channel/lastseq.h"
#include "tll/channel/autoseq.h"
#include "tll/channel/prefix.h"

#include <list>

namespace tll::channel {

class StreamServer : public tll::channel::LastSeqTx<StreamServer, tll::channel::Prefix<StreamServer>>
{
	tll::channel::autoseq::AutoSeq _autoseq;
	std::unique_ptr<Channel> _request;
	std::unique_ptr<Channel> _storage;

	tll::Channel::Url _storage_url;

	std::string _blocks_filename;
	std::map<std::string, std::list<long long>, std::less<>> _blocks;

	long long _seq = -1;

	struct Client
	{
		Client(StreamServer * s) : parent(s) {}
		void reset();
		tll::result_t<int> init(const tll_msg_t * msg);

		long long seq = -1;
		tll_msg_t msg = {};
		std::unique_ptr<Channel> storage;
		std::string name;
		StreamServer * parent = nullptr;
		enum class State { Closed, Active, Error, Done } state = State::Closed;

		int on_storage(const tll_msg_t *);

		static int on_storage(const tll_channel_t *, const tll_msg_t * msg, void * user)
		{
			return static_cast<Client *>(user)->on_storage(msg);
		}
	};

	std::map<uint64_t, Client> _clients;

	int _control_msgid_full = 0;
	int _control_msgid_ready = 0;
	int _control_msgid_disconnect = 0;

 public:
	static constexpr std::string_view channel_protocol() { return "stream+"; }

	std::optional<const tll_channel_impl_t *> _init_replace(const tll::Channel::Url &url, tll::Channel *master);

	int _init(const tll::Channel::Url &url, tll::Channel *master);
	void _free()
	{
		_request.reset();
		_storage.reset();
		return Base::_free();
	}

	int _open(const tll::ConstConfig &cfg);
	int _close(bool force);

	const Scheme * scheme(int type) const
	{
		if (type == TLL_MESSAGE_CONTROL)
			return _scheme_control.get();
		return Base::scheme(type);
	}

	int _post(const tll_msg_t *msg, int flags);

	int _on_data(const tll_msg_t *msg) { return 0; }
	int _on_active() { return _check_state(tll::state::Active); }
	int _on_closed() { return _check_state(tll::state::Closed); }

 private:
	static int _on_request(const tll_channel_t *c, const tll_msg_t *msg, void * user)
	{
		return static_cast<StreamServer *>(user)->_on_request(msg);
	}

	int _on_request(const tll_msg_t *msg)
	{
		switch (msg->type) {
		case TLL_MESSAGE_DATA:
			return _on_request_data(msg);
		case TLL_MESSAGE_STATE:
			return _on_request_state(msg);
		case TLL_MESSAGE_CONTROL:
			return _on_request_control(msg);
		default:
			break;
		}
		return 0;
	}

	int _on_request_data(const tll_msg_t *msg);
	int _on_request_state(const tll_msg_t *msg);
	int _on_request_control(const tll_msg_t *msg);

	int _check_state(tll_state_t s);
	int _post_block(const tll_msg_t *msg);

	int _create_block(std::string_view block, long long seq, bool store = true);
};

} // namespace tll::channel

#endif//_TLL_IMPL_CHANNEL_STREAM_SERVER_H
