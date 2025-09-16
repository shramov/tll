/*
 * Copyright (c) 2018-2021 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#ifndef _TLL_CHANNEL_DIRECT_H
#define _TLL_CHANNEL_DIRECT_H

#include "tll/channel/autoseq.h"
#include "channel/emulate-control.h"

#include <memory>
#include <mutex>

class ChDirect : public tll::channel::EmulateControl<ChDirect, tll::channel::AutoSeq<ChDirect>>
{
	using Base = tll::channel::AutoSeq<ChDirect>;
	enum Mode { Master = 0, Slave = 1 } _mode = Slave;
	struct Pointers {
		ChDirect * master = nullptr;
		ChDirect * slave = nullptr;

		ChDirect ** get(Mode mode) { if (mode == Master) return &master; else return &slave; }
	};
	std::shared_ptr<Pointers> _ptr;

	bool _notify_state = false;
	bool _manual_open = false;

 public:
	static constexpr std::string_view channel_protocol() { return "direct"; }
	static constexpr auto process_policy() { return ProcessPolicy::Never; }
	static constexpr auto open_policy() { return OpenPolicy::Manual; }

	int _init(const tll::Channel::Url &url, tll::Channel * master);
	int _open(const tll::ConstConfig &cfg)
	{
		if (auto r = Base::_open(cfg); r)
			return _log.fail(EINVAL, "Base open failed");
		_update_state(tll::state::Opening);
		*_ptr->get(_mode) = this;
		if (!_manual_open)
			_update_state(tll::state::Active);
		return 0;
	}

	int _close()
	{
		_update_state(tll::state::Closing);
		*_ptr->get(_mode) = nullptr;
		_update_state(tll::state::Closed);
		return 0;
	}

	void _free()
	{
		_ptr.reset();
	}

	int _post(const tll_msg_t *msg, int flags)
	{
		auto ptr = *_ptr->get(_invert(_mode));
		if (ptr) {
			if (msg->type == TLL_MESSAGE_STATE) {
				auto s = (tll_state_t) msg->msgid;
				_log.info("Change sibling state {} to {}", ptr->name, tll_state_str(s));
				ptr->state(s);
				return 0;
			}
			ptr->_callback(_autoseq.update(msg));
		}
		return 0;
	}

	int _process(long timeout, int flags) { return EAGAIN; }

	void _update_state(tll_state_t state);
 private:
	Mode _invert(Mode m) { return (Mode) (1 - m); }
};

#endif//_TLL_CHANNEL_DIRECT_H
