#include "channel/direct.h"
#include "channel/emulate-control.hpp"

#include "tll/scheme/channel/direct.h"

#include "tll/scheme/merge.h"

using namespace tll;

TLL_DEFINE_IMPL(ChDirect);

int ChDirect::_init(const tll::Channel::Url &url, tll::Channel * master)
{
	if (auto r = Base::_init(url, master); r)
		return _log.fail(r, "Base init failed");
	auto reader = channel_props_reader(url);
	if (auto r = _init_emulate_control(reader); r)
		return r;

	if (!master)
		_notify_state = reader.getT("notify-state", false);
	else
		_manual_open = reader.getT("manual-open", false);

	if (!reader)
		return _log.fail(EINVAL, "Invalid url: {}", reader.error());

	if (_notify_state && _merge_control(direct_scheme::scheme_string, "state update scheme"))
		return EINVAL;

	if (!master) {
		_mode = Master;
		_ptr.reset(new Pointers);
		return 0;
	} else
		_mode = Slave;

	auto ptr = tll::channel_cast<ChDirect>(master);
	if (!ptr)
		return _log.fail(EINVAL, "Parent {} must be direct:// channel", master->name());
	if (ptr->_mode != Master)
		return _log.fail(EINVAL, "Master {} has it's own master, can not bind", ptr->name);
	if (!_scheme_url)
		_scheme_url = ptr->_scheme_url;
	if (!_scheme_control && ptr->_scheme_control) {
		_log.info("Inherit control scheme from master");
		_scheme_control.reset(ptr->_scheme_control->ref());
	}
	_ptr = ptr->_ptr;
	_log.debug("Init child of master {}", ptr->name);
	if (_ptr.use_count() == 3)
		return _log.fail(EINVAL, "Direct master {} already has slave", master->name());
	return 0;
}

void ChDirect::_update_state(tll_state_t s)
{
	state(s);
	if (_mode == Master)
		return;

	auto ptr = *_ptr->get(_invert(_mode));
	if (!ptr)
		return _log.error("Master channel is detached (closed or destroyed)");
	else if (ptr->state() != tll::state::Active)
		return _log.warning("Master channel {} is not active", ptr->name);
	if (!ptr->_notify_state)
		return;

	std::array<char, direct_scheme::DirectStateUpdate::meta_size()> buf;
	auto data = direct_scheme::DirectStateUpdate::bind(buf);
	data.set_state((direct_scheme::DirectStateUpdate::State) s);
	tll_msg_t msg = { TLL_MESSAGE_CONTROL };
	msg.msgid = data.meta_id();
	msg.data = data.view().data();
	msg.size = data.view().size();
	ptr->_callback(msg);
}
