/*
 * Copyright (c) 2018-2020 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#ifndef _TLL_CHANNEL_NULL_H
#define _TLL_CHANNEL_NULL_H

#include "tll/channel/base.h"
#include "tll/util/size.h"

class ChNull : public tll::channel::Base<ChNull>
{
 public:
	static constexpr std::string_view param_prefix() { return "null"; }
	static constexpr auto process_policy() { return ProcessPolicy::Never; }

	int _init(const tll::UrlView &, tll::Channel *master) { return 0; }

	int _process(long timeout, int flags) { return 0; }
	int _post(const tll_msg_t *msg, int flags) { return 0; }
};

class ChZero : public tll::channel::Base<ChZero>
{
	size_t _size = 1024;
	std::vector<char> _buf;
	tll_msg_t _msg = { TLL_MESSAGE_DATA };
 public:
	static constexpr std::string_view param_prefix() { return "zero"; }

	int _init(const tll::UrlView &url, tll::Channel *master)
	{
		auto reader = channel_props_reader(url);
		_size = reader.getT<tll::util::Size>("size", 1024);
		if (!reader)
			return _log.fail(EINVAL, "Invalid url: {}", reader.error());

		_buf.resize(_size);
		_msg.data = _buf.data();
		_msg.size = _buf.size();
		memset(_buf.data(), 'z', _buf.size());
		_dcaps_pending(true);
		return 0;
	}

	int _process(long timeout, int flags)
	{
		_callback(&_msg);
		return 0;
	}
};

#endif//_TLL_CHANNEL_NULL_H
