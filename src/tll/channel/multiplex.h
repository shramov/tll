// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Pavel Shramov <shramov@mexmat.net>

#ifndef _TLL_CHANNEL_MULTIPLEX_H
#define _TLL_CHANNEL_MULTIPLEX_H

#include <tll/channel/base.h>

namespace tll::channel {

struct MultiplexData {
	struct SchemeAndUrl
	{
		std::string url;
		tll::scheme::ConstSchemePtr ptr;
	};

	std::map<unsigned short, SchemeAndUrl> scheme;
	tll_msg_t msg_encode = {};
	tll_msg_t msg_decode = {};

	static constexpr unsigned short type_min = TLL_MESSAGE_CHANNEL + 1;

	static constexpr int encode_msgid(unsigned short type, int msgid)
	{
		if (type >= type_min)
			return ((0x8000u | (type - type_min)) << 16) | (msgid & 0xffff);
		else
			return msgid;
	}

	static constexpr int encode_msgid(const tll_msg_t * msg)
	{
		return encode_msgid(msg->type, msg->msgid);
	}

	static constexpr std::pair<unsigned short, int> decode_msgid(unsigned msgid)
	{
		if (!(msgid & 0x80000000))
			return std::make_pair(0, msgid);
		return std::make_pair((0x8000 ^ (msgid >> 16)) + type_min, msgid & 0xffff);
	}

	const tll_msg_t * encode(const tll_msg_t * msg)
	{
		if (msg->type <= TLL_MESSAGE_CHANNEL) // Well known types
			return msg;
		msg_encode = *msg;
		msg_encode.type = TLL_MESSAGE_DATA;
		msg_encode.msgid = encode_msgid(msg->type, msg->msgid);
		return &msg_encode;
	}

	const tll_msg_t * decode(const tll_msg_t * msg)
	{
		if (msg->type != TLL_MESSAGE_DATA)
			return msg;
		msg_decode = *msg;
		auto r = decode_msgid(msg->msgid);
		msg_decode.type = r.first;
		msg_decode.msgid = r.second;
		return &msg_decode;
	}
};

/// Channel mixin that encodes and decodes multiple data streams into single msgid space
template <typename T, typename S = Base<T>>
class Multiplex : public S
{
 protected:
	using Base = S;

	MultiplexData _multiplex;

 public:
	const Scheme * scheme(int type) const
	{
		if (type <= TLL_MESSAGE_CHANNEL)
			return Base::scheme(type);
		if (auto it = _multiplex.scheme.find(type); it != _multiplex.scheme.end())
			return it->second.ptr.get();
		return Base::scheme(type);
	}

	int _init(const tll::Channel::Url &url, tll::Channel *master)
	{
		for (auto &[path, cfg]: url.browse("multiplex.*")) {
			auto name = path.substr(strlen("multiplex."));
			auto id = tll::conv::to_any<unsigned short>(name);
			if (!id)
				return this->_log.fail(EINVAL, "Invalid multiplex index '{}': {}", name, id.error());
			auto v = cfg.get();
			if (v)
				_multiplex.scheme.emplace(*id, MultiplexData::SchemeAndUrl{std::string(*v)});
		}
		return Base::_init(url, master);
	}

	int _open(const tll::ConstConfig &cfg)
	{
		for (auto &[k, s]: _multiplex.scheme) {
			this->_log.debug("Loading scheme from {}...", s.url.substr(0, 64));
			scheme::ConstSchemePtr r { this->context().scheme_load(s.url, this->_scheme_cache) };
			if (!r)
				return this->_log.fail(EINVAL, "Failed to load multiplexed scheme '{}' from {}...", k, s.url.substr(0, 64));
			s.ptr = std::move(r);
		}
		return Base::_open(cfg);
	}

	int _post(const tll_msg_t * msg, int flags)
	{
		return Base::_post(_multiplex.encode(msg), flags);
	}
};

} // namespace tll::channel

#endif//_TLL_CHANNEL_MULTIPLEX_H
