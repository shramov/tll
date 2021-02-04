/*
 * Copyright (c) 2021 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#ifndef _TLL_UTIL_CURLPP_H
#define _TLL_UTIL_CURLPP_H

#include <memory>

#define CURL_NO_OLDIES

#include <curl/curl.h>

namespace tll::curl {

using CURL_ptr = std::unique_ptr<CURL, decltype(&curl_easy_cleanup)>;
using CURLM_ptr = std::unique_ptr<CURLM, decltype(&curl_multi_cleanup)>;
using CURLU_ptr = std::unique_ptr<CURLU, decltype(&curl_url_cleanup)>;

namespace {
template <CURLoption option> struct _curlopt {};

template <> struct _curlopt<CURLOPT_URL> { using type = const char *; };

template <> struct _curlopt<CURLOPT_CURLU> { using type = CURLU *; };
template <> struct _curlopt<CURLOPT_HTTPHEADER> { using type = struct curl_slist *; };

template <> struct _curlopt<CURLOPT_EXPECT_100_TIMEOUT_MS> { using type = long; };
template <> struct _curlopt<CURLOPT_FOLLOWLOCATION> { using type = long; };
template <> struct _curlopt<CURLOPT_MAXREDIRS> { using type = long; };
template <> struct _curlopt<CURLOPT_UPLOAD> { using type = long; };

template <> struct _curlopt<CURLOPT_INFILESIZE_LARGE> { using type = curl_off_t; };

template <> struct _curlopt<CURLOPT_PRIVATE> { using type = void *; };

template <> struct _curlopt<CURLOPT_HEADERDATA> { using type = void *; };
template <> struct _curlopt<CURLOPT_READDATA> { using type = void *; };
template <> struct _curlopt<CURLOPT_WRITEDATA> { using type = void *; };

template <> struct _curlopt<CURLOPT_HEADERFUNCTION> { using type = curl_read_callback; };
template <> struct _curlopt<CURLOPT_READFUNCTION> { using type = curl_read_callback; };
template <> struct _curlopt<CURLOPT_WRITEFUNCTION> { using type = curl_write_callback; };

template <CURLMoption option> struct _curlmopt {};

template <> struct _curlmopt<CURLMOPT_SOCKETDATA> { using type = void *; };
template <> struct _curlmopt<CURLMOPT_TIMERDATA> { using type = void *; };

template <> struct _curlmopt<CURLMOPT_SOCKETFUNCTION> { using type = curl_socket_callback; };
template <> struct _curlmopt<CURLMOPT_TIMERFUNCTION> { using type = curl_multi_timer_callback; };

template <CURLINFO option> struct _curlinfo {};

template <> struct _curlinfo<CURLINFO_PRIVATE> { using type = void *; };
template <> struct _curlinfo<CURLINFO_RESPONSE_CODE> { using type = long; };
template <> struct _curlinfo<CURLINFO_CONTENT_LENGTH_DOWNLOAD_T> { using type = curl_off_t; };
template <> struct _curlinfo<CURLINFO_EFFECTIVE_URL> { using type = const char *; };
}

template <CURLINFO info>
std::optional<typename _curlinfo<info>::type> getinfo(CURL * curl)
{
	typename _curlinfo<info>::type v;
	auto r = curl_easy_getinfo(curl, info, &v);
	if (r)
		return std::nullopt;
	return v;
}

template <CURLoption option>
CURLcode setopt(CURL * curl, typename _curlopt<option>::type v)
{
	return curl_easy_setopt(curl, option, v);
}

template <CURLMoption option>
CURLMcode setopt(CURLM * multi, typename _curlmopt<option>::type v)
{
	return curl_multi_setopt(multi, option, v);
}

} // namespace tll::curl

#endif//_TLL_UTIL_CURLPP_H
