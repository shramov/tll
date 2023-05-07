/*
 * Copyright (c) 2018-2021 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#ifndef _TLL_CHANNEL_FRAME_H
#define _TLL_CHANNEL_FRAME_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#pragma pack(push, 1)
struct tll_frame_t
{
	uint32_t size;
	int32_t msgid;
	int64_t seq;
};

struct tll_frame_short_t
{
	uint16_t size;
	int16_t msgid;
	int64_t seq;
};

struct tll_frame_tiny_t
{
	uint16_t size;
	int16_t msgid;
	int32_t seq;
};

struct tll_frame_seq32_t
{
	uint32_t seq;
};

struct tll_frame_seq64_t
{
	uint64_t seq;
};

struct tll_frame_size32_t
{
	uint32_t size;
};

struct tll_frame_bson_t
{
	uint32_t size;
};
#pragma pack(pop)

#ifdef __cplusplus
} // extern "C"
#endif

#ifdef __cplusplus

#include <string_view>
#include <vector>

#include "tll/channel.h"

namespace tll::frame {

template <typename T>
struct FrameBaseT
{
	using frame_type = T;

	/// Size of frame
	static constexpr size_t frame_size() { return sizeof(frame_type); }
	/// Skip frame or it's part of data
	static constexpr size_t frame_skip_size() { return frame_size(); }
};

template <typename T>
struct FrameFillT : public FrameBaseT<T>
{
	static void read(tll_msg_t *m, const T * data)
	{
		m->seq = data->seq;
		m->msgid = data->msgid;
		m->size = data->size;
	}

	static void write(const tll_msg_t *m, T * data)
	{
		data->seq = m->seq;
		data->msgid = m->msgid;
		data->size = m->size;
	}
};

template <typename T>
struct FrameT
{
	//static constexpr std::string_view name() { static_assert(false, "Undefined frame"); return ""; }
};

template <>
struct FrameT<tll_frame_t> : public FrameFillT<tll_frame_t>
{
	static std::vector<std::string_view> name() { return {"std", "l4m4s8"}; }
};

template <>
struct FrameT<tll_frame_short_t> : public FrameFillT<tll_frame_short_t>
{
	static std::vector<std::string_view> name() { return {"short", "l2m2s8"}; }
};

template <>
struct FrameT<tll_frame_tiny_t> : public FrameFillT<tll_frame_tiny_t>
{
	static std::vector<std::string_view> name() { return {"tiny", "l2m2s4"}; }
};

template <>
struct FrameT<tll_frame_seq32_t> : public FrameBaseT<tll_frame_seq32_t>
{
	static std::vector<std::string_view> name() { return {"seq32", "s4"}; }
	static void read(tll_msg_t *m, const frame_type * data) { m->seq = data->seq; }
	static void write(const tll_msg_t *m, frame_type * data) { data->seq = m->seq; }
};

template <>
struct FrameT<tll_frame_seq64_t> : public FrameBaseT<tll_frame_seq64_t>
{
	static std::vector<std::string_view> name() { return {"seq64", "s8"}; }
	static void read(tll_msg_t *m, const frame_type * data) { m->seq = data->seq; }
	static void write(const tll_msg_t *m, frame_type * data) { data->seq = m->seq; }
};

template <>
struct FrameT<tll_frame_size32_t> : public FrameBaseT<tll_frame_size32_t>
{
	static std::vector<std::string_view> name() { return {"size32", "l4"}; }
	static void read(tll_msg_t *m, const frame_type * data) { m->size = data->size; }
	static void write(const tll_msg_t *m, frame_type * data) { data->size = m->size; }
};

template <>
struct FrameT<tll_frame_bson_t> : public FrameBaseT<tll_frame_bson_t>
{
	static constexpr size_t frame_skip_size() { return 0; }
	static std::vector<std::string_view> name() { return {"bson"}; }
	static void read(tll_msg_t *m, const frame_type * data) { m->size = data->size; }
	static void write(const tll_msg_t *m, frame_type * data) { data->size = m->size; }
};

} // namespace tll::frame

#endif//__cplusplus

#endif//_TLL_CHANNEL_FRAME_H
