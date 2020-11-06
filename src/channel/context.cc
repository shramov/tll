/*
 * Copyright (c) 2018-2020 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#include <map>
#include <memory>
#include <set>
#include <shared_mutex>
#include <string>

#include <dlfcn.h>

#include "tll/channel.h"
#include "tll/channel/impl.h"
#include "tll/config.h"
#include "tll/logger.h"
#include "tll/logger/prefix.h"
#include "tll/util/listiter.h"
#include "tll/util/refptr.h"
#include "tll/util/url.h"


#include "channel/direct.h"
#include "channel/ipc.h"
#include "channel/mem.h"
#include "channel/null.h"
#include "channel/serial.h"
#include "channel/tcp.h"
#include "channel/timer.h"
#include "channel/zero.h"

using namespace tll;

TLL_DEFINE_IMPL(ChDirect);
TLL_DEFINE_IMPL(ChNull);

TLL_DECLARE_IMPL(ChIpc);
TLL_DECLARE_IMPL(ChMem);
TLL_DECLARE_IMPL(ChSerial);
TLL_DECLARE_IMPL(ChTcp);
TLL_DECLARE_IMPL(ChTimer);
TLL_DECLARE_IMPL(ChZero);

struct tll_channel_context_t : public tll::util::refbase_t<tll_channel_context_t>
{
	Logger _log = {"tll.context"};

	std::map<std::string, const tll_channel_impl_t *, std::less<>> registry;
	std::map<std::string_view, tll_channel_t *> channels;
	std::map<std::string, scheme::SchemePtr, std::less<>> scheme_cache;
	std::shared_mutex scheme_cache_lock;

	Config config;
	Config config_defaults;

	tll_channel_context_t(Config defaults) : config_defaults(defaults)
	{
		reg(&ChDirect::impl);
		reg(&ChIpc::impl);
		reg(&ChMem::impl);
		reg(&ChNull::impl);
		reg(&ChSerial::impl);
		reg(&ChTcp::impl);
		reg(&ChTimer::impl);
		reg(&ChZero::impl);
	}

	tll_channel_t * init(std::string_view params, tll_channel_t *master, const tll_channel_impl_t *impl)
	{
		auto url = ConfigUrl::parse(params);
		if (!url)
			return _log.fail(nullptr, "Invalid url '{}': {}", params, url.error());
		return init(*url, master, impl);
	}

	tll_channel_t * init(const Channel::Url &url, tll_channel_t *master, const tll_channel_impl_t *impl);

	tll_channel_t * get(std::string_view name) const
	{
		auto it = channels.find(name);
		if (it == channels.end()) return 0;
		return it->second;
	}

	int reg(const tll_channel_impl_t *impl, std::string_view name = "")
	{
		if (name.empty())
			name = impl->name;
		_log.debug("Register channel {} as {}", impl->name, name);
		if (!registry.emplace(name, impl).second)
			return _log.fail(EEXIST, "Failed to register '{}': duplicate name", name);
		return 0;
	}

	int unreg(const tll_channel_impl_t *impl, std::string_view name = "")
	{
		if (name.empty())
			name = impl->name;
		auto it = registry.find(name);
		if (it == registry.end())
			return _log.fail(ENOENT, "Failed to unregister '{}': not found", name);
		if (it->second != impl)
			return _log.fail(EINVAL, "Failed to unregister '{}': invalid impl pointer", name);
		registry.erase(it);
		return 0;
	}

	int load(const std::string &p, const std::string &symbol)
	{
		std::string_view name = p;
		auto sep = name.rfind('/');
		if (sep != name.npos)
			name = name.substr(sep + 1);
		auto log = _log.prefix("Module {}:", name);

		auto path = fmt::format("{}lib{}.so", sep == name.npos?std::string():p.substr(0, sep + 1), name);

		log.debug("Loading from {}", path);
		auto module = dlopen(path.c_str(), RTLD_GLOBAL | RTLD_NOW);
		if (!module)
			return log.fail(EINVAL, "Failed to load: {}", dlerror());
		auto f = (tll_channel_module_t *) dlsym(module, symbol.c_str());
		if (!f)
			return log.fail(EINVAL, "Failed to load: {} not found", symbol);
		if (f->impl) {
			for (auto i = f->impl; *i; i++) {
				reg(*i);
			}
		} else
			log.info("No channels defined in module {}");
		//(*f->init)(this);
		return 0;
	}

	const tll_channel_impl_t * lookup(std::string_view proto) const
	{
		_log.debug("Lookup proto '{}'", proto);
		auto i = registry.find(proto);
		if (i != registry.end())
			return i->second;

		auto sep = proto.find('+');
		if (sep == proto.npos)
			return nullptr;

		_log.debug("Lookup prefix '{}'", proto.substr(0, sep));
		i = registry.find(proto.substr(0, sep));
		if (i == registry.end())
			return nullptr;
		if (!i->second->prefix)
			return _log.fail(nullptr, "Channel impl '{}' is not prefix impl", proto.substr(0, sep));
		return i->second;

	}

	const tll_scheme_t * scheme_load(std::string_view url, bool cache)
	{
		if (url.substr(0, 10) == "channel://") {
			auto name = url.substr(10);
			auto c = get(name);
			if (!c)
				return _log.fail(nullptr, "Failed to load scheme '{}', channel '{}' not found", url, name);
			return tll_scheme_ref(tll_channel_scheme(c, 0));
		}

		if (!cache)
			return Scheme::load(url);

		{
			std::shared_lock<std::shared_mutex> lock(scheme_cache_lock);
			auto it = scheme_cache.find(url);
			if (it != scheme_cache.end())
				return tll_scheme_ref(it->second.get());
		}

		auto s = Scheme::load(url);
		if (!s) return nullptr;

		std::unique_lock<std::shared_mutex> lock(scheme_cache_lock);
		if (!scheme_cache.insert({std::string(url), scheme::SchemePtr {s, &tll_scheme_unref}}).second)
			return s;
		return tll_scheme_ref(s);
	}
};

std::mutex _context_lock;
tll::util::refptr_t<tll_channel_context_t> _context;

namespace {
void context_init()
{
	if (!_context) {
		std::unique_lock lock(_context_lock);
		if (_context) return;
		_context.reset(new tll_channel_context_t(Config()));
	}
}

tll_channel_context_t * context(tll_channel_context_t *ctx = nullptr)
{
	if (ctx)
		return ctx;
	context_init();
	return _context.get();
}

const tll_channel_context_t * context(const tll_channel_context_t *ctx)
{
	if (ctx)
		return ctx;
	context_init();
	return _context.get();
}
}

tll_channel_context_t * tll_channel_context_new(tll_config_t *defaults)
{
	Config cfg;
	if (defaults)
		cfg = Config(defaults);
	return new tll_channel_context_t(cfg);
}

tll_channel_context_t * tll_channel_context_ref(tll_channel_context_t * ctx)
{
	return context(ctx)->ref();
}

void tll_channel_context_free(tll_channel_context_t * ctx)
{
	context(ctx)->unref();
}

tll_channel_context_t * tll_channel_context_default()
{
	return context();
}

tll_config_t * tll_channel_context_config(tll_channel_context_t *c)
{
	if (!c) return 0;
	tll_config_ref(c->config);
	return c->config;
}

tll_config_t * tll_channel_context_config_defaults(tll_channel_context_t *c)
{
	if (!c) return 0;
	tll_config_ref(c->config_defaults);
	return c->config_defaults;
}

const tll_scheme_t * tll_channel_context_scheme_load(tll_channel_context_t *c, const char *url, int len, int cache)
{
	if (!c || !url) return 0;
	return c->scheme_load(tll::string_view_from_c(url, len), cache);
}

int tll_channel_impl_register(tll_channel_context_t *ctx, const tll_channel_impl_t *impl, const char *name)
{
	return context(ctx)->reg(impl, name?name:"");
}

int tll_channel_impl_unregister(tll_channel_context_t *ctx, const tll_channel_impl_t *impl, const char *name)
{
	return context(ctx)->unreg(impl, name?name:"");
}

const tll_channel_impl_t * tll_channel_impl_get(const tll_channel_context_t *ctx, const char *name)
{
	if (!name) return nullptr;
	return context(ctx)->lookup(name);
}

int tll_channel_module_load(tll_channel_context_t *ctx, const char *module, const char * symbol)
{
	return context(ctx)->load(module, symbol);
}

tll_channel_t * tll_channel_new(tll_channel_context_t * ctx, const char *str, size_t len, tll_channel_t *master, const tll_channel_impl_t *impl)
{
	return context(ctx)->init(string_view_from_c(str, len), master, impl);
}

tll_channel_t * tll_channel_new_url(tll_channel_context_t * ctx, const tll_config_t *curl, tll_channel_t *master, const tll_channel_impl_t *impl)
{
	const tll::Channel::Url url(const_cast<tll_config_t *>(curl));

	return context(ctx)->init(url, master, impl);
}

tll_channel_t * tll_channel_context_t::init(const tll::Channel::Url &url, tll_channel_t *master, const tll_channel_impl_t * impl)
{
	if (!impl) {
		impl = lookup(url.proto());
		if (!impl)
			return _log.fail(nullptr, "Channel '{}' not found", url.proto());
	}

	auto internal = url.getT("tll.internal", false);
	if (!internal)
		return _log.fail(nullptr, "Invalid tll.internal parameter: {}", internal.error());

	if (!master && url.has("master")) {
		auto pi = url.get("master");
		auto it = channels.find(*pi);
		if (it == channels.end())
			return _log.fail(nullptr, "Failed to create channel: master '{}' not found", *pi);
		master = it->second;
	}

	auto c = std::make_unique<tll_channel_t>();
	if (!c) return nullptr;

	std::set<const tll_channel_impl_t *> impllog;

	auto url_str = conv::to_string(url);

	do {
		*c = {};
		c->context = this;
		c->impl = impl;
		_log.debug("Initialize channel with impl '{}'", c->impl->name);
		auto r = (*c->impl->init)(c.get(), url, master, this);
		if (r == EAGAIN && c->impl != nullptr && c->impl != impl) {
			_log.info("Reinitialize channel with different impl '{}'", c->impl->name);
			if (impllog.find(c->impl) != impllog.end())
				return _log.fail(nullptr, "Detected loop in channel initialization");
			impllog.insert(impl);
			impl = c->impl;
			continue;
		} else if (r)
			return _log.fail(nullptr, "Failed to init channel {}", url_str);
		if (*internal)
			c->internal->caps |= caps::Custom;
		if (!c->internal)
			return _log.fail(nullptr, "Failed to init channel {}: NULL internal pointer", url_str);
		if (!*internal && c->internal->name) {
			channels.emplace(c->internal->name, c.get()); // Check for dup
			tll_config_set_config(config, c->internal->name, -1, c->internal->config, 0);
		}
		break;
	} while (true);

	tll_channel_context_ref(c->context);
	return c.release();
}

tll_state_t tll_channel_state(const tll_channel_t *c) { return c->internal->state; }
const char * tll_channel_name(const tll_channel_t *c) { return c->internal->name; }
unsigned tll_channel_caps(const tll_channel_t *c) { if (c->internal) return c->internal->caps; return 0; }
unsigned tll_channel_dcaps(const tll_channel_t *c) { if (c->internal) return c->internal->dcaps; return 0; }
int tll_channel_fd(const tll_channel_t *c) { if (c->internal) return c->internal->fd; return -1; }
tll_config_t * tll_channel_config(tll_channel_t *c) { if (!c->internal) return 0; tll_config_ref(c->internal->config); return c->internal->config; }
tll_channel_list_t * tll_channel_children(tll_channel_t *c) { if (!c->internal) return 0; return c->internal->children; }
tll_channel_context_t * tll_channel_context(const tll_channel_t *c) { return tll_channel_context_ref(c->context); }

void tll_channel_free(tll_channel_t *c)
{
	if (!c) return;
	std::string_view name = tll_channel_name(c);
	if ((tll_channel_caps(c) & caps::Custom) == 0) {
		c->context->channels.erase(name);
		tll_config_del(c->context->config, name.data(), name.size(), 0);
	}
	if (c->impl)
		(*c->impl->free)(c);
	tll_channel_context_free(c->context);
	// XXX: cleanup kids list
	delete c;
}

int tll_channel_process(tll_channel_t *c, long timeout, int flags)
{
	if (!c || !c->impl || !c->internal) return EINVAL;
	if (!tll::dcaps::need_process(c->internal->dcaps)) return EAGAIN;
	return (*c->impl->process)(c, timeout, flags);
}

int tll_channel_post(tll_channel_t *c, const tll_msg_t *msg, int flags)
{
	if (!c || !c->impl) return EINVAL;
	return (*c->impl->post)(c, msg, flags);
}

namespace {
inline void suspend(tll_channel_t *c)
{
	c->internal->dcaps |= dcaps::Suspend;
	for (auto & i : tll::util::list_wrap(c->internal->children))
		suspend(i.channel);
}

inline void resume(tll_channel_t *c)
{
	if ((c->internal->dcaps & dcaps::SuspendPermanent) != 0)
		return;
	c->internal->dcaps &= ~dcaps::Suspend;
	for (auto & i : tll::util::list_wrap(c->internal->children))
		resume(i.channel);
}
}

int tll_channel_suspend(tll_channel_t *c)
{
	if (!c || !c->internal) return EINVAL;

	c->internal->dcaps |= dcaps::SuspendPermanent;
	suspend(c);
	return 0;
}

int tll_channel_resume(tll_channel_t *c)
{
	if (!c || !c->internal) return EINVAL;

	c->internal->dcaps &= ~dcaps::SuspendPermanent;
	resume(c);
	return 0;
}

#define FORWARD_SAFE_ERR(func, err, c, ...)	\
{					\
	if (!c || !c->impl || !c->impl->func) return err;	\
	return (*c->impl->func)(c, ##__VA_ARGS__);          	\
}

#define FORWARD_SAFE(func, c, ...) FORWARD_SAFE_ERR(func, EINVAL, c, ##__VA_ARGS__)

int tll_channel_open(tll_channel_t *c, const char *str, size_t len)
	FORWARD_SAFE(open, c, str, len);

int tll_channel_close(tll_channel_t *c)
	FORWARD_SAFE(close, c);

const tll_scheme_t * tll_channel_scheme(const tll_channel_t *c, int type)
	FORWARD_SAFE_ERR(scheme, NULL, c, type);

namespace {
using cb_pair_t = tll_channel_callback_pair_t;
int callback_add(cb_pair_t **c, unsigned &size, cb_pair_t *cb, bool fixed = false)
{
	auto end = *c + size;
	cb_pair_t * empty = nullptr;
	for (auto ptr = *c; ptr < end; ptr++) {
		if (!ptr->cb && !empty) {
			empty = ptr;
			continue;
		}
		if (ptr->cb == cb->cb && ptr->user == cb->user) {
			// Update mask, duplicate ok
			ptr->mask |= cb->mask;
			return 0;
		}
	}
	if (empty) {
		*empty = *cb;
		return 0;
	}
	auto l = (cb_pair_t *) realloc(*c, sizeof(cb_pair_t) * (size + 1));
	if (!l)
		return ENOMEM;
	l[size++] = *cb;
	*c = l;
	return 0;
}

size_t callback_shrink(cb_pair_t *c, size_t size)
{
	for (; size; size--) {
		if (c[size - 1].cb != nullptr)
			break;
	}
	return size;
}

int callback_del(cb_pair_t *c, unsigned &size, cb_pair_t *cb)
{
	for (auto ptr = c; ptr < c + size; ptr++) {
		if (ptr->cb != cb->cb || ptr->user != cb->user)
			continue;
		ptr->mask &= ~cb->mask;
		if (ptr->mask != 0)
			return 0;
		ptr->cb = nullptr;
		ptr->user = nullptr;
		ptr->mask = 0;
		size = callback_shrink(c, size);
		return 0;
	}
	return ENOENT;
}

}

int tll_channel_callback_add(tll_channel_t *c, tll_channel_callback_t cb, void *user, unsigned mask)
{
	tll::Logger _log(std::string("tll.channel.") + tll_channel_name(c));
	auto in = c->internal;
	tll_channel_callback_pair_t p = { cb, user, mask };
	if (mask & TLL_MESSAGE_MASK_DATA) {
		mask ^= TLL_MESSAGE_MASK_DATA;
		p.mask = TLL_MESSAGE_MASK_DATA;
		if (callback_add(&in->data_cb, in->data_cb_size, &p))
			return ENOMEM;
		_log.info("Data callbacks (add): {}", in->data_cb_size);
		p.mask = mask;
		if (mask == 0)
			return 0;
	}

	return callback_add(&in->cb, in->cb_size, &p);
}

int tll_channel_callback_del(tll_channel_t *c, tll_channel_callback_t cb, void *user, unsigned mask)
{
	tll::Logger _log(std::string("tll.channel.") + tll_channel_name(c));
	auto in = c->internal;
	tll_channel_callback_pair_t p = { cb, user, mask };
	if (mask & TLL_MESSAGE_MASK_DATA) {
		p.mask = TLL_MESSAGE_MASK_DATA;
		mask ^= TLL_MESSAGE_MASK_DATA;
		callback_del(in->data_cb, in->data_cb_size, &p);
		_log.info("Data callbacks (del): {}", in->data_cb_size);
		p.mask = mask;
		if (mask == 0)
			return 0;
	}

	return callback_del(in->cb, in->cb_size, &p);
}

tll_channel_t * tll_channel_get(const tll_channel_context_t *c, const char * name, int len)
{
	return context(c)->get(std::string_view(name, len));
}
