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
struct curl_slist;
//typedef struct Curl_multi CURLM;

class ChCURL;

struct curl_session_t
{
	ChCURL * parent = nullptr;

	CURL * curl = nullptr;
	CURLU * url = nullptr;

	tll_addr_t addr = {};

	std::map<std::string, std::string, std::less<>> headers;
	struct curl_slist * headers_list = nullptr;

	tll_state_t state = tll::state::Closed;

	std::optional<ssize_t> wsize;
	std::vector<char> wbuf;

	ssize_t rsize = -1;
	size_t roff = 0;
	std::vector<char> rbuf;

	~curl_session_t() { reset(); }
	void reset();

	int init();

	size_t header(char * data, size_t size);
	size_t read(char * data, size_t size);
	size_t write(char * data, size_t size);
	void connected();

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

	std::map<uint64_t, std::unique_ptr<curl_session_t>> _sessions;

	CURLU * _curl_url = nullptr;

	bool _recv_chunked = false;
	size_t _recv_size = 0;

	bool _autoclose = false;

	std::string_view _method;
	std::map<std::string, std::string, std::less<>> _headers;

	std::chrono::milliseconds _expect_timeout = std::chrono::milliseconds(1000);

	enum class Mode { Single, Data, Full } _mode = Mode::Single;

 public:
	static constexpr std::string_view param_prefix() { return "curl"; }
	static constexpr bool impl_prefix_channel() { return true; }

	static constexpr auto process_policy() { return ProcessPolicy::Custom; }

	~ChCURL() { _free(); }

	tll_channel_impl_t * _init_replace(const tll::Channel::Url &url);
	int _init(const tll::Channel::Url &, tll::Channel *master);
	int _open(const tll::PropsView &);
	int _close();
	void _free();

	int _process(long timeout, int flags);
	int _post(const tll_msg_t *msg, int flags);
};

#endif//_TLL_CHANNEL_CURL_H
