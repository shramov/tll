#include "channel/direct.h"
#include "tll/scheme/channel/direct.h"

#include "tll/scheme/merge.h"

using namespace tll;

TLL_DEFINE_IMPL(ChDirect);

int ChDirect::_init(const tll::Channel::Url &url, tll::Channel * master)
{
	auto reader = channel_props_reader(url);
	auto control = reader.get("scheme-control");
	if (!master)
		_notify_state = reader.getT("notify-state", false);
	if (!reader)
		return _log.fail(EINVAL, "Invalid url: {}", reader.error());

	if (control) {
		_log.debug("Loading control scheme from {}...", control->substr(0, 64));

		_scheme_control.reset(context().scheme_load(*control));
		if (!_scheme_control)
			return _log.fail(EINVAL, "Failed to load control scheme from {}...", control->substr(0, 64));

		if (_notify_state) {
			std::unique_ptr<const tll::Scheme> external, internal;
			external = std::move(_scheme_control);
			internal.reset(context().scheme_load(direct_scheme::scheme_string));
			if (!internal)
				return _log.fail(EINVAL, "Failed to load direct:// control scheme");

			auto control = tll::scheme::merge({external.get(), internal.get()});
			if (!control)
				return _log.fail(EINVAL, "Failed to merge control scheme: {}", control.error());
			_scheme_control.reset(*control);
		}
	} else if (_notify_state) {
		_scheme_control.reset(context().scheme_load(direct_scheme::scheme_string));
		if (!_scheme_control)
			return _log.fail(EINVAL, "Failed to load direct:// control scheme");
	}

	if (!master)
		return 0;

	_sub = true;
	_sibling = tll::channel_cast<ChDirect>(master);
	if (!_sibling)
		return _log.fail(EINVAL, "Parent {} must be direct:// channel", master->name());
	if (_sibling->_sub)
		return _log.fail(EINVAL, "Master {} has it's own master, can not bind", _sibling->name);
	_log.debug("Init child of master {}", _sibling->name);
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
