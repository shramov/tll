/*
 * Copyright (c) 2018-2021 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#include "channel/pub-mem.h"

#include "tll/channel/frame.h"
#include "tll/channel/lastseq.h"
#include "tll/ring.h"
#include "tll/util/size.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <filesystem>

#ifndef MAP_POPULATE
#define MAP_POPULATE 0
#endif

using namespace tll;

struct Frame {
	int64_t seq;
	int32_t msgid;
};

struct PubMemCommon
{
	std::string filename;
	int fd = -1;

	int init(const tll::Channel::Url &url, tll::Logger &log)
	{
		filename = url.host();
		if (filename.empty())
			return log.fail(EINVAL, "Empty or missing filename");
		return 0;
	}

	void close(const ring_header_t * header, tll::Logger &log)
	{
		if (header && munmap((void *) header, header->size + sizeof(*header)))
			log.error("Failed to unmap ring of size {}: {}", header->size + sizeof(*header), strerror(errno));

		if (fd != -1)
			::close(fd);
		fd = -1;
	}
};

class PubMemClient : public tll::channel::LastSeqRx<PubMemClient>
{
	using Base = tll::channel::LastSeqRx<PubMemClient>;

	ringiter_t _iter = {};
	std::vector<char> _buf;

	PubMemCommon _common = {};

 public:
	static constexpr std::string_view channel_protocol() { return "pub+mem"; }
	static constexpr std::string_view param_prefix() { return "pub"; }

	int _init(const tll::Channel::Url &, tll::Channel *master);
	int _open(const tll::ConstConfig &);
	int _close();

	int _process(long timeout, int flags);
};

class PubMemServer : public tll::channel::LastSeqTx<PubMemServer>
{
	using Base = tll::channel::LastSeqTx<PubMemServer>;

	ringbuffer_t _ring = {};

	PubMemCommon _common = {};
	size_t _size = 0;
	bool _unlink = false;

 public:
	static constexpr std::string_view channel_protocol() { return "pub+mem"; }
	static constexpr std::string_view param_prefix() { return "pub"; }
	static constexpr auto process_policy() { return ProcessPolicy::Never; }

	int _init(const tll::Channel::Url &, tll::Channel *master);
	int _open(const tll::ConstConfig &);
	int _close();

	int _post(const tll_msg_t *msg, int flags);
};

TLL_DEFINE_IMPL(ChPubMem);
TLL_DEFINE_IMPL(PubMemServer);
TLL_DEFINE_IMPL(PubMemClient);

std::optional<const tll_channel_impl_t *> ChPubMem::_init_replace(const tll::Channel::Url &url, tll::Channel *master)
{
	auto reader = channel_props_reader(url);
	auto server = reader.getT("mode", false, {{"server", true}, {"client", false}});
	if (!reader)
		return _log.fail(std::nullopt, "Invalid url: {}", reader.error());

	if (server)
		return &PubMemServer::impl;
	return &PubMemClient::impl;
}

int PubMemServer::_init(const tll::Channel::Url &url, tll::Channel *master)
{
	if (auto r = _common.init(url, _log))
		return r;

	auto reader = this->channel_props_reader(url);
	_size = reader.getT("size", util::Size {64 * 1024});
	if (!reader)
		return this->_log.fail(EINVAL, "Invalid url: {}", reader.error());

	return Base::_init(url, master);
}

int PubMemServer::_open(const tll::ConstConfig &cfg)
{
	std::error_code ec, uec;
	std::string fn = _common.filename + ".XXXXXX";
	_common.fd = mkstemp(fn.data());
	if (_common.fd == -1)
		return _log.fail(EINVAL, "Failed to create temporary file {}: {}", fn, strerror(errno));

	auto full = sizeof(ring_header_t) + _size;
	if (auto r = posix_fallocate(_common.fd, 0, full); r) {
		std::filesystem::remove(fn, uec);
		return _log.fail(EINVAL, "Failed to allocate {} bytes of space: {}", full, strerror(r));
	}

	auto buf = mmap(nullptr, full, PROT_READ | PROT_WRITE, MAP_SHARED, _common.fd, 0);
	if (buf == MAP_FAILED) {
		std::filesystem::remove(fn, uec);
		return _log.fail(EINVAL, "Failed to mmap memory: {}", strerror(errno));
	}

	_ring.header = static_cast<ring_header_t *>(buf);
	*_ring.header = {};
	_ring.header->size = _size;
	_ring.header->magic = ring_magic;

	_log.info("Rename temporary file {} to {}", fn, _common.filename);
	if (std::filesystem::rename(fn, _common.filename, ec); ec) {
		std::filesystem::remove(fn, uec);
		return _log.fail(EINVAL, "Failed to rename temporary file '{}' to '{}': {}", fn, _common.filename, ec.message());
	}
	_unlink = true;

	return Base::_open(cfg);
}

int PubMemServer::_close()
{
	if (_unlink)
		unlink(_common.filename.c_str());
	_unlink = false;
	_common.close(_ring.header, _log);
	_ring = {};

	return Base::_close();
}

int PubMemServer::_post(const tll_msg_t *msg, int flags)
{
	if (msg->type != TLL_MESSAGE_DATA)
		return 0;

	if (msg->size > _size / 4)
		return _log.fail(EMSGSIZE, "Message size too large: {} > max {}", msg->size, _size / 4);

	Frame * frame;
	const size_t size = sizeof(Frame) + msg->size;

	while (ring_write_begin(&_ring, (void **) &frame, size)) {
		ring_shift(&_ring);
	}

	frame->seq = msg->seq;
	frame->msgid = msg->msgid;
	memcpy(frame + 1, msg->data, msg->size);
	ring_write_end(&_ring, frame, size);
	return 0;
}

int PubMemClient::_init(const tll::Channel::Url &url, tll::Channel *master)
{
	if (auto r = _common.init(url, _log))
		return r;
	return Base::_init(url, master);
}

int PubMemClient::_open(const tll::ConstConfig &cfg)
{
	_common.fd = ::open(_common.filename.c_str(), O_RDONLY);
	if (_common.fd == -1)
		return _log.fail(EINVAL, "Failed to open file {}: {}", _common.filename, strerror(errno));

	ring_header_t hdr = {};
	auto r = read(_common.fd, &hdr, sizeof(hdr));
	if (r < 0)
		return _log.fail(EINVAL, "Failed to read ring header from file {}: {}", _common.filename, strerror(errno));
	if ((size_t) r < sizeof(hdr))
		return _log.fail(EINVAL, "Failed to read ring header from file {}: got {} bytes of {} needed", _common.filename, r, sizeof(hdr));
	if (hdr.magic != ring_magic)
		return _log.fail(EINVAL, "Invalid ring magic in file {}: expected 0x{08:x}, got 0x{08:x}", _common.filename, ring_magic, hdr.magic);

	auto buf = mmap(nullptr, sizeof(hdr) + hdr.size, PROT_READ, MAP_SHARED | MAP_POPULATE, _common.fd, 0);
	if (buf == MAP_FAILED)
		return _log.fail(EINVAL, "Failed to mmap memory from {}: {}", _common.filename, strerror(errno));

	ringbuffer_t ring = { static_cast<ring_header_t *>(buf) };
	if (ring_iter_init(&ring, &_iter))
		return _log.fail(EINVAL, "Failed to init iterator: writer is too fast");

	_buf.resize(hdr.size / 4);

	_dcaps_pending(true);

	return Base::_open(cfg);
}

int PubMemClient::_close()
{
	_common.close(_iter.header, _log);
	_iter = {};

	return Base::_close();
}

int PubMemClient::_process(long timeout, int flags)
{
	tll_msg_t msg = { TLL_MESSAGE_DATA };
	const Frame * frame;
	size_t size;

	if (auto r = ring_iter_read(&_iter, (const void **) &frame, &size); r) {
		if (r == EAGAIN) return r;
		return _log.fail(EINVAL, "Ring iterator invalidated");
	}

	if (size > _buf.size())
		return _log.fail(EMSGSIZE, "Got invalid payload size {} > max size {}", size, _buf.size());

	memcpy(_buf.data(), frame, size);
	if (ring_iter_invalid(&_iter))
		return _log.fail(EINVAL, "Ring iterator invalidated");

	frame = (const Frame *) _buf.data();

	if (size < sizeof(Frame))
		return _log.fail(EMSGSIZE, "Got invalid payload size {} < {}", size, sizeof(Frame));
	ring_iter_shift(&_iter);

	msg.seq = frame->seq;
	msg.msgid = frame->msgid;
	msg.size = size - sizeof(Frame);
	msg.data = frame + 1;
	_callback_data(&msg);
	return 0;
}
