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
#include "tll/util/tempfile.h"

#include "tll/compat/fmt/std.h"

#include <fcntl.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

using namespace tll;
using namespace tll::file;

template <>
struct tll::conv::dump<Compression> : public to_string_from_string_buf<Compression>
{
	template <typename Buf>
	static std::string_view to_string_buf(const Compression &v, Buf &buf)
        {
		switch (v) {
		case Compression::None: return "none";
		case Compression::LZ4: return "lz4";
		}
		return "unknown";
	}
};

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

#ifndef __linux__
#define MAP_POPULATE 0
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
	size_t block_end = 0;
	size_t block_size = 0;

	enum Mode { Read, Write };

	int init(const tll::Logger &log, size_t block_size, Mode mode)
	{
		this->block_size = block_size;
		return 0;
	}

	void reset()
	{
		fd = -1;
		offset = 0;
		block_end = 0;
		block_size = 0;
	}

	void shift(size_t size) { offset += size; }

	template <typename ... Args>
	int writev(frame_size_t frame, Args && ... args)
	{
		return ENOSYS;
	}

	tll::const_memory read(size_t size) { return { nullptr, ENOSYS }; }

	int block(size_t start)
	{
		offset = start;
		block_end = start + block_size;
		return 0;
	}
};

struct IOPosix : public IOBase
{
	static constexpr std::string_view protocol() { return "file-posix"; }
	static constexpr std::string_view name() { return protocol().substr(5);; }

	std::vector<char> buf;

	int init(const tll::Logger &log, size_t block, Mode mode)
	{
		buf.resize(block);
		return IOBase::init(log, block, mode);
	}

	template <typename ... Args>
	int writev(frame_size_t frame, Args && ... args)
	{
		constexpr unsigned N = sizeof...(Args);
		iovec iov[N + 1];
		iov[0].iov_base = &frame;
		iov[0].iov_len = sizeof(frame);

		std::array<const_memory, N> data({const_memory(std::forward<Args>(args))...});

		for (unsigned i = 0; i < N; i++) {
			iov[i + 1].iov_base = (void *) data[i].data;
			iov[i + 1].iov_len = data[i].size;
		}

		return pwritev(fd, iov, N + 1, offset);
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

struct IOMMap : public IOBase
{
	static constexpr std::string_view protocol() { return "file-mmap"; }
	static constexpr std::string_view name() { return protocol().substr(5);; }

	size_t block_start = 0;
	void * base = nullptr;
	int mmap_prot = 0;
	int mmap_flags = 0;

	int init(const tll::Logger &log, size_t block, Mode mode)
	{
		auto page = sysconf(_SC_PAGE_SIZE);
		if (block % page != 0)
			return log.fail(EINVAL, "Block size {} is not multiple of the page size {}", block, page);
		switch(mode) {
		case Read:
			mmap_prot = PROT_READ;
			mmap_flags = MAP_SHARED | MAP_POPULATE;
			break;
		case Write:
			mmap_prot = PROT_READ | PROT_WRITE;
			mmap_flags = MAP_SHARED;
			break;
		}
		return IOBase::init(log, block, mode);
	}

	void unmap()
	{
		if (base)
			munmap(base, block_size);
		base = nullptr;
	}

	void reset()
	{
		unmap();
		return IOBase::reset();
	}

	int block(size_t start)
	{
		unmap();
		block_start = start;
		if (auto r = mmap(nullptr, block_size, mmap_prot, mmap_flags, fd, block_start); r == MAP_FAILED)
			return errno;
		else
			base = r;
		return IOBase::block(start);
	}

	tll::const_memory read(size_t size, size_t off = 0)
	{
		return { static_cast<char *>(base) + offset - block_start + off, size };
	}

	template <typename ... Args>
	int writev(frame_size_t frame, Args && ... args)
	{
		constexpr unsigned N = sizeof...(Args);
		std::array<const_memory, N> data({const_memory(std::forward<Args>(args))...});

		auto base = static_cast<uint8_t *>(this->base) + offset - block_start;

		auto fptr = reinterpret_cast<const uint8_t *>(&frame);
		base[0] = fptr[0];
		base[1] = fptr[1];
		base[2] = fptr[2];

		((std::atomic<uint8_t> *) base)[3].store(fptr[3], std::memory_order_release);

		auto off = sizeof(frame);
		for (unsigned i = 0; i < N; i++) {
			memcpy(base + off, data[i].data, data[i].size);
			off += data[i].size;
		}

		return off;
	}
};

TLL_DEFINE_IMPL(tll::channel::FileInit);
TLL_DEFINE_IMPL(File<IOPosix>);
TLL_DEFINE_IMPL(File<IOMMap>);

std::optional<const tll_channel_impl_t *> tll::channel::FileInit::_init_replace(const tll::Channel::Url &url, tll::Channel *master)
{
	auto reader = channel_props_reader(url);
	auto posix = reader.getT("io", false, {{ "posix", true }, { "mmap", false}});
	if (!reader)
		return this->_log.fail(std::nullopt, "Invalid url: {}", reader.error());
	if (posix)
		return &File<IOPosix>::impl;
	return &File<IOMMap>::impl;
}

template <typename TIO>
constexpr std::string_view File<TIO>::scheme_control_string() const
{
	if (this->internal.caps & caps::Input)
		return control_scheme;
	return "";
}

template <typename TIO>
int File<TIO>::_init(const tll::Channel::Url &url, tll::Channel *master)
{
	this->_log.info("Initialize file channel with {} io", _io.name());

	auto reader = this->channel_props_reader(url);
	_block_init = reader.getT("block", util::Size {1024 * 1024});
	_compression_init = reader.getT("compression", Compression::None, {{"none", Compression::None}, {"lz4", Compression::LZ4}});
	_version_init = reader.getT("version", Version::Stable, {{"0", Version::V0}, {"1", Version::V1}, {"stable", Version::Stable}});
	_autoclose = reader.getT("autoclose", true);
	_tail_extra_size = reader.getT("extra-space", util::Size { 0 });
	_access_mode = reader.getT("access-mode", 0644u);
	_exact_last_seq = reader.getT("exact-last-seq", true);
	if (!reader)
		return this->_log.fail(EINVAL, "Invalid url: {}", reader.error());

	if (_access_mode > 0777)
		return this->_log.fail(EINVAL, "Invalid file access-mode parameter: 0{:o} greater then maximum 0777", _access_mode);

	_filename = url.host();

	if ((this->internal.caps & caps::InOut) == 0) // Defaults to input
		this->internal.caps |= caps::Input;
	if ((this->internal.caps & caps::InOut) == caps::InOut)
		return this->_log.fail(EINVAL, "file:// can be either read-only or write-only, need proper dir in parameters");

	if (_io.name() == "mmap" && _tail_extra_size == 0)
		_tail_extra_size = 1;

	return Base::_init(url, master);
}

template <typename TIO>
int File<TIO>::_open(const ConstConfig &props)
{
	auto filename = _filename;
	_end_of_data = false;
	_compression = _compression_init;
	_version = _version_init;
	_size_marker = 0;

	if (filename.empty()) {
		auto fn = props.get("filename");
		if (!fn || fn->empty())
			return this->_log.fail(EINVAL, "No filename in init and no 'filename' parameter in open");
		filename = *fn;
	}

	this->_log.debug("Open file {}", filename);

	_io.reset();
	_block_size = _block_init;
	_file_size_cache = 0;

	_seq = _seq_begin = -1;
	this->config_info().set_ptr("seq-begin", &_seq_begin);
	this->config_info().set_ptr("seq", &_seq);
	_delta_seq_base = 0;

	auto reader = tll::make_props_reader(props);
	if (this->internal.caps & caps::Input) {
		_io.fd = ::open(filename.c_str(), O_RDONLY, 0644);
		if (_io.fd == -1)
			return this->_log.fail(EINVAL, "Failed to open file {} for reading: {}", filename, strerror(errno));

		if (auto r = _read_meta(); r)
			return this->_log.fail(EINVAL, "Failed to read metadata");

		if (_io.init(this->_log, _block_size, IOBase::Read))
			return this->_log.fail(EINVAL, "Failed to init io");

		if (_version >= Version::V1)
			_size_marker = 0x80000000u;

		if (_compression == Compression::LZ4) {
			if (auto r = _lz4_init(_block_size); r)
				return r;
		}

		enum Mode { Seq, Last, End };
		auto mode = reader.getT("mode", Seq, {{"seq", Seq}, {"last", Last}, {"end", End}});
		auto seq = reader.getT("seq", std::optional<long long>());
		if (!reader)
			return this->_log.fail(EINVAL, "Invalid params: {}", reader.error());

		if (auto r = _file_bounds(); r && r != EAGAIN)
			return this->_log.fail(EINVAL, "Failed to load file bounds");

		if (mode == End) {
			// Do nothing, offset is after last message
		} else if (mode == Last) {
			if (auto r = _seek(_seq); r && r != EAGAIN)
				return this->_log.fail(EINVAL, "Seek failed");
		} else {
			if (seq) {
				if (auto r = _seek(*seq); r && r != EAGAIN)
					return this->_log.fail(EINVAL, "Seek failed");
			} else {
				if (auto r = _seek_start(); r)
					return this->_log.fail(EINVAL, "Failed to seek to first message");
			}
		}
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

		if (overwrite) {
			tll::util::TempFile tmp(filename);
			if (!tmp)
				return this->_log.fail(EINVAL, "Failed to create temporary file {}: {}", tmp.filename(), tmp.strerror());
			_io.fd = tmp.release_fd();
			if (fchmod(_io.fd, _access_mode))
				return this->_log.fail(EINVAL, "Failed to set file mode of {} to 0{:o}: {}", tmp.filename(), _access_mode, strerror(errno));

			if (_write_meta())
				return this->_log.fail(EINVAL, "Failed to write metadata");

			if (flock(_io.fd, LOCK_EX | LOCK_NB))
				return this->_log.fail(EINVAL, "Failed to lock file {} for writing: {}", filename, strerror(errno));

			this->_log.info("Rename temporary file {} to {}", tmp.filename(), filename);
			if (rename(tmp.filename().c_str(), filename.c_str()))
				return this->_log.fail(EINVAL, "Failed to rename {} to {}: {}", tmp.filename(), filename, strerror(errno));
			tmp.release();
		} else {
			_io.fd = ::open(filename.c_str(), O_RDWR, 0600);
			if (_io.fd == -1)
				return this->_log.fail(EINVAL, "Failed to open file {} for writing: {}", filename, strerror(errno));

			if (flock(_io.fd, LOCK_EX | LOCK_NB))
				return this->_log.fail(EINVAL, "Failed to lock file {} for writing: {}", filename, strerror(errno));

			if (auto r = _read_meta(); r)
				return this->_log.fail(EINVAL, "Failed to read metadata");
		}

		if (_version >= Version::V1)
			_size_marker = 0x80000000u;

		if (_compression == Compression::LZ4) {
			if (auto r = _lz4_init(_block_size); r)
				return r;
		}

		_tail_extra_blocks = 0;
		if (_tail_extra_size) {
			_tail_extra_blocks = (_tail_extra_size + _block_size - 1) / _block_size;
			this->_log.info("Keep up to extra {} blocks at file end", _tail_extra_blocks);
		}

		if (_io.init(this->_log, _block_size, IOBase::Write))
			return this->_log.fail(EINVAL, "Failed to init io");

		if (auto r = _file_bounds(); r && r != EAGAIN)
			return this->_log.fail(EINVAL, "Failed to load file bounds");

		this->_autoseq.reset(_seq);

		auto size = _file_size();
		if (size != (ssize_t) _io.offset) {
			if (auto r = _io.read(sizeof(frame_size_t)); r.data) {
				auto frame = *static_cast<const frame_size_t *>(r.data);
				if (frame)
					this->_log.warning("Trailing data in file: {} < {}, last frame: {}", _io.offset, size, frame);
				_truncate(_io.offset);
			}
			size = _io.offset;
		}
		if (_tail_extra_blocks) {
			this->_log.info("Extend file with {} extra blocks", _tail_extra_blocks);
			_truncate(_io.block_end + _block_size * _tail_extra_blocks);
		}
	}

	this->config_info().setT("block", util::Size { _block_size });
	this->config_info().setT("compression", _compression);
	this->config_info().setT("version", (unsigned) _version);
	return 0;
}

template <typename TIO>
int File<TIO>::_close()
{
	if (_io.fd != -1)
		::close(_io.fd);
	_io.reset();
	this->config_info().setT("seq-begin", _seq_begin);
	this->config_info().setT("seq", _seq);
	return 0;
}

template <typename TIO>
void File<TIO>::_truncate(size_t offset)
{
	auto r = ftruncate(_io.fd, offset);
	_file_size_cache = offset;
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

	if (auto r = pread(_io.fd, &frame, sizeof(frame), 0); r != sizeof(frame)) {
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

	if (auto r = pread(_io.fd, buf.data(), buf.size(), sizeof(frame)); r != (ssize_t) buf.size()) {
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

	if (auto v = meta.get_version(); v >= (unsigned) Version::Max)
		return this->_log.fail(EINVAL, "Unsupported version {}, max known is {}", v, (unsigned) Version::Max - 1);
	else
		_version = (Version) v;

	if (_version > Version::Stable)
		this->_log.info("Using unstable version {}", (unsigned) _version);

	_block_size = meta.get_block();
	auto comp = meta.get_compression();

	switch (comp) {
	case file_scheme::Meta::Compression::None:
		_compression = Compression::None;
		break;
	case file_scheme::Meta::Compression::LZ4:
		_compression = Compression::LZ4;
		break;
	default:
		return this->_log.fail(EINVAL, "Compression {} not supported", (uint8_t) comp);
	}

	this->_log.info("Meta info: block size {}, compression {}", _block_size, _compression);

	std::string_view scheme = meta.get_scheme();
	if (this->_scheme) {
		if (scheme.size())
			this->_log.info("Ignore scheme from meta, use explicit init parameter");
	} else  if (scheme.size()) {
		auto s = this->context().scheme_load(scheme);
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
	meta.set_version((uint8_t) _version);
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

	if (auto r = _check_write(buf.size(), pwrite(_io.fd, buf.data(), buf.size(), 0)); r)
		return r;

	_shift(buf.size());
	return 0;
}

template <typename TIO>
void File<TIO>::_shift(size_t size)
{
	this->_log.trace("Shift offset {} + {}", _io.offset, size);
	_io.offset += size;
}

template <typename TIO>
int File<TIO>::_shift_block(size_t offset)
{
	this->_log.trace("Shift block to 0x{:x}", _io.block_end);
	if (auto r = _io.block(offset); r)
		return this->_log.fail(r, "Failed to shift block: {}", strerror(r));

	if (_compression == Compression::LZ4) {
		this->_log.debug("Reset encoder/decoder at new block {}", _io.offset);
		_lz4_reset();
		_delta_seq_base = 0;
	}

	frame_size_t frame;
	if (auto r = _read_frame_nocheck(&frame); r)
		return r;
	if (frame == -1)
		return this->_log.fail(EINVAL, "Skip frame at block start 0x{:x}", _io.offset);
	_io.shift(frame);
	return 0;
}

template <typename TIO>
ssize_t File<TIO>::_file_size()
{
	struct stat stat;
	auto r = fstat(_io.fd, &stat);
	if (r < 0)
		return this->_log.fail(-1, "Failed to get file size: {}", strerror(errno));
	_file_size_cache = stat.st_size;
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

	for (auto last = (size + _block_size - 1) / _block_size - 1; last >= 0; last--) {
		auto r = _block_seq(last, &msg);
		if (r == 0)
			break;
		else if (r != EAGAIN)
			return this->_log.fail(EINVAL, "Failed to read seq of block {}", last);
	}

	_seq = msg.seq;

	long long delta_seq_prev = 0;
	auto block_prev = _io.block_end;
	do {
		if (!_exact_last_seq && _io.block_end != block_prev)
			break;

		frame_size_t frame;
		if (auto r = _read_frame(&frame); r) {
			if (r == EAGAIN)
				break;
			return r;
		}

		if (frame == -1) {
			_shift_skip();
			continue;
		}

		this->_log.trace("Check seq at 0x{:x}", _io.offset);
		auto r = _read_seq(frame, &msg);
		if (r == EAGAIN)
			break;
		else if (r)
			return r;
		_seq = msg.seq;

		if ((this->internal.caps & caps::Output) && _compression == Compression::LZ4) {
			frame_t meta = { &msg };
			if (block_prev != _io.block_end) {
				block_prev = _io.block_end;
				delta_seq_prev = 0;
			}
			meta.seq -= delta_seq_prev;
			delta_seq_prev = _delta_seq_base;
			auto r = _compress_datav(const_memory { &meta, sizeof(meta) }, const_memory { msg.data, msg.size });
			if (!r.data)
				return this->_log.fail(EINVAL, "Failed to compress data");
			this->_log.trace("Recompress data at {}: {} -> {}", _io.offset, sizeof(meta) + msg.size, r.size);
		}
		_shift(frame);
	} while (true);

	this->_log.info("First seq: {}, last seq: {}", _seq_begin, _seq);
	return 0;
}

template <typename TIO>
int File<TIO>::_seek_start()
{
	if (auto r = _shift_block(0); r)
		return this->_log.fail(EINVAL, "Failed to prepare block 0: {}", strerror(r));

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

	first = _io.block_end / _block_size - 1; // Meta fill first block

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

	if (auto r = _shift_block(first * _block_size); r)
		return this->_log.fail(EINVAL, "Failed to prepare block {}: {}", first, strerror(r));

	do {
		if (_io.offset + _block_size == _io.block_end) {
			if (auto r = _shift_block(_io.block_end); r) {
				if (r == EAGAIN)
					return EAGAIN;
				return this->_log.fail(r, "Failed to prepare block {}: {}", _io.block_end / _block_size, strerror(r));
			}
		}

		frame_size_t frame;
		if (auto r = _read_frame(&frame); r)
			return r;
		if (frame == -1) {
			_shift_skip();
			continue;
		}

		this->_log.trace("Check seq at 0x{:x}", _io.offset);
		if (auto r = _read_seq(frame, &msg); r)
			return r;
		this->_log.trace("Message {}/{} at 0x{:x}", msg.seq, msg.size, _io.offset);
		if (msg.seq >= seq)
			break;
		_shift(frame);
	} while (true);

	if (msg.seq > seq)
		this->_log.warning("Seek seq {}: found closest seq {}", seq, msg.seq);
	return 0;
}

template <typename TIO>
int File<TIO>::_block_seq(size_t block, tll_msg_t *msg)
{
	this->_log.debug("Check block seq at {}", block);
	if (auto r = _io.block(block * _block_size); r) {
		if (r == EAGAIN)
			return r;
		return this->_log.fail(EINVAL, "Failed to prepare block: {}", strerror(r));
	}

	if (_compression == Compression::LZ4) {
		_lz4_reset();
		_delta_seq_base = 0;
	}

	// Skip metadata
	frame_size_t frame;
	if (auto r = _read_frame(&frame); r)
		return r;
	if (frame == -1) {
		if (_io.block_end != _io.block_size) // Skip frame possible only in first block
			return EAGAIN;
		return _block_seq(1, msg);
	}

	_io.shift(frame);
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
	if (size < sizeof(frame_size_t))
		return this->_log.fail(EINVAL, "Invalid frame size at 0x{:x}: {} too small", _io.offset, size);

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
	auto r = _write_data(&meta, const_memory { msg->data, msg->size });
	if (!r) {
		_seq = msg->seq;
		if (_seq_begin == -1)
			_seq_begin = _seq;
	}
	return r;
}

template <typename TIO>
int File<TIO>::_write_data(frame_t * meta, tll::const_memory data)
{
	frame_size_t frame;
	uint8_t tail = 0x80;
	tll::const_memory mmeta = { meta, sizeof(*meta) }, mdata = data, mtail = { &tail, sizeof(tail) };

	size_t size = mmeta.size + mdata.size;

	bool recompress = false;

	if (_compression == Compression::LZ4) {
		_delta_seq_base = meta->seq;
		if (_seq != -1)
			meta->seq -= _seq;

		auto r = _compress_datav(const_memory { meta, sizeof(*meta) }, data);
		if (!r.data)
			return this->_log.fail(EINVAL, "Failed to compress data");
		this->_log.trace("Original size: {}, compressed size: {}", size, r.size);
		mdata = r;
		mmeta = {};
		size = r.size;
	}

	size += sizeof(frame) + 1;

	if (size > _block_size)
		return this->_log.fail(EMSGSIZE, "Full size too large: {}, block size is {}", size, _block_size);

	if (_io.offset + size > _io.block_end) {
		frame_size_t frame = -1;
		if (_io.offset + sizeof(frame) < _io.block_end) {
			if (_check_write(sizeof(frame), _io.writev(frame)))
				return EINVAL;
		}

		if (auto r = _io.block(_io.block_end); r)
			return this->_log.fail(EINVAL, "Failed to prepare block: {}", strerror(r));

		recompress = true;
	}

	if (_io.offset + _block_size == _io.block_end) { // Write block meta
		if (auto r = _write_block(_io.offset); r)
			return r;

		if (recompress && _compression == Compression::LZ4) {
			meta->seq = _delta_seq_base;

			// Recompress
			_lz4_encode.reset();
			auto r = _compress_datav(const_memory { meta, sizeof(*meta) }, data);
			if (!r.data)
				return this->_log.fail(EINVAL, "Failed to compress data");
			this->_log.trace("Recompress, original size: {}, compressed size: {}", size, r.size);
			mdata = r;
			size = r.size + sizeof(frame) + 1;
		}
	}

	frame = size | _size_marker;
	this->_log.trace("Write frame {} at {}", frame, _io.offset);
	if (_check_write(size, _io.writev(frame, mmeta, mdata, mtail)))
		return EINVAL;

	_shift(size);
	return 0;
}

template <typename TIO>
int File<TIO>::_write_block(size_t offset)
{
	if (auto r = _io.block(offset); r)
		return this->_log.fail(EINVAL, "Failed to prepare block: {}", strerror(r));

	uint8_t tail = 0x80;
	if (_check_write(5, _io.writev(5 | _size_marker, tll::const_memory { &tail, sizeof(tail) })))
		return EINVAL;
	_io.offset += 5;

	// Ensure there is always at least one empty block in the end
	if (_tail_extra_blocks && _file_size_cache <= _io.block_end) {
		this->_log.debug("Extend file with {} extra blocks", _tail_extra_blocks);
		_truncate(_io.block_end + _tail_extra_blocks * _block_size);
	}
	return 0;
}

template <typename TIO>
int File<TIO>::_read_frame(frame_size_t *ptr)
{
	if (_io.offset + sizeof(full_frame_t) + 1 > _io.block_end) {
		if (auto r = _shift_block(_io.block_end); r)
			return r;
	}

	return _read_frame_nocheck(ptr);
}

template <typename TIO>
int File<TIO>::_read_frame_nocheck(frame_size_t *ptr)
{
	auto r = _io.read(sizeof(*ptr));

	if (!r.data) {
		if (r.size == EAGAIN)
			return EAGAIN;
		return this->_log.fail(EINVAL, "Failed to read frame at 0x{:x}: {}", _io.offset, strerror(r.size));
	}

	auto frame = *static_cast<const frame_size_t *>(r.data);
	if (_size_marker && _io.offset != 0) {
		auto marker = static_cast<const std::atomic<uint8_t> *>(r.data)[3].load(std::memory_order_acquire);
		if ((marker & 0x80) == 0)
			return EAGAIN;
		frame = *static_cast<const frame_size_t *>(r.data);
		if (frame != -1)
			frame &= 0x7fffffffu;
	}
	*ptr = frame;

	if (frame > (ssize_t) sizeof(frame)) {
		if (_io.offset + frame > _io.block_end)
			return this->_log.fail(EMSGSIZE, "Invalid frame size at 0x{:x}: {} excedes block boundary", _io.offset, frame);
		return 0;
	} else if (frame == 0) {
		return EAGAIN;
	} else if (frame < 0) {
		this->_log.trace("Found skip frame at offset 0x{:x}", _io.offset);
		return 0;
	}

	if (frame < (ssize_t) sizeof(frame) + 1)
		return this->_log.fail(EMSGSIZE, "Invalid frame size at 0x{:x}: {} < minimum {}", _io.offset, frame, sizeof(frame) + 1);
	return EINVAL;
}

template <typename TIO>
int File<TIO>::_read_data(size_t size, tll_msg_t *msg)
{
	if (_compression == Compression::LZ4 && _lz4_decode_offset == (ssize_t) _io.offset) {
		auto meta = (frame_t *) _lz4_decode_last.data;
		msg->msgid = meta->msgid;
		msg->seq = meta->seq;
		msg->seq = _delta_seq_base;
		msg->size = _lz4_decode_last.size - sizeof(*meta);
		msg->data = meta + 1;
		return 0;
	}

	this->_log.trace("Read {} bytes of data at {} + {}", size, _io.offset, sizeof(frame_size_t));
	auto r = _io.read(size + 1, sizeof(frame_size_t));

	if (!r.data) {
		if (r.size == EAGAIN)
			return EAGAIN;
		return this->_log.fail(EINVAL, "Failed to read data at 0x{:x}: {}", _io.offset, strerror(r.size));
	}

	if ((((const uint8_t *) r.data)[size] & 0x80) == 0) // No tail marker
		return EAGAIN;
	r.size -= 1;

	if (_compression == Compression::LZ4) {
		_lz4_decode_last = _lz4_decode.decompress(r.data, r.size);
		if (!_lz4_decode_last.data)
			return this->_log.fail(EINVAL, "Failed to decompress {} bytes of data at 0x{:x}", r.size, _io.offset);
		this->_log.trace("Original size: {}, decompressed size: {}", r.size, _lz4_decode_last.size);
		r = _lz4_decode_last;
		_lz4_decode_offset = _io.offset;
	}

	if (r.size < sizeof(frame_t))
		return this->_log.fail(EINVAL, "Invalid data size at {}: {} too small", _io.offset, r.size);

	auto meta = (frame_t *) r.data;
	msg->msgid = meta->msgid;
	msg->seq = meta->seq;
	msg->size = r.size - sizeof(*meta);
	msg->data = meta + 1;

	if (_compression == Compression::LZ4) {
		msg->seq += _delta_seq_base;
		_delta_seq_base = msg->seq;
	}

	return 0;
}

template <typename TIO>
int File<TIO>::_process(long timeout, int flags)
{
	tll_msg_t msg = { TLL_MESSAGE_DATA };
	frame_size_t frame;

	if (_io.offset + _io.block_size == _io.block_end) {
		frame_size_t frame;
		if (auto r = _read_frame_nocheck(&frame); r)
			return r;
		if (frame == -1)
			return this->_log.fail(EINVAL, "Skip frame at block start 0x{:x}", _io.offset);
		_io.shift(frame);
	}

	if (auto r = _read_frame(&frame); r) {
		if (r != EAGAIN)
			return r;
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
	}

	if (frame == -1) {
		_shift_skip();
		return _process(timeout, flags);
	}

	size_t size = _data_size(frame);
	if (size < sizeof(frame_size_t))
		return this->_log.fail(EINVAL, "Invalid frame size at 0x{:x}: {} too small", _io.offset, frame);

	if (auto r = _read_data(size, &msg); r) {
		if (r == EAGAIN)
			this->_dcaps_pending(false);
		return r;
	}

	_shift(frame);
	this->_dcaps_pending(true);

	this->_callback_data(&msg);

	return 0;
}
