// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Pavel Shramov <shramov@mexmat.net>

#ifndef _TLL_CHANNEL_ASYNC_H
#define _TLL_CHANNEL_ASYNC_H

#include "tll/channel/event.h"
#include "tll/channel/prefix.h"
#include "tll/util/markerqueue.h"
#include "tll/util/ownedmsg.h"

#include <mutex>

namespace tll::channel {
class Async : public Event<Async, Prefix<Async>>
{
	using Base = Event<Async, Prefix<Async>>;
	using OwnedMessage = util::OwnedMessage;

	std::mutex _lock;
	MarkerQueue<OwnedMessage *, nullptr, std::default_delete<OwnedMessage>> _queue;

 public:
	static constexpr std::string_view channel_protocol() { return "async+"; }
	static constexpr auto process_api_version() { return ProcessAPI::Void; }

	int _init(const tll::Channel::Url &cfg, tll::Channel *master);
	int _open(const tll::ConstConfig &cfg)
	{
		_queue.clear();
		_update_dcaps(dcaps::Process);
		return Base::_open(cfg);
	}

	int _process();

	int _post(const tll_msg_t *msg, int flags);
};

} // namespace tll::channel

#endif//_TLL_CHANNEL_ASYNC_H
