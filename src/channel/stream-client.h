/*
 * Copyright (c) 2022 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#ifndef _TLL_IMPL_CHANNEL_STREAM_CLIENT_H
#define _TLL_IMPL_CHANNEL_STREAM_CLIENT_H

#include "tll/channel/frame.h"
#include "tll/channel/lastseq.h"
#include "tll/channel/prefix.h"
#include "tll/util/cppring.h"

namespace tll::channel {

class StreamClient : public tll::channel::LastSeqRx<StreamClient, tll::channel::Prefix<StreamClient>>
{
 public:
	enum class State { Closed, Opening, Connected, Overlapped, Drain, Online };

 private:
	using Base = LastSeqRx<StreamClient, Prefix<StreamClient>>;

	tll::util::DataRing<tll_frame_t> _ring;

	std::unique_ptr<Channel> _request;
	std::vector<char> _request_buf;
	tll::Config _request_open;

	State _state;

	long long _seq = -1;
	long long _server_seq = -1;
	long long _block_end = -1;
	std::optional<long long> _open_seq;
	std::string _peer;

	tll::Config _reopen_cfg;
	bool _report_block_end = true;
	bool _protocol_old = true;

 public:
	static constexpr std::string_view channel_protocol() { return "stream+"; }
	static constexpr auto prefix_config_policy() { return PrefixConfigPolicy::Manual; }
	static constexpr std::string_view scheme_control_string();

	int _init(const tll::Channel::Url &url, tll::Channel *master);
	void _free()
	{
		_request.reset();
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

	int _on_data(const tll_msg_t *msg);

	int _on_active();
	int _on_closing()
	{
		switch (_request->state()) {
		case tll::state::Opening:
		case tll::state::Active:
		case tll::state::Error:
			_request->close();
			break;
		default:
			break;
		}
		return Base::_on_closing();
	}

	int _process(long timeout, int flags);

 private:
	int _on_request_state(const tll::Channel *, const tll_msg_t *msg)
	{
		switch ((tll_state_t) msg->msgid) {
		case tll::state::Active: return _on_request_active();
		case tll::state::Error: return _on_request_error();
		case tll::state::Closing: return _on_request_closing();
		case tll::state::Closed: return _on_request_closed();
		default:
			return 0;
		}
	}

	inline int _on_request_data(const tll::Channel *, const tll_msg_t *msg);
	int _on_request_active();
	int _on_request_error();
	int _on_request_closing() { return 0; }
	int _on_request_closed();

	int _report_online();
	int _report_block();
	int _post_done(long long seq);

	void _reset_config_cb(tll::Config cfg, std::string_view path)
	{
		if (auto v = cfg.get(path); v)
			cfg.set(path, *v); // Replace pointer
	}

	static char * _config_seq(int * len, void * data)
	{
		auto self = static_cast<const StreamClient *>(data);
		auto seq = self->_seq;
		if (seq == -1)
			seq = self->_open_seq.value_or(seq);
		auto buf = (char *) malloc(22); // sign, 21 digit, null byte
		auto size = snprintf(buf, 22, "%lld", seq);
		if (len)
			*len = size;
		return buf;
	}
};

} // namespace tll::channel

#endif//_TLL_IMPL_CHANNEL_STREAM_CLIENT_H
