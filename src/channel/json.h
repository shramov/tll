/*
 * Copyright (c) 2018-2019 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#ifndef _TLL_CHANNEL_JSON_H
#define _TLL_CHANNEL_JSON_H

#include "tll/channel/codec.h"

#include "tll/util/json.h"

class ChJSON : public tll::channel::Codec<ChJSON>
{
	tll::json::JSON _json = { _log };

	using Base = tll::channel::Prefix<ChJSON>;

 public:
	static constexpr std::string_view channel_protocol() { return "json+"; }

	int _init(const tll::Channel::Url &, tll::Channel *parent);

	const tll_msg_t * _encode(const tll_msg_t *msg)
	{
		tll_msg_copy_info(&_msg_enc, msg);
		auto r = _json.encode(msg, &_msg_enc);
		if (!r)
			return _log.fail(nullptr, "Failed to encode JSON");
		_msg_enc.data = r->data;
		_msg_enc.size = r->size;
		return &_msg_enc;
	}

	const tll_msg_t * _decode(const tll_msg_t *msg)
	{
		tll_msg_copy_info(&_msg_enc, msg);
		auto r = _json.decode(msg, &_msg_enc);
		if (!r)
			return _log.fail(nullptr, "Failed to decode JSON");
		_msg_enc.data = r->data;
		_msg_enc.size = r->size;
		return &_msg_enc;
	}

	int _on_active()
	{
		if (_json.init_scheme(_child->scheme()))
			return _log.fail(EINVAL, "Failed to initialize scheme");
		return Base::_on_active();
	}
};

//extern template class tll::channel::Base<ChRate>;

#endif//_TLL_CHANNEL_JSON_H
