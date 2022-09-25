/*
 * Copyright (c) 2022 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#ifndef _TLL_CHANNEL_CODEC_H
#define _TLL_CHANNEL_CODEC_H

#include "tll/channel/prefix.h"

namespace tll::channel {

template <typename T, typename Base = Prefix<T>>
class Codec : public Base
{
 protected:
	std::vector<char> _buffer_enc;
	std::vector<char> _buffer_dec;
	tll_msg_t _msg_enc = {};
	tll_msg_t _msg_dec = {};

	bool _inverted = false;

 public:
	int _init(const tll::Channel::Url &url, tll::Channel *master)
	{
		auto reader = this->channel_props_reader(url);
		_inverted = reader.getT("inverted", _inverted);
		if (!reader)
			return this->_log.fail(EINVAL, "Invalid url: {}", reader.error());
		return Base::_init(url, master);
	}

	int _post(const tll_msg_t *msg, int flags)
	{
		if (msg->type != TLL_MESSAGE_DATA)
			return Base::_post(msg, flags);
		auto m = _inverted ? this->channelT()->_decode(msg) : this->channelT()->_encode(msg);
		if (!m)
			return this->_log.fail(EINVAL, "Failed to {} data ({} bytes)", _inverted ? "decode" : "encode", msg->size);
		return Base::_post(m, flags);
	}

	int _on_data(const tll_msg_t *msg)
	{
		auto m = _inverted ? this->channelT()->_encode(msg) : this->channelT()->_decode(msg);
		if (!m)
			return this->_log.fail(EINVAL, "Failed to {} data ({} bytes)", _inverted ? "encode" : "decode", msg->size);
		return this->_callback_data(m);
	}

	const tll_msg_t * _encode(const tll_msg_t *msg);
	const tll_msg_t * _decode(const tll_msg_t *msg);
};

} // namespace tll::channel

#endif//_TLL_CHANNEL_CODEC_H
