/*
 * Copyright (c) 2023 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#ifndef _CHANNEL_RATE_H
#define _CHANNEL_RATE_H

#include "tll/channel/prefix.h"
#include "tll/channel/rate.h"
#include "tll/util/pointer_list.h"

namespace tll::channel {

class Rate : public tll::channel::Prefix<Rate>
{
	std::unique_ptr<tll::Channel> _timer;

	struct Bucket : public rate::Bucket
	{
		rate::Settings conf;
	};
	std::vector<Bucket> _buckets;

	Rate * _master = nullptr;
	tll::util::PointerList<Rate> _notify;
	size_t _notify_last = 0;

 public:
	using Base = tll::channel::Prefix<Rate>;
	static constexpr std::string_view channel_protocol() { return "rate+"; }
	static constexpr auto prefix_export_policy() { return PrefixExportPolicy::Strip; }

	const Scheme * scheme(int type) const
	{
		if (type == TLL_MESSAGE_CONTROL)
			return _scheme_control.get();
		return Base::scheme(type);
	}

	int _init(const Channel::Url &url, tll::Channel *master);
	void _free()
	{
		for (auto s : _notify) {
			if (s)
				s->_master = nullptr;
		}
	}

	int _open(const tll::ConstConfig &cfg)
	{
		for (auto & b: _buckets)
			b.reset();
		if (_timer)
			_timer->open();
		if (_master) {
			_master->_notify.insert(this);
			auto empty = false;
			for (auto b : _master->_buckets)
				empty = empty || b.empty();
			if (empty)
				_rate_full();
		}

		return Base::_open(cfg);
	}

	int _on_closed()
	{
		if (_timer)
			_timer->close(true);
		if (!(internal.caps & tll::caps::Output) && (_child->dcaps() & tll::dcaps::SuspendPermanent)) {
			if (internal.dcaps & tll::dcaps::Suspend) {
				_child->internal->dcaps ^= tll::dcaps::SuspendPermanent; // Remove suspend lock
			} else
				_child->resume();
		}
		if (_master)
			_master->_detach(this);
		return Base::_on_closed();
	}

	int _on_data(const tll_msg_t *);

	int _post(const tll_msg_t *msg, int flags);

 private:
	int _on_timer(const tll_msg_t *msg);
	int _rearm(const tll::duration &dt);

	void _rate_full();
	void _rate_ready();
	int _update_buckets(tll::time_point now, size_t size);

	void _detach(const Rate *ptr)
	{
		_notify.erase(ptr);
		_notify_last = 0;
	}

	int _parse_bucket(const tll::ConstConfig &cfg);

	void _callback_control(int msgid)
	{
		tll_msg_t msg = { TLL_MESSAGE_CONTROL };
		msg.msgid = msgid;
		_callback(&msg);
	}
};

} // namespace tll::channel

#endif//_CHANNEL_RATE_H
