#include "framed.h"

#include <tll/channel/frame.h>
#include <tll/util/memoryview.h>
#include <tll/util/size.h>

TLL_DEFINE_IMPL(Framed<tll_frame_t>);
TLL_DEFINE_IMPL(Framed<tll_frame_short_t>);
TLL_DEFINE_IMPL(Framed<tll_frame_tiny_t>);

template <typename Frame>
std::optional<const tll_channel_impl_t *> Framed<Frame>::_init_replace(const tll::Channel::Url &url, tll::Channel *master)
{
	if constexpr (!std::is_same_v<Frame, tll_frame_t>)
		return nullptr;

	auto reader = this->channel_props_reader(url);
	auto frame = reader.template getT<std::string>("frame", "std");
	if (!reader)
		return this->_log.fail(std::nullopt, "Invalid url: {}", reader.error());

	for (auto & n : tll::frame::FrameT<tll_frame_t>::name()) {
		if (n == frame)
			return nullptr; //&Framed<tll_frame_t>::impl;
	}

	for (auto & n : tll::frame::FrameT<tll_frame_short_t>::name()) {
		if (n == frame)
			return &Framed<tll_frame_short_t>::impl;
	}

	for (auto & n : tll::frame::FrameT<tll_frame_tiny_t>::name()) {
		if (n == frame)
			return &Framed<tll_frame_tiny_t>::impl;
	}

	return this->_log.fail(std::nullopt, "Unknown frame '{}", frame);
}

template <typename Frame>
int Framed<Frame>::_init(const tll::Channel::Url &url, tll::Channel *master)
{
	auto reader = this->channel_props_reader(url);
	_max_size = reader.getT("max-size", tll::util::Size { 1024 * 1024 });
	if (!reader)
		return this->_log.fail(EINVAL, "Invalid url: {}", reader.error());

	return Base::_init(url, master);
}

template <typename Frame>
int Framed<Frame>::_open(const tll::ConstConfig &cfg)
{
	_recv_start = _recv_end = 0;
	return Base::_open(cfg);
}

template <typename Frame>
int Framed<Frame>::_post(const tll_msg_t *msg, int flags)
{
	if (msg->type != TLL_MESSAGE_DATA)
		return this->_child->post(msg, flags);
	auto full = msg->size + sizeof(Frame);
	if (_buf_send.size() < full)
		_buf_send.resize(full);
	auto frame = (Frame *) _buf_send.data();
	tll::frame::FrameT<Frame>::write(msg, frame);
	memcpy(frame + 1, msg->data, msg->size);
	_msg_send = *msg;
	_msg_send.size = full;
	_msg_send.data = _buf_send.data();
	return this->_child->post(&_msg_send, flags);
}

template <typename Frame>
void Framed<Frame>::_process_data(const Frame * frame)
{
	tll::frame::FrameT<Frame>::read(&_msg_recv, frame);
	_msg_recv.data = frame + 1;
	_recv_start += sizeof(Frame) + frame->size;
	if (_recv_start == _recv_end)
		_recv_start = _recv_end = 0;
	this->_callback_data(&_msg_recv);
}

template <typename Frame>
int Framed<Frame>::_on_data(const tll_msg_t *msg)
{
	auto frame = _frame();
	_msg_recv = *msg;
	if (frame != nullptr)
		_process_data(frame);
	if (!msg->size) {
		_pending(_frame());
		return 0;
	}
	if (_recv_end + msg->size > _max_size && _recv_start > 0) {
		memmove(_buf_recv.data(), &_buf_recv[_recv_start], _size());
		_recv_end -= _recv_start;
		_recv_start = 0;
	}
	if (_recv_end + msg->size > _buf_recv.size())
		_buf_recv.resize(_recv_end + msg->size);
	memcpy(&_buf_recv[_recv_end], msg->data, msg->size);
	_recv_end += msg->size;

	if (frame == nullptr && (frame = _frame()) != nullptr)
		_process_data(frame);

	_pending(_frame());
	return 0;
}

template <typename Frame>
int Framed<Frame>::_process(long timeout, int flags)
{
	auto frame = _frame();
	if (frame == nullptr) {
		_pending(frame);
		return EAGAIN;
	}

	_process_data(frame);
	_pending(_frame());
	return 0;
}
