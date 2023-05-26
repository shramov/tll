/*
 * Copyright (c) 2018-2021 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

/** @file
 * Configuration subsystem.
 */

#ifndef _TLL_CONFIG_H
#define _TLL_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

/// Config structure
typedef struct tll_config_t tll_config_t;

/**
 * Callback function that is used to list config nodes
 * @param key path to node
 * @param value node config object
 * @param data user data supplied to list function
 * @ref tll_config_list
 * @ref tll_config_browse
 */
typedef int (*tll_config_callback_t)(const char * key, int klen, const tll_config_t *value, void * data);
typedef char * (*tll_config_value_callback_t)(int * len, void * data);
typedef void (*tll_config_value_callback_free_t)(tll_config_value_callback_t cb, void * data);

/** Check if key exists
 *
 * @param path Path string, may be non NULL-terminated.
 * @param plen Length of the string. If set to -1 then length ``path`` must be NULL-terminated.
 */
int tll_config_has(const tll_config_t *, const char * path, int plen);

/// Get subtree
tll_config_t * tll_config_sub(tll_config_t *, const char * path, int plen, int create);
const tll_config_t * tll_config_sub_const(const tll_config_t *, const char * path, int plen);

/// Set plain string value
int tll_config_set(tll_config_t *cfg, const char * path, int plen, const char * value, int vlen);

/// Set callback value
int tll_config_set_callback(tll_config_t *, const char * path, int plen, tll_config_value_callback_t cb, void * user, tll_config_value_callback_free_t deleter);

/// Set config subtree link
int tll_config_set_link(tll_config_t *, const char * path, int plen, const char * dest, int dlen);

/// Unset value in config node, does not affect structure
int tll_config_unset(tll_config_t *, const char * path, int plen);

/// Unlink subtree in config, removes node and all it child. For links - removes link
int tll_config_unlink(tll_config_t *, const char * path, int plen);

/// Unset node value and if it has no child nodes - remove it. For links - removes link
int tll_config_remove(tll_config_t *, const char * path, int plen);

/// Delete node, deprecated variant. Always recursive
__attribute__((deprecated)) int tll_config_del(tll_config_t *, const char * path, int plen, int recursive);

/** Set config subtree
 *
 * @param c Pointer to config object that will be set as subtree
 * @param consume Consume config object without increasing reference count if not zero, increase reference count if zero
 */
int tll_config_set_config(tll_config_t *, const char * path, int plen, tll_config_t *c, int consume);

/** Merge other config, moving nodes if possible
 * @param dest Config to merge data into
 * @param src Source config
 * @param overwrite overwrite existing data in dest if true
 *
 * @note Nodes are moved from ``src`` to ``dest`` so source config may be not usable after this operation.
 */
int tll_config_merge(tll_config_t * dest, tll_config_t * src, int overwrite);

/// Get value
int tll_config_get(const tll_config_t *, const char * path, int plen, char * value, int * vlen);
/// Get allocated copy of value from config, call @ref tll_config_value_free after it's not needed
char * tll_config_get_copy(const tll_config_t *, const char * path, int plen, int * vlen);
/// Free value returned from tll_config_get_copy
void tll_config_value_free(const char * value);
/// Duplicate string, should be freed with tll_config_value_free
char * tll_config_value_dup(const char * value, int vlen);

/// List child nodes
int tll_config_list(const tll_config_t *, tll_config_callback_t cb, void * data);

/// List child nodes matching mask
int tll_config_browse(const tll_config_t *, const char * mask, int mlen, tll_config_callback_t cb, void * data);

/// Check if config has value (string or callback)
int tll_config_value(const tll_config_t *);

/// User defined function that loads config from ``path``
typedef tll_config_t * (*tll_config_load_t)(const char * path, int plen, void * data);
/// Register new load function with protocol ``prefix``
int tll_config_load_register(const char * prefix, int plen, tll_config_load_t cb, void * data);
/// Unregister load function with protocol ``prefix``
int tll_config_load_unregister(const char * prefix, int plen, tll_config_load_t cb, void * data);

/// Create new empty config
tll_config_t * tll_config_new(void);
/// Create copy of config
tll_config_t * tll_config_copy(const tll_config_t *);
/// Create new config filled with values loaded from ``path``
tll_config_t * tll_config_load(const char * path, int plen);
/// Create new config filled with values loaded from ``data`` with format determined by ``proto``
tll_config_t * tll_config_load_data(const char * proto, int plen, const char * data, int dlen);

/// Load additional configs specified in ``path`` subtree
int tll_config_process_imports(tll_config_t *, const char * path, int plen);

/// Increase reference count
const tll_config_t * tll_config_ref(const tll_config_t *);
/// Decrease reference count
void tll_config_unref(const tll_config_t *);


/**
 * Get parent config object
 * @return Reference to parent config or NULL if it's top-level object
 */
tll_config_t * tll_config_parent(tll_config_t *);

/**
 * Get root config object
 * @return NULL only if cfg is NULL
 */
tll_config_t * tll_config_root(tll_config_t *);

/// Detach config from parent
tll_config_t * tll_config_detach(tll_config_t *, const char * path, int plen);

#ifdef __cplusplus
} // extern "C"
#endif

#ifdef __cplusplus
#include <cstring>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>

#include "tll/util/conv.h"
#include "tll/util/props.h"
#include "tll/util/result.h"

namespace tll {

class config_string
{
	const char * _data = nullptr;
	size_t _size = 0;
public:
	explicit config_string(const char * data) : _data(data), _size(strlen(data)) {}
	explicit config_string(const char * data, size_t size) : _data(data), _size(size) {}

	config_string(const config_string & rhs) : _data(tll_config_value_dup(rhs._data, rhs._size)), _size(rhs._size) {}
	config_string(config_string && rhs) { std::swap(_data, rhs._data); std::swap(_size, rhs._size); }

	~config_string() { tll_config_value_free(_data); }

	std::string_view string() { return { _data, _size }; }
	const std::string_view string() const { return { _data, _size }; }

	operator std::string_view () { return string(); }
	operator const std::string_view () const { return string(); }

	bool operator == (std::string_view rhs) const { return string() == rhs; }
	bool operator != (std::string_view rhs) const { return string() != rhs; }

	operator std::string () const { return { _data, _size }; }

	size_t size() const { return _size; }
	const char * data() const { return _data; }
};

class optional_config_string
{
	std::string_view _data = { nullptr, 0 };
 public:
	explicit optional_config_string(const char * data, size_t size) : _data(data, size) {}
	optional_config_string() {}
	optional_config_string(const std::nullopt_t &) {}
	optional_config_string(optional_config_string && rhs) { std::swap(_data, rhs._data); }

	~optional_config_string() { if (_data.data()) tll_config_value_free(_data.data()); }

	std::string_view operator * () const { return _data; }
	const std::string_view * operator -> () const { return &_data; }
	operator bool () const { return _data.data() != nullptr; }

	std::string_view value_or(std::string_view s) const { if (*this) return _data; return s; }
};

class Config;
class ConfigUrl;

template <bool Const>
class ConfigT
{
 protected:
	friend class ConfigT<!Const>;

	using config_t = std::conditional_t<Const, const tll_config_t, tll_config_t>;
	config_t *_cfg = nullptr;

	struct consume_t {};
	explicit ConfigT(config_t * cfg, const consume_t &n) : _cfg(cfg) {}

 public:
	typedef std::string string_type;

	explicit ConfigT(config_t * cfg) : _cfg(cfg) { tll_config_ref(cfg); }

	ConfigT() : _cfg(tll_config_new()) {}

	ConfigT(const ConfigT &cfg) : _cfg(cfg._cfg) { tll_config_ref(_cfg); }
	ConfigT(const ConfigT<!Const> &cfg) : _cfg((config_t *) cfg._cfg) { tll_config_ref(_cfg); }
	ConfigT(ConfigT &&cfg) { std::swap(_cfg, cfg._cfg); }
	ConfigT(ConfigT<!Const> &&cfg) : _cfg((config_t *) cfg._cfg) { cfg._cfg = nullptr; }

	static ConfigT consume(config_t * cfg) { return ConfigT(cfg, consume_t()); }

	~ConfigT()
	{
		if (_cfg) tll_config_unref(_cfg);
		_cfg = nullptr;
	}

	ConfigT & operator = (ConfigT rhs) { std::swap(_cfg, rhs._cfg); return *this; }

	static std::optional<ConfigT> load(std::string_view path)
	{
		auto c = tll_config_load(path.data(), path.size());
		if (!c) return std::nullopt;
		return ConfigT::consume(c);
	}

	static std::optional<ConfigT> load(std::string_view proto, std::string_view data)
	{
		auto c = tll_config_load_data(proto.data(), proto.size(), data.data(), data.size());
		if (!c) return std::nullopt;
		return ConfigT::consume(c);
	}

	Config copy() const;

	std::optional<ConfigT<true>> sub(std::string_view path) const
	{
		auto c = tll_config_sub_const(_cfg, path.data(), path.size());
		if (!c) return std::nullopt;
		return ConfigT<true>::consume(c);
	}

	operator config_t * () { return _cfg; }
	operator const config_t * () const { return _cfg; }

	bool has(std::string_view path) const { return tll_config_has(_cfg, path.data(), path.size()); }

	bool value() const { return tll_config_value(_cfg); }

	optional_config_string get() const
	//std::optional<config_string> get() const
	{
		if (!value()) return std::nullopt;
		int len = 0;
		auto ptr = tll_config_get_copy(_cfg, nullptr, 0, &len);
		if (!ptr)
			return std::nullopt;
		return optional_config_string(ptr, len);
	}

	optional_config_string get(std::string_view path) const
	//std::optional<config_string> get(std::string_view path) const
	{
		auto c = sub(path);
		if (!c) return std::nullopt;
		return c->get();
	}

	template <typename T>
	std::map<std::string, T> listT() const
	{
		struct cb_t {
			static int cb(const char *key, int klen, const tll_config_t *value, void * data)
			{
				auto l = static_cast<std::map<std::string, ConfigT> *>(data);
				l->emplace(std::string_view(key, klen), T((tll_config_t *) value));
				return 0;
			}
		};
		std::map<std::string, T> map;
		tll_config_list(_cfg, cb_t::cb, &map);
		return map;
	}

	std::map<std::string, ConfigT> list() const { return listT<ConfigT>(); }

	template <typename T>
	std::map<std::string, T> browseT(std::string_view mask, bool dir = false) const
	{
		struct cb_t {
			bool dir = false;
			std::map<std::string, T> map;
			static int cb(const char *key, int klen, const tll_config_t *value, void * data)
			{
				auto l = static_cast<cb_t *>(data);
				if (!tll_config_value(value) && !l->dir) return 0;
				l->map.emplace(std::string_view(key, klen), T((tll_config_t *) value));
				return 0;
			}
		} cb = { dir };
		tll_config_browse(_cfg, mask.data(), mask.size(), cb_t::cb, &cb);
		return std::move(cb.map);
	}

	std::map<std::string, ConfigT> browse(std::string_view mask, bool dir = false) const
	{
		return browseT<ConfigT>(mask, dir);
	}

	template <typename T>
	result_t<T> getT(std::string_view key) const;

	template <typename T>
	result_t<T> getT(std::string_view key, const T& def) const;

	template <typename T>
	result_t<T> getT(std::string_view key, const T& def, const std::map<std::string_view, T> m) const
	{
		return tll::getter::getT<ConfigT, T>(*this, key, def, m);
	}
};

using ConstConfig = ConfigT<true>;

class Config : public ConfigT<false>
{
 public:
	explicit Config(tll_config_t * cfg) : ConfigT<false>(cfg) {}
	explicit Config(config_t * cfg, const consume_t &n) : ConfigT<false>(cfg, n) {}

	Config() {}

	//Config(const Config & cfg) = delete;
	Config(const Config & cfg) : ConfigT<false>(cfg) {}
	Config(Config && cfg) : ConfigT<false>(std::move(cfg)) {}
	Config(const ConfigT<false> & cfg) : ConfigT<false>(cfg) {}
	Config(ConfigT<false> && cfg) : ConfigT<false>(std::move(cfg)) {}

	Config & operator = (const Config &rhs) { tll_config_unref(_cfg); _cfg = rhs._cfg; tll_config_ref(_cfg); return *this; }
	Config & operator = (Config &&rhs) { std::swap(_cfg, rhs._cfg); return *this; }

	static std::optional<Config> load(std::string_view path) { return ConfigT<false>::load(path); }
	static std::optional<Config> load(std::string_view proto, std::string_view data) { return ConfigT<false>::load(proto, data); }

	int set(std::string_view value) { return tll_config_set(_cfg, nullptr, 0, value.data(), value.size()); }
	int set(tll_config_value_callback_t cb, void * user) { return tll_config_set_callback(_cfg, nullptr, 0, cb, user, nullptr); }

	int set(std::string_view path, std::string_view value) { return tll_config_set(_cfg, path.data(), path.size(), value.data(), value.size()); }
	int set(std::string_view path, ConfigT & cfg) { return tll_config_set_config(_cfg, path.data(), path.size(), cfg, 0); }
	int set(std::string_view path, tll_config_t * cfg) { return tll_config_set_config(_cfg, path.data(), path.size(), cfg, 0); }
	int set(std::string_view path, tll_config_value_callback_t cb, void * user) { return tll_config_set_callback(_cfg, path.data(), path.size(), cb, user, nullptr); }

	template <typename T> int setT(const T &v) { return set(tll::conv::to_string(v)); }
	template <typename T> int setT(std::string_view path, const T &v) { return set(path, tll::conv::to_string(v)); }

	template <typename V>
	int set_ptr(const V * ptr)
	{
		return tll_config_set_callback(_cfg, nullptr, 0, _to_string<V>, (void *) ptr, nullptr);
	}

	template <typename V>
	int set_ptr(std::string_view path, const V * ptr)
	{
		return tll_config_set_callback(_cfg, path.data(), path.size(), _to_string<V>, (void *) ptr, nullptr);
	}

	int link(std::string_view path, std::string_view dest) { return tll_config_set_link(_cfg, path.data(), path.size(), dest.data(), dest.size()); }

	int unset(std::string_view path) { return tll_config_unset(_cfg, path.data(), path.size()); }
	int unset() { return tll_config_unset(_cfg, nullptr, 0); }

	int unlink(std::string_view path) { return tll_config_unlink(_cfg, path.data(), path.size()); }

	int remove(std::string_view path) { return tll_config_remove(_cfg, path.data(), path.size()); }

	int merge(ConfigT & cfg, bool overwrite = true) { return tll_config_merge(_cfg, cfg, overwrite); }
	int process_imports(std::string_view path) { return tll_config_process_imports(_cfg, path.data(), path.size()); }

	std::optional<Config> sub(std::string_view path, bool create = false)
	{
		auto c = tll_config_sub(_cfg, path.data(), path.size(), create);
		if (!c) return std::nullopt;
		return Config(c, consume_t());
	}

	std::optional<ConstConfig> sub(std::string_view path) const { return ConfigT<false>::sub(path); }

	std::map<std::string, Config> list() { return listT<Config>(); }
	std::map<std::string, ConstConfig> list() const { return listT<ConstConfig>(); }

	std::map<std::string, Config> browse(std::string_view mask, bool dir = false)
	{
		return browseT<Config>(mask, dir);
	}

	std::map<std::string, ConstConfig> browse(std::string_view mask, bool dir = false) const
	{
		return browseT<ConstConfig>(mask, dir);
	}
private:
	template <typename V>
	static char * _to_string(int * len, void * data)
	{
		auto s = tll::conv::to_string<V>(*(const V *) data);
		*len = s.size();
		return strdup(s.c_str());
	}
};

template <bool Const>
Config ConfigT<Const>::copy() const
{
	auto r = tll_config_copy(*this);
	return Config::consume(r);
}

class ConfigUrl : public Config
{
public:
	using Config::Config;

	ConfigUrl copy() const { return ConfigUrl(Config::copy()); }

	std::string proto() const { return std::string(get("tll.proto").value_or("")); }
	std::string host() const { return std::string(get("tll.host").value_or("")); }

	void proto(std::string_view v) { set("tll.proto", v); }
	void host(std::string_view v) { set("tll.host", v); }

	static result_t<ConfigUrl> parse(std::string_view s)
	{
		auto sep = s.find("://");
		if (sep == s.npos)
			return tll::error("No :// found in url");
		auto proto = s.substr(0, sep);
		if (proto.size() == 0)
			return tll::error("Empty protocol in url");
		auto hsep = s.find_first_of(';', sep + 3);
		auto host = s.substr(sep + 3, hsep - (sep + 3));

		ConfigUrl cfg;

		if (hsep == s.npos) {
			cfg.set("tll.proto", proto);
			cfg.set("tll.host", host);
			return cfg;
		}

		auto r = parse_props(s.substr(hsep + 1));
		if (!r)
			return tll::error(r.error());

		if (r->has("tll.proto")) return tll::error("Duplicate key: tll.proto");
		if (r->has("tll.host")) return tll::error("Duplicate key: tll.host");
		cfg = *r;
		cfg.set("tll.proto", proto);
		cfg.set("tll.host", host);
		return cfg;
	}

	static result_t<Config> parse_props(std::string_view s)
	{
		Config r;
		for (const auto i : split<';'>(s)) {
			if (i.empty()) continue;
			auto sep = i.find_first_of('=');
			if (sep == i.npos)
				return tll::error("Missing '='");
			auto key = i.substr(0, sep);
			auto value = i.substr(sep + 1);
			if (r.has(key))
				return tll::error("Duplicate key: " + std::string(key));
			r.set(key, value);
		}
		return r;
	}
};

/**
 * Get Url from config
 *
 *  Url can be specified in 3 different forms:
 *   - String: key: 'proto://;params...'
 *   - Unpacked: key: { tll.proto: proto, params... }
 *   - Mixed: key: { url: 'proto://;params...', extra params... }
 *
 *  Also for transition period legacy variant is supported:
 *    - key: 'proto://;params...', key: { extra params... }
 */
namespace _config {
template <bool Const>
result_t<ConfigUrl> _get_url(const ConfigT<Const> &cfg, std::string_view key)
{
	auto sub = cfg.sub(key);
	if (!sub)
		return tll::error("Url not found at '" + std::string(key) + "'");

	if (auto proto = sub->get("tll.proto"); proto) // Unpacked variant
		return sub->copy();

	auto url = sub->get("url");
	auto str = sub->get();
	if (url && str)
		return tll::error("Both " + std::string(key) + " and " + std::string(key) + ".url found");

	ConfigUrl result;
	if (url) {
		auto r = ConfigUrl::parse(*url);
		if (!r)
			return tll::error(r.error());
		result.merge(*r);
	} else if (str) {
		auto r = ConfigUrl::parse(*str);
		if (!r)
			return tll::error(r.error());
		result.merge(*r);
	}

	for (auto & [k,c] : sub->browse("**")) {
		auto v = c.get();
		if (k == "url" || !v)
			continue;
		if (result.has(k))
			return tll::error("Duplicate key " + std::string(k));
		result.set(k, *v);
	}

	return result;
}
}

template <bool Const>
template <typename T>
result_t<T> ConfigT<Const>::getT(std::string_view key) const
{
	if constexpr (std::is_same_v<T, ConfigUrl>)
		return _config::_get_url(*this, key);
	else
		return tll::getter::getT<ConfigT<Const>, T>(*this, key);
}

template <bool Const>
template <typename T>
result_t<T> ConfigT<Const>::getT(std::string_view key, const T& def) const
{
	if constexpr (std::is_same_v<T, ConfigUrl>) {
		auto c = sub(key);
		if (!c)
			return def;
		return _config::_get_url(*this, key);
	} else
		return tll::getter::getT<ConfigT<Const>, T>(*this, key, def);
}


template <>
struct conv::dump<ConfigUrl> : public conv::to_string_buf_from_string<ConfigUrl>
{
	static std::string to_string(const ConfigUrl &url)
	{
		std::string r = std::string(url.proto()) + "://" + std::string(url.host());
		for (auto & p : url.browse("**")) {
			if (p.first == "tll.proto" || p.first == "tll.host")
				continue;
			r += ";" + std::string(p.first) + "=" + std::string(p.second.get().value_or(""));
		}
		return r;
	}
};

} // namespace tll
#endif

#endif//_TLL_CONFIG_H
