#include "channel/resolve.h"
#include "tll/scheme/logic/resolve.h"

using namespace tll::channel;

TLL_DEFINE_IMPL(Resolve);

int Resolve::_init(const Channel::Url &url, tll::Channel *master)
{
	auto reader = channel_props_reader(url);
	auto service = reader.getT<std::string>("resolve.service");
	auto channel = reader.getT<std::string>("resolve.channel");
	if (!reader)
		return _log.fail(EINVAL, "Invalid url: {}", reader.error());

	tll::Channel::Url curl;
	if (_config_defaults.sub("resolve.request")) {
		auto r = _config_defaults.getT<tll::Channel::Url>("resolve.request");
		if (!r)
			return _log.fail(EINVAL, "Failed to get request url: {}", r.error());
		curl = *r;
	} else {
		auto r = child_url_parse("ipc://;mode=client;master=_tll_resolve_master", "resolve");
		if (!r)
			return _log.fail(EINVAL, "Failed to parse request url: {}", r.error());
		curl = *r;
	}
	child_url_fill(curl, "request");

	_request = context().channel(curl, master);
	if (!_request)
		return _log.fail(EINVAL, "Failed to create request channel");
	_request->callback_add(_on_request, this, TLL_MESSAGE_MASK_DATA | TLL_MESSAGE_MASK_STATE);
	_child_add(_request.get(), "request");

	_log.info("Resolve service: {}, channel: {}", service, channel);
	auto req = resolve_scheme::Request::bind_reset(_request_buf);
	req.set_service(service);
	req.set_channel(channel);

	return tll::channel::Base<Resolve>::_init(url, master);
}

int Resolve::_on_request_active()
{
	tll_msg_t msg = {};
	msg.msgid = resolve_scheme::Request::meta_id();
	msg.data = _request_buf.data();
	msg.size = _request_buf.size();
	_log.debug("Sending resolve request");
	if (auto r = _request->post(&msg); r)
		return state_fail(EINVAL, "Failed to post request: {}", strerror(r));
	return 0;
}

int Resolve::_on_request_data(const tll_msg_t *msg)
{
	if (_child)
		return 0;
	if (state() != tll::state::Opening)
		return 0;
	if (msg->msgid != resolve_scheme::ExportChannel::meta_id())
		return state_fail(0, "Invalid message id: {}", msg->msgid);
	auto data = resolve_scheme::ExportChannel::bind(*msg);
	if (msg->size < data.meta_size())
		return _log.fail(EMSGSIZE, "Message size too small: {} < min {}", msg->size, data.meta_size());
	tll::Config cfg;
	for (auto m : data.get_config())
		cfg.set(m.get_key(), m.get_value());
	tll::Channel::Url url;
	if (auto sub = cfg.sub("init"); sub)
		url = *sub;
	else
		return _log.fail(EINVAL, "No 'init' subtree in resolved config");
	for (auto &[k, v] : cfg.browse("scheme.**")) {
		auto body = v.get();
		if (!body) continue;
		auto hash = k.substr(strlen("scheme."));
		_log.debug("Preload scheme {}", hash);
		if (auto r = context().scheme_load(*body); r)
			tll_scheme_unref(r);
		else
			return state_fail(0, "Failed to load scheme with hash {}", hash);
	}
	child_url_fill(url, "resolve");
	_child = context().channel(url);
	if (!_child)
		return state_fail(0, "Failed to create resolved channel");
	_child_add(_child.get(), "resolve");
	_child->callback_add(this);
	_request->close();
	_child->open(_open_cfg);
	return 0;
}
