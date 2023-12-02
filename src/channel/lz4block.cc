// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Pavel Shramov <shramov@mexmat.net>

#include "channel/lz4block.h"

#include "tll/util/memoryview.h"
#include "tll/util/size.h"

#include <lz4.h>

using namespace tll;

TLL_DEFINE_IMPL(ChLZ4B);

struct __attribute__((packed)) meta_t
{
	int32_t msgid;
	int64_t seq;
};

int ChLZ4B::_init(const tll::Channel::Url &url, tll::Channel *master)
{
	auto reader = channel_props_reader(url);
	_block = reader.getT<util::Size>("block", 1024 * 1024);
	_level = reader.getT("level", 0u);
	if (!reader)
		return _log.fail(EINVAL, "Invalid url: {}", reader.error());

	if (auto r = _lz4_enc.init(_block); r)
		return _log.fail(EINVAL, "Failed to initialize lz4 encoder: {}", strerror(r));
	if (auto r = _lz4_dec.init(_block); r)
		return _log.fail(EINVAL, "Failed to initialize lz4 decoder: {}", strerror(r));

	_buffer_enc.resize(LZ4_compressBound(_block));
	_buffer_dec.resize(_block);

	return Base::_init(url, master);
}

int ChLZ4B::_open(const tll::ConstConfig &props)
{
	_lz4_enc.reset();
	_lz4_dec.reset();
	_seq_enc = -1;
	_seq_dec = -1;

	return Base::_open(props);
}

const tll_msg_t * ChLZ4B::_encode(const tll_msg_t *msg)
{
	if (msg->size > _block - sizeof(meta_t))
		return _log.fail(nullptr, "Message size too large: {} > block {}", msg->size, _block - sizeof(meta_t));
	auto view = _lz4_enc.ring.view();
	auto meta = view.dataT<meta_t>();
	meta->msgid = msg->msgid;
	if (_seq_enc != -1)
		meta->seq = msg->seq - _seq_enc;
	else
		meta->seq = msg->seq;
	_seq_enc = msg->seq;

	memcpy(view.view(sizeof(*meta)).data(), msg->data, msg->size);

	auto r = _lz4_enc.compress(_buffer_enc, sizeof(*meta) + msg->size, _level);
	if (!r.data)
		return _log.fail(nullptr, "Failed to compress data");

	_log.debug("Compressed size: {}, offset: {}, data: {}", r.size, _lz4_enc.ring.offset, msg->size);

	tll_msg_copy_info(&_msg_enc, msg);
	_msg_enc.data = r.data;
	_msg_enc.size = r.size;
	return &_msg_enc;
}

const tll_msg_t * ChLZ4B::_decode(const tll_msg_t *msg)
{
	auto r = _lz4_dec.decompress(msg->data, msg->size);
	if (!r.data)
		return _log.fail(nullptr, "Failed to decompress");
	_log.debug("Decompress data {}, offset {}, result {}", msg->size, _lz4_dec.ring.offset, r.size);

	tll_msg_copy_info(&_msg_dec, msg);
	auto meta = static_cast<const meta_t *>(r.data);
	_msg_dec.msgid = meta->msgid;
	if (_seq_dec == -1)
		_msg_dec.seq = meta->seq;
	else
		_msg_dec.seq = _seq_dec + meta->seq;
	_seq_dec = _msg_dec.seq;
	_msg_dec.data = meta + 1;
	_msg_dec.size = r.size - sizeof(*meta);
	return &_msg_dec;
}
