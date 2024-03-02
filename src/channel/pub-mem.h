// SPDX-License-Identifier: MIT

#ifndef _TLL_IMPL_CHANNEL_PUB_MEM_H
#define _TLL_IMPL_CHANNEL_PUB_MEM_H

#include "tll/channel/base.h"

class ChPubMem : public tll::channel::Base<ChPubMem>
{
 public:
	static constexpr std::string_view channel_protocol() { return "pub+mem"; }

	std::optional<const tll_channel_impl_t *> _init_replace(const tll::Channel::Url &url, tll::Channel *master);

	int _init(const tll::Channel::Url &url, tll::Channel * master) { return this->_log.fail(EINVAL, "Failed to choose proper pub+mem channel"); }
};

#endif//_TLL_IMPL_CHANNEL_PUB_MEM_H
