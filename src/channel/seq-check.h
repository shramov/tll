/*
 * Copyright (c) 2021 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#include "tll/channel/prefix.h"

namespace tll::channel {
class SeqCheck : public Prefix<SeqCheck>
{
	long long _seq = -1;
 public:
	using Base = Prefix<SeqCheck>;

	static constexpr std::string_view channel_protocol() { return "seq-check+"; }

	int _on_active()
	{
		_seq = -1;
		return Base::_on_active();
	}

	int _on_data(const tll_msg_t *msg)
	{
		if (_seq != -1 && _seq + 1 != msg->seq)
			_log.error("Gap in stream: {} -> {}", _seq + 1, msg->seq);
		_seq = msg->seq;
		return Base::_on_data(msg);
	}
};

} // namespace tll::channel
