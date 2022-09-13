/*
 * Copyright (c) 2018-2021 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#include "channel/file.h"

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
	_block_size = reader.getT("block", util::Size {1024 * 1024});
	_compression = reader.getT("compress", Compression::None, {{"no", Compression::None}, {"lz4", Compression::LZ4}});
	if (!reader)
		return _log.fail(EINVAL, "Invalid url: {}", reader.error());

	if (_compression != Compression::None)
		return _log.fail(EINVAL, "Compression not supported");

	_buf.resize(_block_size);

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
	auto mode = O_RDONLY;
	if (internal.caps & caps::Output)
		mode = O_RDWR | O_CREAT;

	auto fd = ::open(_filename.c_str(), mode, 0644);
	if (fd == -1)
		return _log.fail(EINVAL, "Failed to open file {} for writing: {}", _filename, strerror(errno));
	_update_fd(fd);

	_offset = 0;
	_block_end = _block_size;

	if (internal.caps & caps::Input) {
		auto reader = tll::make_props_reader(props);
		auto seq = reader.getT<long long>("seq", 0);
		if (!reader)
			return _log.fail(EINVAL, "Invalid params: {}", reader.error());
		if (auto r = _seek(seq); r && r != EAGAIN)
			return _log.fail(EINVAL, "Seek failed");
		_update_dcaps(dcaps::Process | dcaps::Pending);
	} else {
		if (auto r = _seek(std::nullopt); r && r != EAGAIN)
			return _log.fail(EINVAL, "Seek failed");
	}
	return 0;
}

int File::_close()
{
	auto fd = this->_update_fd(-1);
	if (fd != -1)
		::close(fd);
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

int File::_shift(size_t size)
{
	_log.trace("Shift offset {} + {}", _offset, size);
	_offset += size;

	if (_offset + sizeof(full_frame_t) > _block_end) {
		_log.trace("Shift block to {:x}", _block_end);
		_offset = _block_end;
		_block_end += _block_size;
	}

	return 0;
}

int File::_seek(std::optional<long long> seq)
{
	struct stat stat;
	auto r = fstat(fd(), &stat);
	if (r < 0)
		return _log.fail(EINVAL, "Failed to get file size: {}", strerror(errno));

	tll_msg_t msg;

	size_t first = 0;
	size_t last = stat.st_size / _block_size;

	for (; last > 0; last--) {
		auto r = _block_seq(last, &msg);
		if (r == 0) {
			_log.trace("Found data in block {}: seq {}", last, msg.seq);
			break;
		} else if (r != EAGAIN)
			return _log.fail(EINVAL, "Failed to read seq of block {}", last);
	}

	if (!seq) { // Seek to end
		do {
			auto r = _read_seq(&msg);
			if (r == EAGAIN)
				break;
			else if (r)
				return r;
			_shift(&msg);
		} while (true);

		if (stat.st_size != (off_t) _offset) {
			_log.warning("Trailing data in file: {} < {}", _offset, stat.st_size);
			_truncate(_offset);
		}
	}

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
		if (msg.seq == *seq)
			return 0;
		if (msg.seq > *seq)
			last = mid;
		else
			first = mid;
	}

	_offset = first * _block_size;
	_block_end = _offset + _block_size;

	frame_size_t frame;
	if (auto r = _read_frame(&frame); r)
		return r;
	_shift(frame);

	do {
		_log.debug("Check seq at {}", _offset);
		if (auto r = _read_seq(&msg); r)
			return r;
		_log.debug("Message {}/{} at {}", msg.seq, msg.size, _offset);
		if (msg.seq >= *seq)
			break;
		_shift(&msg);
	} while (true);

	if (msg.seq > *seq)
		_log.warning("Seek seq {}: found closest seq {}", *seq, msg.seq);
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

	auto r = _read_data(sizeof(frame_t), msg);
	msg->size = frame - sizeof(frame) - sizeof(frame_t);
	return r;
}

int File::_post(const tll_msg_t *msg, int flags)
{
	if (internal.caps & caps::Input)
		return ENOSYS;

	if (msg->type != TLL_MESSAGE_DATA)
		return 0;

	const size_t size = sizeof(full_frame_t) + msg->size;

	if (size > _block_size)
		return _log.fail(EMSGSIZE, "Message size too large: {}, block size is {}", msg->size, _block_size);

	frame_t meta = { msg };
	return _write_datav(const_memory { &meta, sizeof(meta) }, const_memory { msg->data, msg->size });
}

template <typename ... Args>
int File::_write_datav(Args && ... args)
{
	constexpr unsigned N = sizeof...(Args);
	std::array<const_memory, N> data({const_memory(std::forward<Args>(args))...});
	struct iovec iov[N + 1];

	frame_size_t frame;
	iov[0].iov_base = &frame;
	iov[0].iov_len = sizeof(frame);

	size_t size = sizeof(frame);

	for (unsigned i = 0; i < N; i++) {
		iov[i + 1].iov_base = (void *) data[i].data;
		iov[i + 1].iov_len = data[i].size;
		size += data[i].size;
	}

	if (size > _block_size)
		return _log.fail(EMSGSIZE, "Full size too large: {}, block size is {}", size, _block_size);

	if (_offset + size > _block_end) {
		frame_size_t frame = -1;

		if (_check_write(sizeof(frame), pwrite(fd(), &frame, sizeof(frame), _offset)))
			return EINVAL;

		_offset = _block_end;
		_block_end += _block_size;
	}

	if (_offset + _block_size == _block_end) {
		frame = 4;
		if (_check_write(sizeof(frame), pwrite(fd(), &frame, sizeof(frame), _offset)))
			return EINVAL;
		_offset += sizeof(frame);
	}

	frame = size;
	if (_check_write(size, pwritev(fd(), iov, N + 1, _offset)))
		return EINVAL;

	return _shift(size);
}

int File::_read_frame(frame_size_t *frame)
{
	auto r = pread(fd(), frame, sizeof(*frame), _offset);

	if (r < 0)
		return _log.fail(EINVAL, "Failed to read frame at {:x}: {}", _offset, strerror(errno));

	if (r < (ssize_t) sizeof(*frame))
		return EAGAIN;

	if (*frame == 0)
		return EAGAIN;

	if (*frame != -1)
		return 0;

	_log.trace("Found skip frame at offset {:x}", _offset);
	if (_offset + _block_size == _block_end)
		return EAGAIN;

	_offset = _block_end;
	_block_end += _block_size;

	return _read_frame(frame);
}

int File::_read_data(size_t size, tll_msg_t *msg)
{
	_log.trace("Read {} bytes of data at {} + {}", size, _offset, sizeof(frame_size_t));
	auto r = pread(fd(), _buf.data(), size, _offset + sizeof(frame_size_t));

	if (r < 0)
		return _log.fail(EINVAL, "Failed to read data at {:x}: {}", _offset, strerror(errno));
	if (r < (ssize_t) size)
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
		_dcaps_pending(false);
		return EAGAIN;
	} else if (r)
		return r;

	if (_offset + _block_size == _block_end) {
		_shift(frame);
		return _process(timeout, flags);
	}

	ssize_t size = frame - sizeof(frame);
	if (size < (ssize_t) sizeof(frame_t))
		return _log.fail(EINVAL, "Invalid frame size at {:x}: {} too small", _offset, frame);

	if (_offset + frame > _block_end)
		return _log.fail(EMSGSIZE, "Invalid frame size at {:x}: {} excedes block boundary", _offset, frame);

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
