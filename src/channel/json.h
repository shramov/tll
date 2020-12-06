/*
 * Copyright (c) 2018-2019 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#ifndef _TLL_CHANNEL_JSON_H
#define _TLL_CHANNEL_JSON_H

#include "tll/channel/prefix.h"

#include "tll/util/json.h"

class ChJSON : public tll::channel::Prefix<ChJSON>
{
	tll::json::JSON _json = { _log };
	bool _inverted = false;

	using Base = tll::channel::Prefix<ChJSON>;

 public:
	static constexpr std::string_view channel_protocol() { return "json+"; }

	int _init(const tll::Channel::Url &, tll::Channel *parent);
	//int _open(const tll::PropsView &);
	//int _close();

	int _encode(const tll_msg_t *msg, rapidjson::MemoryBuffer &buf);
	int _decode(const tll_msg_t *msg, tll::util::buffer &buf);

	template <typename W, typename Buf>
	int _encode_field(W &writer, const Buf &data, const tll::scheme::Field *field);

	template <typename W, typename Buf>
	int _encode_message(W &writer, const Buf &data, const tll::scheme::Message *msg);

	int _post(const tll_msg_t *msg, int flags);
	int _on_data(const tll_msg_t *msg);

	int _on_active()
	{
		if (_json.init_scheme(_child->scheme()))
			return _log.fail(EINVAL, "Failed to initialize scheme");
		return Base::_on_active();
	}
};

//extern template class tll::channel::Base<ChRate>;

#endif//_TLL_CHANNEL_JSON_H
