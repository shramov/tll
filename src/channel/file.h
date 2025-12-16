/*
 * Copyright (c) 2018-2021 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#ifndef _TLL_CHANNEL_FILE_H
#define _TLL_CHANNEL_FILE_H

#include "tll/channel/autoseq.h"

#include "tll/util/lz4block.h"
#include "tll/util/memoryview.h"

struct iovec;

namespace tll::file {

using frame_size_t = int32_t;

struct __attribute__((packed)) frame_t
{
	frame_t() {}
	frame_t(int32_t m, int64_t s) : msgid(m), seq(s) {}
	frame_t(const tll_msg_t *msg) : msgid(msg->msgid), seq(msg->seq) {}
	int32_t msgid = 0;
	int64_t seq = 0;
};

enum class Compression : uint8_t { None = 0, LZ4 = 1};
enum class Version : uint8_t { V0 = 0, Stable = V0, V1, Max };

template <typename TIO>
class File : public tll::channel::AutoSeq<File<TIO>>
{
	using Base = tll::channel::AutoSeq<File<TIO>>;
	using IO = TIO;

	IO _io = {};

	long long _seq = -1;
	long long _seq_begin = -1;
	long long _seq_eod = -1;

	size_t _block_size = 0;
	size_t _block_init = 0;
	size_t _tail_extra_size = 0;
	size_t _tail_extra_blocks = 0;
	size_t _file_size_cache = 0;

	std::string _filename;

	tll::lz4::StreamDecode _lz4_decode;
	tll::const_memory _lz4_decode_last = {};
	ssize_t _lz4_decode_offset = -1;
	tll::lz4::StreamEncode _lz4_encode;
	std::vector<char> _lz4_buf;

	long long _delta_seq_base = 0;
	Compression _compression = Compression::None, _compression_init = Compression::None;
	Version _version = Version::Stable, _version_init = Version::Stable;
	uint32_t _size_marker = 0;
	bool _autoclose = true;
	enum class EOD : char { Once, BeforeClose, Many } _end_of_data = EOD::Once;
	bool _exact_last_seq = true;
	unsigned _access_mode = 0644;

public:
	static constexpr std::string_view channel_protocol() { return IO::protocol(); }
	static constexpr std::string_view param_prefix() { return "file"; }
	static constexpr auto process_policy() { return Base::ProcessPolicy::Custom; }

	constexpr std::string_view scheme_control_string() const;

	int _init(const tll::Channel::Url &, tll::Channel *master);
	int _open(const tll::ConstConfig &);
	int _close();

	int _process(long timeout, int flags);
	int _post(const tll_msg_t *msg, int flags);

private:
	int _read_frame(frame_size_t *);
	int _read_frame_nocheck(frame_size_t *);
	int _read_data(size_t size, tll_msg_t *msg);

	size_t _data_size(frame_size_t frame) { return frame - sizeof(frame) - 1; }

	int _write_data(frame_t * meta, tll::const_memory data);

	template <typename ... Args>
	tll::const_memory _compress_datav(Args && ... args)
	{
		constexpr unsigned N = sizeof...(Args);
		std::array<const_memory, N> data({const_memory(std::forward<Args>(args))...});

		size_t size = 0;
		auto view = _lz4_encode.view();
		for (unsigned i = 0; i < N; i++) {
			memcpy(view.data(), data[i].data, data[i].size);
			size += data[i].size;
			view = view.view(data[i].size);
		}
		return _lz4_encode.compress(_lz4_buf, size, 0);
	}

	int _lz4_init(size_t block)
	{
		if (this->internal.caps & caps::Output) {
			_lz4_buf.resize(LZ4_compressBound(block));
			if (_lz4_encode.init(block))
				return this->_log.fail(EINVAL, "Failed to init lz4 encoder with block size {}", block);
		}
		if (_lz4_decode.init(block))
			return this->_log.fail(EINVAL, "Failed to init lz4 decoder with block size {}", block);
		_lz4_decode_offset = -1;
		_lz4_decode_last = {};
		return 0;
	}

	void _lz4_reset()
	{
		if (this->internal.caps & caps::Output)
			_lz4_encode.reset();
		_lz4_decode.reset();
		_lz4_decode_offset = -1;
	}

	int _write_block(size_t offset);

	int _file_bounds();
	ssize_t _file_size();

	int _seek(long long seq);
	int _seek_start();
	int _block_seq(size_t block, tll_msg_t *msg);
	int _read_seq(frame_size_t frame, tll_msg_t *msg);
	int _read_seq(tll_msg_t *msg);
	int _read_meta();
	int _write_meta();

	void _shift(size_t size);
	void _shift(const tll_msg_t * msg) { _shift(sizeof(frame_size_t) + sizeof(frame_t) + msg->size + 1); }
	void _shift_skip() { _io.offset = _io.block_end; }
	int _shift_block(size_t offset);
	void _truncate(size_t offset);
	int _check_write(size_t size, int r);
};

} // namespace tll::file

#endif//_TLL_CHANNEL_FILE_H
