#include "channel/zero.h"
#include "tll/channel/event.hpp"

#include "tll/util/size.h"

using namespace tll;

TLL_DEFINE_IMPL(ChZero);

int ChZero::_init(const tll::Channel::Url &url, tll::Channel *master)
{
	auto reader = channel_props_reader(url);
	_size = reader.getT<tll::util::Size>("size", 1024);
	_with_pending = reader.getT("pending", true);
	if (!reader)
		return _log.fail(EINVAL, "Invalid url: {}", reader.error());

	_buf.resize(_size);
	_msg.data = _buf.data();
	_msg.size = _buf.size();
	memset(_buf.data(), 'z', _buf.size());
	return Event<ChZero>::_init(url, master);
}

int ChZero::_open(const PropsView &url)
{
	int r = Event<ChZero>::_open(url);
	if (r)
		return r;
	event_notify();
	if (_with_pending)
		_dcaps_pending(true);
	return 0;
}
