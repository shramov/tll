/*
 * Copyright (c) 2018-2020 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#ifndef _TLL_UTIL_ZLIB_H
#define _TLL_UTIL_ZLIB_H

#include <memory>
#include <vector>
#include <zlib.h>

namespace tll::zlib {

template <typename T, typename Buf>
inline result_t<bool> decompress(const T &data, Buf & buf)
{
	if (data.size() == 0) {
		buf.resize(0);
		return true;
	}

	z_stream stream = {0};

	if (inflateInit(&stream))
		return error("Failed to init inflate stream");
	std::unique_ptr<z_stream, decltype(&inflateEnd)> _stream = { &stream, inflateEnd };

	buf.resize(data.size() * 2);
	stream.avail_in = data.size();
	stream.next_in = (Bytef *) data.data();
	stream.avail_out = buf.size();
	stream.next_out = (Bytef *) buf.data();

	do {
		auto r = inflate(&stream, Z_FINISH);
		if (r == Z_STREAM_END) break;
		if (r != Z_OK && r != Z_BUF_ERROR)
			return error("Failed to decompress data");
		if (stream.avail_in == 0)
			return error("Truncated compressed data");
		if (buf.size() * 2 > 16 * 1024 * 1024)
			return error("Requested too large buffer (> 16Mb)");
		buf.resize(buf.size() * 2);
		stream.avail_out = buf.size() - stream.total_out;
		stream.next_out = stream.total_out + (Bytef *) buf.data();
	} while (true);

	buf.resize(stream.total_out);
	return true;
}

template <typename T>
inline result_t<std::vector<char>> decompress(const T &data)
{
	std::vector<char> buf;
	auto r = decompress(data, buf);
	if (!r) return error(r.error());
	return buf;
}

template <typename T, typename Buf>
inline result_t<bool> compress(const T &data, Buf & buf, int level = Z_DEFAULT_COMPRESSION)
{
	if (data.size() == 0) {
		buf.resize(0);
		return true;
	}

	z_stream stream = {0};

	if (deflateInit(&stream, level))
		return error("Failed to init deflate stream");

	std::unique_ptr<z_stream, decltype(&deflateEnd)> _stream = { &stream, deflateEnd };

	buf.resize(deflateBound(&stream, data.size()));
	stream.avail_in = data.size();
	stream.next_in = (Bytef *) data.data();
	stream.avail_out = buf.size();
	stream.next_out = (Bytef *) buf.data();

	do {
		auto r = deflate(&stream, Z_FINISH);
		if (r == Z_STREAM_END) break;
		if (r != Z_OK && r != Z_BUF_ERROR)
			return error("Failed to compress data");
		if (buf.size() * 2 > 16 * 1024 * 1024)
			return error("Requested too large buffer (> 16Mb)");
		buf.resize(buf.size() * 2);
		stream.avail_out = buf.size() - stream.total_out;
		stream.next_out = stream.total_out + (Bytef *) buf.data();
	} while (true);

	buf.resize(stream.total_out);

	return true;
}

template <typename T>
inline result_t<std::vector<char>> compress(const T &data, int level = Z_DEFAULT_COMPRESSION)
{
	std::vector<char> buf;
	auto r = compress(data, buf, level);
	if (!r) return error(r.error());
	return buf;
}

} // namespace tll::util

#endif//_TLL_UTIL_ZLIB_H
