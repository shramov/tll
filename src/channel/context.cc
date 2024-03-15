/*
 * Copyright (c) 2018-2021 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#include <map>
#include <memory>
#include <set>
#include <shared_mutex>
#include <string>
#include <variant>

#include <dlfcn.h>

#include "tll/channel.h"
#include "tll/channel/impl.h"
#include "tll/channel/module.h"
#include "tll/config.h"
#include "tll/logger.h"
#include "tll/logger/prefix.h"
#include "tll/stat.h"
#include "tll/util/listiter.h"
#include "tll/util/refptr.h"

#include "channel/channels.h"

#include "channel/blocks.h"
#include "channel/direct.h"
#include "channel/ipc.h"
#include "channel/file-init.h"
#include "channel/framed.h"
#ifdef WITH_RAPIDJSON
#include "channel/json.h"
#endif
#include "channel/loader.h"
#include "channel/lz4.h"
#include "channel/mem.h"
#include "channel/null.h"
#include "channel/pub.h"
#include "channel/random.h"
#include "channel/rate.h"
#include "channel/resolve.h"
#include "channel/rotate.h"
#include "channel/serial.h"
#include "channel/seq-check.h"
#include "channel/stream-server.h"
#include "channel/tcp.h"
#include "channel/timeit.h"
#include "channel/timer.h"
#include "channel/udp.h"
#include "channel/yaml.h"
#include "channel/zero.h"

using namespace tll;

TLL_DEFINE_IMPL(ChLoader);
TLL_DEFINE_IMPL(ChNull);
TLL_DEFINE_IMPL(tll::channel::Random);
TLL_DEFINE_IMPL(tll::channel::SeqCheck);

TLL_DECLARE_IMPL(tll::channel::Blocks);
TLL_DECLARE_IMPL(ChDirect);
TLL_DECLARE_IMPL(ChIpc);
TLL_DECLARE_IMPL(channel::FileInit);
TLL_DECLARE_IMPL(Framed);
#ifdef WITH_RAPIDJSON
TLL_DECLARE_IMPL(ChJSON);
#endif
TLL_DECLARE_IMPL(ChMem);
TLL_DECLARE_IMPL(ChLZ4);
TLL_DECLARE_IMPL(ChPubServer);
TLL_DECLARE_IMPL(ChSerial);
TLL_DECLARE_IMPL(tll::channel::StreamServer);
TLL_DECLARE_IMPL(tll::channel::Rate);
TLL_DECLARE_IMPL(tll::channel::Resolve);
TLL_DECLARE_IMPL(tll::channel::Rotate);
TLL_DECLARE_IMPL(ChTcp);
TLL_DECLARE_IMPL(ChTimer);
TLL_DECLARE_IMPL(ChTimeIt);
TLL_DECLARE_IMPL(ChUdp);
TLL_DECLARE_IMPL(ChYaml);
TLL_DECLARE_IMPL(ChZero);

struct tll_channel_context_t : public tll::util::refbase_t<tll_channel_context_t>
{
	Logger _log = {"tll.context"};
	tll::stat::OwnedList stat_list;

	using impl_t = std::variant<const tll_channel_impl_t *, tll::Channel::Url>;
	std::map<std::string, impl_t, std::less<>> registry;
	std::map<std::string_view, tll_channel_t *> channels;
	std::map<std::string, scheme::SchemePtr, std::less<>> scheme_cache;
	std::map<void *, tll_channel_module_t *> modules;
	std::shared_mutex scheme_cache_lock;

	unsigned _noname_idx = 0;

	Config config;
	Config config_defaults;

	tll_channel_context_t(Config defaults) : config_defaults(defaults)
	{
		reg(&tll::channel::Blocks::impl);
		reg(&ChDirect::impl);
		reg(&ChIpc::impl);
		reg(&channel::FileInit::impl);
		reg(&Framed::impl);
#ifdef WITH_RAPIDJSON
		reg(&ChJSON::impl);
#endif
		reg(&ChMem::impl);
		reg(&ChLZ4::impl);
		reg(&ChLoader::impl);
		reg(&ChNull::impl);
		reg(&ChPubServer::impl);
		reg(&tll::channel::Random::impl);
		reg(&tll::channel::Rate::impl);
		reg(&tll::channel::Resolve::impl);
		reg(&tll::channel::Rotate::impl);
		reg(&ChSerial::impl);
		reg(&tll::channel::StreamServer::impl);
		reg(&ChTcp::impl);
		reg(&ChTimeIt::impl);
		reg(&ChTimer::impl);
		reg(&ChUdp::impl);
		reg(&ChYaml::impl);
		reg(&ChZero::impl);

		reg(&tll::channel::SeqCheck::impl);

		auto cfg = tll::Channel::Url::parse("udp://;udp.multicast=yes");
		if (cfg) alias_reg("mudp", *cfg);
	}

	~tll_channel_context_t()
	{
		_log.info("Destroy context");
		for (auto & m : modules) {
			if (m.second->free)
				(*m.second->free)(m.second, this);
			dlclose(m.first);
		}
		modules.clear();
		channels.clear();
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
		if (!std::holds_alternative<const tll_channel_impl_t *>(it->second))
			return _log.fail(EINVAL, "Failed to unregister '{}': not impl, but alias", name);
		if (std::get<const tll_channel_impl_t *>(it->second) != impl)
			return _log.fail(EINVAL, "Failed to unregister '{}': invalid impl pointer", name);
		registry.erase(it);
		return 0;
	}

	int alias_reg(std::string_view name, tll::Channel::Url cfg)
	{
		if (name.empty())
			return _log.fail(EINVAL, "Failed to register: Empty alias name");
		constexpr std::string_view keys[] = {"tll.host", "name"};

		for (auto &k : keys) {
			auto v = cfg.get(k);
			if (!v || v->empty()) continue;
			return _log.fail(EINVAL, "Aliases has non-empty field '{}'", *v);
		}
		tll::Channel::Url url = cfg.copy();
		auto r = lookup(cfg);
		if (r == nullptr)
			return _log.fail(ENOENT, "Failed to register '{}': can not resolve protocol '{}'", name, cfg.proto());
		_log.debug("Register alias {} as {}", name, cfg.proto());
		if (!registry.emplace(name, cfg).second)
			return _log.fail(EEXIST, "Failed to register '{}': duplicate name", name);
		return 0;
	}

	int alias_unreg(std::string_view name, tll::Channel::Url cfg)
	{
		if (name.empty())
			return _log.fail(EINVAL, "Failed to unregister: Empty alias name");
		auto it = registry.find(name);
		if (it == registry.end())
			return _log.fail(ENOENT, "Failed to unregister '{}': not found", name);
		if (!std::holds_alternative<tll::Channel::Url>(it->second))
			return _log.fail(EINVAL, "Failed to unregister '{}': not alias, but impl", name);
		if (std::get<tll::Channel::Url>(it->second).proto() != cfg.proto())
			return _log.fail(EINVAL, "Failed to unregister '{}': invalid alias protocol", name, cfg.proto());
		registry.erase(it);
		return 0;
	}

	static constexpr std::string_view library_suffix()
	{
#ifdef __APPLE__
		return "dylib";
#else
		return "so";
#endif
	}

	int load(std::string_view p, std::string_view symbol, const tll::ConstConfig &cfg)
	{
		std::string_view name = p;
		auto sep = name.rfind('/');
		if (sep != name.npos)
			name = name.substr(sep + 1);
		auto log = _log.prefix("Module {}:", name);

		auto path = fmt::format("{}lib{}.{}", sep == name.npos?std::string():p.substr(0, sep + 1), name, library_suffix());

		log.debug("Loading from {}", path);
		std::unique_ptr<void, void (*)(void *)> module = {
			dlopen(path.c_str(), RTLD_LOCAL | RTLD_NOW),
			[](void *m) { if (m) dlclose(m); }
		};

		if (!module)
			return log.fail(EINVAL, "Failed to load: {}", dlerror());

		auto it = modules.find(module.get());
		if (it != modules.end()) {
			log.info("Module already loaded");
			return 0;
		}

		if (!symbol.size())
			symbol = "tll_channel_module";

		auto func = (tll_channel_module_func_t) dlsym(module.get(), symbol.data());

		// XXX: Temporary code, remove when transition is finished
		if (!func && symbol == "channel_module") {
			func = (tll_channel_module_func_t) dlsym(module.get(), "tll_channel_module");
		} else if (!func && symbol == "tll_channel_module") {
			func = (tll_channel_module_func_t) dlsym(module.get(), "channel_module");
		}

		if (!func)
			return log.fail(EINVAL, "Failed to load: {} not found", symbol);

		auto f = func();
		if (!f)
			return log.fail(EINVAL, "Module loader {} returns null pointer", symbol);

		if (f->version > TLL_CHANNEL_MODULE_VERSION)
			return log.fail(EINVAL, "Mismatched module version: expected {}, got {}", TLL_CHANNEL_MODULE_VERSION, f->version);

		if (f->flags & TLL_CHANNEL_MODULE_DLOPEN_GLOBAL) {
			log.debug("Reload with RTLD_GLOBAL");
			if (!dlopen(path.c_str(), RTLD_GLOBAL | RTLD_NOLOAD | RTLD_NOW))
				return log.fail(EINVAL, "Failed to load: failed to reload with RTLD_GLOBAL: {}", dlerror());
		}

		if (f->init) {
			auto r = 0;
			if (f->version == 1) {
				r = (*(tll_channel_module_init_v1_t *) f->init)(f, this);
			} else
				r = (*f->init)(f, this, cfg);
			if (r)
				return log.fail(EINVAL, "Failed to load: init function returned error");
		}

		if (f->impl) {
			for (auto i = f->impl; *i; i++) {
				reg(*i);
			}
		} else if (!f->init)
			log.info("No channels defined in module {}:{}", path, symbol);
		modules.insert({module.release(), f});
		return 0;
	}

	const impl_t * lookup(std::string_view proto) const
	{
		_log.debug("Lookup proto '{}'", proto);
		auto i = registry.find(proto);
		if (i != registry.end())
			return &i->second;

		auto sep = proto.find('+');
		if (sep == proto.npos)
			return nullptr;

		auto prefix = proto.substr(0, sep + 1);

		_log.debug("Lookup prefix '{}'", prefix);
		i = registry.find(prefix);
		if (i == registry.end())
			return nullptr;

		return &i->second;
	}

	const tll_channel_impl_t * lookup(tll::Channel::Url &url) const
	{
		auto proto = url.proto();
		do {
			auto impl = lookup(proto);
			if (!impl)
				return _log.fail(nullptr, "Channel impl '{}' not found", proto);
			if (std::holds_alternative<const tll_channel_impl_t *>(*impl))
				return std::get<const tll_channel_impl_t *>(*impl);
			auto alias = std::get<tll::Channel::Url>(*impl);
			_log.debug("Found alias '{}' for '{}'", alias.proto(), proto);
			auto aproto = alias.proto();
			auto sep = proto.find('+');
			if (sep != proto.npos && aproto.back() == '+') {
				proto = aproto + proto.substr(sep + 1);
			} else
				proto = aproto;
			for (auto &[k, v]: alias.browse("**")) {
				if (k == "tll.proto" || k == "tll.host") continue;
				if (url.has(k))
					return _log.fail(nullptr, "Duplicate field '{}': both in alias '{}' and in url", k, alias.proto());
				url.set(k, *v.get());
			}
			url.proto(proto);
		} while (true);
		return nullptr;
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

		auto hashproto = (url.substr(0, 6 + 3) == "sha256://");

		if (!cache && !hashproto)
			return Scheme::load(url);

		{
			std::shared_lock<std::shared_mutex> lock(scheme_cache_lock);
			auto it = scheme_cache.find(url);
			if (it != scheme_cache.end())
				return it->second->ref();
		}

		if (hashproto)
			return _log.fail(nullptr, "Hashed scheme '{}' not found in the cache", url);

		auto result = Scheme::load(url);
		if (!result) return nullptr;

		{
			std::unique_lock<std::shared_mutex> lock(scheme_cache_lock);
			if (!scheme_cache.insert({std::string(url), scheme::SchemePtr {result->ref()}}).second)
				return result;
			auto hash = result->dump("sha256");
			if (hash) {
				_log.debug("Register scheme hash '{}'", *hash);
				scheme_cache.insert({std::string(*hash), scheme::SchemePtr {result->ref()}});
			}
		}
		return result;
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
	if (!ctx) return nullptr;
	return ctx->ref();
}

void tll_channel_context_free(tll_channel_context_t * ctx)
{
	if (!ctx) return;
	ctx->unref();
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

tll_stat_list_t * tll_channel_context_stat_list(tll_channel_context_t *c)
{
	if (!c) return nullptr;
	return c->stat_list;
}

const tll_scheme_t * tll_channel_context_scheme_load(tll_channel_context_t *c, const char *url, int len, int cache)
{
	if (!c || !url) return 0;
	return c->scheme_load(tll::string_view_from_c(url, len), cache);
}

int tll_channel_impl_register(tll_channel_context_t *ctx, const tll_channel_impl_t *impl, const char *name)
{
	if (!ctx) return EINVAL;
	return ctx->reg(impl, name?name:"");
}

int tll_channel_impl_unregister(tll_channel_context_t *ctx, const tll_channel_impl_t *impl, const char *name)
{
	if (!ctx) return EINVAL;
	return ctx->unreg(impl, name?name:"");
}

const tll_channel_impl_t * tll_channel_impl_get(const tll_channel_context_t *ctx, const char *name)
{
	if (!ctx || !name) return nullptr;
	auto r = ctx->lookup(name);
	if (r == nullptr)
		return nullptr;
	if (!std::holds_alternative<const tll_channel_impl_t *>(*r))
		return nullptr;
	return std::get<const tll_channel_impl_t *>(*r);
}

int tll_channel_alias_register(tll_channel_context_t *ctx, const char *name, const char *url, int len)
{
	if (!ctx || !name || !url) return EINVAL;
	auto cfg = tll::Channel::Url::parse(string_view_from_c(url, len));
	if (!cfg) return EINVAL;
	return ctx->alias_reg(name, std::move(*cfg));
}

int tll_channel_alias_register_url(tll_channel_context_t *ctx, const char *name, const tll_config_t *cfg)
{
	if (!ctx || !name || !cfg) return EINVAL;
	return ctx->alias_reg(name, tll::ConstConfig(cfg).copy());
}

int tll_channel_alias_unregister(tll_channel_context_t *ctx, const char *name, const char *url, int len)
{
	if (!ctx || !name || !url) return EINVAL;
	auto cfg = tll::Channel::Url::parse(string_view_from_c(url, len));
	if (!cfg) return EINVAL;
	return ctx->alias_unreg(name, std::move(*cfg));
}


int tll_channel_alias_unregister_url(tll_channel_context_t *ctx, const char *name, const tll_config_t *cfg)
{
	if (!ctx || !name || !cfg) return EINVAL;
	return ctx->alias_unreg(name, tll::ConstConfig(cfg).copy());
}

int tll_channel_module_load_cfg(tll_channel_context_t *ctx, const char *module, const char * symbol, const tll_config_t * cfg)
{
	if (!ctx) return EINVAL;
	return ctx->load(module, symbol ? symbol : "", cfg ? tll::ConstConfig(cfg) : tll::ConstConfig());
}

int tll_channel_module_load(tll_channel_context_t *ctx, const char *module, const char * symbol)
{
	if (!ctx) return EINVAL;
	return ctx->load(module, symbol ? symbol : "", tll::ConstConfig());
}

tll_channel_t * tll_channel_new(tll_channel_context_t * ctx, const char *str, size_t len, tll_channel_t *master, const tll_channel_impl_t *impl)
{
	if (!str) return nullptr;
	return context(ctx)->init(string_view_from_c(str, len), master, impl);
}

tll_channel_t * tll_channel_new_url(tll_channel_context_t * ctx, const tll_config_t *curl, tll_channel_t *master, const tll_channel_impl_t *impl)
{
	if (!curl) return nullptr;
	const tll::Channel::Url url(const_cast<tll_config_t *>(curl));

	return context(ctx)->init(url, master, impl);
}

tll_channel_t * tll_channel_context_t::init(const tll::Channel::Url &_url, tll_channel_t *master, const tll_channel_impl_t * impl)
{
	tll::Channel::Url url = _url.copy();
	if (!impl) {
		impl = lookup(url);
		if (!impl)
			return _log.fail(nullptr, "Channel '{}' not found", url.proto());
	}

	{
		auto name = url.get("name");
		if (!name || *name == "")
			url.set("name", fmt::format("noname-{}", _noname_idx++));
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
		break;
	} while (true);

	if (!*internal && c->internal->name) {
		auto dup = !channels.emplace(c->internal->name, c.get()).second;
		if (dup) {
			_log.warning("Duplicate channel name: {}", c->internal->name);
		} else
			tll_config_set_config(config, c->internal->name, -1, c->internal->config, 0);
	}

	if (c->internal->stat) {
		_log.info("Register channel {} stat", c->internal->name);
		if (!c->internal->stat->name && c->internal->name) {
			_log.info("Set stat name for channel {}", c->internal->name);
			c->internal->stat->name = c->internal->name;
		} else
			_log.info("Stat name for channel {}: '{}'", c->internal->name, c->internal->stat->name);
		stat_list.add(c->internal->stat);
	}

	tll_channel_context_ref(c->context);
	return c.release();
}

tll_state_t tll_channel_state(const tll_channel_t *c) { return c->internal->state; }
const char * tll_channel_name(const tll_channel_t *c) { return c->internal->name; }
unsigned tll_channel_caps(const tll_channel_t *c) { if (c->internal) return c->internal->caps; return 0; }
unsigned tll_channel_dcaps(const tll_channel_t *c) { if (c->internal) return c->internal->dcaps; return 0; }
int tll_channel_fd(const tll_channel_t *c) { if (c->internal) return c->internal->fd; return -1; }
tll_config_t * tll_channel_config(tll_channel_t *c) { if (!c->internal) return 0; tll_config_ref(c->internal->config); return c->internal->config; }
const tll_channel_list_t * tll_channel_children(const tll_channel_t *c) { if (!c->internal) return 0; return c->internal->children; }
tll_channel_context_t * tll_channel_context(const tll_channel_t *c) { return tll_channel_context_ref(c->context); }

void tll_channel_free(tll_channel_t *c)
{
	if (!c) return;
	std::string_view name = tll_channel_name(c);

	auto state = tll_channel_state(c);
	if (state != TLL_STATE_DESTROY && state != TLL_STATE_CLOSED)
		tll_channel_close(c, 1);

	if (c->internal->stat) {
		c->context->stat_list.remove(c->internal->stat);
	}

	if ((tll_channel_caps(c) & caps::Custom) == 0) {
		auto it = c->context->channels.find(name);
		if (it->second == c)
			c->context->channels.erase(it);

		auto cfg = c->context->config.sub(name);
		if (cfg == c->internal->config)
			c->context->config.unlink(name);
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
	auto r = (*c->impl->post)(c, msg, flags);
	if (!r && msg->type == TLL_MESSAGE_DATA && c->internal->stat) {
		auto p = tll::stat::acquire(c->internal->stat);
		if (p) {
			auto f = (tll_channel_stat_t *) p->fields;
			f->tx.update(1);
			f->txb.update(msg->size);
			tll::stat::release(c->internal->stat, p);
		}
	}
	return r;
}

namespace {
inline void suspend(tll_channel_t *c)
{
	unsigned old = c->internal->dcaps;
	if (old & dcaps::Suspend)
		return;
	c->internal->dcaps |= dcaps::Suspend;

	tll_msg_t msg = {TLL_MESSAGE_CHANNEL, TLL_MESSAGE_CHANNEL_UPDATE};
	msg.data = &old;
	msg.size = sizeof(old);
	tll_channel_callback(c->internal, &msg);

	for (auto & i : tll::util::list_wrap(c->internal->children))
		suspend(i.channel);
}

inline void resume(tll_channel_t *c)
{
	if ((c->internal->dcaps & dcaps::SuspendPermanent) != 0)
		return;
	unsigned old = c->internal->dcaps;
	if ((old & dcaps::Suspend) == 0)
		return;

	c->internal->dcaps &= ~dcaps::Suspend;

	tll_msg_t msg = {TLL_MESSAGE_CHANNEL, TLL_MESSAGE_CHANNEL_UPDATE};
	msg.data = &old;
	msg.size = sizeof(old);
	tll_channel_callback(c->internal, &msg);

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
{
	if (!c || !c->impl || !c->impl->open) return EINVAL;
	if (!str || !len) {
		tll::Config cfg;
		return (*c->impl->open)(c, cfg);
	}
	auto r = tll::ConfigUrl::parse_props(string_view_from_c(str, len));
	if (!r) {
		tll::Logger log(fmt::format("tll.channel.{}", tll_channel_name(c)));
		return log.fail(EINVAL, "Invalid property string: {}", r.error());
	}
	return (*c->impl->open)(c, *r);
}

int tll_channel_open_cfg(tll_channel_t *c, const tll_config_t *cfg)
{
	if (!c || !c->impl || !c->impl->open) return EINVAL;
	if (!cfg) {
		tll::Config cfg;
		return (*c->impl->open)(c, cfg);
	}
	return (*c->impl->open)(c, cfg);
}

int tll_channel_close(tll_channel_t *c, int force)
	FORWARD_SAFE(close, c, force);

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
	if (!c || !cb) return EINVAL;
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
	if (!c || !cb) return EINVAL;
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

tll_channel_t * tll_channel_get(const tll_channel_context_t *ctx, const char * name, int len)
{
	if (!ctx) return nullptr;
	return ctx->get(std::string_view(name, len));
}
