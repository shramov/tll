/*
 * Copyright (c) 2021 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#include "tll/channel/module.h"
#include "tll/channel/tagged.h"

#include "tll/processor/scheme.h"

namespace tll::channel {

struct Processor : public Tag<TLL_MESSAGE_MASK_ALL> { static constexpr std::string_view name() { return "processor"; } };

class Control : public Tagged<Control, Input, Processor>
{
	using Base = Tagged<Control, Input, Processor>;

	std::set<std::pair<uint64_t, tll::Channel *>> _addr;
	std::vector<char> _buf;

	int _msgid_connect = 0;
	int _msgid_disconnect = 0;

 public:
	static constexpr std::string_view channel_protocol() { return "control"; }

	int _init(const tll::Channel::Url &, tll::Channel *master);
	int _open(const tll::ConstConfig &);

	int _on_processor_active()
	{
		_log.debug("Request state dump");
		tll_msg_t msg = { TLL_MESSAGE_DATA };
		msg.msgid = processor_scheme::StateDump::meta_id();
		return _processor()->post(&msg);
	}

	int callback_tag(TaggedChannel<Input> *, const tll_msg_t *msg);
	int callback_tag(TaggedChannel<Processor> *, const tll_msg_t *msg);

	int _forward(const tll_msg_t * msg)
	{
		tll_msg_t m = *msg;
		for (auto & [a, c] : _addr) {
			m.addr.u64 = a;
			c->post(&m);
		}
		return 0;
	}

 private:

	tll::Channel * _processor() { return _channels.get<Processor>().front().first; }
};

int Control::_init(const tll::Channel::Url &url, tll::Channel * master)
{
	if (auto r = Base::_init(url, master); r)
		return _log.fail(EINVAL, "Base init failed");

	auto control = _channels.get<Processor>();
	if (control.size() != 1)
		return _log.fail(EINVAL, "Need exactly one 'processor', got {}", control.size());

	return 0;
}

int Control::_open(const tll::ConstConfig &)
{
	if (_processor()->state() == tll::state::Active) {
		return _on_processor_active();
	}
	return 0;
}

int Control::callback_tag(TaggedChannel<Input> *c, const tll_msg_t *msg)
{
	_log.debug("Input message");
	if (msg->type == TLL_MESSAGE_CONTROL) {
		auto s = c->scheme(msg->type);
		if (!s)
			return 0;
		auto m = s->lookup(msg->msgid);
		if (m == nullptr)
			return 0;
		std::string_view name = m->name;
		if (name == "Connect") {
			_log.debug("Connected client {:x} from {}", msg->addr.u64, c->name());
			_addr.emplace(msg->addr.u64, c);
		} else if (name == "Disconnect") {
			_log.debug("Disconnected client {:x} from {}", msg->addr.u64, c->name());
			_addr.erase(std::make_pair(msg->addr.u64, c));
		}
		return 0;
	} else if (msg->type != TLL_MESSAGE_DATA) {
		return 0;
	}
	return 0;
}

int Control::callback_tag(TaggedChannel<Processor> *, const tll_msg_t *msg)
{
	_log.debug("Processor message");
	if (msg->type != TLL_MESSAGE_DATA) {
		if (msg->type == TLL_MESSAGE_STATE && msg->msgid == tll::state::Active)
			return _on_processor_active();
		return 0;
	}
	switch (msg->msgid) {
	case processor_scheme::StateUpdate::meta_id(): {
		auto data = processor_scheme::StateUpdate::bind(*msg);
		_log.debug("Channel {} state {}", data.get_channel(), data.get_state());
		_forward(msg);
		break;
	}
	case processor_scheme::StateDumpEnd::meta_id():
		_forward(msg);
		break;
	default:
		break;
	}
	return 0;
}

} // namespace tll::channel

TLL_DEFINE_IMPL(tll::channel::Control);

TLL_DEFINE_MODULE(tll::channel::Control);
