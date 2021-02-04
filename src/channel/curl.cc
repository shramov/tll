/*
 * Copyright (c) 2018-2021 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#include "channel/curl.h"

#include "tll/util/curl++.h"
#include "tll/util/memoryview.h"
#include "tll/util/size.h"

#include <unistd.h>

#include "channel/timer-scheme.h"
#include "channel/curl-scheme.h"

using namespace tll;

class ChCURLSocket;

class ChCURLMulti : public tll::channel::Base<ChCURLMulti>
{
	CURLM * _multi = nullptr;

	std::unique_ptr<tll::Channel> _timer;
	std::list<std::unique_ptr<tll::Channel>> _sockets;

 public:
	static constexpr std::string_view param_prefix() { return "curl"; }

	static constexpr auto process_policy() { return ProcessPolicy::Never; }

	int _init(const tll::Channel::Url &, tll::Channel *master);
	int _open(const tll::PropsView &);
	int _close();
	void _free();

	int _process(long timeout, int flags);

	CURLM * multi() { return _multi; }

 private:

	int _timer_cb();
	int _curl_timer_cb(CURLM *multi, std::chrono::milliseconds timeout);
	int _curl_socket_cb(CURL *e, curl_socket_t s, int what, ChCURLSocket *sockp);
};

class ChCURLSocket : public tll::channel::Base<ChCURLSocket>
{
 	ChCURLMulti * _master = nullptr;

 public:
	static constexpr std::string_view param_prefix() { return "curl"; }
	static constexpr std::string_view impl_protocol() { return "curl-socket"; } // Only visible in logs

	int _init(const tll::Channel::Url &url, tll::Channel *master)
	{
		if (!master)
			return _log.fail(EINVAL, "Socket needs master channel");
		_master = tll::channel_cast<ChCURLMulti>(master);
		if (!_master)
			return _log.fail(EINVAL, "Socket needs CURLMulti master channel, got {}", master->name());
		return 0;
	}

	int bind(int fd) { return this->_update_fd(fd); }

	void update_poll(unsigned caps) { _dcaps_poll(caps); }

	int _process(long timeout, int flags)
	{
		_log.debug("Run curl socket action");
		int running = 0;
		int events = 0;
		if (auto r = curl_multi_socket_action(_master->multi(), fd(), events, &running); r)
			_log.warning("curl_multi_socket_action({}) failed: {}", fd(), curl_multi_strerror(r));
		return _master->_process(timeout, flags);
	}
};

TLL_DEFINE_IMPL(ChCURL);
TLL_DEFINE_IMPL(ChCURLMulti);
TLL_DEFINE_IMPL(ChCURLSocket);

tll_channel_impl_t * ChCURL::_init_replace(const Channel::Url &url)
{
	auto proto = url.proto();
	auto sep = proto.find("+");
	if (sep == proto.npos)
		return &ChCURLMulti::impl;

	return nullptr;
}

int ChCURLMulti::_init(const tll::Channel::Url &url, tll::Channel *master)
{
	_scheme_control.reset(context().scheme_load(curl_scheme::scheme));
	if (!_scheme_control.get())
		return _log.fail(EINVAL, "Failed to load control scheme");

	auto reader = channel_props_reader(url);
	if (!reader)
		return _log.fail(EINVAL, "Invalid url: {}", reader.error());

	{
		tll::Channel::Url turl;
		turl.proto("timer");
		turl.set("name", fmt::format("{}/timer", this->name));
		turl.set("tll.internal", "yes");
		turl.set("timer.clock", "monotonic");
		_timer = context().channel(turl);

		if (!_timer)
			return _log.fail(EINVAL, "Failed to create timer channel");

		_timer->callback_add([](const tll_channel_t *, const tll_msg_t *, void *user) {
			return static_cast<ChCURLMulti *>(user)->_timer_cb();
		}, this, TLL_MESSAGE_MASK_DATA);

		_child_add(_timer.get(), "timer");
	}

	if (auto r = curl_global_init(CURL_GLOBAL_DEFAULT); r)
		return _log.fail(EINVAL, "curl_global_init failed: {}", curl_easy_strerror(r));
	
	return 0;
}

int ChCURLMulti::_open(const PropsView &url)
{
	if (_timer->open())
		return _log.fail(EINVAL, "Failed to open timer");

	_multi = curl_multi_init();
	if (!_multi)
		return _log.fail(EINVAL, "Failed to init curl multi handle");

	// CURLMOPT_PIPELINING is set by default on recent versions of libcurl

	tll::curl::setopt<CURLMOPT_SOCKETDATA>(_multi, this);
	tll::curl::setopt<CURLMOPT_SOCKETFUNCTION>(_multi, [](CURL *e, curl_socket_t s, int what, void *user, void *sockp) {
		return static_cast<ChCURLMulti *>(user)->_curl_socket_cb(e, s, what, static_cast<ChCURLSocket *>(sockp));
	});

	/*
	tll::curl::setopt<CURLMOPT_TIMERDATA>(_multi, this);
	tll::curl::setopt<CURLMOPT_TIMERFUNCTION>(_multi, [](CURLM * multi, long ms, void *user) {
		return static_cast<ChCURLMulti *>(user)->_curl_timer_cb(multi, std::chrono::milliseconds(ms));
	});
	*/

	// XXX: This code is left as demo of lambda as function argument. Note + before lambda!
	curl_multi_setopt(_multi, CURLMOPT_TIMERDATA, this);
	curl_multi_setopt(_multi, CURLMOPT_TIMERFUNCTION, +[](CURLM * multi, long ms, void *user) {
			return static_cast<ChCURLMulti *>(user)->_curl_timer_cb(multi, std::chrono::milliseconds(ms));
	});

	return 0;
}

int ChCURLMulti::_close()
{
	// TODO: Can not be called from curl callback
	_sockets.clear();

	if (_multi) {
		curl_multi_cleanup(_multi);
		_multi = nullptr;
	}

	_timer->close();

	return 0;
}

void ChCURLMulti::_free()
{
	if (_timer) {
		_child_del(_timer.get(), "timer");
		_timer.reset();
	}

	_log.debug("Run curl global cleanup");
	curl_global_cleanup();
}

int ChCURLMulti::_process(long timeout, int flags)
{
	int remaining = 0;
	_log.debug("Check for curl info messages");
	do {
		auto msg = curl_multi_info_read(_multi, &remaining);
		if (!msg) break;
		_log.debug("Got curl info message {}", msg->msg);
		if (msg->msg == CURLMSG_DONE) {
			auto curl = msg->easy_handle;
			auto session = static_cast<curl_session_t *>(*tll::curl::getinfo<CURLINFO_PRIVATE>(curl));
			auto code = tll::curl::getinfo<CURLINFO_RESPONSE_CODE>(curl);

			if (msg->data.result) {
				_log.warning("Transfer for {} failed: {}", session->parent->name, curl_easy_strerror(msg->data.result));
			} else {
				_log.info("Transfer for {} finished: {}", session->parent->name, code?*code:0);
			}
			session->finalize(msg->data.result);
		}
	} while (true);
	return EAGAIN;
}

int ChCURLMulti::_timer_cb()
{
	int running = 0;
	if (auto r = curl_multi_socket_action(_multi, CURL_SOCKET_TIMEOUT, 0, &running); r)
		_log.warning("curl_multi_socket_action(timer) failed: {}", curl_multi_strerror(r));
	return 0;
}

int ChCURLMulti::_curl_timer_cb(CURLM *multi, std::chrono::milliseconds timeout)
{
	_log.debug("Update timeout callback {}", timeout);
	timer_scheme::relative data = {};
	if (timeout.count() > 0)
		data.ts = timeout;
	else if (timeout.count() == 0)
		data.ts = std::chrono::nanoseconds(1);
	tll_msg_t msg = { TLL_MESSAGE_DATA };
	msg.msgid = timer_scheme::relative::id;
	msg.data = &data;
	msg.size = sizeof(data);

	if (_timer->post(&msg))
		return _log.fail(EINVAL, "Failed to update timer");
	return 0;
}

namespace {
static constexpr std::string_view what2str(int what)
{
	switch (what) {
	case CURL_POLL_IN: return "CURL_POLL_IN";
	case CURL_POLL_OUT: return "CURL_POLL_IN";
	case CURL_POLL_INOUT: return "CURL_POLL_IN";
	case CURL_POLL_REMOVE: return "CURL_POLL_IN";
	}
	return "CURL_POLL unknown";
}

static constexpr std::string_view curl_url_strerror(CURLUcode r)
{
	switch(r) {
	case CURLUE_OK: return "OK";
	case CURLUE_BAD_HANDLE: return "Bad handle";
	case CURLUE_BAD_PARTPOINTER: return "Bad partpointer";
	case CURLUE_MALFORMED_INPUT: return "Malformed input";
	case CURLUE_BAD_PORT_NUMBER: return "Bad port number";
	case CURLUE_UNSUPPORTED_SCHEME: return "Unsupported scheme";
	case CURLUE_URLDECODE: return "URLDecode";
	case CURLUE_OUT_OF_MEMORY: return "Out of memory";
	case CURLUE_USER_NOT_ALLOWED: return "User not allowed";
	case CURLUE_UNKNOWN_PART: return "Unknown part";
	case CURLUE_NO_SCHEME: return "No scheme";
	case CURLUE_NO_USER: return "No user";
	case CURLUE_NO_PASSWORD: return "No password";
	case CURLUE_NO_OPTIONS: return "No options";
	case CURLUE_NO_HOST: return "No host";
	case CURLUE_NO_PORT: return "No port";
	case CURLUE_NO_QUERY: return "No query";
	case CURLUE_NO_FRAGMENT: return "No fragment";
	}
	return "Unknown error";
}

static constexpr std::string_view method_str(curl_scheme::method_t m)
{
       using method_t = curl_scheme::method_t;

       switch (m) {
       case method_t::UNDEFINED: return "UNDEFINED";
       case method_t::GET: return "GET";
       case method_t::HEAD: return "HEAD";
       case method_t::POST: return "POST";
       case method_t::PUT: return "PUT";
       case method_t::DELETE: return "DELETE";
       case method_t::CONNECT: return "CONNECT";
       case method_t::OPTIONS: return "OPTIONS";
       case method_t::TRACE: return "TRACE";
       case method_t::PATCH: return "PATCH";
       }
       return "UNDEFINED";
}
}

int ChCURLMulti::_curl_socket_cb(CURL *e, curl_socket_t fd, int what, ChCURLSocket *c)
{
	_log.debug("Curl socket callback {}", what2str(what));
	if (what == CURL_POLL_REMOVE) {
		if (!c) return 0;
		auto channel = c->self();

		_log.debug("Remove curl socket channel {}", channel->name());
		channel->close();
		_child_del(channel);

		auto it = std::find_if(_sockets.begin(), _sockets.end(), [channel](auto & i) { return i.get() == channel; });
		if (it != _sockets.end())
			_sockets.erase(it);
		return 0;
	}

	if (!c) {
		_log.debug("Create new socket channel for {}", fd);
		auto r = context().channel(fmt::format("curl-socket://;tll.internal=yes;name={}/{}", this->name, fd), self(), &ChCURLSocket::impl);
		if (!r)
			return this->_log.fail(EINVAL, "Failed to init curl socket channel");
		_child_add(r.get());

		c = channel_cast<ChCURLSocket>(r.get());
		c->bind(fd);
		c->self()->open();

		_sockets.emplace_back(r.release());

		curl_multi_assign(_multi, fd, c);
	}

	unsigned caps = 0;
	switch (what) {
	case CURL_POLL_IN: caps = dcaps::CPOLLIN; break;
	case CURL_POLL_OUT: caps = dcaps::CPOLLOUT; break;
	case CURL_POLL_INOUT: caps = dcaps::CPOLLIN | dcaps::CPOLLOUT; break;
	}
	c->update_poll(caps);

	return 0;
}

int ChCURL::_init(const tll::Channel::Url &url, tll::Channel *master)
{

	if (!master) {
		_master_ptr = context().channel(fmt::format("curl://;tll.internal=yes;name={}/multi", this->name), nullptr, &ChCURLMulti::impl);
		if (!_master_ptr)
			return _log.fail(EINVAL, "Failed to create curl multi channel");
		master = _master_ptr.get();
	}

	_master = tll::channel_cast<ChCURLMulti>(master);
	if (!_master)
		return _log.fail(EINVAL, "CURL needs CURLMulti master channel, got {}", master->name());

	this->_scheme_control.reset(tll_scheme_ref(_master->_scheme_control.get()));

	auto proto = url.proto();
	auto sep = proto.find("+");
	if (sep == proto.npos)
		return _log.fail(EINVAL, "Invalid curl proto '{}': no + found", proto);

	_host = url.host();
	if (!_host.size())
		return _log.fail(EINVAL, "Empty http host name");

	_host = proto.substr(sep + 1) + "://" + _host;

	{
		_curl_url = curl_url();

		if (auto r = curl_url_set(_curl_url, CURLUPART_URL, _host.c_str(), 0); r)
			return _log.fail(EINVAL, "Failed to parse url '{}': {}", _host, curl_url_strerror(r));
	}

	auto reader = channel_props_reader(url);

	_recv_chunked = reader.getT("recv-chunked", false);
	_recv_size = reader.getT<tll::util::Size>("recv-size", 64 * 1024);
	_expect_timeout = reader.getT("expect-timeout", _expect_timeout);

	_mode = reader.getT("transfer", Mode::Single, {{"single", Mode::Single}, {"data", Mode::Data}, {"control", Mode::Full}});
	if (_mode == Mode::Single)
		_autoclose = reader.getT("autoclose", false);

	using method_t = curl_scheme::method_t;
	auto method = reader.getT("method", method_t::GET, {{"GET", method_t::GET}, {"HEAD", method_t::HEAD}, {"POST", method_t::POST}, {"PUT", method_t::PUT}, {"DELETE", method_t::DELETE}, {"CONNECT", method_t::CONNECT}, {"OPTIONS", method_t::OPTIONS}, {"TRACE", method_t::TRACE}, {"PATCH", method_t::PATCH}});
	_method = method_str(method);

	for (auto & [k, c] : url.browse("header.**")) {
		auto v = c.get();
		if (v)
			_headers.emplace(k.substr(strlen("header.")), *v);
	}

	if (!reader)
		return _log.fail(EINVAL, "Invalid url: {}", reader.error());

	if (_master_ptr)
		_child_add(_master_ptr.get(), "multi");

	return 0;
}

int curl_session_t::init()
{
	auto & _log = parent->_log;

	state = tll::state::Closed;

	bool http = (parent->_host.substr(0, 4) == "http");

	if (curl)
		curl_easy_cleanup(curl);

	curl = curl_easy_init();

	//tll::curl::CURL_ptr curl = { curl_easy_init(), curl_easy_cleanup };

	//tll::curl::CURLU_ptr url = { curl_url(), curl_url_cleanup };

	if (!curl)
		return _log.fail(EINVAL, "Failed to init curl easy handle");

	tll::curl::setopt<CURLOPT_CURLU>(curl, url);

	curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, parent->_method.data());
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
	curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 32);

	tll::curl::setopt<CURLOPT_PRIVATE>(curl, this);

	if (rsize != -1) {
		_log.debug("Set upload size to {}", rsize);
		tll::curl::setopt<CURLOPT_INFILESIZE_LARGE>(curl, rsize);
	}
	if (rsize > 0) {
		_log.debug("Enable upload");
		tll::curl::setopt<CURLOPT_UPLOAD>(curl, 1);
		tll::curl::setopt<CURLOPT_EXPECT_100_TIMEOUT_MS>(curl, parent->_expect_timeout.count());
	}

	if (http) {
		tll::curl::setopt<CURLOPT_HEADERDATA>(curl, this);
		tll::curl::setopt<CURLOPT_HEADERFUNCTION>(curl, [](char *data, size_t size, size_t nmemb, void *user) {
			return static_cast<curl_session_t *>(user)->header(data, size * nmemb);
		});

		for (auto & [k, v] : headers)
			headers_list = curl_slist_append(headers_list, fmt::format("{}: {}", k, v).c_str());

		if (headers_list)
			tll::curl::setopt<CURLOPT_HTTPHEADER>(curl, headers_list);
	}

	tll::curl::setopt<CURLOPT_WRITEDATA>(curl, this);
	tll::curl::setopt<CURLOPT_WRITEFUNCTION>(curl, [](char *data, size_t size, size_t nmemb, void *user) {
		return static_cast<curl_session_t *>(user)->write(data, size * nmemb);
	});

	tll::curl::setopt<CURLOPT_READDATA>(curl, this);
	tll::curl::setopt<CURLOPT_READFUNCTION>(curl, [](char *data, size_t size, size_t nmemb, void *user) {
		return static_cast<curl_session_t *>(user)->read(data, size * nmemb);
	});

	curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2);

	state = tll::state::Opening;
	headers.clear();

	return 0;
}

void ChCURL::_free()
{
	// TODO: Can not be called from curl callback
	if (_curl_url)
		curl_url_cleanup(_curl_url);

	_curl_url = nullptr;
}

int ChCURL::_open(const PropsView &)
{
	if (_master_ptr) {
		auto r = _master_ptr->open();
		if (r)
			return _log.fail(r, "Failed to open curl multi channel");
	}

	_log.info("Create curl easy handle for {}", _host);

	if (_mode == Mode::Single) {
		std::unique_ptr<curl_session_t> s(new curl_session_t);
		s->parent = this;
		s->url = curl_url_dup(_curl_url);
		s->headers = _headers;

		if (s->init())
			return _log.fail(EINVAL, "Failed to init base curl handle");

		_log.debug("Add curl handle to {}", _master->name);
		if (auto r = curl_multi_add_handle(_master->multi(), s->curl); r)
			return _log.fail(EINVAL, "curl_multi_add_handle({}) failed: {}", _host, curl_multi_strerror(r));

		_sessions.emplace(0, s.release());
	}

	return 0;
}

int ChCURL::_close()
{
	// TODO: Can not be called from curl callback

	_sessions.clear();

	if (_master_ptr)
		_master_ptr->close();

	return 0;
}

int ChCURL::_post(const tll_msg_t *msg, int flags)
{
	if (msg->type != TLL_MESSAGE_DATA)
		return 0;

	if (_mode == Mode::Data) {
		_log.debug("Create new session {} with data size {}", msg->addr.u64, msg->size);

		if (_sessions.find(msg->addr.u64) != _sessions.end())
			return _log.fail(EEXIST, "Failed to create new session: address {} already used", msg->addr.u64);
		std::unique_ptr<curl_session_t> s(new curl_session_t);
		s->parent = this;
		s->url = curl_url_dup(_curl_url);
		s->headers = _headers;
		s->addr = msg->addr;

		s->rsize = msg->size;
		s->rbuf.resize(msg->size);
		memcpy(s->rbuf.data(), msg->data, msg->size);

		if (s->init())
			return _log.fail(EINVAL, "Failed to init base curl handle");

		_log.debug("Add curl handle to {}", _master->name);
		if (auto r = curl_multi_add_handle(_master->multi(), s->curl); r)
			return _log.fail(EINVAL, "curl_multi_add_handle({}) failed: {}", _host, curl_multi_strerror(r));

		_sessions.emplace(msg->addr.u64, s.release());
	}
	return 0;
}

int ChCURL::_process(long timeout, int flags)
{
	auto i = _sessions.begin();
	while (i != _sessions.end()) {
		auto & s = i->second;
		if (s->state == tll::state::Closing || s->state == tll::state::Error) {
			std::unique_ptr<curl_session_t> ptr;
			ptr.swap(s);

			i = _sessions.erase(i);

			ptr->close();
		} else
			i++;
	}
	_update_dcaps(0, dcaps::Pending | dcaps::Process);

	if (_autoclose && _sessions.empty())
		close();
	return EAGAIN;
}

namespace {

std::string asciilower(std::string_view str)
{
	std::string r(str.size(), '\0');
	for (auto i = 0u; i < str.size(); i++) {
		auto c = str[i];
		if ('A' <= c && c <= 'Z')
			r[i] = c - 'A' + 'a';
		else
			r[i] = c;
	}
	return r;
}

template <typename Buf, typename T, typename Ptr>
void offset_ptr_resize(Buf & buf, tll::scheme::offset_ptr_t<T, Ptr> * ptr, size_t size)
{
	if (size == 0) {
		ptr->size = 0;
		return;
	}

	auto view = tll::make_view(buf);
	auto off = ((char *) ptr) - view.template dataT<char>();

	auto poff = buf.size() - off;

	buf.resize(buf.size() + sizeof(T) * size);

	ptr = view.view(off).template dataT<tll::scheme::offset_ptr_t<T, Ptr>>();
	ptr->offset = poff;
	ptr->size = size;

	if constexpr (!std::is_same_v<Ptr, tll_scheme_offset_ptr_legacy_short_t>) {
		ptr->entity = sizeof(T);
	}
}

template <typename Buf, typename T, typename Ptr>
void offset_ptr_resize(Buf & buf, tll::scheme::offset_ptr_t<T, Ptr> &ptr, size_t size) { return offset_ptr_resize(buf, &ptr, size); }

}

size_t curl_session_t::header(char * cdata, size_t size)
{
	if (size < 2) return 0;

	auto data = std::string_view(cdata, size - 2); // Strip \r\n

	if (data.empty()) {
		parent->_log.debug("Last header");
		return size;
	}

	if (data.substr(0, 5) == "HTTP/") {
		parent->_log.debug("Start of header block: '{}'", data);
		headers.clear();
		wsize = std::nullopt;
		return size;
	}

	auto sep = data.find(':');
	if (sep == data.npos) {
		parent->_log.debug("No colon in header: '{}'", data);
		return size;
	}

	auto k = data.substr(0, sep);
	auto v = data.substr(sep + 1);
	while (v.size() && v[0] == ' ')
		v = v.substr(1);

	parent->_log.debug("Header: '{}': '{}'", k, v);
	headers.emplace(asciilower(k), std::string(v));
	return size;
}

size_t curl_session_t::read(char * data, size_t size)
{
	parent->_log.error("Requested {} bytes of data", size);
	if (roff == rbuf.size())
		return 0; //CURL_READFUNC_PAUSE;

	auto s = std::min(rbuf.size() - roff, size);
	parent->_log.debug("Send {} bytes of data (requested {})", s, size);
	memcpy(data, rbuf.data() + roff, s);
	roff += s;
	return s;
}

void curl_session_t::connected()
{
	state = tll::state::Active;

	wsize = tll::curl::getinfo<CURLINFO_CONTENT_LENGTH_DOWNLOAD_T>(curl);
	if (wsize)
		parent->_log.info("Content-Size: {}", *wsize);
	else
		parent->_log.info("Content-Size is not supported for this protocol");

	std::string_view url = tll::curl::getinfo<CURLINFO_EFFECTIVE_URL>(curl).value_or("");
	parent->_log.info("Send connect message for {}", url);

	std::vector<unsigned char> buf;
	buf.resize(sizeof(curl_scheme::connect));
	auto data = (curl_scheme::connect *) buf.data();

	data->code = tll::curl::getinfo<CURLINFO_RESPONSE_CODE>(curl).value_or(0);
	data->method = curl_scheme::method_t::UNDEFINED;
	data->size = wsize.value_or(-1);

	offset_ptr_resize(buf, data->path, url.size() + 1);

	data = (curl_scheme::connect *) buf.data();
	memcpy(data->path.data(), url.data(), url.size());

	offset_ptr_resize(buf, data->headers, headers.size());

	auto i = 0u;
	for (auto & [k, v] : headers) {
		data = (curl_scheme::connect *) buf.data();
		offset_ptr_resize(buf, data->headers.data()[i].header, k.size() + 1);
		data = (curl_scheme::connect *) buf.data();
		memcpy(data->headers.data()[i].header.data(), k.data(), k.size());

		offset_ptr_resize(buf, data->headers.data()[i].value, v.size() + 1);
		data = (curl_scheme::connect *) buf.data();
		memcpy(data->headers.data()[i].value.data(), v.data(), v.size());

		i++;
	}

	tll_msg_t msg = {};
	msg.type = TLL_MESSAGE_CONTROL;
	msg.msgid = curl_scheme::connect::id;
	msg.addr = addr;
	msg.data = buf.data();
	msg.size = buf.size();
	parent->_callback(&msg);
}

size_t curl_session_t::callback_data(const void * data, size_t size)
{
	tll_msg_t msg = { TLL_MESSAGE_DATA };
	msg.addr = addr;
	msg.data = data;
	msg.size = size;
	parent->_callback_data(&msg);
	return size;
}

size_t curl_session_t::write(char * data, size_t size)
{
	if (state == tll::state::Opening)
		connected();

	if (state != tll::state::Active) // Session is not active, don't generate messages
		return size;

	if (parent->_recv_chunked)
		return callback_data(data, size);

	if (wbuf.size() == 0 && size > parent->_recv_size) // No cached data, incoming data too large
		return callback_data(data, size);

	auto off = wbuf.size();
	auto head = std::min(size, parent->_recv_size - wbuf.size());

	wbuf.resize(wbuf.size() + head);
	memmove(&wbuf[off], data, head);

	if (wbuf.size() < parent->_recv_size)
		return size;

	callback_data(wbuf.data(), wbuf.size());

	wbuf.resize(0);

	if (size > head)
		write(data + head, size - head);

	return size;
}

void curl_session_t::finalize(int code)
{
	parent->_log.info("Finalize transfer: {}", code);
	state = tll::state::Closing;

	parent->_update_dcaps(dcaps::Pending | dcaps::Process);

	if (!wbuf.size())
		return;

	callback_data(wbuf.data(), wbuf.size());
}

void curl_session_t::reset()
{
	if (curl) {
		curl_multi_remove_handle(parent->_master->multi(), curl);
		curl_easy_cleanup(curl);
	}
	curl = nullptr;

	if (headers_list)
		curl_slist_free_all(headers_list);
	
	headers_list = nullptr;

	if (url)
		curl_url_cleanup(url);
	url = nullptr;

	headers.clear();
}

void curl_session_t::close()
{
	if (curl) {
		curl_multi_remove_handle(parent->_master->multi(), curl);
		curl_easy_cleanup(curl);
	}

	curl = nullptr;
}
