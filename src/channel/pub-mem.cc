/*
 * Copyright (c) 2018-2021 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#include "channel/pub-mem.h"

#include "tll/channel/frame.h"
#include "tll/channel/lastseq.h"
#include "tll/cppring.h"
#include "tll/util/size.h"

#include "tll/compat/fallocate.h"

#include <fcntl.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <unistd.h>

#include <array>
#include <filesystem>
#include <utility>

#ifndef MAP_POPULATE
#define MAP_POPULATE 0
#endif

using namespace tll;

constexpr std::string_view scheme_string = R"(yamls://
- name: Connect
  id: 10

- name: Disconnect
  id: 20
)";

constexpr auto scheme_msgid_connect = 10;
constexpr auto scheme_msgid_disconnect = 20;

struct Frame {
	int64_t seq;
	int32_t msgid;
};

enum class Control : uint32_t
{
	Connect = 1,
	Disconnect = 2,
};

template <typename T>
struct MemCommon : public tll::channel::Base<T>
{
	using Base = tll::channel::Base<T>;

 protected:
	std::string _filename;
	bool _create = false;
	bool _unlink = false;
	size_t _size = 0;

	int _fd = -1;

 public:
	int _init(const tll::Channel::Url &url, tll::Channel *master)
	{
		_filename = url.host();
		if (_filename.empty())
			return this->_log.fail(EINVAL, "Empty or missing filename");

		auto reader = this->channel_props_reader(url);
		_create = reader.getT("mode", false, {{"server", true}, {"pub-client", false}, {"client", false}, {"sub-server", true}});
		if (_create)
			_size = reader.getT("size", util::Size {64 * 1024});
		if (!reader)
			return this->_log.fail(EINVAL, "Invalid url: {}", reader.error());

		return Base::_init(url, master);
	}

	tll::PubRing * _file_create();
	tll::PubRing * _file_open(bool readwrite);

	int _close()
	{
		if (auto fd = std::exchange(_fd, -1); fd != -1)
			::close(fd);
		if (_unlink)
			unlink(_filename.c_str());

		_unlink = false;
		return 0;
	}

	void _unmap(const tll::PubRing * header)
	{
		if (header && munmap((void *) header, header->size() + sizeof(*header)))
			this->_log.error("Failed to unmap ring of size {}: {}", header->size() + sizeof(*header), strerror(errno));
	}
};

class MemSub : public tll::channel::LastSeqRx<MemSub, MemCommon<MemSub>>
{
	using Base = tll::channel::LastSeqRx<MemSub, MemCommon<MemSub>>;

	tll::PubRing::Iterator _iter = {};
	std::vector<char> _buf;

 public:
	static constexpr std::string_view channel_protocol() { return "pub+mem"; }
	static constexpr std::string_view param_prefix() { return "pub"; }

	constexpr std::string_view scheme_control_string() const
	{
		if (_create)
			return scheme_string;
		return "";
	}

	int _open(const tll::ConstConfig &);
	int _close();

	int _process(long timeout, int flags);
};

class MemPub : public tll::channel::LastSeqTx<MemPub, MemCommon<MemPub>>
{
	using Base = tll::channel::LastSeqTx<MemPub, MemCommon<MemPub>>;

	tll::PubRing * _ring = nullptr;

 public:
	static constexpr std::string_view channel_protocol() { return "pub+mem"; }
	static constexpr std::string_view param_prefix() { return "pub"; }
	static constexpr auto process_policy() { return ProcessPolicy::Never; }

	int _open(const tll::ConstConfig &);
	int _close();

	int _post(const tll_msg_t *msg, int flags);
};

TLL_DEFINE_IMPL(ChPubMem);
TLL_DEFINE_IMPL(MemPub);
TLL_DEFINE_IMPL(MemSub);

std::optional<const tll_channel_impl_t *> ChPubMem::_init_replace(const tll::Channel::Url &url, tll::Channel *master)
{
	auto reader = channel_props_reader(url);
	auto pub = reader.getT("mode", false, {{"server", true}, {"pub-client", true}, {"client", false}, {"sub-server", false}});
	if (!reader)
		return _log.fail(std::nullopt, "Invalid url: {}", reader.error());

	if (pub)
		return &MemPub::impl;
	return &MemSub::impl;
}

template <typename T>
tll::PubRing * MemCommon<T>::_file_create()
{
	std::error_code ec, uec;
	std::string fn = _filename + ".XXXXXX";
	_fd = mkstemp(fn.data());
	if (_fd == -1)
		return this->_log.fail(nullptr, "Failed to create temporary file {}: {}", fn, strerror(errno));

	auto full = sizeof(tll::Ring) + _size;
	if (auto r = posix_fallocate(_fd, 0, full); r) {
		std::filesystem::remove(fn, uec);
		return this->_log.fail(nullptr, "Failed to allocate {} bytes of space: {}", full, strerror(r));
	}

	auto buf = mmap(nullptr, full, PROT_READ | PROT_WRITE, MAP_SHARED, _fd, 0);
	if (buf == MAP_FAILED) {
		std::filesystem::remove(fn, uec);
		return this->_log.fail(nullptr, "Failed to mmap memory: {}", strerror(errno));
	}

	auto ring = static_cast<tll::PubRing *>(buf);
	ring->init(_size);

	this->_log.info("Rename temporary file {} to {}", fn, _filename);
	if (std::filesystem::rename(fn, _filename, ec); ec) {
		std::filesystem::remove(fn, uec);
		return this->_log.fail(nullptr, "Failed to rename temporary file '{}' to '{}': {}", fn, _filename, ec.message());
	}
	_unlink = true;

	return ring;
}

template <typename T>
tll::PubRing * MemCommon<T>::_file_open(bool rw)
{
	_fd = ::open(_filename.c_str(), rw ? O_RDWR : O_RDONLY);
	if (_fd == -1)
		return this->_log.fail(nullptr, "Failed to open file {}: {}", _filename, strerror(errno));

	std::array<char, sizeof(tll::Ring)> hbuf;
	auto r = read(_fd, hbuf.data(), hbuf.size());
	if (r < 0)
		return this->_log.fail(nullptr, "Failed to read ring header from file {}: {}", _filename, strerror(errno));
	if ((size_t) r < hbuf.size())
		return this->_log.fail(nullptr, "Failed to read ring header from file {}: got {} bytes of {} needed", _filename, r, sizeof(hbuf.data()));
	auto hdr = (const tll::PubRing *) hbuf.data();
	if (hdr->magic() != hdr->Magic)
		return this->_log.fail(nullptr, "Invalid ring magic in file {}: expected 0x{:08x}, got 0x{:08x}", _filename, hdr->Magic, hdr->magic());

	const auto buf = mmap(nullptr, sizeof(*hdr) + hdr->size(), PROT_READ | (rw ? PROT_WRITE : 0), MAP_SHARED | MAP_POPULATE, _fd, 0);
	if (buf == MAP_FAILED)
		return this->_log.fail(nullptr, "Failed to mmap memory from {}: {}", _filename, strerror(errno));

	return static_cast<tll::PubRing *>(buf);
}

int MemPub::_open(const tll::ConstConfig &cfg)
{
	if (auto r = Base::_open(cfg); r)
		return r;
	if (_create)
		_ring = _file_create();
	else
		_ring = _file_open(true);

	if (!_ring)
		return _log.fail(EINVAL, "Failed to open ring buffer file '{}'", _filename);

	if (flock(_fd, LOCK_EX | LOCK_NB))
		return _log.fail(EINVAL, "Failed to flock file descriptor: {}", strerror(errno));

	auto it = _ring->begin();
	const Frame * frame;
	size_t size;

	long long seq = -1;
	while (it.read((const void **) &frame, &size) == 0) {
		if (size >= sizeof(Frame))
			seq = frame->seq;
		it.shift();
	}

	if (seq >= 0)
		_log.info("Last seq in the ring: {}", seq);

	Control * marker;
	while (_ring->write_begin((void **) &marker, sizeof(*marker))) {
		_ring->shift();
	}
	*marker = Control::Connect;
	_ring->write_end(marker, sizeof(*marker));

	return 0;
}

int MemPub::_close()
{
	if (_ring) {
		Control * marker;
		while (_ring->write_begin((void **) &marker, sizeof(*marker))) {
			_ring->shift();
		}
		*marker = Control::Disconnect;
		_ring->write_end(marker, sizeof(*marker));
	}

	if (_fd != -1)
		flock(_fd, LOCK_UN | LOCK_NB);

	_unmap(_ring);

	return Base::_close();
}

int MemPub::_post(const tll_msg_t *msg, int flags)
{
	if (msg->type != TLL_MESSAGE_DATA)
		return 0;

	if (msg->size > _ring->size() / 4)
		return _log.fail(EMSGSIZE, "Message size too large: {} > max {}", msg->size, _size / 4);

	Frame * frame;
	const size_t size = sizeof(Frame) + msg->size;

	while (_ring->write_begin((void **) &frame, size)) {
		_ring->shift();
	}

	frame->seq = msg->seq;
	frame->msgid = msg->msgid;
	memcpy(frame + 1, msg->data, msg->size);
	_ring->write_end(frame, size);
	return 0;
}

int MemSub::_open(const tll::ConstConfig &cfg)
{
	const tll::PubRing * ring = nullptr;
	if (_create)
		ring = _file_create();
	else
		ring = _file_open(false);

	if (!ring)
		return _log.fail(EINVAL, "Failed to open file '{}'", _filename);

	_iter = ring->end();
	if (!_iter.valid())
		return _log.fail(EINVAL, "Failed to init iterator: writer is too fast");

	_buf.resize(ring->size() / 4);

	_dcaps_pending(true);

	return Base::_open(cfg);
}

int MemSub::_close()
{
	_unmap(_iter.ring);
	_iter = {};

	return Base::_close();
}

int MemSub::_process(long timeout, int flags)
{
	tll_msg_t msg = { TLL_MESSAGE_DATA };
	const Frame * frame;
	size_t size;

	if (auto r = _iter.read((const void **) &frame, &size); r) {
		if (r == EAGAIN) return r;
		return _log.fail(EINVAL, "Ring iterator invalidated");
	}

	if (size > _buf.size())
		return _log.fail(EMSGSIZE, "Got invalid payload size {} > max size {}", size, _buf.size());

	memcpy(_buf.data(), frame, size);
	if (_iter.shift())
		return _log.fail(EINVAL, "Ring iterator invalidated");

	frame = (const Frame *) _buf.data();

	if (size < sizeof(Frame)) {
		if (size == sizeof(Control)) {
			auto control = *(const Control *) frame;
			switch (control) {
			case Control::Connect:
				_log.info("Publisher connected");
				if (_create)
					_callback({.type = TLL_MESSAGE_CONTROL, .msgid = scheme_msgid_connect});
				break;
			case Control::Disconnect:
				_log.info("Publisher is closed");
				if (_create)
					_callback({.type = TLL_MESSAGE_CONTROL, .msgid = scheme_msgid_disconnect});
				else
					this->close();
				break;
			default:
				return _log.fail(EINVAL, "Unknown control message: {}", (uint32_t) control);
			}
			return 0;
		}
		return _log.fail(EMSGSIZE, "Got invalid payload size {} < {}", size, sizeof(Frame));
	}

	msg.seq = frame->seq;
	msg.msgid = frame->msgid;
	msg.size = size - sizeof(Frame);
	msg.data = frame + 1;
	_callback_data(&msg);
	return 0;
}
