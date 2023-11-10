// SPDX-License-Identifier: MIT

#ifndef _TLL_CHANNEL_FILE_INIT_H
#define _TLL_CHANNEL_FILE_INIT_H

namespace tll::channel {

class FileInit : public tll::channel::Base<FileInit>
{
 public:
	static constexpr std::string_view channel_protocol() { return "file"; }

	std::optional<const tll_channel_impl_t *> _init_replace(const tll::Channel::Url &url, tll::Channel *master);

	int _init(const tll::Channel::Url &url, tll::Channel * master) { return _log.fail(EINVAL, "Failed to choose proper file channel"); }
};

} // namespace tll::channel

#endif//_TLL_CHANNEL_FILE_INIT_H
