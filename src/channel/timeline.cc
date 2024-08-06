// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Pavel Shramov <shramov@mexmat.net>

#include "channel/timeline.h"

#include "tll/scheme/channel/timer.h"

using namespace tll::channel;

TLL_DEFINE_IMPL(TimeLine);

int TimeLine::_init(const Channel::Url &url, tll::Channel *master)
{
	using namespace std::chrono;

	if (auto r = Base::_init(url, master); r)
		return r;

	auto reader = channel_props_reader(url);

	_speed = reader.getT<double>("speed", 1.0);

	if (!reader)
		return _log.fail(EINVAL, "Invalid url: {}", reader.error());

	if (_speed < 0)
		return _log.fail(EINVAL, "Negative speed is invalid: {}", _speed);

	auto curl = this->child_url_parse("timer://;clock=realtime", "timer");
	if (!curl)
		return this->_log.fail(EINVAL, "Failed to parse timer url: {}", curl.error());
	_timer = this->context().channel(*curl);
	if (!_timer)
		return this->_log.fail(EINVAL, "Failed to create timer channel");
	_timer->callback_add<TimeLine, &TimeLine::_on_timer>(this, TLL_MESSAGE_MASK_DATA);
	this->_child_add(_timer.get(), "timer");

	return 0;
}

int TimeLine::_on_timer(const tll::Channel *, const tll_msg_t *)
{
	_callback_data(&_msg);
	_child->resume();

	return 0;
}

int TimeLine::_on_data(const tll_msg_t * msg)
{
	if (!msg->time) {
		_callback_data(msg);
		return 0;
	}

	auto now = tll::time::now();
	if (_next == tll::time_point()) {
		_next = now;
		tll_msg_copy_info(&_msg, msg);
		_callback_data(msg);
		return 0;
	}

	auto dt = std::chrono::nanoseconds(msg->time - _msg.time);

	_next += std::chrono::duration_cast<std::chrono::nanoseconds>(dt / _speed);
	tll_msg_copy_info(&_msg, msg);

	if (_next > now) {
		if (_buf.size() < msg->size)
			_buf.resize(msg->size);
		memcpy(_buf.data(), msg->data, msg->size);
		_msg.data = _buf.data();
		_msg.size = msg->size;

		if (_rearm(_next - now))
			return _log.fail(EINVAL, "Failed to rearm timer");
		_child->suspend();
	} else
		_callback_data(&_msg);
	return 0;
}

int TimeLine::_rearm(const tll::duration &dt)
{
	timer_scheme::relative data = { dt };
	tll_msg_t msg = {};
	msg.msgid = data.id;
	msg.data = &data;
	msg.size = sizeof(data);
	return _timer->post(&msg);
}
