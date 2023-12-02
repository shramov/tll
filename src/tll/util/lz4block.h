// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Pavel Shramov <shramov@mexmat.net>

#ifndef _TLL_UTIL_LZ4BLOCK_H
#define _TLL_UTIL_LZ4BLOCK_H

#include <cerrno>
#include <memory>
#include <optional>
#include <vector>

#include "tll/util/memoryview.h"

#include <lz4.h>

namespace tll::lz4 {

struct lz4_stream_delete { void operator () (LZ4_stream_t *ptr) const { LZ4_freeStream(ptr); } };
struct lz4_stream_decode_delete { void operator () (LZ4_streamDecode_t *ptr) const { LZ4_freeStreamDecode(ptr); } };

struct Ring
{
	static constexpr size_t prefix_size = 64 * 1024; // LZ4 prefix size

	std::vector<char> ring;
	size_t block = 0;
	size_t offset = 0;

	int init(size_t block)
	{
		this->block = block;
		offset = 0;
		ring.resize(0);
		ring.resize(std::max((size_t) LZ4_decoderRingBufferSize(block), 2 * block + prefix_size));
		return 0;
	}

	void reset() { offset = 0; }

	void shift(size_t size)
	{
		offset += size;
		if (offset + block > ring.size())
			offset = 0;
	}

	auto view() { return tll::make_view(ring).view(offset); }
	auto view() const { return tll::make_view(ring).view(offset); }
};

struct StreamEncode
{
	Ring ring;

	std::unique_ptr<LZ4_stream_t, lz4_stream_delete> stream;

	int init(size_t block)
	{
		stream.reset(LZ4_createStream());
		if (stream == nullptr)
			return ENOMEM;
		return ring.init(block);
	}

	void reset()
	{
		ring.reset();
		LZ4_resetStream_fast(stream.get());
	}

	template <typename Buf>
	tll::const_memory compress(Buf &result, size_t size, int level)
	{
		auto view = ring.view();
		ring.shift(size);
		auto r = LZ4_compress_fast_continue(stream.get(), view.dataT<char>(), result.data(), size, result.size(), level);
		if (r < 0)
			return { nullptr, 0 };
		return tll::const_memory { result.data(), (size_t) r };
	}

	auto view() { return ring.view(); }
};

struct StreamDecode
{
	Ring ring;

	std::unique_ptr<LZ4_streamDecode_t, lz4_stream_decode_delete> stream;

	int init(size_t block)
	{
		stream.reset(LZ4_createStreamDecode());
		if (stream == nullptr)
			return ENOMEM;
		return ring.init(block);
	}

	void reset()
	{
		ring.reset();
		if (stream) LZ4_setStreamDecode(stream.get(), nullptr, 0);
	}

	tll::const_memory decompress(const void * data, size_t size)
	{
		auto view = ring.view();
		auto r = LZ4_decompress_safe_continue(stream.get(), (const char *) data, view.dataT<char>(), size, ring.block);
		if (r < 0)
			return { nullptr, 0 };
		ring.shift(r);
		return tll::const_memory { view.data(), (size_t) r };
	}
};

} // namespace tll::util

#endif//_TLL_UTIL_LZ4BLOCK_H
