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
	std::unique_ptr<Channel> _blocks;
	std::unique_ptr<Channel> _storage_load;

	tll::Channel::Url _storage_url;
	tll::Channel::Url _blocks_url;

	long long _seq = -1;

	struct Client
	{
		Client(StreamServer * s) : parent(s) {}
		void reset();
		tll::result_t<int> init(const tll_msg_t * msg);

		long long seq = -1;
		long long block_end = -1;

		tll_msg_t msg = {};
		std::unique_ptr<Channel> storage; ///< Current storage channel
		std::unique_ptr<Channel> storage_next; ///< Next storage to process
		std::string name;
		StreamServer * parent = nullptr;
		enum class State { Closed, Opening, Active, Error, Done } state = State::Closed;

		int on_storage(const tll_msg_t *);
		int on_storage_state(tll_state_t);

		static int on_storage(const tll_channel_t *, const tll_msg_t * msg, void * user)
		{
			return static_cast<Client *>(user)->on_storage(msg);
		}

		/*
		static int on_blocks(const tll_channel_t *, const tll_msg_t * msg, void * user)
		{
			return static_cast<Client *>(user)->on_blocks(msg);
		}
		*/
	};

	std::map<uint64_t, Client> _clients;
	std::list<tll_addr_t> _clients_drop;

	int _control_msgid_full = 0;
	int _control_msgid_ready = 0;
	int _control_msgid_disconnect = 0;

	const tll::Scheme * _control_child = nullptr;
	const tll::Scheme * _control_request = nullptr;
	const tll::Scheme * _control_blocks = nullptr;
	const tll::Scheme * _control_storage = nullptr;

	tll::Config _child_open;

	std::string _init_message;
	tll::ConstConfig _init_config;
	long long _init_seq;
	std::string _init_block;

	std::string _rotate_on_block;

 public:
	static constexpr std::string_view channel_protocol() { return "stream+"; }
	static constexpr auto process_policy() { return ProcessPolicy::Never; }

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
	int _process(long timeout, int flags);

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
	int _request_disconnect(std::string_view name, const tll_addr_t & addr);

	int _on_storage_load(const tll_msg_t * msg);
	int _try_rotate_on_block(const tll::scheme::Message * message, const tll_msg_t * msg);
};

} // namespace tll::channel

#endif//_TLL_IMPL_CHANNEL_STREAM_SERVER_H
