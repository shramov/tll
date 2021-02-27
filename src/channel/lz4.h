/*
 * Copyright (c) 2018-2021 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#ifndef _TLL_CHANNEL_LZ4_H
#define _TLL_CHANNEL_LZ4_H

#include "tll/channel/prefix.h"

class ChLZ4 : public tll::channel::Prefix<ChLZ4>
{
	using Base = tll::channel::Prefix<ChLZ4>;

	std::vector<char> _buffer_enc;
	std::vector<char> _buffer_dec;
	std::vector<char> _lz4_enc;
	std::vector<char> _lz4_dec;
	tll_msg_t _msg_enc = {};
	tll_msg_t _msg_dec = {};

	int _level = 0;
	bool _inverted = false;
	size_t _max_size = 0;

 public:
	static constexpr std::string_view channel_protocol() { return "lz4+"; }

	int _init(const tll::Channel::Url &, tll::Channel *master);

	int _post(const tll_msg_t *msg, int flags);
	int _on_data(const tll_msg_t *msg);

 private:
	const tll_msg_t * _encode(const tll_msg_t *msg);
	const tll_msg_t * _decode(const tll_msg_t *msg);
};

#endif//_TLL_CHANNEL_LZ4_H
