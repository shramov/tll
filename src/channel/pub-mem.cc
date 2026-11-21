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
#include "tll/util/scoped_fd.h"
#include "tll/util/size.h"
#include "tll/util/sockaddr.h"
#include "tll/util/tempfile.h"

#include "tll/compat/fallocate.h"
#include "tll/compat/fmt/std.h"

#include <fcntl.h>
#include <poll.h>
#ifdef __linux__
#include <sys/eventfd.h>
#endif
#include <sys/file.h>
#include <sys/mman.h>
#include <unistd.h>

#include <array>
#include <filesystem>
#include <utility>

#ifndef MAP_POPULATE
#define MAP_POPULATE 0
#endif

#ifndef MSG_CMSG_CLOEXEC
#define MSG_CMSG_CLOEXEC 0
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

struct TailExtra
{
	std::atomic<long long> seq;

	void update(long long v) { seq.store(v, std::memory_order_release); }
};

class MemServer : public tll::channel::Base<MemServer>
{
	using Base = tll::channel::Base<MemServer>;

	std::string _filename;
	tll::network::sockaddr_any _addr;
	bool _unlink_socket = false;

	std::vector<int> _fds;
	static constexpr size_t max_fds = 16;

 public:
	static constexpr std::string_view channel_protocol() { return "pub+mem"; }
	static constexpr auto process_api_version() { return ProcessAPI::Void; }

	int _init(const tll::Channel::Url &, tll::Channel *master);
	int _open(const tll::ConstConfig &);
	int _close();

	int _process() const;

	int fds(std::vector<int> fds)
	{
		if (fds.size() > max_fds)
			return _log.fail(EINVAL, "Fd list too large: {} > max {}", fds.size(), max_fds);
		_fds = std::move(fds);
		return 0;
	}
};

TLL_DEFINE_IMPL(MemServer);

template <typename T>
struct MemCommon : public tll::channel::Base<T>
{
	using Base = tll::channel::Base<T>;

 protected:
	std::filesystem::path _socketname;
	std::filesystem::path _filename;
	bool _unlink = false;
	size_t _size = 0;

	tll::util::ScopedFd _efd, _sock, _mfd;

	enum Mode { Client, Server, PubClient, SubServer } _mode = Client;

 public:
	static constexpr auto process_api_version() { return Base::ProcessAPI::Void; }

	int _init(const tll::Channel::Url &url, tll::Channel *master)
	{
		_socketname = url.host();
		if (_socketname.empty())
			return this->_log.fail(EINVAL, "Empty or missing filename");

		auto reader = this->channel_props_reader(url);
		_mode = reader.getT("mode", Client, {{"server", Server}, {"pub-client", PubClient}, {"client", Client}, {"sub-server", SubServer}});
		if (_mode == PubClient || _mode == SubServer)
			std::swap(_filename, _socketname);
		_filename = reader.getT("memory-file", _filename.string());
		if (_mode == Server || _mode == SubServer)
			_size = reader.getT("size", util::Size {64 * 1024});
		if (!reader)
			return this->_log.fail(EINVAL, "Invalid url: {}", reader.error());

		if (_filename.empty()) {
			if (_socketname.empty())
				return this->_log.fail(EINVAL, "Empty memory-file parameter");
			if (_mode == Server) // TODO: Use memfd
				_filename.assign(_socketname) += ".mem";
		}

		return Base::_init(url, master);
	}

	tll::PubRing * _file_create();
	tll::PubRing * _file_open(const std::filesystem::path &filename, bool readwrite);

	tll::PubRing * _ring_open(bool readwrite);

	int _open(const tll::ConstConfig &cfg)
	{
		if (auto r = Base::_open(cfg); r)
			return r;
		if (_mode == Server || _mode == SubServer)
			return _client_export();
		return 0;
	}

	int _client_export();

	int _close()
	{
		this->_update_fd(-1);

		_efd.reset();
		_mfd.reset();
		_sock.reset();
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
	long long _seq = -1;

 public:
	static constexpr std::string_view channel_protocol() { return "pub+mem"; }
	static constexpr std::string_view param_prefix() { return "pub"; }
	static constexpr auto readwrite_policy() { return ReadWritePolicy::ReadOnly; }
	static constexpr auto open_policy() { return OpenPolicy::Manual; }

	constexpr std::string_view scheme_control_string() const
	{
		if (_mode == SubServer)
			return scheme_string;
		return "";
	}

	int _open(const tll::ConstConfig &);
	int _close();

	int _process();
	int _process_connect();
	int _process_open();
};

class MemPub : public tll::channel::LastSeqTx<MemPub, MemCommon<MemPub>>
{
	using Base = tll::channel::LastSeqTx<MemPub, MemCommon<MemPub>>;

	tll::PubRing * _ring = nullptr;

	std::unique_ptr<tll::Channel> _fdserver;

 public:
	static constexpr std::string_view channel_protocol() { return "pub+mem"; }
	static constexpr std::string_view param_prefix() { return "pub"; }
	static constexpr auto process_policy() { return ProcessPolicy::Never; }
	static constexpr auto readwrite_policy() { return ReadWritePolicy::WriteOnly; }

	int _init(const tll::Channel::Url &cfg, tll::Channel * master)
	{
		if (auto r = Base::_init(cfg, master); r)
			return r;
		if (_mode == Server && !_socketname.empty()) {
			tll::Channel::Url cfg;
			cfg.proto("fdserver");
			cfg.host(_socketname.string());
			child_url_fill(cfg, "fdserver");
			_fdserver = context().channel(cfg, nullptr, &MemServer::impl);
			if (!_fdserver)
				return _log.fail(EINVAL, "Failed to create fd server channel");
			internal.caps |= caps::Parent;
			_child_add(_fdserver.get(), "fdserver");
		}
		return 0;
	}

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
	tll::util::TempFile tmp(_filename);
	if (!tmp)
		return this->_log.fail(nullptr, "Failed to create temporary file {}: {}", tmp.filename(), tmp.strerror());
	_mfd.reset(tmp.release_fd());

	auto full = sizeof(tll::Ring) + _size;
	if (auto r = posix_fallocate(_mfd, 0, full); r)
		return this->_log.fail(nullptr, "Failed to allocate {} bytes of space: {}", full, strerror(r));

	auto buf = mmap(nullptr, full, PROT_READ | PROT_WRITE, MAP_SHARED, _mfd, 0);
	if (buf == MAP_FAILED)
		return this->_log.fail(nullptr, "Failed to mmap memory: {}", strerror(errno));

	auto ring = static_cast<tll::PubRing *>(buf);
	ring->init(_size);
	ring->tail_extra<TailExtra>()->update(-1);

	_mfd.reset(open(tmp.filename().c_str(), O_RDONLY));
	if (_mfd == -1)
		return this->_log.fail(nullptr, "Failed to open memory file for reading: {}", strerror(errno));

	this->_log.info("Rename temporary file {} to {}", tmp.filename(), _filename);
	if (std::filesystem::rename(tmp.filename(), _filename, ec); ec)
		return this->_log.fail(nullptr, "Failed to rename temporary file '{}' to '{}': {}", tmp.filename(), _filename, ec.message());
	tmp.release();
	_unlink = true;

	return ring;
}

template <typename T>
tll::PubRing * MemCommon<T>::_file_open(const std::filesystem::path &filename, bool rw)
{
	_mfd.reset(::open(filename.c_str(), rw ? O_RDWR : O_RDONLY));
	if (_mfd == -1)
		return this->_log.fail(nullptr, "Failed to open file {}: {}", filename, strerror(errno));
	return _ring_open(rw);
}

template <typename T>
tll::PubRing * MemCommon<T>::_ring_open(bool rw)
{
	std::array<char, sizeof(tll::Ring)> hbuf;
	auto r = pread(_mfd, hbuf.data(), hbuf.size(), 0);
	if (r < 0)
		return this->_log.fail(nullptr, "Failed to read ring header from file {}: {}", _filename, strerror(errno));
	if ((size_t) r < hbuf.size())
		return this->_log.fail(nullptr, "Failed to read ring header from file {}: got {} bytes of {} needed", _filename, r, sizeof(hbuf.data()));
	auto hdr = (const tll::PubRing *) hbuf.data();
	if (hdr->magic() != hdr->Magic)
		return this->_log.fail(nullptr, "Invalid ring magic in file {}: expected 0x{:08x}, got 0x{:08x}", _filename, hdr->Magic, hdr->magic());

	const auto buf = mmap(nullptr, sizeof(*hdr) + hdr->size(), PROT_READ | (rw ? PROT_WRITE : 0), MAP_SHARED | MAP_POPULATE, _mfd, 0);
	if (buf == MAP_FAILED)
		return this->_log.fail(nullptr, "Failed to mmap memory from {}: {}", _filename, strerror(errno));

	return static_cast<tll::PubRing *>(buf);
}

template <typename T>
int MemCommon<T>::_client_export()
{
	tll::Config client;

	if (this->_scheme) {
		auto full = this->_scheme->dump("yamls+gz");
		if (auto s = this->_scheme->dump("sha256"); s) {
			client.set("init.scheme", *s);
			client.sub("scheme", true)->set(*s, *full);
		} else if (full)
			client.set("init.scheme", *full);
	}

	client.set("init.tll.proto", "pub+mem");
	const auto & fn = _socketname.empty() ? _filename : _socketname;
	client.set("init.tll.host", std::filesystem::weakly_canonical(fn).string());
	if (std::is_same_v<T, MemSub>)
		client.set("init.mode", "pub-client");
	this->_config.set("client", client);

	return 0;
}

int MemPub::_open(const tll::ConstConfig &cfg)
{
	if (auto r = Base::_open(cfg); r)
		return r;

	if (_with_fd) {
#ifdef __linux__
		_efd.reset(eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC));
		if (_efd == -1)
			return _log.fail(EINVAL, "Failed to create eventfd: {}", strerror(errno));
#endif
	}

	if (_fdserver && _fdserver->open())
		return _log.fail(EINVAL, "Failed to open fd server channel");

	if (_mode == Server)
		_ring = _file_create();
	else {
		_ring = _file_open(_filename, true);
		if (flock(_mfd, LOCK_EX | LOCK_NB))
			return _log.fail(EINVAL, "Failed to flock file descriptor: {}", strerror(errno));
	}

	if (!_ring)
		return _log.fail(EINVAL, "Failed to open ring buffer file '{}'", _filename);

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

	_ring->tail_extra<TailExtra>()->update(seq);

	Control * marker;
	while (_ring->write_begin((void **) &marker, sizeof(*marker))) {
		_ring->shift();
	}
	*marker = Control::Connect;
	_ring->write_end(marker, sizeof(*marker));

	if (auto c = tll::channel_cast<MemServer>(_fdserver.get()); c) {
		if (_efd != -1)
			c->fds({_mfd.get(), _efd.get()});
		else
			c->fds({_mfd.get()});
	}

	return 0;
}

int MemPub::_close()
{
	if (_fdserver)
		_fdserver->close();

	if (auto c = tll::channel_cast<MemServer>(_fdserver.get()); c)
		c->fds({});
	if (_ring) {
		Control * marker;
		while (_ring->write_begin((void **) &marker, sizeof(*marker))) {
			_ring->shift();
		}
		*marker = Control::Disconnect;
		_ring->write_end(marker, sizeof(*marker));

#ifdef __linux__
		if (_efd != -1)
			eventfd_write(_efd, 1);
#endif

		_unmap(_ring);
	}
	_ring = nullptr;

	if (_mfd != -1)
		flock(_mfd, LOCK_UN | LOCK_NB);

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
	// Seq is written after message so it is never in the future, it is ok when it is in the past
	_ring->tail_extra<TailExtra>()->update(msg->seq);
#ifdef __linux__
	if (_efd != -1)
		eventfd_write(_efd, 1);
#endif
	return 0;
}

int MemSub::_open(const tll::ConstConfig &cfg)
{
	if (auto r = Base::_open(cfg); r)
		return r;

	const tll::PubRing * ring = nullptr;
	std::error_code ec;
	namespace fs = std::filesystem;
	if (_mode == SubServer)
		ring = _file_create();
	else if (auto status = fs::status(_socketname, ec); ec)
		return _log.fail(EINVAL, "File or socket '{}' does not exist: {}", _socketname, ec.message());
	else if (!fs::exists(status))
		return _log.fail(ENOENT, "File or socket '{}' does not exist", _socketname);
	else if (fs::is_socket(status)) {
		auto addr = tll::network::sockaddr_any::from_path(_socketname.string());
		if (!addr)
			return _log.fail(EINVAL, "Invalid socket name '{}': {}", _socketname, addr.error());
		_sock.reset(socket(AF_UNIX, SOCK_STREAM, 0));
		if (_sock == -1)
			return _log.fail(EINVAL, "Failed to create socket: {}", strerror(errno));
		if (auto r = tll::network::nonblock(_sock); r)
			return _log.fail(r, "Failed to set nonblock: {}", strerror(r));
		_update_fd(_sock);
		auto r = connect(_sock, *addr, addr->size);
		if (r && errno != EINPROGRESS)
			return _log.fail(errno, "Failed to connect to {}: {}", _socketname, strerror(errno));
		else if (r)
			_dcaps_poll(tll::dcaps::CPOLLOUT);
		else
			_dcaps_poll(tll::dcaps::CPOLLIN);
		return 0;
	} else
		ring = _file_open(_socketname, false);

	if (!ring)
		return _log.fail(EINVAL, "Failed to open file '{}'", _filename);

	_seq = ring->tail_extra<TailExtra>()->seq.load(std::memory_order_acquire);
	config_info().set_ptr("seq", &_seq);

	_iter = ring->end();
	if (!_iter.valid())
		return _log.fail(EINVAL, "Failed to init iterator: writer is too fast");
	if (_with_fd)
		_dcaps_pending(!_iter.last());

	_buf.resize(ring->size() / 4);

	state(tll::state::Active);
	return 0;
}

int MemSub::_close()
{
	_unmap(_iter.ring);
	_iter = {};
	config_info().setT("seq", _seq);

	return Base::_close();
}

int MemSub::_process_connect()
{
	struct pollfd pfd = { fd(), POLLOUT };
	auto r = poll(&pfd, 1, 0);
	if (r < 0)
		return _log.fail(errno, "Failed to poll: {}", strerror(errno));
	if (r == 0 || (pfd.revents & (POLLOUT | POLLHUP)) == 0)
		return EAGAIN;

	int err = 0;
	socklen_t len = sizeof(err);
	if (getsockopt(fd(), SOL_SOCKET, SO_ERROR, &err, &len))
		return _log.fail(errno, "Failed to get connect status: {}", strerror(errno));
	if (err)
		return _log.fail(err, "Failed to connect: {}", strerror(err));

	this->_log.info("Connected");
	_dcaps_poll(tll::dcaps::CPOLLIN);
	return 0;
}

int MemSub::_process_open()
{
	if (internal.dcaps & tll::dcaps::CPOLLOUT)
		return _process_connect();

	char rbuf[16];
	alignas(cmsghdr) char cbuf[256];
	iovec iov = {rbuf, sizeof(rbuf)};
	msghdr mhdr = {};
	mhdr.msg_iov = &iov;
	mhdr.msg_iovlen = 1;
	mhdr.msg_control = cbuf;
	mhdr.msg_controllen = sizeof(cbuf);
	auto r = recvmsg(this->fd(), &mhdr, MSG_NOSIGNAL | MSG_CMSG_CLOEXEC);
	if (r < 0) {
		if (errno == EAGAIN)
			return EAGAIN;
		return _log.fail(EINVAL, "Failed to receive data: {}", strerror(errno));
	}

	_log.info("Received result: {}, data: {}, control {}", r, mhdr.msg_iov->iov_len, mhdr.msg_controllen);

	std::vector<tll::util::ScopedFd> fds;
	for (auto cmsg = CMSG_FIRSTHDR(&mhdr); cmsg; cmsg = CMSG_NXTHDR(&mhdr, cmsg)) {
		if (cmsg->cmsg_level != SOL_SOCKET || cmsg->cmsg_type != SCM_RIGHTS)
			continue;

		auto ptr = (const int *) CMSG_DATA(cmsg);
		auto count = (cmsg->cmsg_len - CMSG_LEN(0)) / sizeof(int);
		for (auto i = 0u; i < count; i++)
			fds.emplace_back(ptr[i]);
	}

	if (fds.empty())
		return _log.fail(EINVAL, "Socket not received");

	_mfd = std::move(fds[0]);
	if (_with_fd && fds.size() > 1)
		_efd = std::move(fds[1]);
	_update_fd(_efd.get());
	if (_efd != -1)
		_dcaps_poll(tll::dcaps::CPOLLIN | tll::dcaps::CPOLLET);
	else
		_dcaps_poll(0);
	_sock.reset();

	auto ring = _ring_open(false);

	if (!ring)
		return _log.fail(EINVAL, "Failed to open ring from fd {}", _mfd);

	_seq = ring->tail_extra<TailExtra>()->seq.load(std::memory_order_acquire);
	config_info().set_ptr("seq", &_seq);

	_iter = ring->end();
	if (!_iter.valid())
		return _log.fail(EINVAL, "Failed to init iterator: writer is too fast");
	if (_with_fd)
		_dcaps_pending(!_iter.last());

	_buf.resize(ring->size() / 4);
	state(tll::state::Active);
	return 0;
}

int MemSub::_process()
{
	if (state() == tll::state::Opening)
		return _process_open();
	tll_msg_t msg = { TLL_MESSAGE_DATA };
	const Frame * frame;
	size_t size;

	if (auto r = _iter.read((const void **) &frame, &size); r) {
		if (r == EAGAIN) {
			if (_with_fd)
				_dcaps_pending(!_iter.last());
			return r;
		}
		return _log.fail(EINVAL, "Ring iterator invalidated");
	}

	if (size > _buf.size())
		return _log.fail(EMSGSIZE, "Got invalid payload size {} > max size {}", size, _buf.size());

	memcpy(_buf.data(), frame, size);
	if (_iter.shift())
		return _log.fail(EINVAL, "Ring iterator invalidated");
	if (_with_fd)
		_dcaps_pending(!_iter.last());

	frame = (const Frame *) _buf.data();

	if (size < sizeof(Frame)) {
		if (size == sizeof(Control)) {
			auto control = *(const Control *) frame;
			switch (control) {
			case Control::Connect:
				_log.info("Publisher connected");
				if (_mode == SubServer)
					_callback({.type = TLL_MESSAGE_CONTROL, .msgid = scheme_msgid_connect});
				break;
			case Control::Disconnect:
				_log.info("Publisher is closed");
				if (_mode == SubServer)
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

	msg.seq = _seq = frame->seq;
	msg.msgid = frame->msgid;
	msg.size = size - sizeof(Frame);
	msg.data = frame + 1;
	_callback_data(&msg);
	return 0;
}

int MemServer::_init(const tll::Channel::Url &url, tll::Channel *master)
{
	_filename = url.host();
	if (auto r = tll::network::sockaddr_any::from_path(_filename); !r)
		return _log.fail(EINVAL, "Invalid socket path '{}': {}", _filename, r.error());
	else
		_addr = std::move(*r);
	return 0;
}

int MemServer::_open(const tll::ConstConfig &url)
{
	if (auto fd = socket(_addr->sa_family, SOCK_STREAM, 0); fd == -1)
		return this->_log.fail(errno, "Failed to create socket: {}", strerror(errno));
	else
		_update_fd(fd);
	_dcaps_poll(tll::dcaps::CPOLLIN);

	if (auto r = tll::network::nonblock(fd()); r)
		return _log.fail(r, "Failed to set nonblock: {}", strerror(r));

	_log.info("Listen on {}", this->_addr);

	// TODO: Handle stale socket
	if (bind(fd(), _addr, _addr.size))
		return _log.fail(errno, "Failed to bind: {}", strerror(errno));
	_unlink_socket = _addr.un()->sun_path[0] != 0; // Do not unlink abstract socket
	if (listen(fd(), 10))
		return _log.fail(errno, "Failed to listen: {}", strerror(errno));

	return Base::_open(url);
}

int MemServer::_close()
{
	if (std::exchange(_unlink_socket, false)) {
		this->_log.info("Unlink unix socket {}", _filename);
		if (unlink(_filename.c_str()))
			this->_log.warning("Failed to unlink socket {}: {}", _filename, strerror(errno));
	}
	if (auto fd = _update_fd(-1); fd != -1)
		::close(fd);
	return Base::_close();
}

int MemServer::_process() const
{
	tll::network::sockaddr_any addr = {};
	addr.size = sizeof(addr.buf);

	tll::network::scoped_socket sock(accept(this->fd(), addr, &addr.size)); //XXX: accept4
	if (sock == -1) {
		if (errno == EAGAIN || errno == EWOULDBLOCK)
			return EAGAIN;
		return _log.fail(errno, "Accept failed: {}", strerror(errno));
	}
	_log.debug("Accept fd {}", sock.get());

	if (auto r = tll::network::nonblock(sock); r)
		return _log.fail(r, "Failed to set nonblock: {}", strerror(r));

	char data[] = "FD";
	struct iovec iov = {.iov_base = data, .iov_len = sizeof(data)};

	msghdr m = {};
	m.msg_iov = &iov;
	m.msg_iovlen = 1;

	alignas(cmsghdr) char cbuf[CMSG_SPACE(max_fds * sizeof(int))];
	m.msg_control = cbuf;
	m.msg_controllen = CMSG_SPACE(_fds.size() * sizeof(int));
	struct cmsghdr * cmsg = CMSG_FIRSTHDR(&m);
	cmsg->cmsg_level = SOL_SOCKET;
	cmsg->cmsg_type = SCM_RIGHTS;
	cmsg->cmsg_len = CMSG_LEN(_fds.size() * sizeof(int));
	memcpy(CMSG_DATA(cmsg), _fds.data(), _fds.size() * sizeof(int));

	_log.debug("Send {} fds to client {}: {}", _fds.size(), (int) sock, _fds);
	if (auto r = sendmsg(sock, &m, MSG_NOSIGNAL); r < 0)
		return _log.fail(0, "Failed to send fds: {}", strerror(errno));
	else if ((size_t) r != sizeof(data))
		return _log.fail(0, "Failed to send fds (truncated): {}", strerror(errno));
	return 0;
}
