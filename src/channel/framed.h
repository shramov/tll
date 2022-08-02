/*
 * Copyright (c) 2022 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#ifndef _CHANNEL_FRAMED_H
#define _CHANNEL_FRAMED_H

#include <tll/channel/prefix.h>

template <typename Frame>
class Framed : public tll::channel::Prefix<Framed<Frame>>
{
	using Base = tll::channel::Prefix<Framed<Frame>>;

	std::vector<char> _buf_send;
	std::vector<char> _buf_recv;
	size_t _recv_start = 0;
	size_t _recv_end = 0;
	size_t _max_size = 0;

	tll_msg_t _msg_send = {};
	tll_msg_t _msg_recv = {};

 public:
	static constexpr std::string_view channel_protocol() { return "framed+"; }

	std::optional<const tll_channel_impl_t *> _init_replace(const tll::Channel::Url &url, tll::Channel *master);

	int _init(const tll::Channel::Url &, tll::Channel *master);
	int _open(const tll::ConstConfig &cfg);

	int _post(const tll_msg_t *msg, int flags);
	int _on_data(const tll_msg_t *msg);
	int _process(long timeout, int flags);

 private:
	void _process_data(const Frame * frame);
	size_t _size() const { return _recv_end - _recv_start; }
	const Frame * _frame() const
	{
		auto size = _size();
		if (size < sizeof(Frame)) return nullptr;
		auto frame = (const Frame *) &_buf_recv[_recv_start];
		if (size < sizeof(Frame) + frame->size) return nullptr;
		return frame;
	}

	void _pending(const Frame * frame)
	{
		if (frame == nullptr)
			this->_update_dcaps(0, tll::dcaps::Pending | tll::dcaps::Process);
		else
			this->_update_dcaps(tll::dcaps::Pending | tll::dcaps::Process);
	}
};

#endif//_CHANNEL_FRAMED_H
