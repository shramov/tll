/*
 * Copyright (c) 2021 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#ifndef _TLL_CHANNEL_YAML_H
#define _TLL_CHANNEL_YAML_H

#include "tll/channel/base.h"
#include "tll/scheme/encoder.h"

class ChYaml : public tll::channel::Base<ChYaml>
{
	std::string _filename;
	tll::ConstConfig _config;
	std::optional<tll::ConstConfig> _url_config;
	std::vector<tll::ConstConfig> _messages;
	size_t _idx = 0;
	long long _seq = -1;
	std::vector<char> _buf;

	bool _autoclose = false;
	bool _autoseq = false;
	tll::scheme::ConfigEncoder _encoder;

 public:
	static constexpr std::string_view channel_protocol() { return "yaml"; }

	int _init(const tll::Channel::Url &url, tll::Channel *master);
	int _open(const tll::ConstConfig &url);

	int _process(long timeout, int flags);

	int _fill(const tll::Scheme * scheme, tll_msg_t * msg, tll::ConstConfig &cfg);
};

#endif//_TLL_CHANNEL_YAML_H
