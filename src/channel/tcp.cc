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

static constexpr std::string_view write_buffered_scheme = "yamls://[{name: WriteBuffered, id: 11}]";
static constexpr int write_buffered_msgid = 35;

template <typename T, typename F>
class FramedSocket : public tll::channel::TcpSocket<T>
{
 protected:
	size_t _send_hwm = 0;

 public:
	using Frame = F;
	using FrameT = tll::frame::FrameT<Frame>;
	using Base = tll::channel::TcpSocket<T>;

	static constexpr std::string_view param_prefix() { return "tcp"; }

	int _post_data(const tll_msg_t *msg, int flags);
	int _process(long timeout, int flags);

	void _on_output_full()
	{
		if (this->_wbuf.size() > _send_hwm)
			return Base::_on_output_full();

		tll_msg_t msg = {
			.type = TLL_MESSAGE_CONTROL,
			.msgid = write_buffered_msgid,
			.addr = this->_msg_addr,
		};
		this->_callback(&msg);
	};

	void send_hwm(size_t hwm)
	{
		_send_hwm = hwm;
	}

 private:
	int _pending();
};

template <typename T>
class FramedSocket<T, void> : public tll::channel::TcpSocket<T>
{
 protected:
	size_t _send_hwm = 0;

 public:
	using Base = tll::channel::TcpSocket<T>;
	void _on_output_full()
	{
		if (this->_wbuf.size() > _send_hwm)
			Base::_on_output_full();
	};

	void send_hwm(size_t hwm)
	{
		_send_hwm = hwm;
	}
};

template <typename Frame>
class ChTcpClient : public tll::channel::TcpClient<ChTcpClient<Frame>, FramedSocket<ChTcpClient<Frame>, Frame>>
{
 public:
	using Base = tll::channel::TcpClient<ChTcpClient<Frame>, FramedSocket<ChTcpClient<Frame>, Frame>>;

	static constexpr std::string_view param_prefix() { return "tcp"; }
	static constexpr std::string_view channel_protocol() { return "tcp-client"; } // Only visible in logs

	int _init(const tll::Channel::Url &url, tll::Channel *master)
	{
		if (auto r = Base::_init(url, master); r)
			return r;

		auto reader = this->channel_props_reader(url);
		auto hwm = reader.getT("send-buffer-hwm", tll::util::Size { 0 });
		if (!reader)
			return this->_log.fail(EINVAL, "Invalid url: {}", reader.error());
		if (hwm > this->_settings.snd_buffer_size * 0.8)
			return this->_log.fail(EINVAL, "Send HWM is too large: {} > 80% of send buffer {}", hwm, this->_settings.snd_buffer_size);
		if (hwm)
			this->_log.debug("Store up to {} of data on blocked connection", hwm);
		this->_send_hwm = hwm;
		return 0;
	}
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
	size_t _send_hwm = 0;
 public:
	using Base = tll::channel::TcpServer<ChTcpServer<Frame>, ChFramedSocket<Frame>>;
	using Socket = ChFramedSocket<Frame>;

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

		auto reader = this->channel_props_reader(url);
		auto hwm = reader.getT("send-buffer-hwm", tll::util::Size { 0 });
		if (!reader)
			return this->_log.fail(EINVAL, "Invalid url: {}", reader.error());
		if (hwm > this->_settings.snd_buffer_size * 0.8)
			return this->_log.fail(EINVAL, "Send HWM is too large: {} > 80% of send buffer {}", hwm, this->_settings.snd_buffer_size);
		if (hwm)
			this->_log.debug("Store up to {} of data on blocked connection", hwm);
		this->_send_hwm = hwm;
		return 0;
	}

	int _on_accept(tll::Channel * c) {
		auto socket = tll::channel_cast<Socket>(c);
		if (!socket)
			return this->_log.fail(EINVAL, "Can not cast {} to socket channel", c->name());
		socket->send_hwm(this->_send_hwm);
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

	if (this->_wbuf.size()) {
		if (this->_wbuf.size() > _send_hwm)
			return EAGAIN;
		this->_log.trace("Store {} + {} bytes of data", FrameT::frame_skip_size(), msg->size);
		if constexpr (FrameT::frame_skip_size() != 0) {
			Frame frame;
			tll::frame::FrameT<Frame>::write(msg, &frame);
			this->_store_output(&frame, sizeof(frame));
		}
		this->_store_output(msg->data, msg->size);
		if (this->_wbuf.size() > _send_hwm)
			_on_output_full();
		return 0;
	}

	this->_log.trace("Post {} + {} bytes of data", FrameT::frame_size(), msg->size);
	int r = 0;
	if constexpr (FrameT::frame_skip_size() != 0) {
		Frame frame;
		FrameT::write(msg, &frame);
		r = this->_sendv(tll::memory {(void *) &frame, sizeof(frame)}, *msg);
		if (r)
			return this->_log.fail(r, "Failed to post data");
	} else {
		r = this->_sendv(*msg);
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
