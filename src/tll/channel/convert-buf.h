// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Pavel Shramov <shramov@mexmat.net>

#ifndef _TLL_CHANNEL_CONVERT_H
#define _TLL_CHANNEL_CONVERT_H

#include <tll/channel.h>
#include <tll/scheme/convert.h>

#include <vector>

namespace tll::channel {


struct ConvertBuf : public tll::scheme::Convert
{
	std::vector<char> buffer;
	tll_msg_t msg = {};

	std::optional<const tll_msg_t *> convert(const tll_msg_t * m)
	{
		auto it = map_from.find(m->msgid);
		if (it == map_from.end())
			return fail(std::nullopt, "Message {} not found", m->msgid);
		auto message = it->second;
		if (!message->user)
			return nullptr; // Skip valid message
		buffer.resize(0);
		if (auto r = tll::scheme::Convert::convert(tll::make_view(buffer), message, tll::make_view(*m)); r)
			return std::nullopt;
		msg = *m;
		msg.data = buffer.data();
		msg.size = buffer.size();
		return &msg;
	}
};

} // namespace tll::channel


#endif//_TLL_CHANNEL_CONVERT_H
