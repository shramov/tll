/*
 * Copyright (c) 2018-2021 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#include "channel/lz4.h"
#include "tll/util/memoryview.h"
#include "tll/util/size.h"

#include <lz4.h>

using namespace tll;

TLL_DEFINE_IMPL(ChLZ4);

int ChLZ4::_init(const tll::Channel::Url &url, Channel *parent)
{
	auto reader = channel_props_reader(url);
	_level = reader.getT<int>("level", 1);
	_max_size = reader.getT("max-size", tll::util::Size { 256 * 1024 });
	_inverted = reader.getT("inverted", _inverted);
	if (!reader)
		return _log.fail(EINVAL, "Invalid url: {}", reader.error());

	_lz4_enc.resize(LZ4_sizeofState());
	_buffer_dec.resize(_max_size);
	return Base::_init(url, parent);
}

int ChLZ4::_post(const tll_msg_t *msg, int flags)
{
	if (msg->type != TLL_MESSAGE_DATA)
		return _child->post(msg, flags);
	if (!msg->size)
		return _child->post(msg, flags);
	auto m = _inverted ? _decode(msg) : _encode(msg);
	if (!m)
		return EINVAL;
	return _child->post(m, flags);
}

int ChLZ4::_on_data(const tll_msg_t *msg)
{
	if (!msg->size)
		return _callback_data(msg);
	auto m = _inverted ? _encode(msg) : _decode(msg);
	if (!m)
		return EINVAL;
	return _callback_data(m);
}

const tll_msg_t * ChLZ4::_encode(const tll_msg_t *msg)
{
	if (msg->size > _max_size)
		return _log.fail(nullptr, "Message size too large: {} > limit {}", msg->size, _max_size);
	auto view = make_view(_buffer_enc);
	view.resize(LZ4_compressBound(msg->size));
	auto r = LZ4_compress_fast_extState(_lz4_enc.data(), (const char *) msg->data, view.dataT<char>(), msg->size, view.size(), _level);
	if (!r)
		return _log.fail(nullptr, "Failed to compress");
	_log.trace("Compressed size: {}", r);
	view.resize(r);
	tll_msg_copy_info(&_msg_enc, msg);
	_msg_enc.data = _buffer_enc.data();
	_msg_enc.size = _buffer_enc.size();
	return &_msg_enc;
}

const tll_msg_t * ChLZ4::_decode(const tll_msg_t *msg)
{
	auto view = make_view(*msg);
	tll_msg_copy_info(&_msg_dec, msg);
	//_buffer_dec.resize(_max_size);
	auto r = LZ4_decompress_safe(view.dataT<char>(), _buffer_dec.data(), view.size(), _buffer_dec.size());
	if (r < 0)
		return _log.fail(nullptr, "Failed to decompress");
	_log.trace("Decompressed size: {}", r);
	_msg_dec.data = _buffer_dec.data();
	_msg_dec.size = r;
	return &_msg_dec;
}
