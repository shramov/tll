/*
 * Copyright (c) 2018-2021 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#include "tll/config.h"
#include "tll/util/string.h"
#include "tll/util/refptr.h"
#include "tll/compat/filesystem.h"

#include <atomic>
#include <cstring>
#include <map>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <variant>

using tll::util::refptr_t;

struct tll_config_t : public tll::util::refbase_t<tll_config_t, 0>
{
	typedef std::map<std::string, refptr_t<tll_config_t>, std::less<>> map_t;
	typedef tll::split_helperT<'.'>::iterator path_iterator_t;
	struct cb_pair_t {
		struct internal {
			tll_config_value_callback_t cb = nullptr;
			void * user = nullptr;
			tll_config_value_callback_free_t deleter = nullptr;
			internal(tll_config_value_callback_t c, void * u, tll_config_value_callback_free_t d) : cb(c), user(u), deleter(d) {}
			~internal()
			{
				if (!cb) return;
				if (deleter) deleter(cb, user);
			}
		};

		std::shared_ptr<internal> ptr;

		char * operator () (int * slen) { return (ptr->cb)(slen, ptr->user); }
	};

	typedef std::unique_lock<std::shared_mutex> wlock_t;
	typedef std::shared_lock<std::shared_mutex> rlock_t;
	mutable std::shared_mutex _lock;

	using path_t = std::filesystem::path;

	tll_config_t * parent = 0;
	map_t kids;
	std::variant<std::monostate, std::string, cb_pair_t, path_t> data = {};

	tll_config_t(tll_config_t *p = nullptr) : parent(p) { /*printf("  new %p\n", this);*/ }
	~tll_config_t() { /*printf("  del %p\n", this);*/ }

	tll_config_t(const tll_config_t &_cfg, unsigned depth, bool root)
	{
		refptr_t<const tll_config_t> cfg = &_cfg;
		auto lock = cfg->rlock();
		data = cfg->data;

		if (std::holds_alternative<path_t>(data)) {
			auto path = std::get<path_t>(data);
			bool lookup = false;
			if (path.is_relative()) {
				unsigned d = 0;
				for (const auto & p : path) {
					if (p != "..") break;
					d++;
				}
				lookup = d > depth;
			} else {
				lookup = !root;
			}
			if (lookup) {
				cfg = _lookup_link(cfg.get());
				if (cfg.get()) {
					lock = cfg->rlock();
					data = cfg->data;
				} else {
					data = {};
					return;
				}
			}
		}
		for (auto & k : cfg->kids) {
			auto it = kids.emplace(k.first, new tll_config_t(*k.second, depth + 1, root)).first;
			it->second->parent = this;
		}
	}

	bool value() const
	{
		if (!std::holds_alternative<path_t>(data))
			return !std::holds_alternative<std::monostate>(data);
		auto cfg = _lookup_link(this);
		if (!cfg)
			return false;
		auto lock = cfg->rlock();
		return cfg->value();
	}

	tll::optional_config_string get() const
	{
		auto lock = rlock();
		if (std::holds_alternative<path_t>(data)) {
			auto cfg = _lookup_link(this);
			return cfg->get();
		} else if (std::holds_alternative<std::string>(data)) {
			auto & v = std::get<std::string>(data);
			auto r = (char *) malloc(v.size() + 1);
			memcpy(r, v.data(), v.size() + 1);
			return tll::optional_config_string(r, v.size());
		} else if (std::holds_alternative<tll_config_t::cb_pair_t>(data)) {
			auto v = std::get<tll_config_t::cb_pair_t>(data);
			lock.unlock();
			int vlen = 0;
			auto r = v(&vlen);
			if (!r)
				return {};
			return tll::optional_config_string(r, vlen);
		} else {
			return {};
		}
	}

	template <typename Cfg>
	static inline refptr_t<Cfg> _lookup_link(Cfg *cfg)
	{
		refptr_t<Cfg> r(cfg);
		auto lock = r->rlock();
		if (!std::holds_alternative<path_t>(r->data))
			return r;

		path_t link = std::get<path_t>(r->data);
		auto pi = link.begin();
		if (link.is_absolute()) {
			while (r->parent) {
				r = r->parent;
				lock.unlock();
				lock = r->rlock();
			}
			pi++;
		} else {
			for (; pi != link.end() && *pi == ".."; pi++) {
				if (!r->parent)
					return {};
				r = r->parent;
				lock.unlock();
				lock = r->rlock();
			}
		}

		if (pi == link.end())
			return r;

		r = _lookup<Cfg>(r.get(), pi, link.end());
		if (pi != link.end())
			return {};
		return r;
	}

	template <typename Cfg, typename Iter>
	static inline refptr_t<Cfg> _lookup(Cfg *cfg, Iter &path, const Iter end)
	{
		refptr_t<Cfg> r(cfg);
		for (; path != end; ++path) {
			rlock_t lock(r->_lock);
			r = _lookup_link(r.get());
			if (!r.get())
				return r;
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

	std::optional<path_t> path(refptr_t<const tll_config_t> child = {}) const
	{
		path_t path = {"/"}; //, path_t::format::generic_format};
		refptr_t<const tll_config_t> self = this;
		{
			auto lock = rlock();
			refptr_t<const tll_config_t> prnt = parent;
			lock.unlock();
			if (prnt.get()) {
				auto p = prnt->path(self);
				if (!p)
					return p;
				path = *p;
			}
		}
		if (child.get()) {
			auto lock = rlock();
			for (auto & [k, v]: kids) {
				if (v.get() == child.get())
					return path / k;
			}
			return std::nullopt;
		}
		return path;
	}

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

	int set(const path_t & v)
	{
		auto value = tll::filesystem::lexically_normal(v);
		auto rel = value;
		if (value.is_absolute()) {
			auto p = path();
			if (!p)
				return EINVAL;
			rel = tll::filesystem::relative_simple(value, *p);
			//fmt::print("Convert path {} to relative to {}: {}\n", value.string(), p->string(), r.string());
		}
		if (rel.empty()) // Empty path
			return EINVAL;
		if (*rel.begin() != "..") // Links under itself
			return EINVAL;
		if (rel.filename() == ".." || rel.filename() == ".") // Simple loop
			return EINVAL;
		auto lock = wlock();
		data = value;
		return 0;
	}

	int set(tll_config_value_callback_t cb, void * user, tll_config_value_callback_free_t deleter)
	{
		auto lock = wlock();
		data = cb_pair_t { std::make_shared<cb_pair_t::internal>(cb, user, deleter) };
		return 0;
	}

	int set(std::string_view path, tll_config_t *cfg, bool consume)
	{
		//printf("  set %p\n", this);
		auto s = tll::split<'.'>(path);
		auto pi = s.begin();
		auto v = find(pi, --s.end(), true);

		auto lock = v->wlock();
		auto [it, created] = v->kids.emplace(*pi, cfg);
		if (!created)
			it->second = cfg;

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
				auto lock = ptr->rlock();
				ptr = _lookup_link(ptr.get());
				if (!ptr.get()) continue;
				lock = ptr->rlock();

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
			//if (ptr->value()) return 0;
			auto lock = ptr->rlock();
			ptr = _lookup_link(ptr.get());
			if (!ptr.get()) return 0;

			if (*i == "*" || *i == "**") break;
			lock = ptr->rlock();

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

		if (overwrite && !std::holds_alternative<std::monostate>(rhs->data)) {
			data = rhs->data;
		}

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

	int unlink(std::string_view path)
	{
		auto s = tll::split<'.'>(path);
		auto pi = s.begin();
		auto pe = s.end();
		auto cfg = refptr_t<tll_config_t>(this);
		if (pi != --pe) {
			cfg = lookup(pi, pe);
			if (pi != pe)
				return ENOENT;
		}
		auto last = *pe;
		auto wlock = cfg->wlock();
		auto it = cfg->kids.find(last);
		if (it == cfg->kids.end())
			return ENOENT;
		cfg->kids.erase(it);
		return 0;
	}

	int remove(std::string_view path)
	{
		auto s = tll::split<'.'>(path);
		auto pi = s.begin();
		auto pe = s.end();
		auto cfg = refptr_t<tll_config_t>(this);
		if (pi != --pe) {
			cfg = lookup(pi, pe);
			if (pi != pe)
				return ENOENT;
		}
		auto last = *pe;
		auto wlock = cfg->wlock();
		auto it = cfg->kids.find(last);
		if (it == cfg->kids.end())
			return ENOENT;

		auto kcfg = _lookup_link(it->second.get());
		if (!kcfg)
			return ENOENT;
		auto klock = kcfg->rlock();
		if (kcfg->kids.empty()) {
			cfg->kids.erase(it);
		} else {
			klock.unlock();
			wlock.unlock();
			wlock = kcfg->wlock();
			kcfg->set();
		}
		return 0;
	}

	int process_imports(std::string_view path, const std::list<std::string_view> & parents = {});
};
