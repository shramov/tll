/*
 * Copyright (c) 2018-2020 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#ifndef _TLL_CHANNEL_SERIAL_H
#define _TLL_CHANNEL_SERIAL_H

#include "tll/channel/base.h"

class ChSerial : public tll::channel::Base<ChSerial>
{
	enum class Parity { None, Even, Odd };

	unsigned _speed = 0u;
	unsigned _stop_bits = 1u;
	Parity _parity = Parity::None;

	unsigned _data_bits = 8u;
	bool _flow_control = false;

	std::string _filename;
	std::vector<unsigned char> _buf;

 public:
	static constexpr std::string_view param_prefix() { return "serial"; }

	int _init(const tll::UrlView &, tll::Channel *master);
	int _open(const tll::PropsView &);
	int _close();

	int _process(long timeout, int flags);
	int _post(const tll_msg_t *msg, int flags);
};

#endif//_TLL_CHANNEL_SERIAL_H
