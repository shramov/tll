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

#include "tll/scheme/channel/timer.h"
#include "tll/channel/tcp-client-scheme.h"

using namespace tll::channel;

TLL_DEFINE_IMPL(Rate);

namespace {
bool intersects(const tll::Scheme * s0, const tll::Scheme * s1)
{
	if (!s1)
		return false;
	for (auto & m0 : tll::util::list_wrap(s0->messages)) {
		if (s1->lookup(m0.name))
			return true;
	}
	return false;
}

bool contains(const tll::Scheme * outer, const tll::Scheme * inner)
{
	if (!inner)
		return true;
	for (auto & mi : tll::util::list_wrap(inner->messages)) {
		if (!outer->lookup(mi.name))
			return false;
	}
	return true;
}

const tll::Scheme * merge(tll::channel::Context ctx, std::string_view s0_string, const tll::Scheme * s1)
{
	tll::scheme::ConstSchemePtr s0(ctx.scheme_load(s0_string));
	if (!s1)
		return s0.release();

	if (!intersects(s0.get(), s1)) {
		auto s1_string = tll_scheme_dump(s1, "yamls+gz");
		auto merged = fmt::format("yamls://[{{name: '', import: ['{}', '{}']}}]", s0_string, s1_string);
		free(s1_string);
		return ctx.scheme_load(merged);
	}

	for (auto & m0 : tll::util::list_wrap(s0->messages)) {
		auto m1 = s1->lookup(m0.name);
		if (!m1)
			continue;

		if (m1->msgid != m0.msgid)
			return nullptr;
		if (m1->size != m0.size)
			return nullptr;
	}

	if (contains(s0.get(), s1))
		return s0.release();
	if (contains(s1, s0.get()))
		return s1;
	return nullptr;

}
}

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

	_scheme_control.reset(merge(context(), tcp_client_scheme::scheme_string, _child->scheme(TLL_MESSAGE_CONTROL)));
	if (!_scheme_control.get())
		return _log.fail(EINVAL, "Failed to load control scheme");

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

	_callback_control(tcp_client_scheme::WriteReady::meta_id());

	return 0;
}

int Rate::_post(const tll_msg_t *msg, int flags)
{
	if (msg->type != TLL_MESSAGE_DATA)
		return _child->post(msg, flags);

	size_t size = msg->size;
	auto now = tll::time::now();
	_bucket.update(_conf, now);

	if (_bucket.empty())
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
