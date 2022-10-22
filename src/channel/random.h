/*
 * Copyright (c) 2022 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#ifndef _CHANNEL_RANDOM_H
#define _CHANNEL_RANDOM_H

#include <tll/channel/prefix.h>
#include <tll/util/size.h>

#include <random>

namespace tll::channel {

class Random : public tll::channel::Prefix<Random>
{
	using Base = tll::channel::Prefix<Random>;

	using bits_engine = std::independent_bits_engine<std::default_random_engine, 64, uint64_t>;

	std::default_random_engine _rand_engine = std::default_random_engine { std::random_device()() }; // Seed
	bits_engine _rand_bits = bits_engine { std::random_device()() };
	std::uniform_int_distribution<unsigned> _rand_dist;

	tll_msg_t _msg = {};
	enum class DataMode { Seq, Random, Pattern } _data_mode;
	std::vector<uint64_t> _buf;

 public:
	static constexpr std::string_view channel_protocol() { return "random+"; }

	int _init(const tll::Channel::Url &url, tll::Channel *master)
	{
		auto reader = channel_props_reader(url);
		auto min = reader.getT("min", tll::util::SizeT<unsigned> { 100 });
		auto max = reader.getT("max", tll::util::SizeT<unsigned> { 500 });
		_data_mode = reader.getT("data-mode", DataMode::Seq, {{"seq", DataMode::Seq}, {"random", DataMode::Random}, {"pattern", DataMode::Pattern}});
		auto pattern = reader.getT<uint64_t>("pattern", 0);
		if (!reader)
			return _log.fail(EINVAL, "Invalid url: {}", reader.error());

		if (min >= max)
			return _log.fail(EINVAL, "Invalid min/max values: {}/{}", (unsigned) min, (unsigned) max);

		_rand_dist = std::uniform_int_distribution<unsigned> { min, max };
		_buf.resize(max / sizeof(uint64_t) + 1);

		if (_data_mode == DataMode::Seq) {
			auto ptr = (unsigned char *) _buf.data();
			auto end = ptr + max;
			unsigned char i = 0;
			for (; ptr < end; ptr++)
				*ptr = i++;
		} else if (_data_mode == DataMode::Pattern) {
			for (auto & i : _buf)
				i = pattern;
		}
		_msg.data = _buf.data();

		return Base::_init(url, master);
	}

	int _open(const tll::ConstConfig &params)
	{
		_msg.seq = -1;
		return Base::_open(params);
	}

	int _on_data(const tll_msg_t *msg)
	{
		_msg.size = _rand_dist(_rand_engine);
		if (_data_mode == DataMode::Random) {
			for (auto i = 0u; i < _msg.size / 8; i++)
				_buf[i] = _rand_bits();
		}
		_msg.seq++;
		return _callback_data(&_msg);
	}
};

} // namespace tll::channel

#endif//_CHANNEL_RANDOM_H
