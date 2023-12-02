// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Pavel Shramov <shramov@mexmat.net>

#ifndef _TLL_CHANNEL_LZ4B_H
#define _TLL_CHANNEL_LZ4B_H

#include "tll/channel/codec.h"

#include "tll/util/lz4block.h"

class ChLZ4B : public tll::channel::Codec<ChLZ4B>
{
	using Base = tll::channel::Codec<ChLZ4B>;

	tll::lz4::StreamEncode _lz4_enc;
	tll::lz4::StreamDecode _lz4_dec;

	size_t _block = 64 * 1024;
	unsigned _level = 0;

	long long _seq_enc = -1;
	long long _seq_dec = -1;

 public:
	static constexpr std::string_view channel_protocol() { return "lz4b+"; }

	int _init(const tll::Channel::Url &url, tll::Channel *master);
	int _open(const tll::ConstConfig &);

	const tll_msg_t * _encode(const tll_msg_t *msg);
	const tll_msg_t * _decode(const tll_msg_t *msg);
};

#endif//_TLL_CHANNEL_LZ4B_H
