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

template <typename T, typename F>
class FramedSocket : public tll::channel::TcpSocket<T>
{
 public:
	using Frame = F;
	using FrameT = tll::frame::FrameT<Frame>;
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

#define TCP_DEFINE_IMPL_ALL(frame) \
	TLL_DEFINE_IMPL(ChTcpClient<frame>); \
	TLL_DEFINE_IMPL(ChTcpServer<frame>); \
	TLL_DEFINE_IMPL(ChFramedSocket<frame>); \
	TLL_DEFINE_IMPL(tll::channel::TcpServerSocket<ChTcpServer<frame>>)

TCP_DEFINE_IMPL_ALL(void);
TCP_DEFINE_IMPL_ALL(tll_frame_t);
TCP_DEFINE_IMPL_ALL(tll_frame_short_t);
TCP_DEFINE_IMPL_ALL(tll_frame_tiny_t);
TCP_DEFINE_IMPL_ALL(tll_frame_size32_t);
TCP_DEFINE_IMPL_ALL(tll_frame_bson_t);

namespace tll::frame {

template <>
struct FrameT<void>
{
	using frame_type = void;
	static std::vector<std::string_view> name() { return {"none"}; }
	static void read(tll_msg_t *m, const frame_type * data) {}
	static void write(const tll_msg_t *m, frame_type * data) {}
};

} // namespace tll::frame

using tll::channel::TcpChannelMode;

template <typename Frame>
const tll_channel_impl_t * _check_impl(tll::channel::TcpChannelMode mode, std::string_view frame)
{
	for (auto & n : tll::frame::FrameT<Frame>::name()) {
		if (n == frame) {
			switch (mode) {
			case TcpChannelMode::Client: return &ChTcpClient<Frame>::impl;
			case TcpChannelMode::Server: return &ChTcpServer<Frame>::impl;
			case TcpChannelMode::Socket: return &ChFramedSocket<Frame>::impl;
			}
		}
	}
	return nullptr;
}

std::optional<const tll_channel_impl_t *> ChTcp::_init_replace(const tll::Channel::Url &url, tll::Channel *master)
{
	auto reader = channel_props_reader(url);
	auto mode = reader.getT("mode", TcpChannelMode::Client);
	auto frame = reader.getT<std::string>("frame", "std");
	if (!reader)
		return _log.fail(std::nullopt, "Invalid url: {}", reader.error());

	if (auto r = _check_impl<void>(mode, frame); r) // Empty frame
		return r;
	if (auto r = _check_impl<tll_frame_t>(mode, frame); r)
		return r;
	if (auto r = _check_impl<tll_frame_short_t>(mode, frame); r)
		return r;
	if (auto r = _check_impl<tll_frame_tiny_t>(mode, frame); r)
		return r;
	if (auto r = _check_impl<tll_frame_size32_t>(mode, frame); r)
		return r;
	if (auto r = _check_impl<tll_frame_bson_t>(mode, frame); r)
		return r;

	return _log.fail(std::nullopt, "Unknown frame '{}", frame);
}

template <typename T, typename F>
int FramedSocket<T, F>::_post_data(const tll_msg_t *msg, int flags)
{
	if (msg->type != TLL_MESSAGE_DATA)
		return 0;
	if (this->_wbuf.size())
		return EAGAIN;
	this->_log.trace("Post {} + {} bytes of data", FrameT::frame_size(), msg->size);
	int r = 0;
	if constexpr (FrameT::frame_skip_size() != 0) {
		Frame frame;
		FrameT::write(msg, &frame);
		r = this->template _sendv(tll::memory {(void *) &frame, sizeof(frame)}, *msg);
		if (r)
			return this->_log.fail(r, "Failed to post data");
	} else {
		r = this->template _sendv(*msg);
	}
	if (r)
		return this->_log.fail(r, "Failed to post data");
	return 0;
}


template <typename T, typename F>
int FramedSocket<T, F>::_pending()
{
	auto frame = this->template rdataT<Frame>();
	if (!frame)
		return EAGAIN;
	// Check for pending data
	const auto full_size = FrameT::frame_skip_size() + frame->size;
	if (this->_rbuf.size() < full_size) {
		if (full_size > this->_rbuf.capacity())
			return this->_log.fail(EMSGSIZE, "Message size {} too large", full_size);
		this->_dcaps_pending(false);
		return EAGAIN;
	}

	tll_msg_t msg = { TLL_MESSAGE_DATA };
	FrameT::read(&msg, frame);
	msg.data = this->_rbuf.template dataT<void>(FrameT::frame_skip_size(), 0);
	msg.addr = this->_msg_addr;
	msg.time = this->_timestamp.count();
	this->rdone(full_size);
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
	this->_log.trace("Got {} bytes of data", *s);
	return this->_pending();
}
