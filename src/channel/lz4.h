/*
 * Copyright (c) 2018-2021 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#ifndef _TLL_CHANNEL_LZ4_H
#define _TLL_CHANNEL_LZ4_H

#include "tll/channel/codec.h"

class ChLZ4 : public tll::channel::Codec<ChLZ4>
{
	using Base = tll::channel::Codec<ChLZ4>;

	std::vector<char> _lz4_enc;
	std::vector<char> _lz4_dec;

	int _level = 0;
	size_t _max_size = 0;

 public:
	static constexpr std::string_view channel_protocol() { return "lz4+"; }

	int _init(const tll::Channel::Url &, tll::Channel *master);

	const tll_msg_t * _encode(const tll_msg_t *msg);
	const tll_msg_t * _decode(const tll_msg_t *msg);
};

#endif//_TLL_CHANNEL_LZ4_H
