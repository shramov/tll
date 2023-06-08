/*
 * Copyright (c) 2018-2021 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#include "config/config.h"
#include "config/yaml.h"

#include "tll/config.h"
#include "tll/logger.h"
#include "tll/util/bin2ascii.h"
#include "tll/util/refptr.h"
#include "tll/util/string.h"
#include "tll/util/url.h"
#include "tll/util/zlib.h"

#include <algorithm>
#include <cstring>

using tll::string_view_from_c;

struct context_t {
	std::map<std::string, std::pair<tll_config_load_t, void *>, std::less<>> map;

	context_t()
	{
		map.emplace("url", std::make_pair(load_url, nullptr));
		map.emplace("props", std::make_pair(load_props, nullptr));
		map.emplace("yaml", std::make_pair(load_yaml, nullptr));
		map.emplace("yamls", std::make_pair(load_yamls, nullptr));
		map.emplace("yamls+gz", std::make_pair(load_yamls_gz, nullptr));
	}

	static tll_config_t * load_props(const char * data, int len, void *)
	{
		auto s = string_view_from_c(data, len);
		auto r = tll::ConfigUrl::parse_props(s);
		if (!r)
			return tll::Logger("tll.config").fail(nullptr, "Invalid property string {}: {}", s, r.error());
		tll_config_ref(*r);
		return *r;
	}

	static tll_config_t * load_url(const char * data, int len, void *)
	{
		auto url = string_view_from_c(data, len);
		auto r = tll::ConfigUrl::parse(url);
		if (!r)
			return tll::Logger("tll.config").fail(nullptr, "Invalid url {}: {}", url, r.error());
		tll_config_ref(*r);
		return *r;
	}

	static tll_config_t * load_yaml(const char * data, int len, void *)
	{
		return yaml_load(tll::string_view_from_c(data, len));
	}

	static tll_config_t * load_yamls(const char * data, int len, void *)
	{
		return yaml_load_data(tll::string_view_from_c(data, len));
	}

	static tll_config_t * load_yamls_gz(const char * d, int len, void *)
	{
		tll::Logger _log = { "tll.config" };
		auto data = tll::string_view_from_c(d, len);
		auto zbin = tll::util::b64_decode(data);
		if (!zbin) return _log.fail(nullptr, "Fail to load: invalid base64 data");
		auto bin = tll::zlib::decompress(*zbin);
		if (!bin) return _log.fail(nullptr, "Fail to load: invalid zlib data");
		return yaml_load_data(std::string_view(bin->data(), bin->size()));
	}

	int reg(std::string_view prefix, tll_config_load_t cb, void *data)
	{
		auto it = map.find(prefix);
		if (it != map.end())
			return EEXIST;
		map.emplace(prefix, std::make_pair(cb, data));
		return 0;
	}

	int unreg(std::string_view prefix, tll_config_load_t cb, void *data)
	{
		auto it = map.find(prefix);
		if (it == map.end())
			return ENOENT;
		if (it->second != std::make_pair(cb, data))
			return EINVAL;
		map.erase(it);
		return 0;
	}

	tll_config_t * load(std::string_view s)
	{
		tll::Logger _log = { "tll.config" };
		auto sep = s.find("://");
		if (sep == s.npos)
			return _log.fail(nullptr, "Invalid url {}: no :// found", s);
		return load(s.substr(0, sep), s.substr(sep + 3));
	}

	tll_config_t * load(std::string_view proto, std::string_view data)
	{
		tll::Logger _log = { "tll.config" };
		auto it = map.find(proto);
		if (it == map.end())
			return _log.fail(nullptr, "Unknown config protocol: {}", proto);
		return (*it->second.first)(data.data(), data.size(), it->second.second);
	}
};

static context_t _context;

int tll_config_load_register(const char * prefix, int plen, tll_config_load_t cb, void *data)
{
	return _context.reg(string_view_from_c(prefix, plen), cb, data);
}

int tll_config_load_unregister(const char * prefix, int plen, tll_config_load_t cb, void *data)
{
	return _context.unreg(string_view_from_c(prefix, plen), cb, data);
}

tll_config_t * tll_config_new() { return (new tll_config_t)->ref(); }
tll_config_t * tll_config_copy(const tll_config_t *cfg)
{
	if (cfg == nullptr)
		return nullptr;
	return (new tll_config_t(*cfg, 0, cfg->parent == nullptr))->ref();
}

tll_config_t * tll_config_load(const char * path, int plen)
{
	return _context.load(string_view_from_c(path, plen));
}

tll_config_t * tll_config_load_data(const char * proto, int plen, const char * data, int len)
{
	if (proto == nullptr || data == nullptr)
		return nullptr;
	return _context.load(string_view_from_c(proto, plen), string_view_from_c(data, len));
}

int tll_config_process_imports(tll_config_t *cfg, const char * path, int plen)
{
	return cfg->process_imports(string_view_from_c(path, plen));
}

const tll_config_t * tll_config_ref(const tll_config_t * c) { if (c) c->ref(); return c; }
void tll_config_unref(const tll_config_t * c) { if (c) c->unref(); }

void tll_config_free(tll_config_t * c) { tll_config_unref(c); }

tll_config_t * tll_config_sub(tll_config_t *c, const char *path, int plen, int create)
{
	if (!c) return nullptr;
	return c->find(string_view_from_c(path, plen), create).release();
}

const tll_config_t * tll_config_sub_const(const tll_config_t *c, const char *path, int plen)
{
	if (!c) return nullptr;
	return c->find(string_view_from_c(path, plen)).release();
}

int tll_config_value(const tll_config_t *c)
{
	if (!c) return EINVAL;
	auto lock = c->rlock();
	return c->value();
}

int tll_config_has(const tll_config_t *cfg, const char *path, int plen)
{
	if (!cfg) return EINVAL;
	if (path == nullptr)
		return cfg->value();
	auto v = cfg->find(string_view_from_c(path, plen));
	if (!v) return false;
	auto lock = v->rlock();
	return v->value();
}

int tll_config_del(tll_config_t *cfg, const char *path, int plen, int recursive)
{
	return tll_config_unlink(cfg, path, plen);
}

int tll_config_unlink(tll_config_t *cfg, const char *path, int plen)
{
	if (!cfg) return EINVAL;
	if (!path) return EINVAL;
	return cfg->unlink(string_view_from_c(path, plen));
}

int tll_config_remove(tll_config_t *cfg, const char *path, int plen)
{
	if (!cfg) return EINVAL;
	if (!path) return EINVAL;
	return cfg->remove(string_view_from_c(path, plen));
}

int tll_config_set(tll_config_t *cfg, const char * path, int plen, const char * value, int vlen)
{
	if (!cfg) return EINVAL;
	auto v = string_view_from_c(value, vlen);
	if (path == nullptr)
		return cfg->set(v);
	auto sub = cfg->find(string_view_from_c(path, plen), 1);
	if (!sub) return EINVAL;
	return sub->set(v);
}

int tll_config_set_callback(tll_config_t *cfg, const char * path, int plen, tll_config_value_callback_t cb, void * user, tll_config_value_callback_free_t deleter)
{
	if (!cfg) return EINVAL;
	if (path == nullptr)
		return cfg->set(cb, user, deleter);
	auto sub = cfg->find(string_view_from_c(path, plen), 1);
	if (!sub) return EINVAL;
	return sub->set(cb, user, deleter);
}

int tll_config_set_link(tll_config_t *cfg, const char * path, int plen, const char * dest, int dlen)
{
	if (!cfg) return EINVAL;
	auto sub = cfg->find(string_view_from_c(path, plen), 1);
	if (!sub) return EINVAL;
	return sub->set(std::filesystem::path(string_view_from_c(dest, dlen)));
}

int tll_config_unset(tll_config_t *cfg, const char * path, int plen)
{
	if (!cfg) return EINVAL;
	if (path) {
		auto sub = cfg->find(string_view_from_c(path, plen), 0);
		if (!sub) return ENOENT;
		return sub->set();
	} else
		return cfg->set();
}

int tll_config_set_config(tll_config_t *cfg, const char *path, int plen, tll_config_t *ptr, int consume)
{
	if (!cfg) return EINVAL;
	if (!ptr) return EINVAL;
	return cfg->set(string_view_from_c(path, plen), ptr, consume);
}

int tll_config_merge(tll_config_t *c, tll_config_t *src, int overwrite)
{
	if (!c) return EINVAL;
	return c->merge(src, overwrite);
}

namespace {
inline int _get_sv(std::string_view v, char * value, int * vlen)
{
	if (!value) {
		*vlen = v.size() + 1;
		return EAGAIN;
	}
	if (*vlen < (ssize_t) (v.size() + 1)) {
		*vlen = v.size() + 1;
		return EAGAIN;
	}
	memcpy(value, v.data(), v.size());
	value[v.size()] = '\0';
	*vlen = v.size();
	return 0;
}
}

int tll_config_get(const tll_config_t *c, const char *path, int plen, char *value, int * vlen)
{
	if (!c) return EINVAL;
	refptr_t<const tll_config_t> cfg(c);
	if (path != 0) {
		cfg = c->find(string_view_from_c(path, plen));
		if (!cfg.get()) return ENOENT;
	} else if (!c->value())
		return ENOENT;
	if (!vlen) return EINVAL;
	auto lock = cfg->rlock();
	cfg = tll_config_t::_lookup_link(cfg.get());
	if (!cfg.get())
		return ENOENT;
	c = cfg.get();
	lock = cfg->rlock();
	if (std::holds_alternative<std::string>(c->data)) {
		auto & v = std::get<std::string>(c->data);
		return _get_sv(v, value, vlen);
	} else if (std::holds_alternative<tll_config_t::cb_pair_t>(c->data)) {
		auto v = std::get<tll_config_t::cb_pair_t>(c->data);
		lock.unlock();
		int slen = 0;
		std::unique_ptr<char, decltype(&free)> str(v(&slen), &free);
		return _get_sv(std::string_view(str.get(), slen), value, vlen);
	} else
		return ENOENT;
	return 0;
}

char * tll_config_get_copy(const tll_config_t *c, const char *path, int plen, int * vlen)
{
	if (!c) return nullptr;
	refptr_t<const tll_config_t> cfg(c);
	if (path != nullptr) {
		cfg = c->find(string_view_from_c(path, plen));
		if (!cfg.get()) return nullptr;
	}
	auto r = cfg->get();
	if (!r)
		return nullptr;
	if (vlen)
		*vlen = r->size();
	return (char *) r.release();
}

void tll_config_value_free(const char * ptr)
{
	if (ptr) free((char *) ptr);
}

char * tll_config_value_dup(const char * str, int len)
{
	if (!str) return nullptr;
	auto s = string_view_from_c(str, len);
	auto r = (char *) malloc(s.size() + 1);
	memcpy(r, s.data(), s.size());
	r[s.size()] = 0;
	return r;
}

int tll_config_list(const tll_config_t * c, tll_config_callback_t cb, void *data)
{
	if (!c) return EINVAL;
	auto lock = c->rlock();
	for (auto &[k, v] : c->kids) {
		if (cb(k.c_str(), k.size(), v.get(), data))
			break;
	}
	return 0;
}

int tll_config_browse(const tll_config_t * c, const char * mask, int mlen, tll_config_callback_t cb, void *data)
{
	if (!c) return EINVAL;
	//if (c->value()) return 0;
	return c->browse(string_view_from_c(mask, mlen), cb, data);
}

int tll_config_t::process_imports(std::string_view path, const std::list<std::string_view> & parents)
{
	tll::Logger _log = { "tll.config" };
	auto icfg = find(path, false);
	if (!icfg)
		return 0;
	std::list<std::string> imports;
	{
		auto lock = icfg->rlock();
		for (auto &[_, i] : icfg->kids) {
			if (std::holds_alternative<std::string>(i->data)) {
				imports.push_front(std::get<std::string>(i->data));
			} else if (std::holds_alternative<tll_config_t::cb_pair_t>(i->data)) {
				auto cb = std::get<tll_config_t::cb_pair_t>(i->data);
				int slen = 0;
				std::unique_ptr<char, decltype(&free)> str(cb(&slen), &free);
				if (!str)
					continue;
				imports.push_front(std::string(str.get(), slen));
			} else
				continue;
		}
	}


	for (auto & i : imports) {
		if (std::find(parents.begin(), parents.end(), i) != parents.end())
			return _log.fail(EINVAL, "Detected recursive imports: {}", i);

		std::list<std::string_view> copy(parents);
		copy.push_back(i);

		auto cfg = tll::Config::load(i);
		if (!cfg)
			return _log.fail(EINVAL, "Failed to load import {}", i);
		tll_config_t * c = *cfg;
		if (c->process_imports(path, copy))
			return _log.fail(EINVAL, "Failed to process imports on {}", i);
		if (merge(c, false))
			return _log.fail(EINVAL, "Failed to merge import {}", i);
	}
	return 0;
}
