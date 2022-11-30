/*
 * Copyright (c) 2018-2021 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#include "channel/file.h"
#include "channel/file-scheme.h"

#include "tll/util/size.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

#include <array>

using namespace tll;

using File = tll::channel::File;

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
	File::frame_size_t size;
	File::frame_t frame;
};

TLL_DEFINE_IMPL(File);

int File::_init(const tll::Channel::Url &url, tll::Channel *master)
{
	auto reader = channel_props_reader(url);
	_block_init = reader.getT("block", util::Size {1024 * 1024});
	_compression = reader.getT("compress", Compression::None, {{"no", Compression::None}, {"lz4", Compression::LZ4}});
	_autoclose = reader.getT("autoclose", true);
	if (!reader)
		return _log.fail(EINVAL, "Invalid url: {}", reader.error());

	if (_compression != Compression::None)
		return _log.fail(EINVAL, "Compression not supported");

	_buf.resize(_block_init);

	_filename = url.host();
	if (_filename.empty())
		return _log.fail(EINVAL, "Empty file name");

	if ((internal.caps & caps::InOut) == caps::InOut)
		return _log.fail(EINVAL, "file:// can be either read-only or write-only, need proper dir in parameters");
	return 0;
}

int File::_open(const ConstConfig &props)
{
	_log.debug("Open file {}", _filename);

	_offset = 0;
	_block_end = _block_size = _block_init;

	_seq = _seq_begin = -1;
	_config.set_ptr("seq-begin", &_seq_begin);
	_config.set_ptr("seq", &_seq);

	auto reader = tll::make_props_reader(props);
	if (internal.caps & caps::Input) {
		auto fd = ::open(_filename.c_str(), O_RDONLY, 0644);
		if (fd == -1)
			return _log.fail(EINVAL, "Failed to open file {} for writing: {}", _filename, strerror(errno));
		_update_fd(fd);

		if (auto r = _read_meta(); r)
			return _log.fail(EINVAL, "Failed to read metadata");

		if (auto r = _file_bounds(); r && r != EAGAIN)
			return _log.fail(EINVAL, "Failed to load file bounds");

		auto seq = reader.getT<long long>("seq", 0);
		if (!reader)
			return _log.fail(EINVAL, "Invalid params: {}", reader.error());
		if (auto r = _seek(seq); r && r != EAGAIN)
			return _log.fail(EINVAL, "Seek failed");
		_update_dcaps(dcaps::Process | dcaps::Pending);
	} else {
		auto overwrite = reader.getT("overwrite", false);
		if (!reader)
			return _log.fail(EINVAL, "Invalid params: {}", reader.error());

		if (access(_filename.c_str(), F_OK)) { // File not found, create new
			overwrite = true;
		} else {
			struct stat s;
			auto r = ::stat(_filename.c_str(), &s);
			if (r < 0)
				return _log.fail(EINVAL, "Failed to get file size: {}", strerror(errno));
			if (s.st_size == 0) // Empty file
				overwrite = true;
		}

		std::string fn(_filename);
		if (overwrite) {
			fn += ".XXXXXX";
			auto fd = mkstemp(fn.data());
			if (fd == -1)
				return _log.fail(EINVAL, "Failed to create temporary file {}: {}", fn, strerror(errno));
			_update_fd(fd);

			if (_write_meta()) {
				return _log.fail(EINVAL, "Failed to write metadata");
				unlink(fn.c_str());
			}
			_log.info("Rename temporary file {} to {}", fn, _filename);
			rename(fn.c_str(), _filename.c_str());
		} else {
			auto fd = ::open(fn.data(), O_RDWR | O_CREAT, 0600);
			if (fd == -1)
				return _log.fail(EINVAL, "Failed to open file {} for writing: {}", _filename, strerror(errno));
			_update_fd(fd);

			if (auto r = _read_meta(); r)
				return _log.fail(EINVAL, "Failed to read metadata");
		}

		if (auto r = _file_bounds(); r && r != EAGAIN)
			return _log.fail(EINVAL, "Failed to load file bounds");

		auto size = _file_size();
		if (size != (ssize_t) _offset) {
			_log.warning("Trailing data in file: {} < {}", _offset, size);
			_truncate(_offset);
		}
	}
	_buf.resize(_block_size);
	_config.setT("block", util::Size { _block_size });
	return 0;
}

int File::_close()
{
	auto fd = this->_update_fd(-1);
	if (fd != -1)
		::close(fd);
	_config.setT("seq-begin", _seq_begin);
	_config.setT("seq", _seq);
	return 0;
}

void File::_truncate(size_t offset)
{
	auto r = ftruncate(fd(), offset);
	(void) r;
}

int File::_check_write(size_t size, int r)
{
	if (r < 0) {
		return _log.fail(EINVAL, "Write failed: {}", strerror(errno));
	} else if (r != (ssize_t) size) {
		_truncate(_offset);
		return _log.fail(EINVAL, "Truncated write: {} of {} bytes written", r, size);
	}
	return 0;
}

int File::_read_meta()
{
	_offset = 0;
	frame_size_t frame;

	if (auto r = _read_frame(&frame); r)
		return _log.fail(EINVAL, "Failed to read meta frame");

	size_t size = _data_size(frame);
	if (size < sizeof(frame_t))
		return _log.fail(EINVAL, "Invalid frame size at 0x{:x}: {} too small", _offset, frame);

	tll_msg_t msg = { TLL_MESSAGE_DATA };
	if (auto r = _read_data(size, &msg); r)
		return _log.fail(EINVAL, "Failed to read meta data");

	auto meta = file_scheme::Meta::bind(msg);
	if (msg.msgid != meta.meta_id())
		return _log.fail(EINVAL, "Invalid meta id: expected {}, got {}", msg.msgid, meta.meta_id());
	if (msg.size < meta.meta_size())
		return _log.fail(EINVAL, "Invalid meta size: {} less then minimum {}", msg.size, meta.meta_size());

	_block_end = _block_size = meta.get_block();
	auto comp = meta.get_compression();

	_log.info("Meta info: block size {}, compression {}", _block_size, (uint8_t) comp);

	switch (comp) {
	case file_scheme::Meta::Compression::None:
		_compression = Compression::None;
		break;
	default:
		return _log.fail(EINVAL, "Compression {} not supported", (uint8_t) comp);
	}

	std::string_view scheme = meta.get_scheme();
	if (scheme.size()) {
		auto s = tll::Scheme::load(scheme);
		if (!s)
			return _log.fail(EINVAL, "Failed to load scheme");
		_scheme.reset(s);
	}

	_buf.resize(_block_size);
	return 0;
}

int File::_write_meta()
{
	_offset = 0;

	std::vector<uint8_t> buf;
	buf.resize(sizeof(full_frame_t));

	auto view = tll::make_view(buf, sizeof(full_frame_t));
	auto meta = file_scheme::Meta::bind(view);
	view.resize(meta.meta_size());
	meta.set_meta_size(meta.meta_size());
	meta.set_block(_block_size);
	meta.set_compression((file_scheme::Meta::Compression)_compression);

	if (_scheme) {
		auto s = tll_scheme_dump(_scheme.get(), nullptr);
		if (!s)
			return _log.fail(EINVAL, "Failed to serialize scheme");
		meta.set_scheme(s);
		::free(s);
	}

	buf.push_back(0x80u);

	_log.info("Write {} bytes of metadata ({})", buf.size(), meta.meta_size());

	view = tll::make_view(buf);
	*view.dataT<frame_size_t>() = buf.size();
	view = view.view(sizeof(frame_size_t));
	*view.dataT<frame_t>() = frame_t { meta.meta_id(), 0 };

	_offset = buf.size();
	return _write_raw(buf.data(), buf.size(), 0);
}

int File::_shift(size_t size)
{
	_log.trace("Shift offset {} + {}", _offset, size);
	_offset += size;

	if (_offset + sizeof(full_frame_t) + 1 > _block_end) {
		_log.trace("Shift block to 0x{:x}", _block_end);
		_offset = _block_end;
		_block_end += _block_size;
	}

	return 0;
}

ssize_t File::_file_size()
{
	struct stat stat;
	auto r = fstat(fd(), &stat);
	if (r < 0)
		return _log.fail(-1, "Failed to get file size: {}", strerror(errno));
	return stat.st_size;
}

int File::_file_bounds()
{
	auto size = _file_size();
	if (size < 0)
		return EINVAL;

	tll_msg_t msg;

	if (auto r = _block_seq(0, &msg); r) {
		if (r != EAGAIN)
			return _log.fail(EINVAL, "Failed to read seq of block {}", 0);
		return r;
	}

	_seq_begin = msg.seq;

	for (auto last = size / _block_size; last > 0; last--) {
		auto r = _block_seq(last, &msg);
		if (r == 0)
			break;
		else if (r != EAGAIN)
			return _log.fail(EINVAL, "Failed to read seq of block {}", last);
	}

	do {
		_seq = msg.seq;
		frame_size_t frame;
		if (auto r = _read_frame(&frame); r) {
			if (r == EAGAIN)
				break;
			return r;
		}

		if (_offset + _block_size == _block_end) {
			_shift(frame);
			continue;
		}

		_log.trace("Check seq at 0x{:x}", _offset);
		auto r = _read_seq(frame, &msg);
		if (r == EAGAIN)
			break;
		else if (r)
			return r;
		_shift(&msg);
	} while (true);

	_log.info("First seq: {}, last seq: {}", _seq_begin, _seq);
	return 0;
}

int File::_seek(long long seq)
{
	auto size = _file_size();
	if (size < 0)
		return EINVAL;

	tll_msg_t msg;

	size_t first = 0;
	size_t last = size / _block_size;

	for (; last > 0; last--) {
		auto r = _block_seq(last, &msg);
		if (r == 0) {
			_log.trace("Found data in block {}: seq {}", last, msg.seq);
			break;
		} else if (r != EAGAIN)
			return _log.fail(EINVAL, "Failed to read seq of block {}", last);
	}
	++last;

	if (auto r = _block_seq(0, &msg); r)
		return r; // Error or empty file

	first = _block_end / _block_size - 1; // Meta fill first block

	//if (*seq < msg.seq)
	//	return _log.warning(EINVAL, "Seek seq {} failed: first data seq is {}", seq, msg->seq);

	while (first + 1 < last) {
		_log.debug("Bisect blocks {} and {}", first, last);
		auto mid = (first + last) / 2;
		auto r = _block_seq(mid, &msg);
		if (r == EAGAIN) { // Empty block
			last = mid;
			continue;
		} else if (r)
			return r;
		_log.trace("Block {} seq: {}", mid, msg.seq);
		if (msg.seq == seq)
			return 0;
		if (msg.seq > seq)
			last = mid;
		else
			first = mid;
	}

	_offset = first * _block_size;
	_block_end = _offset + _block_size;

	do {
		frame_size_t frame;
		if (auto r = _read_frame(&frame); r)
			return r;

		if (_offset + _block_size == _block_end) {
			_shift(frame);
			continue;
		}

		_log.trace("Check seq at 0x{:x}", _offset);
		if (auto r = _read_seq(frame, &msg); r)
			return r;
		_log.trace("Message {}/{} at 0x{:x}", msg.seq, msg.size, _offset);
		if (msg.seq >= seq)
			break;
		_shift(&msg);
	} while (true);

	if (msg.seq > seq)
		_log.warning("Seek seq {}: found closest seq {}", seq, msg.seq);
	return 0;
}

int File::_block_seq(size_t block, tll_msg_t *msg)
{
	_offset = block * _block_size;
	_block_end = _offset + _block_size;

	// Skip metadata
	frame_size_t frame;
	if (auto r = _read_frame(&frame); r)
		return r;
	if (frame == -1)
		return EAGAIN;
	_shift(frame);

	return _read_seq(msg);
}

int File::_read_seq(tll_msg_t *msg)
{
	frame_size_t frame;

	if (auto r = _read_frame(&frame); r)
		return r;
	return _read_seq(frame, msg);
}

int File::_read_seq(frame_size_t frame, tll_msg_t *msg)
{
	size_t size = _data_size(frame);
	if (size < sizeof(frame_t))
		return _log.fail(EINVAL, "Invalid frame size at 0x{:x}: {} too small", _offset, frame);

	return _read_data(size, msg);
}

int File::_post(const tll_msg_t *msg, int flags)
{
	if (internal.caps & caps::Input)
		return ENOSYS;

	if (msg->type != TLL_MESSAGE_DATA)
		return 0;

	if (msg->seq <= _seq)
		return _log.fail(EINVAL, "Incorrect messsage seq: {} <= last seq {}", msg->seq, _seq);

	const size_t size = sizeof(full_frame_t) + msg->size;

	if (size > _block_size)
		return _log.fail(EMSGSIZE, "Message size too large: {}, block size is {}", msg->size, _block_size);

	frame_t meta = { msg };
	auto r = _write_datav(const_memory { &meta, sizeof(meta) }, const_memory { msg->data, msg->size });
	if (!r) {
		_seq = msg->seq;
		if (_seq_begin == -1)
			_seq_begin = _seq;
	}
	return r;
}

int File::_write_raw(const void * data, size_t size, size_t offset)
{
	return _check_write(size, pwrite(fd(), data, size, offset));
}

template <typename ... Args>
int File::_write_datav(Args && ... args)
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
		return _log.fail(EMSGSIZE, "Full size too large: {}, block size is {}", size, _block_size);

	if (_offset + size > _block_end) {
		frame_size_t frame = -1;

		if (_write_raw(&frame, sizeof(frame), _offset))
			return EINVAL;

		_offset = _block_end;
		_block_end += _block_size;
	}

	if (_offset + _block_size == _block_end) { // Write block meta
		static constexpr uint8_t header[5] = {5, 0, 0, 0, 0x80};
		if (_write_raw(&header, sizeof(header), _offset))
			return EINVAL;
		_offset += sizeof(header);
	}

	frame = size;
	_log.trace("Write frame {} at {}", frame, _offset);
	if (_check_write(size, pwritev(fd(), iov, N + 2, _offset)))
		return EINVAL;

	return _shift(size);
}

int File::_read_frame(frame_size_t *frame)
{
	auto r = pread(fd(), frame, sizeof(*frame), _offset);

	if (r < 0)
		return _log.fail(EINVAL, "Failed to read frame at 0x{:x}: {}", _offset, strerror(errno));

	if (r < (ssize_t) sizeof(*frame))
		return EAGAIN;

	if (*frame == 0)
		return EAGAIN;

	if (*frame != -1) {
		if (*frame < (ssize_t) sizeof(*frame) + 1)
			return _log.fail(EMSGSIZE, "Invalid frame size at 0x{:x}: {} < minimum {}", _offset, *frame, sizeof(*frame) + 1);

		if (_offset + *frame > _block_end)
			return _log.fail(EMSGSIZE, "Invalid frame size at 0x{:x}: {} excedes block boundary", _offset, *frame);
		return 0;
	}

	_log.trace("Found skip frame at offset 0x{:x}", _offset);
	if (_offset + _block_size == _block_end)
		return EAGAIN;

	_offset = _block_end;
	_block_end += _block_size;

	return _read_frame(frame);
}

int File::_read_data(size_t size, tll_msg_t *msg)
{
	_log.trace("Read {} bytes of data at {} + {}", size, _offset, sizeof(frame_size_t));
	auto r = pread(fd(), _buf.data(), size + 1, _offset + sizeof(frame_size_t));

	if (r < 0)
		return _log.fail(EINVAL, "Failed to read data at 0x{:x}: {}", _offset, strerror(errno));
	if (r < (ssize_t) size + 1)
		return EAGAIN;

	if ((((const uint8_t *) _buf.data())[size] & 0x80) == 0) // No tail marker
		return EAGAIN;

	auto meta = (frame_t *) _buf.data();
	msg->msgid = meta->msgid;
	msg->seq = meta->seq;
	msg->size = size - sizeof(*meta);
	msg->data = meta + 1;

	return 0;
}

int File::_process(long timeout, int flags)
{
	tll_msg_t msg = { TLL_MESSAGE_DATA };
	frame_size_t frame;

	auto r = _read_frame(&frame);

	if (r == EAGAIN) {
		if (_autoclose) {
			_log.info("All messages processed. Closing");
			close();
		} else
			_dcaps_pending(false);
		return EAGAIN;
	} else if (r)
		return r;

	if (_offset + _block_size == _block_end) {
		_shift(frame);
		return _process(timeout, flags);
	}

	size_t size = _data_size(frame);
	if (size < sizeof(frame_t))
		return _log.fail(EINVAL, "Invalid frame size at 0x{:x}: {} too small", _offset, frame);

	r = _read_data(size, &msg);

	if (r == EAGAIN) {
		_dcaps_pending(false);
		return EAGAIN;
	} else if (r)
		return r;

	_shift(frame);

	_callback_data(&msg);

	return 0;
}
