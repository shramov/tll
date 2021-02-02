/*
 * Copyright (c) 2018-2021 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#ifndef _TLL_CHANNEL_CURL_H
#define _TLL_CHANNEL_CURL_H

#include "tll/channel/base.h"
#include "tll/util/time.h"

#include <map>
#include <vector>

typedef struct Curl_easy CURL;
typedef struct Curl_URL CURLU;
//typedef struct Curl_multi CURLM;

class ChCURL;

struct curl_session_t
{
	ChCURL * parent = nullptr;

	CURL * curl = nullptr;
	CURLU * url = nullptr;

	tll_addr_t addr = {};

	std::map<std::string, std::string, std::less<>> headers;

	bool finished = false;

	std::optional<ssize_t> wsize;
	std::vector<char> rbuf;
	std::vector<char> wbuf;

	void reset();

	int init();

	size_t header(char * data, size_t size);
	size_t read(char * data, size_t size);
	size_t write(char * data, size_t size);

	size_t callback_data(const void * data, size_t size);

	void finalize(int code);
	void close();
};

class ChCURLMulti;
class ChCURL : public tll::channel::Base<ChCURL>
{
	friend struct curl_session_t;

	std::string _host;

	ChCURLMulti * _master = nullptr;

	std::unique_ptr<tll::Channel> _master_ptr;

	std::map<uint64_t, curl_session_t> _sessions;

	curl_session_t _base = { this };

	bool _recv_chunked = false;
	size_t _recv_size = 0;

	bool _autoclose = false;

 public:
	static constexpr std::string_view param_prefix() { return "curl"; }
	static constexpr bool impl_prefix_channel() { return true; }

	static constexpr auto process_policy() { return ProcessPolicy::Custom; }

	~ChCURL() { _base.reset(); }

	tll_channel_impl_t * _init_replace(const tll::Channel::Url &url);
	int _init(const tll::Channel::Url &, tll::Channel *master);
	int _open(const tll::PropsView &);
	int _close();

	int _process(long timeout, int flags);
	int _post(const tll_msg_t *msg, int flags);
};

#endif//_TLL_CHANNEL_CURL_H
