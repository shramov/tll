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
using Unit = rate::Settings::Unit;

TLL_DEFINE_IMPL(Rate);

int Rate::_init(const Channel::Url &url, tll::Channel *master)
{
	if (auto r = Base::_init(url, master); r)
		return r;

	_master = tll::channel_cast<Rate>(master);

	if (!_master) {
		if (auto r = _parse_bucket(url); r)
			return r;
	}

	for (auto & [n, cfg] : url.browse("bucket.*", true)) {
		if (auto r = _parse_bucket(cfg); r)
			return _log.fail(EINVAL, "Failed to init bucket {}", n);
	}

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

	if (_master)
		return 0;

	auto curl = this->child_url_parse("timer://;clock=realtime", "timer");
	if (!curl)
		return this->_log.fail(EINVAL, "Failed to parse timer url: {}", curl.error());
	_timer = this->context().channel(*curl);
	if (!_timer)
		return this->_log.fail(EINVAL, "Failed to create timer channel");
	_timer->callback_add([](auto * c, auto * m, void * user) { return static_cast<Rate *>(user)->_on_timer(m); }, this, TLL_MESSAGE_MASK_DATA);
	this->_child_add(_timer.get(), "timer");

	_notify.insert(this);

	return 0;
}

int Rate::_parse_bucket(const tll::ConstConfig &cfg)
{
	using namespace std::chrono;

	auto reader = channel_props_reader(cfg);

	auto interval = reader.getT<rate::Settings::fseconds>("interval", 1s);
	rate::Settings _conf;

	_conf.unit = reader.getT("unit", Unit::Byte, {{"byte", Unit::Byte}, {"message", Unit::Message}});
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

	_buckets.emplace_back(Bucket { .conf = _conf });

	return 0;
}

int Rate::_on_timer(const tll_msg_t *)
{
	auto now = tll::time::now();
	tll::duration next = {};
	for (auto & b : _buckets) {
		b.update(b.conf, now);
		if (b.empty())
			next = std::max(next, b.next(b.conf, now));
	}
	if (next.count()) {
		_rearm(next);
		return 0;
	}

	_notify_last = (_notify_last + 1) % _notify.size();
	for (auto i = _notify_last; i != _notify_last + _notify.size(); i++) {
		if (auto s = _notify[i % _notify.size()]; s)
			s->_rate_ready();
	}
	_notify.rebuild();

	return 0;
}

void Rate::_rate_full()
{
	if (internal.caps & tll::caps::Output)
		_callback_control(tcp_client_scheme::WriteFull::meta_id());
	else
		_child->suspend();
}

void Rate::_rate_ready()
{
	if (internal.caps & tll::caps::Output)
		_callback_control(tcp_client_scheme::WriteReady::meta_id());
	else
		_child->resume();
}

int Rate::_update_buckets(tll::time_point now, size_t count)
{
	if (_master)
		return _master->_update_buckets(now, count);

	tll::duration next = {};
	for (auto & b : _buckets) {
		const size_t size = b.conf.unit == Unit::Byte ? count : 1;
		b.update(b.conf, now);

		b.consume(size);

		if (b.empty())
			next = std::max(next, b.next(b.conf, now));
	}

	if (next.count()) {
		if (_rearm(next))
			return _log.fail(EINVAL, "Failed to rearm timer");
		for (auto s : _notify) {
			if (s)
				s->_rate_full();
		}
	}

	return 0;
}

int Rate::_on_data(const tll_msg_t * msg)
{
	if (internal.caps & tll::caps::Output)
		return Base::_on_data(msg);

	if (auto r = _update_buckets(tll::time::now(), msg->size); r)
		return r;
	return Base::_on_data(msg);
}

int Rate::_post(const tll_msg_t *msg, int flags)
{
	if (msg->type != TLL_MESSAGE_DATA)
		return _child->post(msg, flags);

	if (!(internal.caps & tll::caps::Output))
		return _child->post(msg, flags);

	auto now = tll::time::now();
	bool empty = false;
	for (auto & b : _buckets) {
		b.update(b.conf, now);

		empty = empty || b.empty();
	}

	if (empty && !(flags & TLL_POST_URGENT))
		return EAGAIN;

	if (auto r = _child->post(msg, flags); r)
		return r;

	return _update_buckets(now, msg->size);
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
