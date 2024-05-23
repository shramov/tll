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
#include "tll/scheme/logic/resolve.h"

#include <sys/param.h>
#include <unistd.h>

namespace tll::channel {

struct Processor : public Tag<TLL_MESSAGE_MASK_ALL> { static constexpr std::string_view name() { return "processor"; } };
struct Uplink : public Tag<TLL_MESSAGE_MASK_ALL> { static constexpr std::string_view name() { return "uplink"; } };
struct Resolve : public Tag<TLL_MESSAGE_MASK_ALL> { static constexpr std::string_view name() { return "resolve"; } };

class Control : public Tagged<Control, Input, Processor, Uplink, Resolve>
{
	using Base = Tagged<Control, Input, Processor, Uplink, Resolve>;

	std::set<std::pair<uint64_t, tll::Channel *>> _addr;
	std::vector<char> _buf;

	tll::json::JSON _json = { _log };

	tll::Channel * _resolve = nullptr;
	std::string _service;
	std::string _hostname;
	std::vector<std::string> _service_tags;

	struct ChannelExport {
		std::string name;
		std::string export_name = "";
		tll::ConstConfig config;
	};

	std::map<std::string, ChannelExport, std::less<>> _exports;

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

	int _on_resolve_active()
	{
		if (!_resolve)
			return 0;
		_log.debug("Export service {}", _service);
		tll_msg_t msg = { TLL_MESSAGE_DATA };
		auto data = resolve_scheme::ExportService::bind(_buf);
		data.view().resize(0);
		data.view().resize(data.meta_size());
		data.set_service(_service);
		data.set_host(_hostname);
		data.get_tags().resize(_service_tags.size());
		for (auto i = 0u; i < _service_tags.size(); i++)
			data.get_tags()[i] = _service_tags[i];

		msg.msgid = data.meta_id();
		msg.data = data.view().data();
		msg.size = data.view().size();
		return _resolve->post(&msg);
	}

	int callback_tag(TaggedChannel<Input> *, const tll_msg_t *msg);
	int callback_tag(TaggedChannel<Processor> *, const tll_msg_t *msg);
	int callback_tag(TaggedChannel<Uplink> *, const tll_msg_t *msg);
	int callback_tag(TaggedChannel<Resolve> *, const tll_msg_t *msg);

	int _on_external(tll::Channel * channel, const tll_msg_t * msg);
	int _on_state_update(std::string_view name, tll_state_t state);

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
	tll::result_t<int> _set_log_level(const tll_msg_t * msg);
	tll::result_t<int> _channel_close(const tll_msg_t * msg);
	int _result_wrap(std::string_view, tll::Channel *, const tll_msg_t *, tll::result_t<int>);
};

int Control::_init(const tll::Channel::Url &url, tll::Channel * master)
{
	if (auto r = Base::_init(url, master); r)
		return _log.fail(EINVAL, "Base init failed");

	if (check_channels_size<Processor>(1, 1))
		return EINVAL;
	if (check_channels_size<Resolve>(0, 1))
		return EINVAL;

	auto resolve = _channels.get<Resolve>().size() > 0;

	auto reader = channel_props_reader(url);
	_service = reader.getT<std::string>("service", "");
	if (resolve) {
		if (_service.empty())
			return _log.fail(EINVAL, "Empty service name, mandatory when resolve is enabled");
		_hostname = reader.getT<std::string>("hostname", "");
		_service_tags = reader.getT("service-tags", std::vector<std::string>());
	}
	if (!reader)
		return _log.fail(EINVAL, "Invalid parameters: {}", reader.error());

	if (_hostname.empty()) {
		auto namemax = sysconf(_SC_HOST_NAME_MAX);
		std::vector<char> hostbuf(namemax), domainbuf(namemax);
		if (gethostname(hostbuf.data(), hostbuf.size()))
			return _log.fail(EINVAL, "Failed to get host name: {}", strerror(errno));
		if (getdomainname(domainbuf.data(), domainbuf.size()))
			return _log.fail(EINVAL, "Failed to get domain name: {}", strerror(errno));
		std::string_view host = hostbuf.data(), domain = domainbuf.data();
		if (domain.size())
			_hostname = fmt::format("{}.{}", host, domain);
		else
			_hostname = host;
		_log.info("Service hostname: {}", _hostname);
	}

	return 0;
}

int Control::_open(const tll::ConstConfig &)
{
	if (auto & list = _channels.get<Resolve>(); list.size()) {
		auto c = list.front().first;
		if (c->state() == tll::state::Active)
			_resolve = c;
		_on_resolve_active();
	}
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
		auto data = control_scheme::Hello::bind_reset(_buf);
		data.set_version((uint16_t) control_scheme::Version::Current);
		data.set_service(_service);

		tll_msg_t m = { .type = TLL_MESSAGE_DATA, .msgid = data.meta_id() };
		m.addr = msg->addr;
		m.data = data.view().data();
		m.size = data.view().size();

		c->post(&m);

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
		auto req = control_scheme::ConfigGet::bind(*msg);
		if (msg->size < req.meta_size())
			return _log.fail(EMSGSIZE, "Message size too small: {} < min {}",
				msg->size, req.meta_size());

		auto data = control_scheme::ConfigValue::bind(_buf);

		tll_msg_t m = { TLL_MESSAGE_DATA };
		m.msgid = data.meta_id();
		m.addr = msg->addr;

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
	case control_scheme::MessageForward::meta_id():
		return _result_wrap("forward message", channel, msg, _message_forward(msg));
	case control_scheme::SetLogLevel::meta_id():
		return _result_wrap("set log level", channel, msg, _set_log_level(msg));
	case control_scheme::ChannelClose::meta_id():
		return _result_wrap("channel close", channel, msg, _channel_close(msg));
	case control_scheme::Ping::meta_id(): {
		tll_msg_t m = { .type = TLL_MESSAGE_DATA, .msgid = control_scheme::Pong::meta_id() };
		m.addr = msg->addr;
		channel->post(&m);
		break;
	}
	default:
		break;
	}
	return 0;
}

int Control::_result_wrap(std::string_view message, tll::Channel * channel, const tll_msg_t * src, tll::result_t<int> result)
{
	tll_msg_t reply = { TLL_MESSAGE_DATA };
	reply.seq = src->seq;
	reply.addr = src->addr;
	if (!result) {
		_log.error("Failed to {}: {}", message, result.error());
		auto data = control_scheme::Error::bind(_buf);
		data.view().resize(0);
		data.view().resize(data.meta_size());
		data.set_error(result.error());

		reply.msgid = data.meta_id();
		reply.data = data.view().data();
		reply.size = data.view().size();
	} else {
		reply.msgid = control_scheme::Ok::meta_id();
	}
	channel->post(&reply);
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

	auto body = reqm.get_data();
	if (body.empty()) {
		datam.set_data(std::string(message->size, '\0'));
	} else {
		_json.init_scheme(scheme);

		tll_msg_t out = {};
		if (auto r = _json.decode(message, body, &out); r)
			datam.set_data(std::string_view((const char *) r->data, r->size));
		else
			return error("Failed to decode JSON body");
	}

	datam.set_type(type);
	datam.set_msgid(message->msgid);
	datam.set_seq(reqm.get_seq());
	datam.set_addr(reqm.get_addr());

	m.data = data.view().data();
	m.size = data.view().size();
	_processor()->post(&m);
	return 0;
}


tll::result_t<int> Control::_set_log_level(const tll_msg_t *msg)
{
	auto data = control_scheme::SetLogLevel::bind(*msg);
	if (msg->size < data.meta_size())
		return error(fmt::format("Message size too small: {} < min {}", msg->size, data.meta_size()));

	auto prefix = data.get_prefix();

	auto level = (uint8_t) data.get_level();
	if (level > tll::Logger::Critical)
		return error(fmt::format("Invalid level: {}", level));

	auto recursive = data.get_recursive() == control_scheme::SetLogLevel::recursive::Yes;

	tll::Logger::set(prefix, (tll::Logger::level_t) level, recursive);
	return 0;
}

tll::result_t<int> Control::_channel_close(const tll_msg_t *msg)
{
	auto data = control_scheme::ChannelClose::bind(*msg);
	if (msg->size < data.meta_size())
		return error(fmt::format("Message size too small: {} < min {}", msg->size, data.meta_size()));

	auto name = data.get_channel();

	if (context().get(name) == nullptr)
		return error(fmt::format("Object '{}' not found", name));

	_processor()->post(msg);
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
		if (msg->size < data.meta_size())
			return _log.fail(EMSGSIZE, "Message size too small: {} < min {}",
				msg->size, data.meta_size());
		_log.debug("Channel {} state {}", data.get_channel(), data.get_state());
		_forward(msg);
		_on_state_update(data.get_channel(), (tll_state_t) data.get_state());
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

int Control::callback_tag(TaggedChannel<Resolve> * channel, const tll_msg_t *msg)
{
	if (msg->type != TLL_MESSAGE_STATE)
		return 0;
	switch ((tll_state_t) msg->msgid) {
	case tll::state::Active:
		_resolve = channel;
		_on_resolve_active();
		_on_processor_active();
		break;
	case tll::state::Error:
	case tll::state::Closing:
		_resolve = nullptr;
		break;
	default:
		break;
	}

	return 0;
}

int Control::_on_state_update(std::string_view name, tll_state_t state)
{
	if (!_resolve)
		return 0;
	if (state == tll::state::Destroy) {
		_exports.erase(std::string(name));
		return 0;
	} else if (state != tll::state::Active)
		return 0;
	auto it = _exports.find(name);
	if (it == _exports.end()) {
		it = _exports.emplace(name, ChannelExport { std::string(name) }).first;
		auto channel = context().get(name);
		if (!channel)
			return _log.fail(EINVAL, "State update for unknown channel {}", name);
		auto config = it->second.config = channel->config();

		auto reader = tll::make_props_reader(config);
		auto exp = reader.getT("url.tll.resolve.export", false);
		if (exp)
			it->second.export_name = reader.getT("url.tll.resolve.export-name", std::string(name));
		if (!reader)
			return _log.fail(EINVAL, "Invalid export parameters in url: {}", reader.error());
	}
	if (!it->second.export_name.size())
		return 0;
	auto config = it->second.config;
	auto client = config.sub("client");
	if (!client)
		return 0;
	auto data = resolve_scheme::ExportChannel::bind(_buf);
	data.view().resize(0);
	data.view().resize(data.meta_size());
	data.set_service(_service);
	data.set_channel(it->second.export_name);
	auto curl = client->browse("**");
	data.get_config().resize(curl.size());
	auto di = data.get_config().begin();
	for (auto & [name, cfg] : curl) {
		di->set_key(name);
		auto value = cfg.get();
		if (value)
			di->set_value(*value);
		di++;
	}
	tll_msg_t msg = {};
	msg.msgid = data.meta_id();
	msg.size = data.view().size();
	msg.data = data.view().data();
	_resolve->post(&msg);
	return 0;
}

} // namespace tll::channel

TLL_DEFINE_IMPL(tll::channel::Control);

TLL_DEFINE_MODULE(tll::channel::Control);
