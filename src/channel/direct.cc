#include "channel/direct.h"

#include "tll/scheme/channel/direct.h"
#include "channel/stream-control-server.h"
#include "channel/stream-control.h"
#include "tll/channel/tcp-client-scheme.h"
#include "tll/channel/tcp-scheme.h"

#include "tll/scheme/merge.h"

using namespace tll;

TLL_DEFINE_IMPL(ChDirect);

namespace {
std::optional<std::string_view> lookup_scheme(std::string_view name)
{
	if (name == "stream-server")
		return stream_server_control_scheme::scheme_string;
	else if (name == "stream-client")
		return stream_control_scheme::scheme_string;
	else if (name == "tcp-server")
		return tcp_scheme::scheme_string;
	else if (name == "tcp-client")
		return tcp_client_scheme::scheme_string;
	return std::nullopt;
}
}

int ChDirect::_init(const tll::Channel::Url &url, tll::Channel * master)
{
	if (auto r = Base::_init(url, master); r)
		return _log.fail(r, "Base init failed");
	auto reader = channel_props_reader(url);
	auto control = reader.get("scheme-control");
	auto emulate = reader.getT("emulate-control", std::vector<std::string>{});
	if (!master)
		_notify_state = reader.getT("notify-state", false);
	if (!reader)
		return _log.fail(EINVAL, "Invalid url: {}", reader.error());

	if (control && _merge_control(*control, *control))
		return EINVAL;

	if (_notify_state && _merge_control(direct_scheme::scheme_string, "state update scheme"))
		return EINVAL;
	for (auto name : emulate) {
		auto string = lookup_scheme(name);
		if (!string)
			return _log.fail(EINVAL, "Unknown control emulation tag: {}", name);
		_log.info("Add control scheme for {}", name);
		if (_merge_control(*string, name))
			return EINVAL;
	}

	if (!master)
		return 0;

	_sub = true;
	_sibling = tll::channel_cast<ChDirect>(master);
	if (!_sibling)
		return _log.fail(EINVAL, "Parent {} must be direct:// channel", master->name());
	if (_sibling->_sub)
		return _log.fail(EINVAL, "Master {} has it's own master, can not bind", _sibling->name);
	if (!_scheme_url)
		_scheme_url = _sibling->_scheme_url;
	_log.debug("Init child of master {}", _sibling->name);
	return 0;
}

int ChDirect::_merge_control(std::string_view scheme_string, std::string_view name)
{
	_log.trace("Loading control scheme {}...", name.substr(0, 64));

	std::unique_ptr<const tll::Scheme> ptr { context().scheme_load(scheme_string) };
	if (!ptr)
		return _log.fail(EINVAL, "Failed to load control scheme from {}...", name.substr(0, 64));

	if (_scheme_control) {
		auto control = tll::scheme::merge({_scheme_control.get(), ptr.get()});
		if (!control)
			return _log.fail(EINVAL, "Failed to merge control scheme: {}", control.error());
		_scheme_control.reset(*control);
	} else
		_scheme_control = std::move(ptr);

	return 0;
}

void ChDirect::_update_state(tll_state_t s)
{
	state(s);
	if (!_sibling)
		return _log.error("Master channel destroyed");
	else if (_sibling->state() != tll::state::Active)
		return _log.warning("Master channel {} is not active", _sibling->name);
	if (!_sibling->_notify_state)
		return;

	std::array<char, direct_scheme::DirectStateUpdate::meta_size()> buf;
	auto data = direct_scheme::DirectStateUpdate::bind(buf);
	data.set_state((direct_scheme::DirectStateUpdate::State) s);
	tll_msg_t msg = { TLL_MESSAGE_CONTROL };
	msg.msgid = data.meta_id();
	msg.data = data.view().data();
	msg.size = data.view().size();
	_sibling->_callback(msg);
}
