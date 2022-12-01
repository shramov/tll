/*
 * Copyright (c) 2018-2021 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#include "channel/tcp.h"

#include "tll/channel/frame.h"
#include "tll/channel/tcp.h"
#include "tll/channel/tcp.hpp"

#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>

using namespace tll;

template <typename T, typename Frame>
class FramedSocket : public tll::channel::TcpSocket<T>
{
 public:
	static constexpr std::string_view param_prefix() { return "tcp"; }

	int _post_data(const tll_msg_t *msg, int flags);
	int _process(long timeout, int flags);

 private:
	int _pending();
};

template <typename T>
class FramedSocket<T, void> : public tll::channel::TcpSocket<T> {};

template <typename Frame>
class ChTcpClient : public tll::channel::TcpClient<ChTcpClient<Frame>, FramedSocket<ChTcpClient<Frame>, Frame>>
{
 public:
	static constexpr std::string_view param_prefix() { return "tcp"; }
	static constexpr std::string_view channel_protocol() { return "tcp-client"; } // Only visible in logs
};

template <typename Frame>
class ChFramedSocket : public FramedSocket<ChFramedSocket<Frame>, Frame>
{
 public:
	static constexpr std::string_view param_prefix() { return "tcp"; }
	static constexpr std::string_view channel_protocol() { return "tcp-socket"; } // Only visible in logs
};

template <typename Frame>
class ChTcpServer : public tll::channel::TcpServer<ChTcpServer<Frame>, ChFramedSocket<Frame>>
{
 public:
	using Base = tll::channel::TcpServer<ChTcpServer<Frame>, ChFramedSocket<Frame>>;
	static constexpr std::string_view channel_protocol() { return "tcp"; }

	int _init(const tll::Channel::Url &url, tll::Channel *master)
	{
		if (auto r = Base::_init(url, master))
			return r;
		if constexpr (std::is_same_v<Frame, void>) {
			this->_socket_url.set("frame", "none");
		} else {
			this->_socket_url.set("frame", tll::frame::FrameT<Frame>::name()[0]);
		}
		return 0;
	}
};

TLL_DEFINE_IMPL(ChTcp);

TLL_DEFINE_IMPL(ChTcpClient<void>);
TLL_DEFINE_IMPL(ChTcpServer<void>);
TLL_DEFINE_IMPL(ChFramedSocket<void>);
TLL_DEFINE_IMPL(tll::channel::TcpServerSocket<ChTcpServer<void>>);

TLL_DEFINE_IMPL(ChTcpClient<tll_frame_t>);
TLL_DEFINE_IMPL(ChTcpServer<tll_frame_t>);
TLL_DEFINE_IMPL(ChFramedSocket<tll_frame_t>);
TLL_DEFINE_IMPL(tll::channel::TcpServerSocket<ChTcpServer<tll_frame_t>>);

TLL_DEFINE_IMPL(ChTcpClient<tll_frame_short_t>);
TLL_DEFINE_IMPL(ChTcpServer<tll_frame_short_t>);
TLL_DEFINE_IMPL(ChFramedSocket<tll_frame_short_t>);
TLL_DEFINE_IMPL(tll::channel::TcpServerSocket<ChTcpServer<tll_frame_short_t>>);

TLL_DEFINE_IMPL(ChTcpClient<tll_frame_tiny_t>);
TLL_DEFINE_IMPL(ChTcpServer<tll_frame_tiny_t>);
TLL_DEFINE_IMPL(ChFramedSocket<tll_frame_tiny_t>);
TLL_DEFINE_IMPL(tll::channel::TcpServerSocket<ChTcpServer<tll_frame_tiny_t>>);

std::optional<const tll_channel_impl_t *> ChTcp::_init_replace(const tll::Channel::Url &url, tll::Channel *master)
{
	using tll::channel::TcpChannelMode;

	auto reader = channel_props_reader(url);
	auto mode = reader.getT("mode", TcpChannelMode::Client);
	auto frame = reader.getT<std::string>("frame", "std");
	if (!reader)
		return _log.fail(std::nullopt, "Invalid url: {}", reader.error());

	if (frame == "none") {
		switch (mode) {
		case TcpChannelMode::Client: return &ChTcpClient<void>::impl;
		case TcpChannelMode::Server: return &ChTcpServer<void>::impl;
		case TcpChannelMode::Socket: return &ChFramedSocket<void>::impl;
		}
	}
	for (auto & n : tll::frame::FrameT<tll_frame_t>::name()) {
		if (n == frame) {
			switch (mode) {
			case TcpChannelMode::Client: return &ChTcpClient<tll_frame_t>::impl;
			case TcpChannelMode::Server: return &ChTcpServer<tll_frame_t>::impl;
			case TcpChannelMode::Socket: return &ChFramedSocket<tll_frame_t>::impl;
			}
		}
	}
	for (auto & n : tll::frame::FrameT<tll_frame_short_t>::name()) {
		if (n == frame) {
			switch (mode) {
			case TcpChannelMode::Client: return &ChTcpClient<tll_frame_short_t>::impl;
			case TcpChannelMode::Server: return &ChTcpServer<tll_frame_short_t>::impl;
			case TcpChannelMode::Socket: return &ChFramedSocket<tll_frame_short_t>::impl;
			}
		}
	}
	for (auto & n : tll::frame::FrameT<tll_frame_tiny_t>::name()) {
		if (n == frame) {
			switch (mode) {
			case TcpChannelMode::Client: return &ChTcpClient<tll_frame_tiny_t>::impl;
			case TcpChannelMode::Server: return &ChTcpServer<tll_frame_tiny_t>::impl;
			case TcpChannelMode::Socket: return &ChFramedSocket<tll_frame_tiny_t>::impl;
			}
		}
	}
	return _log.fail(std::nullopt, "Unknown frame '{}", frame);
}

template <typename T, typename Frame>
int FramedSocket<T, Frame>::_post_data(const tll_msg_t *msg, int flags)
{
	if (msg->type != TLL_MESSAGE_DATA)
		return 0;
	if (this->_wbuf.size())
		return EAGAIN;
	this->_log.debug("Post {} + {} bytes of data", sizeof(Frame), msg->size);
	Frame frame;
	tll::frame::FrameT<Frame>::write(msg, &frame);
	int r = this->template _sendv(tll::memory {(void *) &frame, sizeof(frame)}, *msg);
	if (r)
		return this->_log.fail(r, "Failed to post data");
	return 0;
}


template <typename T, typename Frame>
int FramedSocket<T, Frame>::_pending()
{
	auto frame = this->template rdataT<Frame>();
	if (!frame)
		return EAGAIN;
	// Check for pending data
	auto data = this->template rdataT<char>(sizeof(Frame), frame->size);
	if (!data) {
		if (sizeof(Frame) + frame->size > this->_rbuf.capacity())
			return this->_log.fail(EMSGSIZE, "Message size {} too large", frame->size);
		this->_dcaps_pending(false);
		return EAGAIN;
	}

	tll_msg_t msg = { TLL_MESSAGE_DATA };
	tll::frame::FrameT<Frame>::read(&msg, frame);
	msg.data = (void *) data;
	msg.addr = this->_msg_addr;
	msg.time = this->_timestamp.count();
	this->rdone(sizeof(Frame) + frame->size);
	this->_dcaps_pending(this->template rdataT<Frame>());
	this->_callback_data(&msg);
	return 0;
}

template <typename T, typename Frame>
int FramedSocket<T, Frame>::_process(long timeout, int flags)
{
	if (auto r = this->_process_output(); r)
		return r;

	auto r = this->_pending();
	if (r != EAGAIN)
		return r;

	auto s = this->_recv();
	if (!s)
		return EINVAL;
	if (!*s)
		return EAGAIN;
	this->_log.debug("Got {} bytes of data", *s);
	return this->_pending();
}
