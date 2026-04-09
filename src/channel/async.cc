#include "channel/async.h"

#include "tll/channel/event.hpp"
#include "tll/util/size.h"

using Async = tll::channel::Async;

TLL_DEFINE_IMPL(Async);

int Async::_init(const tll::Channel::Url &cfg, tll::Channel *master)
{
	if (auto r = Base::_init(cfg, master); r)
		return r;

	auto reader = channel_props_reader(cfg);
	auto size = reader.getT("size", tll::util::Size { 16 * 1024 } );
	if (!reader)
		return _log.fail(EINVAL, "Invalid init parameters: {}", reader.error());

	_queue.resize(size);

	return 0;
}

int Async::_post(const tll_msg_t *msg, int flags)
{
	std::unique_ptr<OwnedMessage> ptr { new OwnedMessage { msg } };
	if (event_notify())
		return _log.fail(EINVAL, "Failed to arm event");
	if (auto r = _queue.push(ptr.get()); r) {
		event_clear_race([this]() -> bool { return !_queue.empty(); });
		return r;
	}
	ptr.release();
	return 0;
}

int Async::_process()
{
	auto ptr = _queue.peek();
	if (!ptr)
		return EAGAIN;
	if (auto r = _child->post(ptr); r == EAGAIN) {
		// TODO: Switch to periodic processing or wait for WriteReady?
		return EAGAIN;
	} else if (r)
		return _log.fail(r, "Failed to post into the child: {}", strerror(r));
	delete _queue.pop();
	return event_clear_race([this]() -> bool { return !_queue.empty(); });
}
