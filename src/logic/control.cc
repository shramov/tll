/*
 * Copyright (c) 2021 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#include "tll/channel/module.h"
#include "tll/channel/tagged.h"
#include "tll/util/json.h"

#include "tll/processor/scheme.h"
#include "tll/scheme/logic/control.h"

namespace tll::channel {

struct Processor : public Tag<TLL_MESSAGE_MASK_ALL> { static constexpr std::string_view name() { return "processor"; } };
struct Uplink : public Tag<TLL_MESSAGE_MASK_ALL> { static constexpr std::string_view name() { return "uplink"; } };

class Control : public Tagged<Control, Input, Processor, Uplink>
{
	using Base = Tagged<Control, Input, Processor, Uplink>;

	std::set<std::pair<uint64_t, tll::Channel *>> _addr;
	std::vector<char> _buf;

	tll::json::JSON _json = { _log };

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
	int callback_tag(TaggedChannel<Uplink> *, const tll_msg_t *msg);

	int _on_external(tll::Channel * channel, const tll_msg_t * msg);

	int _forward(const tll_msg_t * msg)
	{
		tll_msg_t m = *msg;
		for (auto & [a, c] : _addr) {
			m.addr.u64 = a;
			c->post(&m);
		}

		m.addr = {};
		for (auto & c : _channels.get<Uplink>()) {
			if (c.first->state() == tll::state::Active)
				c.first->post(&m);
		}
		return 0;
	}

 private:

	tll::Channel * _processor() { return _channels.get<Processor>().front().first; }
	tll::result_t<int> _message_forward(const tll_msg_t * msg);
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
	return _on_external(c, msg);
}

int Control::callback_tag(TaggedChannel<Uplink> *c, const tll_msg_t *msg)
{
	if (msg->type == TLL_MESSAGE_STATE && msg->msgid == tll::state::Active) {
		return _on_processor_active();
	} else if (msg->type != TLL_MESSAGE_DATA) {
		return 0;
	}
	return _on_external(c, msg);
}

int Control::_on_external(tll::Channel * channel, const tll_msg_t * msg)
{
	switch (msg->msgid) {
	case control_scheme::ConfigGet::meta_id(): {
		auto data = control_scheme::ConfigValue::bind(_buf);
		if (msg->size < data.meta_size())
			return _log.fail(EMSGSIZE, "Message size too small: {} < min {}",
				msg->size, data.meta_size());

		tll_msg_t m = { TLL_MESSAGE_DATA };
		m.msgid = data.meta_id();
		m.addr = msg->addr;

		auto req = control_scheme::ConfigGet::bind(*msg);
		for (auto & [k, cfg] : _config.root().browse(req.get_path())) {
			auto v = cfg.get();
			if (!v)
				continue;
			data.view().resize(0);
			data.view().resize(data.meta_size());
			data.set_key(k);
			data.set_value(*v);
			m.data = data.view().data();
			m.size = data.view().size();
			channel->post(&m);
		}

		m.msgid = control_scheme::ConfigEnd::meta_id();
		m.data = nullptr;
		m.size = 0;
		channel->post(&m);
		break;
	}
	case control_scheme::MessageForward::meta_id(): {
		tll_msg_t reply = { TLL_MESSAGE_DATA };
		reply.seq = msg->seq;
		reply.addr = msg->addr;
		if (auto r = _message_forward(msg); !r) {
			_log.error("Failed to forward message: {}", r.error());
			auto data = control_scheme::Error::bind(_buf);
			data.view().resize(0);
			data.view().resize(data.meta_size());
			data.set_error(r.error());

			reply.msgid = data.meta_id();
			reply.data = data.view().data();
			reply.size = data.view().size();
		} else {
			reply.msgid = control_scheme::Ok::meta_id();
		}
		channel->post(&reply);
		break;
	}

	default:
		break;
	}
	return 0;
}

tll::result_t<int> Control::_message_forward(const tll_msg_t *msg)
{
	auto req = control_scheme::MessageForward::bind(*msg);
	if (msg->size < req.meta_size())
		return error(fmt::format("Message size too small: {} < min {}",
			msg->size, req.meta_size()));

	auto data = processor_scheme::MessageForward::bind(_buf);
	tll_msg_t m = { TLL_MESSAGE_DATA };
	m.msgid = data.meta_id();

	data.view().resize(0);
	data.view().resize(data.meta_size());

	data.set_dest(req.get_dest());

	auto reqm = req.get_data();
	auto datam = data.get_data();

	auto name = req.get_dest();
	auto channel = context().get(name);
	if (!channel)
		return error(fmt::format("Object '{}' not found", name));
	auto type = (int16_t) reqm.get_type();
	auto scheme = channel->scheme(type);
	if (!scheme)
		return error(fmt::format("No scheme for message type {}", type));

	auto message = scheme->lookup(reqm.get_name());
	if (!message)
		return error(fmt::format("Message '{}' not found", reqm.get_name()));

	_json.init_scheme(scheme);

	tll_msg_t out = {};
	if (auto r = _json.decode(message, reqm.get_data(), &out); r)
		datam.set_data(std::string_view((const char *) r->data, r->size));
	else
		return error("Failed to decode JSON body");

	datam.set_type(type);
	datam.set_msgid(message->msgid);
	datam.set_seq(reqm.get_seq());
	datam.set_addr(reqm.get_addr());

	m.data = data.view().data();
	m.size = data.view().size();
	_processor()->post(&m);
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
