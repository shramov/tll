/*
 * Copyright (c) 2013-2018 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 *
 * Based on bin2ascii from pb2json
 */

#ifndef _TLL_UTIL_BIN2ASCII_H
#define _TLL_UTIL_BIN2ASCII_H

#include <string>

#include "tll/util/result.h"

namespace tll::util {

template <typename T>
static inline result_t<bool> hex2bin(std::string_view s, T & buf)
{
	if (s.size() % 2)
		return error("Odd hex data size");
	static const char lookup[] = ""
		"\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80" // 0x00
		"\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80" // 0x10
		"\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80" // 0x20
		"\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x80\x80\x80\x80\x80\x80" // 0x30
		"\x80\x0a\x0b\x0c\x0d\x0e\x0f\x80\x80\x80\x80\x80\x80\x80\x80\x80" // 0x40
		"\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80" // 0x50
		"\x80\x0a\x0b\x0c\x0d\x0e\x0f\x80\x80\x80\x80\x80\x80\x80\x80\x80" // 0x60
		"\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80" // 0x70
		"\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80" // 0x80
		"\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80" // 0x90
		"\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80" // 0xa0
		"\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80" // 0xb0
		"\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80" // 0xc0
		"\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80" // 0xd0
		"\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80" // 0xe0
		"\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80" // 0xf0
		"";
	buf.resize(s.size() / 2);
	for (size_t i = 0; i < s.size(); i += 2) {
		char hi = lookup[(unsigned char) s[i]];
		char lo = lookup[(unsigned char) s[i+1]];
		if (0x80 & (hi | lo))
			return error("Invalid hex data: " + std::string(s.substr(i, 6)));
		buf[i/2] = (hi << 4) | lo;
	}
	return true;
}

inline result_t<std::vector<char>> hex2bin(std::string_view s)
{
	std::vector<char> buf;
	auto r = hex2bin(s, buf);
	if (!r) return error(r.error());
	return buf;
}

template <typename T, typename Buf >
inline std::string_view bin2hex(const T &s, Buf & buf)
{
	static const char lookup[] = "0123456789abcdef";
	buf.resize(s.size() * 2);
	char * ptr = (char *) buf.data();
	for (size_t i = 0; i < s.size(); i++) {
		unsigned char hi = s[i] >> 4;
		unsigned char lo = s[i] & 0xf;
		*(ptr++) = lookup[hi];
		*(ptr++) = lookup[lo];
	}
	return std::string_view((const char *) buf.data(), buf.size());
}

template <typename T>
inline std::string bin2hex(const T &s)
{
	std::string r;
	bin2hex(s, r);
	return r;
}

template <typename T, typename Buf>
inline std::string_view b64_encode(const T &s, Buf & buf)
{
	if (s.size() == 0) {
		buf.resize(0);
		return "";
	}
	typedef unsigned char u1;
	static const char lookup[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	const u1 * data = (const u1 *) s.data();
	buf.resize(s.size() * 4 / 3 + 3);
	char * ptr = (char *) buf.data();
	for (size_t i = 0; i < s.size(); i += 3) {
		unsigned n = data[i] << 16;
		if (i + 1 < s.size()) n |= data[i + 1] << 8;
		if (i + 2 < s.size()) n |= data[i + 2];

		u1 n0 = (u1)(n >> 18) & 0x3f;
		u1 n1 = (u1)(n >> 12) & 0x3f;
		u1 n2 = (u1)(n >>  6) & 0x3f;
		u1 n3 = (u1)(n      ) & 0x3f;

		*ptr++ = lookup[n0];
		*ptr++ = lookup[n1];
		if (i + 1 < s.size()) *ptr++ = lookup[n2];
		if (i + 2 < s.size()) *ptr++ = lookup[n3];
	}
	for (unsigned i = 0; i < (3 - s.size() % 3) % 3; i++)
		*ptr++ = '=';
	buf.resize(ptr - (char *) buf.data());
	return std::string_view((const char *)buf.data(), buf.size());
}

template <typename T>
static inline std::string b64_encode(const T &s)
{
	std::string r;
	b64_encode(s, r);
	return r;
}

template <typename T>
static inline result_t<bool> b64_decode(std::string_view s, T & buf)
{
	if (s.size() == 0) {
		buf.resize(0);
		return true;
	}
	typedef unsigned char u1;
	static const uint16_t pad2 = (((u1) '=') << 8) | ((u1) '=');
	static const char lookup[] = ""
		"\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80" // 0x00
		"\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80" // 0x10
		"\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x3e\x80\x80\x80\x3f" // 0x20
		"\x34\x35\x36\x37\x38\x39\x3a\x3b\x3c\x3d\x80\x80\x80\xff\x80\x80" // 0x30
		"\x80\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e" // 0x40
		"\x0f\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x80\x80\x80\x80\x80" // 0x50
		"\x80\x1a\x1b\x1c\x1d\x1e\x1f\x20\x21\x22\x23\x24\x25\x26\x27\x28" // 0x60
		"\x29\x2a\x2b\x2c\x2d\x2e\x2f\x30\x31\x32\x33\x80\x80\x80\x80\x80" // 0x70
		"\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80" // 0x80
		"\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80" // 0x90
		"\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80" // 0xa0
		"\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80" // 0xb0
		"\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80" // 0xc0
		"\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80" // 0xd0
		"\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80" // 0xe0
		"\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80\x80" // 0xf0
		"";
	if (s.size() % 4)
		return error("Invalid base64 data size");

	buf.resize(s.size() * 3 / 4 + 3);
	auto ptr = (u1 *) buf.data();
	size_t i = 0;
	for (; i < s.size() - 4; i += 4) {
		u1 n0 = lookup[(u1) s[i+0]];
		u1 n1 = lookup[(u1) s[i+1]];
		u1 n2 = lookup[(u1) s[i+2]];
		u1 n3 = lookup[(u1) s[i+3]];
		if (0x80 & (n0 | n1 | n2 | n3))
			return error("Invalid base64 data: " + std::string(s.substr(i, 4)));
		unsigned n = (n0 << 18) | (n1 << 12) | (n2 << 6) | n3;
		*ptr++ = (n >> 16) & 0xff;
		*ptr++ = (n >>  8) & 0xff;
		*ptr++ = (n      ) & 0xff;
	}

	{
		u1 n0 = lookup[(u1) s[i+0]];
		u1 n1 = lookup[(u1) s[i+1]];
		if (0x80 & (n0 | n1))
			return error("Invalid base64 data: " + std::string(s.substr(i, 4)));
		unsigned n = (n0 << 18) | (n1 << 12);
		*ptr++ = (n >> 16) & 0xff;
		if (*(uint16_t *) (&s[i + 2]) == pad2) { // 1 byte
		} else if (s[i + 3] == '=') { // 2 bytes
			u1 n2 = lookup[(u1) s[i+2]];
			if (0x80 & n2)
				return error("Invalid base64 data: " + std::string(s.substr(i, 4)));
			n |= (n2 << 6);
			*ptr++ = (n >>  8) & 0xff;
		} else { // 3 bytes
			u1 n2 = lookup[(u1) s[i+2]];
			u1 n3 = lookup[(u1) s[i+3]];
			if (0x80 & (n2 | n3))
				return error("Invalid base64 data: " + std::string(s.substr(i, 4)));
			n |= (n2 << 6) | n3;
			*ptr++ = (n >>  8) & 0xff;
			*ptr++ = (n      ) & 0xff;
		}
	}
	buf.resize(ptr - (u1 *) buf.data());
	return true;
}

static inline result_t<std::vector<char>> b64_decode(std::string_view s)
{
	std::vector<char> buf;
	auto r = b64_decode(s, buf);
	if (!r) return error(r.error());
	return buf;
}

} // namespace tll::util

#endif//_TLL_UTIL_BIN2ASCII_H
