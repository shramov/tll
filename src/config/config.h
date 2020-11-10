/*
 * Copyright (c) 2018-2020 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#include "tll/config.h"
#include "tll/util/string.h"
#include "tll/util/refptr.h"

#include <atomic>
#include <cstring>
#include <map>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <variant>

using tll::util::refptr_t;

struct tll_config_t : public tll::util::refbase_t<tll_config_t, 0>
{
	typedef std::map<std::string, refptr_t<tll_config_t>, std::less<>> map_t;
	typedef tll::split_helperT<'.'>::iterator path_iterator_t;
	typedef std::pair<tll_config_value_callback_t, void *> cb_pair_t;

	typedef std::unique_lock<std::shared_mutex> wlock_t;
	typedef std::shared_lock<std::shared_mutex> rlock_t;
	mutable std::shared_mutex _lock;

	tll_config_t * parent = 0;
	map_t kids;
	std::variant<std::monostate, std::string, cb_pair_t> data = {};

	tll_config_t(tll_config_t *p = nullptr) : parent(p) { /*printf("  new %p\n", this);*/ }
	~tll_config_t() { /*printf("  del %p\n", this);*/ }

	tll_config_t(const tll_config_t &cfg)
	{
		auto lock = cfg.rlock();
		data = cfg.data;
		for (auto & k : cfg.kids) {
			auto it = kids.emplace(k.first, new tll_config_t(*k.second)).first;
			it->second->parent = this;
		}
	}

	bool value() const { return !std::holds_alternative<std::monostate>(data); }

	template <typename Cfg>
	static inline refptr_t<Cfg> _lookup(Cfg *cfg, path_iterator_t &path, const path_iterator_t end)
	{
		refptr_t<Cfg> r(cfg);
		for (; path != end; ++path) {
			rlock_t lock(r->_lock);
			auto it = r->kids.find(*path);
			if (it == r->kids.end()) return r;
			r = it->second.get();
		}
		return r;
	}


	inline refptr_t<const tll_config_t> lookup(path_iterator_t &path, const path_iterator_t end) const
	{
		return _lookup<const tll_config_t>(this, path, end);
	}

	inline refptr_t<tll_config_t> lookup(path_iterator_t &path, const path_iterator_t end)
	{
		return _lookup<tll_config_t>(this, path, end);
	}

	rlock_t rlock() const { return rlock_t(_lock); }
	wlock_t wlock() { return wlock_t(_lock); }

	/*
	inline refptr_t<tll_config_t> lookup(path_iterator_t &path, const path_iterator_t end)
	{
		if (path == end) return {this};
		if (!std::holds_alternative<map_t>(data)) return {this};
		auto & map = std::get<map_t>(data);
		auto it = map.find(*path);
		if (it == map.end()) return {this};
		++path;
		return it->second->lookup(path, end);
	}
	*/

	int set()
	{
		data.emplace<std::monostate>();
		return 0;
	}

	int set(std::string_view value)
	{
		auto lock = wlock();
		data = std::string(value);
		return 0;
	}

	int set(tll_config_value_callback_t cb, void * user)
	{
		auto lock = wlock();
		data = std::make_pair(cb, user);
		return 0;
	}

	int set(std::string_view path, tll_config_t *cfg, bool consume)
	{
		//printf("  set %p\n", this);
		auto s = tll::split<'.'>(path);
		auto pi = s.begin();
		auto v = find(pi, --s.end(), true);

		auto lock = v->wlock();
		if (v->kids.find(*pi) != v->kids.end()) return EEXIST;

		v->kids.emplace(*pi, cfg);
		cfg->parent = v.get();
		if (consume)
			cfg->unref();
		return 0;
	}

	refptr_t<const tll_config_t> find(std::string_view path) const
	{
		auto s = tll::split<'.'>(path);
		auto pi = s.begin();
		auto v = lookup(pi, s.end());
		if (pi != s.end()) return {nullptr};
		return v;
	}

	refptr_t<tll_config_t> find(std::string_view path, bool create)
	{
		auto s = tll::split<'.'>(path);
		auto pi = s.begin();
		return find(pi, s.end(), create);
	}

	refptr_t<tll_config_t> find(path_iterator_t &pi, const path_iterator_t end, bool create)
	{
		auto v = lookup(pi, end);
		if (pi == end) return v;
		if (!create) return {nullptr};

		auto lock = v->wlock();
		for (; pi != end; pi++) {
			auto it = v->kids.emplace(*pi, new tll_config_t(v.get())).first;
			v = it->second.get();
			lock = v->wlock();
			//printf("  add %p %d\n", v.get(), v->_ref.load());
		}
		return v;
	}

	typedef std::vector<std::string_view> mask_vector_t;
	/*
	 * NOTE: This method is called with external read lock
	 */
	int browse(const mask_vector_t &mask, mask_vector_t::const_iterator start, const std::string &prefix, tll_config_callback_t cb, void *user) const
	{
		if (start == mask.end())
			return 0;
		auto & map = kids;
		auto & m = *start;
		bool filler = m == "**";
		if (m == "*" || m == "**") {
			auto next = start + 1;
			for (auto & i : map) {
				std::string p = prefix + i.first;
				refptr_t<const tll_config_t> ptr = i.second.get();
				auto lock = i.second->rlock();
				if (next == mask.end()) {
					if (int r = (*cb)(p.c_str(), p.size(), ptr.get(), user))
						return r;
				}
				if (int r = ptr->browse(mask, next, p + ".", cb, user))
					return r;
				if (filler) {
					if (int r = ptr->browse(mask, start, p + ".", cb, user))
						return r;
				}
			}
		} else {
			auto di = map.find(m);
			if (di == map.end()) return 0;
			std::string p = prefix + std::string(m);
			rlock_t lock(di->second->_lock);
			if (++start == mask.end())
				return (*cb)(p.c_str(), p.size(), di->second.get(), user);
			return di->second->browse(mask, start, p + ".", cb, user);
		}
		return 0;
	}

	int browse(std::string_view mask, tll_config_callback_t cb, void *user) const
	{
		std::vector<std::string_view> mv;
		bool dstar = false;
		for (auto s : tll::split<'.'>(mask)) {
			mv.push_back(s);
			if (s == "**") {
				if (dstar)
					return EINVAL;
				dstar = true;
			}
		}
		std::string prefix;
		auto i = mv.begin();
		refptr_t<const tll_config_t> ptr = this;
		for (; i != mv.end(); i++) {
			if (ptr->value()) return 0;
			if (*i == "*" || *i == "**") break;
			auto lock = ptr->rlock();
			auto di = ptr->kids.find(*i);
			if (di == ptr->kids.end()) return 0;
			prefix += std::string(*i) + ".";
			ptr.reset(di->second.get());
		}
		auto lock = ptr->rlock();
		return ptr->browse(mv, i, prefix, cb, user);
	}

	int merge(tll_config_t * rhs, bool overwrite)
	{
		auto lock = wlock();
		auto rhs_lock = rhs->wlock();

		if (rhs->value() && overwrite)
			data = rhs->data;

		auto & lmap = kids;
		auto & rmap = rhs->kids;
		for (auto ri = rmap.begin(); ri != rmap.end();) {
			auto li = lmap.find(ri->first);
			if (li == lmap.end()) {
				auto i = ri++;
				i->second->parent = this;
				lmap.insert(rmap.extract(i));
				continue;
			}
			li->second->merge(ri->second.get(), overwrite);
			ri++;
		}
		return 0;
	}

	int process_imports(std::string_view path, const std::list<std::string_view> & parents = {});
};
