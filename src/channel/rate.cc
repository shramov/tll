/*
 * Copyright (c) 2023 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#include "channel/rate.h"

#include "tll/util/listiter.h"
#include "tll/util/size.h"
#include "tll/util/time.h"

#include "tll/scheme/merge.h"

#include "tll/scheme/channel/timer.h"
#include "tll/channel/tcp-client-scheme.h"

using namespace tll::channel;

TLL_DEFINE_IMPL(Rate);

int Rate::_init(const Channel::Url &url, tll::Channel *master)
{
	using namespace std::chrono;

	if (auto r = Base::_init(url, master); r)
		return r;

	auto reader = channel_props_reader(url);

	auto interval = reader.getT<rate::Settings::fseconds>("interval", 1s);
	_conf.speed = reader.getT<tll::util::SizeT<double>>("speed");
	_conf.limit = reader.getT<tll::util::Size>("max-window", 16 * 1024);
	_conf.initial = reader.getT<tll::util::Size>("initial", _conf.limit / 2);

	if (!reader)
		return _log.fail(EINVAL, "Invalid url: {}", reader.error());

	if (interval.count() == 0)
		return _log.fail(EINVAL, "Zero interval");

	_conf.speed /= interval.count();

	if (_conf.speed == 0) return _log.fail(EINVAL, "Zero speed");
	if (_conf.limit <= 0) return _log.fail(EINVAL, "Invalid window size: {}", _conf.limit);

	if ((internal.caps & tll::caps::InOut) == 0)
		internal.caps |= tll::caps::Output;

	auto cscheme = _child->scheme(TLL_MESSAGE_CONTROL);
	if (internal.caps & tll::caps::Output) {
		auto r = tll::scheme::merge({context().scheme_load(tcp_client_scheme::scheme_string), cscheme});
		if (!r)
			return _log.fail(EINVAL, "Failed to merge control scheme: {}", r.error());
		_scheme_control.reset(*r);
	} else
		_scheme_control.reset(cscheme);

	auto curl = this->child_url_parse("timer://;clock=realtime", "timer");
	if (!curl)
		return this->_log.fail(EINVAL, "Failed to parse timer url: {}", curl.error());
	_timer = this->context().channel(*curl);
	if (!_timer)
		return this->_log.fail(EINVAL, "Failed to create timer channel");
	_timer->callback_add([](auto * c, auto * m, void * user) { return static_cast<Rate *>(user)->_on_timer(m); }, this, TLL_MESSAGE_MASK_DATA);
	this->_child_add(_timer.get(), "timer");

	return 0;
}

int Rate::_on_timer(const tll_msg_t *)
{
	auto empty = _bucket.empty();
	auto now = tll::time::now();
	_bucket.update(_conf, now);
	if (empty == _bucket.empty()) {
		_rearm(_bucket.next(_conf, now));
		return 0;
	}

	if (internal.caps & tll::caps::Output)
		_callback_control(tcp_client_scheme::WriteReady::meta_id());
	else
		_child->resume();

	return 0;
}

int Rate::_on_data(const tll_msg_t * msg)
{
	if (internal.caps & tll::caps::Output)
		return Base::_on_data(msg);

	size_t size = msg->size;
	auto now = tll::time::now();
	_bucket.update(_conf, now);

	_bucket.consume(size);

	if (_bucket.empty()) {
		if (_rearm(_bucket.next(_conf, now)))
			return _log.fail(EINVAL, "Failed to rearm timer");
		_child->suspend();
	}
	return Base::_on_data(msg);
}

int Rate::_post(const tll_msg_t *msg, int flags)
{
	if (msg->type != TLL_MESSAGE_DATA)
		return _child->post(msg, flags);

	if (!(internal.caps & tll::caps::Output))
		return _child->post(msg, flags);

	size_t size = msg->size;
	auto now = tll::time::now();
	_bucket.update(_conf, now);

	if (_bucket.empty() && !(flags & TLL_POST_URGENT))
		return EAGAIN;

	if (auto r = _child->post(msg, flags); r)
		return r;

	_bucket.consume(size);

	if (_bucket.empty()) {
		if (_rearm(_bucket.next(_conf, now)))
			return _log.fail(EINVAL, "Failed to rearm timer");
		_callback_control(tcp_client_scheme::WriteFull::meta_id());
	}
	return 0;
}

int Rate::_rearm(const tll::duration &dt)
{
	timer_scheme::relative data = { dt };
	tll_msg_t msg = {};
	msg.msgid = data.id;
	msg.data = &data;
	msg.size = sizeof(data);
	return _timer->post(&msg);
}
