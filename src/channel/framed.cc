#include "framed.h"

#include <tll/channel/frame.h>
#include <tll/util/memoryview.h>
#include <tll/util/size.h>

template <typename T, typename Frame>
class CommonFrame : public tll::channel::Prefix<T>
{
	using Base = tll::channel::Prefix<T>;
	using FrameT = tll::frame::FrameT<Frame>;

 protected:
	std::vector<char> _buf_send;
	tll_msg_t _msg_send = {};

 public:
	int _post(const tll_msg_t *msg, int flags)
	{
		if (msg->type != TLL_MESSAGE_DATA)
			return this->_child->post(msg, flags);
		if constexpr (FrameT::frame_skip_size() != 0) {
			auto full = msg->size + FrameT::frame_skip_size();
			if (_buf_send.size() < full)
				_buf_send.resize(full);
			auto frame = (Frame *) _buf_send.data();
			FrameT::write(msg, frame);
			memcpy(frame + 1, msg->data, msg->size);
			_msg_send = *msg;
			_msg_send.size = full;
			_msg_send.data = _buf_send.data();
			return this->_child->post(&_msg_send, flags);
		} else {
			return this->_child->post(msg, flags);
		}
	}
};

template <typename Frame>
class TcpFrame : public CommonFrame<TcpFrame<Frame>, Frame>
{
	using Base = CommonFrame<TcpFrame<Frame>, Frame>;

	std::vector<char> _buf_recv;
	size_t _recv_start = 0;
	size_t _recv_end = 0;
	size_t _max_size = 0;

	tll_msg_t _msg_recv = {};

 public:
	static constexpr std::string_view channel_protocol() { return "frame+"; }

	int _init(const tll::Channel::Url &, tll::Channel *master);
	int _open(const tll::ConstConfig &cfg);

	int _on_data(const tll_msg_t *msg);
	int _process(long timeout, int flags);

 private:
	void _process_data(const Frame * frame);
	size_t _size() const { return _recv_end - _recv_start; }
	const Frame * _frame() const
	{
		auto size = _size();
		if (size < sizeof(Frame)) return nullptr;
		auto frame = (const Frame *) &_buf_recv[_recv_start];
		if (size < tll::frame::FrameT<Frame>::frame_skip_size() + frame->size) return nullptr;
		return frame;
	}

	void _pending(const Frame * frame)
	{
		if (frame == nullptr)
			this->_update_dcaps(0, tll::dcaps::Pending | tll::dcaps::Process);
		else
			this->_update_dcaps(tll::dcaps::Pending | tll::dcaps::Process);
	}
};

template <typename Frame>
class UdpFrame : public CommonFrame<UdpFrame<Frame>, Frame>
{
	using Base = CommonFrame<UdpFrame<Frame>, Frame>;

	tll_msg_t _msg_recv = {};

 public:
	static constexpr std::string_view channel_protocol() { return "frame+"; }

	int _on_data(const tll_msg_t *msg)
	{
		if (msg->size < sizeof(Frame))
			return this->_log.fail(EMSGSIZE, "Message size {} < frame size {}", msg->size, sizeof(Frame));
		auto frame = static_cast<const Frame *>(msg->data);

		_msg_recv.size = msg->size - sizeof(Frame);
		tll::frame::FrameT<Frame>::read(&_msg_recv, frame);
		if (_msg_recv.size + sizeof(Frame) > msg->size)
			return this->_log.fail(EMSGSIZE, "Frame size {} > data size {}", _msg_recv.size, msg->size - sizeof(Frame));
		_msg_recv.data = frame + 1;
		this->_callback_data(&_msg_recv);
		return 0;
	}
};

TLL_DEFINE_IMPL(Framed);

TLL_DEFINE_IMPL(TcpFrame<tll_frame_t>);
TLL_DEFINE_IMPL(TcpFrame<tll_frame_short_t>);
TLL_DEFINE_IMPL(TcpFrame<tll_frame_tiny_t>);
TLL_DEFINE_IMPL(TcpFrame<tll_frame_size32_t>);
TLL_DEFINE_IMPL(TcpFrame<tll_frame_bson_t>);

TLL_DEFINE_IMPL(UdpFrame<tll_frame_t>);
TLL_DEFINE_IMPL(UdpFrame<tll_frame_short_t>);
TLL_DEFINE_IMPL(UdpFrame<tll_frame_seq32_t>);

template <typename Frame, bool Tcp, bool Udp>
const tll_channel_impl_t * _check_impl(std::string_view frame, bool tcp)
{
	for (auto & n : tll::frame::FrameT<Frame>::name()) {
		if (n == frame) {
			if (Tcp && tcp)
				return &TcpFrame<Frame>::impl;
			else if (Udp && !tcp)
				return &UdpFrame<Frame>::impl;
		}
	}
	return nullptr;
}

std::optional<const tll_channel_impl_t *> Framed::_init_replace(const tll::Channel::Url &url, tll::Channel *master)
{
	auto reader = this->channel_props_reader(url);
	auto frame = reader.getT<std::string>("frame", "std");
	auto tcp = reader.getT("type", true, {{"tcp", true}, {"udp", false}});
	if (!reader)
		return this->_log.fail(std::nullopt, "Invalid url: {}", reader.error());

	if (auto r = _check_impl<tll_frame_t, true, true>(frame, tcp); r)
		return r;
	if (auto r = _check_impl<tll_frame_short_t, true, true>(frame, tcp); r)
		return r;
	if (auto r = _check_impl<tll_frame_tiny_t, true, false>(frame, tcp); r)
		return r;
	if (auto r = _check_impl<tll_frame_size32_t, true, false>(frame, tcp); r)
		return r;
	if (auto r = _check_impl<tll_frame_bson_t, true, false>(frame, tcp); r)
		return r;
	if (auto r = _check_impl<tll_frame_seq32_t, false, true>(frame, tcp); r)
		return r;

	return _log.fail(std::nullopt, "Unknown frame '{}' for {}", frame, tcp ? "tcp" : "udp");
}

template <typename Frame>
int TcpFrame<Frame>::_init(const tll::Channel::Url &url, tll::Channel *master)
{
	auto reader = this->channel_props_reader(url);
	_max_size = reader.getT("max-size", tll::util::Size { 1024 * 1024 });
	if (!reader)
		return this->_log.fail(EINVAL, "Invalid url: {}", reader.error());

	return Base::_init(url, master);
}

template <typename Frame>
int TcpFrame<Frame>::_open(const tll::ConstConfig &cfg)
{
	_recv_start = _recv_end = 0;
	return Base::_open(cfg);
}

template <typename Frame>
void TcpFrame<Frame>::_process_data(const Frame * frame)
{
	tll::frame::FrameT<Frame>::read(&_msg_recv, frame);
	const auto full_size = tll::frame::FrameT<Frame>::frame_skip_size() + frame->size;
	if constexpr (tll::frame::FrameT<Frame>::frame_skip_size() != 0) {
		_msg_recv.data = frame + 1;
	} else {
		_msg_recv.data = frame;
	}
	_recv_start += full_size;
	if (_recv_start == _recv_end)
		_recv_start = _recv_end = 0;
	this->_callback_data(&_msg_recv);
}

template <typename Frame>
int TcpFrame<Frame>::_on_data(const tll_msg_t *msg)
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
int TcpFrame<Frame>::_process(long timeout, int flags)
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
