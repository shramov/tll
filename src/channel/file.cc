/*
 * Copyright (c) 2018-2021 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#include "channel/file.h"
#include "channel/file-init.h"
#include "channel/file-scheme.h"

#include "tll/util/size.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

#include <array>

using namespace tll;
using namespace tll::file;

static constexpr std::string_view control_scheme = R"(yamls://
- name: Seek
  id: 10
- name: EndOfData
  id: 20
)";
static constexpr int control_seek_msgid = 10;
static constexpr int control_eod_msgid = 20;

#ifdef __APPLE__
#if MAC_OS_X_VERSION_MIN_REQUIRED <= 1010

ssize_t pwritev(int fd, const struct iovec *iov, int iovcnt, off_t offset)
{
	if (lseek(fd, offset, SEEK_SET) < 0)
		return -1;
	return writev(fd, iov, iovcnt);
}

#endif
#endif

struct __attribute__((packed)) full_frame_t
{
	frame_size_t size = 0;
	frame_t frame;
};

struct IOBase
{
	int fd = -1;
	size_t offset = 0;
	size_t block_size = 0;

	int init(int fd, size_t block_size) { this->fd = fd; this->block_size = block_size; return 0; }
	void reset()
	{
		fd = -1;
		offset = 0;
		block_size = 0;
	}

	void shift(size_t size) { offset += size; }

	int writev(const struct iovec * iov, size_t size) { return ENOSYS; }

	int write(const void * data, size_t size) { return ENOSYS; }

	tll::const_memory read(size_t size) { return { nullptr, ENOSYS }; }
};

struct IOPosix : public IOBase
{
	static constexpr std::string_view name() { return "posix"; }

	std::vector<char> buf;

	int init(int fd, size_t block)
	{
		buf.resize(block);
		return IOBase::init(fd, block);
	}

	int writev(const struct iovec * iov, size_t size)
	{
		return pwritev(fd, iov, size, offset);
	}

	int write(const void * data, size_t size)
	{
		return pwrite(fd, data, size, offset);
	}

	tll::const_memory read(size_t size, size_t off = 0)
	{
		auto r = pread(fd, buf.data(), size, offset + off);
		if (r == (ssize_t) size)
			return { buf.data(), size };
		if (r < 0)
			return { nullptr, (size_t) errno };
		return { nullptr, EAGAIN };
	}
};

TLL_DEFINE_IMPL(tll::channel::FileInit);
TLL_DEFINE_IMPL(File<IOPosix>);

std::optional<const tll_channel_impl_t *> tll::channel::FileInit::_init_replace(const tll::Channel::Url &url, tll::Channel *master)
{
	auto reader = channel_props_reader(url);
	auto posix = reader.getT("io", true, {{ "posix", true }});
	if (!reader)
		return this->_log.fail(std::nullopt, "Invalid url: {}", reader.error());
	if (posix)
		return &File<IOPosix>::impl;
	return std::nullopt;
}

template <typename TIO>
int File<TIO>::_init(const tll::Channel::Url &url, tll::Channel *master)
{
	auto reader = this->channel_props_reader(url);
	_block_init = reader.template getT("block", util::Size {1024 * 1024});
	_compression = reader.template getT("compress", Compression::None, {{"no", Compression::None}, {"lz4", Compression::LZ4}});
	_autoclose = reader.template getT("autoclose", true);
	if (!reader)
		return this->_log.fail(EINVAL, "Invalid url: {}", reader.error());

	if (_compression != Compression::None)
		return this->_log.fail(EINVAL, "Compression not supported");

	_filename = url.host();

	if ((this->internal.caps & caps::InOut) == 0) // Defaults to input
		this->internal.caps |= caps::Input;
	if ((this->internal.caps & caps::InOut) == caps::InOut)
		return this->_log.fail(EINVAL, "file:// can be either read-only or write-only, need proper dir in parameters");

	if (this->internal.caps & caps::Input) {
		this->_scheme_control.reset(this->context().scheme_load(control_scheme));
		if (!this->_scheme_control.get())
			return this->_log.fail(EINVAL, "Failed to load control scheme");
	}

	return Base::_init(url, master);
}

template <typename TIO>
int File<TIO>::_open(const ConstConfig &props)
{
	auto filename = _filename;
	_end_of_data = false;

	if (filename.empty()) {
		auto fn = props.get("filename");
		if (!fn || fn->empty())
			return this->_log.fail(EINVAL, "No filename in init and no 'filename' parameter in open");
		filename = *fn;
	}

	this->_log.debug("Open file {}", filename);

	_io.offset = 0;
	_block_end = _block_size = _block_init;

	_seq = _seq_begin = -1;
	this->config_info().set_ptr("seq-begin", &_seq_begin);
	this->config_info().set_ptr("seq", &_seq);

	auto reader = tll::make_props_reader(props);
	if (this->internal.caps & caps::Input) {
		auto fd = ::open(filename.c_str(), O_RDONLY, 0644);
		if (fd == -1)
			return this->_log.fail(EINVAL, "Failed to open file {} for reading: {}", filename, strerror(errno));
		this->_update_fd(fd);

		if (auto r = _read_meta(); r)
			return this->_log.fail(EINVAL, "Failed to read metadata");

		if (_io.init(this->fd(), _block_size))
			return this->_log.fail(EINVAL, "Failed to init io");

		if (auto r = _file_bounds(); r && r != EAGAIN)
			return this->_log.fail(EINVAL, "Failed to load file bounds");

		auto seq = reader.getT<long long>("seq", 0);
		if (!reader)
			return this->_log.fail(EINVAL, "Invalid params: {}", reader.error());
		if (auto r = _seek(seq); r && r != EAGAIN)
			return this->_log.fail(EINVAL, "Seek failed");
		this->_update_dcaps(dcaps::Process | dcaps::Pending);
	} else {
		auto overwrite = reader.getT("overwrite", false);
		if (!reader)
			return this->_log.fail(EINVAL, "Invalid params: {}", reader.error());

		if (access(filename.c_str(), F_OK)) { // File not found, create new
			overwrite = true;
		} else {
			struct stat s;
			auto r = ::stat(filename.c_str(), &s);
			if (r < 0)
				return this->_log.fail(EINVAL, "Failed to get file size: {}", strerror(errno));
			if (s.st_size == 0) // Empty file
				overwrite = true;
		}

		std::string fn(filename);
		if (overwrite) {
			fn += ".XXXXXX";
			auto fd = mkstemp(fn.data());
			if (fd == -1)
				return this->_log.fail(EINVAL, "Failed to create temporary file {}: {}", fn, strerror(errno));
			this->_update_fd(fd);

			if (_write_meta()) {
				return this->_log.fail(EINVAL, "Failed to write metadata");
				unlink(fn.c_str());
			}
			this->_log.info("Rename temporary file {} to {}", fn, filename);
			rename(fn.c_str(), filename.c_str());
		} else {
			auto fd = ::open(fn.data(), O_RDWR | O_CREAT, 0600);
			if (fd == -1)
				return this->_log.fail(EINVAL, "Failed to open file {} for writing: {}", filename, strerror(errno));
			this->_update_fd(fd);

			if (auto r = _read_meta(); r)
				return this->_log.fail(EINVAL, "Failed to read metadata");
		}

		if (_io.init(this->fd(), _block_size))
			return this->_log.fail(EINVAL, "Failed to init io");

		if (auto r = _file_bounds(); r && r != EAGAIN)
			return this->_log.fail(EINVAL, "Failed to load file bounds");

		this->_autoseq.reset(_seq);

		auto size = _file_size();
		if (size != (ssize_t) _io.offset) {
			this->_log.warning("Trailing data in file: {} < {}", _io.offset, size);
			_truncate(_io.offset);
		}
	}

	this->config_info().setT("block", util::Size { _block_size });
	return 0;
}

template <typename TIO>
int File<TIO>::_close()
{
	auto fd = this->_update_fd(-1);
	if (fd != -1)
		::close(fd);
	_io.reset();
	this->config_info().setT("seq-begin", _seq_begin);
	this->config_info().setT("seq", _seq);
	return 0;
}

template <typename TIO>
void File<TIO>::_truncate(size_t offset)
{
	auto r = ftruncate(this->fd(), offset);
	(void) r;
}

template <typename TIO>
int File<TIO>::_check_write(size_t size, int r)
{
	if (r < 0) {
		return this->_log.fail(EINVAL, "Write failed: {}", strerror(errno));
	} else if (r != (ssize_t) size) {
		_truncate(_io.offset);
		return this->_log.fail(EINVAL, "Truncated write: {} of {} bytes written", r, size);
	}
	return 0;
}

template <typename TIO>
int File<TIO>::_read_meta()
{
	_io.offset = 0;
	full_frame_t frame;

	if (auto r = pread(this->fd(), &frame, sizeof(frame), 0); r != sizeof(frame)) {
		if (r < 0)
			return this->_log.fail(EINVAL, "Failed to read meta frame: {}", strerror(errno));
		return this->_log.fail(EINVAL, "Failed to read meta frame: truncated file");
	}

	if (frame.frame.msgid != file_scheme::Meta::meta_id())
		return this->_log.fail(EINVAL, "Not a tll data file: expected meta id {}, got {}", file_scheme::Meta::meta_id(), (int) frame.frame.msgid);

	size_t size = _data_size(frame.size);
	if (_data_size(size) < sizeof(frame.frame))
		return this->_log.fail(EINVAL, "Invalid frame size at 0x{:x}: {} too small", 0, (size_t) frame.size);

	std::vector<unsigned char> buf;
	buf.resize(size - sizeof(frame.frame) + 1); // Tail indicator

	if (auto r = pread(this->fd(), buf.data(), buf.size(), sizeof(frame)); r != (ssize_t) buf.size()) {
		if (r < 0)
			return this->_log.fail(EINVAL, "Failed to read meta data: {}", strerror(errno));
		return this->_log.fail(EINVAL, "Failed to read meta data: truncated file");
	}

	if ((buf.back() & 0x80u) == 0)
		return this->_log.fail(EINVAL, "Failed to read meta data: zero tail marker");
	buf.resize(buf.size() - 1);

	auto meta = file_scheme::Meta::bind(buf);
	if (buf.size() < meta.meta_size())
		return this->_log.fail(EINVAL, "Invalid meta size: {} less then minimum {}", buf.size(), meta.meta_size());

	_block_end = _block_size = meta.get_block();
	auto comp = meta.get_compression();

	this->_log.info("Meta info: block size {}, compression {}", _block_size, (uint8_t) comp);

	switch (comp) {
	case file_scheme::Meta::Compression::None:
		_compression = Compression::None;
		break;
	default:
		return this->_log.fail(EINVAL, "Compression {} not supported", (uint8_t) comp);
	}

	std::string_view scheme = meta.get_scheme();
	if (scheme.size()) {
		auto s = tll::Scheme::load(scheme);
		if (!s)
			return this->_log.fail(EINVAL, "Failed to load scheme");
		this->_scheme.reset(s);
	}

	return 0;
}

template <typename TIO>
int File<TIO>::_write_meta()
{
	_io.offset = 0;

	std::vector<uint8_t> buf;
	buf.resize(sizeof(full_frame_t));

	auto view = tll::make_view(buf, sizeof(full_frame_t));
	auto meta = file_scheme::Meta::bind(view);
	view.resize(meta.meta_size());
	meta.set_meta_size(meta.meta_size());
	meta.set_block(_block_size);
	meta.set_compression((file_scheme::Meta::Compression)_compression);

	if (this->_scheme) {
		auto s = this->_scheme->dump("yamls+gz");
		if (!s)
			return this->_log.fail(EINVAL, "Failed to serialize scheme");
		meta.set_scheme(*s);
	}

	buf.push_back(0x80u);

	this->_log.info("Write {} bytes of metadata ({})", buf.size(), meta.meta_size());

	view = tll::make_view(buf);
	*view.dataT<frame_size_t>() = buf.size();
	view = view.view(sizeof(frame_size_t));
	*view.dataT<frame_t>() = frame_t { meta.meta_id(), 0 };

	if (auto r = _check_write(buf.size(), pwrite(this->fd(), buf.data(), buf.size(), 0)); r)
		return r;
	_shift(buf.size());
	return 0;
}

template <typename TIO>
int File<TIO>::_shift(size_t size)
{
	this->_log.trace("Shift offset {} + {}", _io.offset, size);
	_io.offset += size;

	if (_io.offset + sizeof(full_frame_t) + 1 > _block_end) {
		this->_log.trace("Shift block to 0x{:x}", _block_end);
		_io.offset = _block_end;
		_block_end += _block_size;
	}

	return 0;
}

template <typename TIO>
ssize_t File<TIO>::_file_size()
{
	struct stat stat;
	auto r = fstat(this->fd(), &stat);
	if (r < 0)
		return this->_log.fail(-1, "Failed to get file size: {}", strerror(errno));
	return stat.st_size;
}

template <typename TIO>
int File<TIO>::_file_bounds()
{
	auto size = _file_size();
	if (size <= 0)
		return EINVAL;

	tll_msg_t msg;

	if (auto r = _block_seq(0, &msg); r) {
		if (r != EAGAIN)
			return this->_log.fail(EINVAL, "Failed to read seq of block {}", 0);
		return r;
	}

	_seq_begin = msg.seq;

	for (auto last = (size + _block_size - 1) / _block_size - 1; last > 0; last--) {
		auto r = _block_seq(last, &msg);
		if (r == 0)
			break;
		else if (r != EAGAIN)
			return this->_log.fail(EINVAL, "Failed to read seq of block {}", last);
	}

	do {
		_seq = msg.seq;
		frame_size_t frame;
		if (auto r = _read_frame(&frame); r) {
			if (r == EAGAIN)
				break;
			return r;
		}

		if (_io.offset + _block_size == _block_end) {
			_shift(frame);
			continue;
		}

		this->_log.trace("Check seq at 0x{:x}", _io.offset);
		auto r = _read_seq(frame, &msg);
		if (r == EAGAIN)
			break;
		else if (r)
			return r;
		_shift(&msg);
	} while (true);

	this->_log.info("First seq: {}, last seq: {}", _seq_begin, _seq);
	return 0;
}

template <typename TIO>
int File<TIO>::_seek(long long seq)
{
	auto size = _file_size();
	if (size < 0)
		return EINVAL;

	tll_msg_t msg;

	size_t first = 0;
	size_t last = (size + _block_size - 1) / _block_size - 1;

	for (; last > 0; last--) {
		auto r = _block_seq(last, &msg);
		if (r == 0) {
			this->_log.trace("Found data in block {}: seq {}", last, msg.seq);
			break;
		} else if (r != EAGAIN)
			return this->_log.fail(EINVAL, "Failed to read seq of block {}", last);
	}
	++last;

	if (auto r = _block_seq(0, &msg); r)
		return r; // Error or empty file

	first = _block_end / _block_size - 1; // Meta fill first block

	//if (*seq < msg.seq)
	//	return this->_log.warning(EINVAL, "Seek seq {} failed: first data seq is {}", seq, msg->seq);

	while (first + 1 < last) {
		this->_log.debug("Bisect blocks {} and {}", first, last);
		auto mid = (first + last) / 2;
		auto r = _block_seq(mid, &msg);
		if (r == EAGAIN) { // Empty block
			last = mid;
			continue;
		} else if (r)
			return r;
		this->_log.trace("Block {} seq: {}", mid, msg.seq);
		if (msg.seq == seq)
			return 0;
		if (msg.seq > seq)
			last = mid;
		else
			first = mid;
	}

	_io.offset = first * _block_size;
	_block_end = _io.offset + _block_size;

	do {
		frame_size_t frame;
		if (auto r = _read_frame(&frame); r)
			return r;

		if (_io.offset + _block_size == _block_end) {
			_shift(frame);
			continue;
		}

		this->_log.trace("Check seq at 0x{:x}", _io.offset);
		if (auto r = _read_seq(frame, &msg); r)
			return r;
		this->_log.trace("Message {}/{} at 0x{:x}", msg.seq, msg.size, _io.offset);
		if (msg.seq >= seq)
			break;
		_shift(&msg);
	} while (true);

	if (msg.seq > seq)
		this->_log.warning("Seek seq {}: found closest seq {}", seq, msg.seq);
	return 0;
}

template <typename TIO>
int File<TIO>::_block_seq(size_t block, tll_msg_t *msg)
{
	_io.offset = block * _block_size;
	_block_end = _io.offset + _block_size;

	// Skip metadata
	frame_size_t frame;
	if (auto r = _read_frame(&frame); r)
		return r;
	if (frame == -1)
		return EAGAIN;
	_shift(frame);

	return _read_seq(msg);
}

template <typename TIO>
int File<TIO>::_read_seq(tll_msg_t *msg)
{
	frame_size_t frame;

	if (auto r = _read_frame(&frame); r)
		return r;
	return _read_seq(frame, msg);
}

template <typename TIO>
int File<TIO>::_read_seq(frame_size_t frame, tll_msg_t *msg)
{
	size_t size = _data_size(frame);
	if (size < sizeof(frame_t))
		return this->_log.fail(EINVAL, "Invalid frame size at 0x{:x}: {} too small", _io.offset, frame);

	return _read_data(size, msg);
}

template <typename TIO>
int File<TIO>::_post(const tll_msg_t *msg, int flags)
{
	if (this->internal.caps & caps::Input) {
		if (msg->type == TLL_MESSAGE_CONTROL) {
			if (msg->msgid == control_seek_msgid) {
				if (auto r = _seek(msg->seq); r) {
					if (r != EAGAIN)
						this->_log.error("Seek failed: seq {} not found", msg->seq);
					else
						this->_log.info("Requested seq {} not available in file", msg->seq);
					return r;
				}
			}
			return 0;
		}
		return ENOSYS;
	}

	if (msg->type != TLL_MESSAGE_DATA)
		return 0;

	msg = this->_autoseq.update(msg);
	if (msg->seq <= _seq)
		return this->_log.fail(EINVAL, "Incorrect messsage seq: {} <= last seq {}", msg->seq, _seq);

	const size_t size = sizeof(full_frame_t) + msg->size;

	if (size > _block_size)
		return this->_log.fail(EMSGSIZE, "Message size too large: {}, block size is {}", msg->size, _block_size);

	frame_t meta = { msg };
	auto r = _write_datav(const_memory { &meta, sizeof(meta) }, const_memory { msg->data, msg->size });
	if (!r) {
		_seq = msg->seq;
		if (_seq_begin == -1)
			_seq_begin = _seq;
	}
	return r;
}

template <typename TIO>
template <typename ... Args>
int File<TIO>::_write_datav(Args && ... args)
{
	constexpr unsigned N = sizeof...(Args);
	std::array<const_memory, N> data({const_memory(std::forward<Args>(args))...});
	struct iovec iov[N + 2];

	frame_size_t frame;
	uint8_t tail = 0x80;
	iov[0].iov_base = &frame;
	iov[0].iov_len = sizeof(frame);
	iov[N + 1].iov_base = &tail;
	iov[N + 1].iov_len = sizeof(tail);

	size_t size = sizeof(frame) + 1;

	for (unsigned i = 0; i < N; i++) {
		iov[i + 1].iov_base = (void *) data[i].data;
		iov[i + 1].iov_len = data[i].size;
		size += data[i].size;
	}

	if (size > _block_size)
		return this->_log.fail(EMSGSIZE, "Full size too large: {}, block size is {}", size, _block_size);

	if (_io.offset + size > _block_end) {
		frame_size_t frame = -1;

		if (_write_raw(&frame, sizeof(frame)))
			return EINVAL;

		_io.offset = _block_end;
		_block_end += _block_size;
	}

	if (_io.offset + _block_size == _block_end) { // Write block meta
		static constexpr uint8_t header[5] = {5, 0, 0, 0, 0x80};
		if (_write_raw(&header, sizeof(header)))
			return EINVAL;
		_io.offset += sizeof(header);
	}

	frame = size;
	this->_log.trace("Write frame {} at {}", frame, _io.offset);
	if (_check_write(size, _io.writev(iov, N + 2)))
		return EINVAL;

	return _shift(size);
}

template <typename TIO>
int File<TIO>::_read_frame(frame_size_t *ptr)
{
	auto r = _io.read(sizeof(*ptr));

	if (!r.data) {
		if (r.size == EAGAIN)
			return EAGAIN;
		return this->_log.fail(EINVAL, "Failed to read frame at 0x{:x}: {}", _io.offset, strerror(errno));
	}

	auto frame = *static_cast<const frame_size_t *>(r.data);

	if (frame == 0)
		return EAGAIN;

	if (frame != -1) {
		if (frame < (ssize_t) sizeof(frame) + 1)
			return this->_log.fail(EMSGSIZE, "Invalid frame size at 0x{:x}: {} < minimum {}", _io.offset, frame, sizeof(frame) + 1);

		if (_io.offset + frame > _block_end)
			return this->_log.fail(EMSGSIZE, "Invalid frame size at 0x{:x}: {} excedes block boundary", _io.offset, frame);
		*ptr = frame;
		return 0;
	}

	this->_log.trace("Found skip frame at offset 0x{:x}", _io.offset);
	if (_io.offset + _block_size == _block_end)
		return EAGAIN;

	_io.offset = _block_end;
	_block_end += _block_size;

	return _read_frame(ptr);
}

template <typename TIO>
int File<TIO>::_read_data(size_t size, tll_msg_t *msg)
{
	this->_log.trace("Read {} bytes of data at {} + {}", size, _io.offset, sizeof(frame_size_t));
	auto r = _io.read(size + 1, sizeof(frame_size_t));

	if (!r.data) {
		if (r.size == EAGAIN)
			return EAGAIN;
		return this->_log.fail(EINVAL, "Failed to read data at 0x{:x}: {}", _io.offset, strerror(r.size));
	}

	if ((((const uint8_t *) r.data)[size] & 0x80) == 0) // No tail marker
		return EAGAIN;

	auto meta = (frame_t *) r.data;
	msg->msgid = meta->msgid;
	msg->seq = meta->seq;
	msg->size = size - sizeof(*meta);
	msg->data = meta + 1;

	return 0;
}

template <typename TIO>
int File<TIO>::_process(long timeout, int flags)
{
	tll_msg_t msg = { TLL_MESSAGE_DATA };
	frame_size_t frame;

	auto r = _read_frame(&frame);

	if (r == EAGAIN) {
		if (!_end_of_data) {
			if (_autoclose) {
				this->_log.info("All messages processed. Closing");
				this->close();
				return EAGAIN;
			}
			_end_of_data = true;
			tll_msg_t msg = { TLL_MESSAGE_CONTROL };
			msg.msgid = control_eod_msgid;
			this->_callback(&msg);
		} else
			this->_dcaps_pending(false);
		return EAGAIN;
	} else if (r)
		return r;

	if (_io.offset + _block_size == _block_end) {
		_shift(frame);
		return _process(timeout, flags);
	}

	size_t size = _data_size(frame);
	if (size < sizeof(frame_t))
		return this->_log.fail(EINVAL, "Invalid frame size at 0x{:x}: {} too small", _io.offset, frame);

	r = _read_data(size, &msg);

	if (r == EAGAIN) {
		this->_dcaps_pending(false);
		return EAGAIN;
	} else if (r)
		return r;

	_shift(frame);

	this->_callback_data(&msg);

	return 0;
}
