/*
 * Copyright (c) 2018-2021 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#include "tll/config.h"
#include "tll/logger.h"
#include "tll/scheme.h"

#include "tll/compat/filesystem.h"

#include "tll/util/bin2ascii.h"
#include "tll/util/conv.h"
#include "tll/util/listiter.h"
#include "tll/util/result.h"
#include "tll/util/string.h"
#include "tll/util/url.h"
#include "tll/util/zlib.h"

#include "scheme-config.h"

#include <errno.h>
#include <stdlib.h>

#include <atomic>
#include <memory>
#include <set>

void tll_scheme_free(tll_scheme_t *);

using namespace tll::scheme;
using tll::util::list_wrap;

struct tll_scheme_internal_t
{
	std::atomic<int> ref = { 1 };
};

namespace {
template <typename T>
typename T::pointer find_entry(std::string_view name, T &l)
{
	for (auto & i : l) {
		if (i.name == name)
			return &i;
	}
	return nullptr;
}

template <typename T>
typename T::pointer find_entry(std::string_view name, T &l0, T &l1)
{
	if (auto r = find_entry(name, l0); r) return r;
	return find_entry(name, l1);
}

template <typename T>
T * find_entry(std::string_view name, T *l0, T *l1 = nullptr)
{
	auto w = list_wrap(l0);
	if (auto r = find_entry(name, w); r)
		return r;
	if (l1 == nullptr) return nullptr;
	return find_entry(name, l1);
}

bool starts_with(std::string_view l, std::string_view r)
{
	return l.size() >= r.size() && l.substr(0, r.size()) == r;
}

tll::scheme::Option * alloc_option(std::string_view name, std::string_view value, tll::scheme::Option * next = nullptr)
{
	auto o = (tll::scheme::Option *) malloc(sizeof(tll::scheme::Option));
	*o = {};
	o->name = strndup(name.data(), name.size());
	o->value = strndup(value.data(), value.size());
	o->next = next;
	return o;
}

tll_scheme_bit_field_t * alloc_bit_field(std::string_view name, size_t size, size_t offset)
{
	auto f = (tll_scheme_bit_field_t*) malloc(sizeof(tll_scheme_bit_field_t));
	*f = {};
	f->name = strndup(name.data(), name.size());
	f->offset = offset;
	f->size = size;
	return f;
}

std::string_view offset_ptr_type_name(tll_scheme_offset_ptr_version_t v)
{
	switch (v) {
	case TLL_SCHEME_OFFSET_PTR_DEFAULT: return "default";
	case TLL_SCHEME_OFFSET_PTR_LEGACY_SHORT: return "legacy-short";
	case TLL_SCHEME_OFFSET_PTR_LEGACY_LONG: return "legacy-long";
	}
	return "";
}

int fix_offset_ptr_options(tll_scheme_field_t * f)
{
	if (f->offset_ptr_version == TLL_SCHEME_OFFSET_PTR_DEFAULT) return 0;
	std::string_view type = offset_ptr_type_name(f->offset_ptr_version);
	auto o = f->options;
	for (; o; o = o->next) {
		if (o->name == std::string_view("offset-ptr-type"))
			break;
	}
	if (!o) {
		f->options = alloc_option("offset-ptr-type", type, f->options);
	} else if (o->value != type) {
		return EINVAL;
	}
	return 0;
}

std::list<std::filesystem::path> scheme_search_path()
{
	std::filesystem::path p(DATADIR);
	std::list<std::filesystem::path> deflist = {p / "tll" / "scheme"};
	if (p != "/usr/share")
		deflist.push_back("/usr/share/tll/scheme");

	auto v = getenv("TLL_SCHEME_PATH");
	if (!v)
		return deflist;
	std::list<std::filesystem::path> r;
	tll::splitl<':', true>(r, v);
	r.insert(r.end(), deflist.begin(), deflist.end());
	return r;
}
}

namespace tll::scheme::internal {

inline std::optional<tll_scheme_field_type_t> parse_type_int(std::string_view type)
{
	if (type == "int8") return tll::scheme::Field::Int8;
	else if (type == "int16") return tll::scheme::Field::Int16;
	else if (type == "int32") return tll::scheme::Field::Int32;
	else if (type == "int64") return tll::scheme::Field::Int64;
	else if (type == "uint8") return tll::scheme::Field::UInt8;
	else if (type == "uint16") return tll::scheme::Field::UInt16;
	else if (type == "uint32") return tll::scheme::Field::UInt32;
	else
		return std::nullopt;
}

struct Options : public tll::Props
{

	tll_scheme_option_t * finalize()
	{
		tll_scheme_option_t * r = nullptr;
		auto last = &r;
		for (auto & [name, value] : *this) {
			*last = alloc_option(name, value);
			last = &(*last)->next;
		}
		return r;
	}

	static std::optional<Options> parse(tll::Config &cfg, std::string_view key = "options")
	{
		Options r;
		auto sc = cfg.sub(key);
		if (!sc) return r;
		for (auto & [k, kc] : sc->browse("**")) {
			auto v = kc.get();
			if (!v) return std::nullopt;
			r[k] = *v;
		}
		return r;
	}
};

struct Enum
{
	std::string name;
	Options options;
	size_t size = -1;
	tll_scheme_field_type_t type = TLL_SCHEME_FIELD_INT8;
	std::list<std::pair<std::string, long long>> values;

	using unique_ptr = std::unique_ptr<tll::scheme::Enum, decltype(&tll_scheme_enum_free)>;

	tll::scheme::Enum * finalize()
	{
		auto r = (tll::scheme::Enum *) malloc(sizeof(tll::scheme::Enum));
		*r = {};
		r->type = type;
		r->size = size;
		r->name = strdup(name.c_str());
		r->options = options.finalize();
		auto last = &r->values;
		for (auto & [name, value] : values) {
			auto ev = (tll_scheme_enum_value_t *) malloc(sizeof(tll_scheme_enum_value_t));
			*ev = {};
			ev->name = strdup(name.c_str());
			ev->value = value;
			*last = ev;
			last = &ev->next;
		}
		return r;
	}

	static int parse_list(tll::Logger &_log, tll::Config &cfg, std::list<Enum> &r)
	{
		std::set<std::string_view> names;
		for (auto & e : r) names.insert(e.name);
		for (auto & [path, ec] : cfg.browse("enums.*", true)) {
			auto n = path.substr(6);
			//auto n = ec.get("name");
			//if (!n || !n->size())
			//	return _log.fail(EINVAL, "Enum without name");
			if (names.find(n) != names.end())
				return _log.fail(EINVAL, "Duplicate enum name {}", n);
			auto e = Enum::parse(ec, n);
			if (!e)
				return _log.fail(EINVAL, "Failed to load enum {}", n);
			r.push_back(*e);
			names.insert(r.back().name);
		}
		return 0;
	}

	static std::optional<Enum> parse(tll::Config &cfg, std::string_view name)
	{
		Enum r;
		r.name = name;
		tll::Logger _log = {"tll.scheme.enum." + r.name};

		auto type = cfg.get("type");
		if (!type)
			return _log.fail(std::nullopt, "Failed to parse enum {}: missing type", name);
		auto t = parse_type_int(*type);
		if (!t) {
			using F = tll::scheme::Field;
			auto s = conv::select<decltype(F::Int8)>(*type, {{"enum1", F::Int8}, {"enum2", F::Int16}, {"enum4", F::Int32}, {"enum8", F::Int64}});
			if (!s)
				return _log.fail(std::nullopt, "Failed to parse enum {}: invalid type: {}", name, *type);
			_log.warning("Deprecated enum notation: {}, use int8/int16/...", *type);
			t = *s;
		}
		r.type = *t;

		switch (r.type) {
		case tll::scheme::Field::Int8: r.size = 1; break;
		case tll::scheme::Field::Int16: r.size = 2; break;
		case tll::scheme::Field::Int32: r.size = 4; break;
		case tll::scheme::Field::Int64: r.size = 8; break;
		default: break;
		}

		auto o = Options::parse(cfg);
		if (!o) return _log.fail(std::nullopt, "Failed to parse options for enum {}", name);
		std::swap(r.options, *o);

		auto vc = cfg.sub("enum");
		if (!vc) return _log.fail(std::nullopt, "Failed to parse enum {}: no values", name);

		for (auto & [k, c] : vc->browse("**")) {
			auto v = c.get();
			if (!v)
				return _log.fail(std::nullopt, "Failed to parse enum {}: no value for key {}", name, k);
			auto i = conv::to_any<long long>(*v);
			if (!i)
				return _log.fail(std::nullopt, "Failed to parse enum {}: invalid value key {}: {} {}", name, k, *v, i.error());
			r.values.push_back(std::make_pair(k, *i));
		}
		return r;
	}
};

struct Message;
struct Field
{
	std::string name;
	Message * parent = nullptr;
	Options options;
	Options list_options;
	tll_scheme_field_type_t type;
	tll_scheme_sub_type_t sub_type = TLL_SCHEME_SUB_NONE;
	typedef std::pair<size_t, tll_scheme_field_type_t> array_t;
	typedef std::variant<tll_scheme_offset_ptr_version_t, array_t> nested_t;
	std::list<nested_t> nested;
	int size = -1;
	std::string type_msg;
	std::string type_enum;
	std::string type_union;
	unsigned fixed_precision; // For fixed
	time_resolution_t time_resolution = TLL_SCHEME_TIME_NS;
	std::list<std::string> bitfields; // For bitfield

	void finalize(tll::scheme::Scheme *s, tll::scheme::Message *m, tll::scheme::Field *r);
	tll::scheme::Field * finalize(tll::scheme::Scheme *s, tll::scheme::Message *m)
	{
		auto r = (tll::scheme::Field *) malloc(sizeof(tll::scheme::Field));
		*r = {};
		finalize(s, m, r);
		return r;
	}

	int lookup(std::string_view type);
	int parse_enum_inline(tll::Config &cfg);
	int parse_union_inline(tll::Config &cfg);
	int parse_type(tll::Config &cfg, std::string_view type);
	int parse_sub_type(tll::Config &cfg, std::string_view t)
	{
		using tll::scheme::Field;
		switch (type) {
		case Field::Bytes:
			if (t == "string") sub_type = Field::ByteString;
			break;
		case Field::Int8:
			if (t == "string") {
				if (nested.size() == 0 || std::holds_alternative<array_t>(nested.back()))
					return EINVAL;
				sub_type = Field::ByteString;
			}
		case Field::Int16:
		case Field::Int32:
		case Field::Int64:
		case Field::UInt8:
		case Field::UInt16:
		case Field::UInt32:
			if (starts_with(t, "fixed")) {
				sub_type = tll::scheme::Field::Fixed;
				auto s = tll::conv::to_any<unsigned>(t.substr(5));
				if (!s)
					return EINVAL; //_log.fail(EINVAL, "Invalid decimal precision {}: {}", t.substr(5), s.error());
				fixed_precision = *s;
			} else if (t == "bits") {
				sub_type = tll::scheme::Field::Bits;
				for (auto & [k, c] : cfg.browse("bits.*")) {
					bitfields.emplace_back(*c.get());
				}
			}
		case Field::Double:
			if (t == "enum") {
				if (parse_enum_inline(cfg))
					return EINVAL;
			} else if (t == "time_point" || t == "duration") {
				sub_type = t == "duration" ? Field::Duration : Field::TimePoint;

				auto r = options.get("resolution");
				if (!r)
					return EINVAL;

				if (*r == "ns" || *r == "nanosecond") time_resolution = TLL_SCHEME_TIME_NS;
				else if (*r == "us" || *r == "microsecond") time_resolution = TLL_SCHEME_TIME_US;
				else if (*r == "ms" || *r == "millisecond") time_resolution = TLL_SCHEME_TIME_MS;
				else if (*r == "s" || *r == "second") time_resolution = TLL_SCHEME_TIME_SECOND;
				else if (*r == "m" || *r == "minute") time_resolution = TLL_SCHEME_TIME_MINUTE;
				else if (*r == "h" || *r == "hour") time_resolution = TLL_SCHEME_TIME_HOUR;
				else if (*r == "d" || *r == "day") time_resolution = TLL_SCHEME_TIME_DAY;
				else
					return EINVAL;
			}
			break;
		default:
			break;
		}
		return 0;
	}

	int alias(const Field & a)
	{
		type = a.type;
		sub_type = a.sub_type;
		type_msg = a.type_msg;
		type_enum = a.type_enum;
		size = a.size;
		fixed_precision = a.fixed_precision;
		time_resolution = a.time_resolution;

		for (auto & o : a.options) {
			if (!options.has(o.first))
				options[o.first] = o.second;
		}

		for (auto & o : a.list_options) {
			if (!list_options.has(o.first))
				list_options[o.first] = o.second;
		}

		for (auto & n : a.nested)
			nested.push_back(n);

		return 0;
	}

	static std::optional<Field> parse(Message & m, tll::Config &cfg, std::string_view name);
};

struct Union
{
	Scheme * parent = nullptr;
	std::string name;
	Options options;
	tll_scheme_field_type_t type = TLL_SCHEME_FIELD_INT8;
	std::list<Field> fields;

	using unique_ptr = std::unique_ptr<tll::scheme::Enum, decltype(&tll_scheme_enum_free)>;

	tll::scheme::Union * finalize(tll::scheme::Scheme * s, tll::scheme::Message * m)
	{
		auto r = (tll::scheme::Union *) malloc(sizeof(tll::scheme::Union));
		*r = {};
		r->name = strdup(name.c_str());
		r->options = options.finalize();
		Field f;
		f.type = tll::scheme::Field::Int8;
		f.name = "_type";
		f.options["_auto"] = "union";
		r->type_ptr = f.finalize(s, m);
		r->fields_size = fields.size();
		r->fields = (tll_scheme_field_t *) malloc(sizeof(tll_scheme_field_t) * fields.size());
		auto it = fields.begin();
		for (auto uf = r->fields; uf != r->fields + r->fields_size; uf++, it++) {
			*uf = {};
			it->finalize(s, m, uf);
		}
		return r;
	}

	static int parse_list(tll::Logger &_log, Message &m, tll::Config &cfg, std::list<Union> &r)
	{
		std::set<std::string_view> names;
		for (auto & e : r) names.insert(e.name);
		for (auto & [path, c] : cfg.browse("unions.*", true)) {
			auto n = path.substr(7);
			if (names.find(n) != names.end())
				return _log.fail(EINVAL, "Duplicate union name {}", n);
			auto u = Union::parse(m, c, n);
			if (!u)
				return _log.fail(EINVAL, "Failed to load union {}", n);
			r.push_back(*u);
			names.insert(r.back().name);
		}
		return 0;
	}

	static std::optional<Union> parse(Message &m, tll::Config &cfg, std::string_view name)
	{
		Union r;
		r.name = name;
		tll::Logger _log = {"tll.scheme.union." + r.name};

		for (auto &[k, c]: cfg.browse("union.*", true)) {
			auto n = c.get("name");
			if (!n || !n->size())
				return _log.fail(std::nullopt, "Union field without name");

			auto f = Field::parse(m, c, *n);
			if (!f)
				return _log.fail(std::nullopt, "Failed to load union field {}", *n);
			r.fields.push_back(*f);
		}
		return r;
	}
};

struct Scheme;
struct Message
{
	int msgid = 0;
	std::string name;
	Scheme * parent = nullptr;
	Options options;
	std::list<Field> fields;
	std::list<Enum> enums;
	std::list<Union> unions;

	tll::scheme::Message * finalize(tll::scheme::Scheme * s)
	{
		auto r = (tll::scheme::Message *) malloc(sizeof(tll::scheme::Message));
		*r = {};
		r->name = strdup(name.c_str());
		r->msgid = msgid;
		r->options = options.finalize();
		auto elast = &r->enums;
		for (auto & e : enums) {
			*elast = e.finalize();
			elast = &(*elast)->next;
		}
		auto ulast = &r->unions;
		for (auto & u : unions) {
			*ulast = u.finalize(s, r);
			ulast = &(*ulast)->next;
		}
		auto flast = &r->fields;
		for (auto & f : fields) {
			*flast = f.finalize(s, r);
			for (; *flast; flast = &(*flast)->next) {}
		}
		return r;
	}

	static std::optional<Message> parse(Scheme & s, tll::Config &cfg, std::string_view name)
	{
		Message m;
		m.name = name;
		m.parent = &s;
		tll::Logger _log = {"tll.scheme.message." + m.name};

		auto reader = make_props_reader(cfg);

		m.msgid = reader.getT<int>("id", 0);
		if (!reader)
			return _log.fail(std::nullopt, "Failed to parse message {}: {}", m.name, reader.error());


		auto o = Options::parse(cfg);
		if (!o) return _log.fail(std::nullopt, "Failed to parse options for message {}", m.name);
		std::swap(m.options, *o);

		if (Enum::parse_list(_log, cfg, m.enums))
			return _log.fail(std::nullopt, "Failed to parse enums");

		if (Union::parse_list(_log, m, cfg, m.unions))
			return _log.fail(std::nullopt, "Failed to parse unions");

		for (auto & [unused, fc] : cfg.browse("fields.*", true)) {
			auto n = fc.get("name");
			if (!n || !n->size())
				return _log.fail(std::nullopt, "Field without name");

			for (auto & f : m.fields) {
				if (f.name == *n)
					return _log.fail(std::nullopt, "Duplicate field name {}", *n);
			}


			_log.trace("Loading field {}", *n);
			auto f = Field::parse(m, fc, *n);
			if (!f)
				return _log.fail(std::nullopt, "Failed to load field {}", *n);
			m.fields.push_back(*f);
		}
		return m;
	}
};

struct Scheme
{
	struct Search
	{
		std::filesystem::path current;
		std::list<std::filesystem::path> search;
	};

	Options options;
	std::list<Message> messages;
	std::list<Enum> enums;
	std::list<Union> unions;
	std::list<Field> aliases;

	std::map<std::string, std::string> imports;

	tll::scheme::Scheme * finalize()
	{
		auto r = (tll::scheme::Scheme *) malloc(sizeof(tll::scheme::Scheme));
		*r = {};
		r->options = options.finalize();
		auto elast = &r->enums;
		for (auto & e : enums) {
			*elast = e.finalize();
			elast = &(*elast)->next;
		}

		auto ulast = &r->unions;
		for (auto & u : unions) {
			tll::scheme::Message m = {};
			*ulast = u.finalize(r, &m);
			ulast = &(*ulast)->next;
		}

		auto alast = &r->aliases;
		for (auto & f : aliases) {
			tll::scheme::Message m = {};
			*alast = f.finalize(r, &m);
			alast = &(*alast)->next;
		}

		auto mlast = &r->messages;
		for (auto & m : messages) {
			*mlast = m.finalize(r);
			mlast = &(*mlast)->next;
		}
		return r;
	}

	static tll::result_t<std::pair<std::string, std::filesystem::path>> lookup(std::string_view path, const Search &search = {})
	{
		namespace fs = std::filesystem;
		using tll::filesystem::lexically_normal;

		if (starts_with(path, "yamls"))
			return std::make_pair(path, "");

		if (!starts_with(path, "yaml://"))
			return std::make_pair(path, "");

		auto url = tll::Url::parse(path);
		if (!url)
			return error("Invalid url");
		if (!url->host.size())
			return error("Zero length filename");

		fs::path fn = url->host;
		if (fn.is_absolute()) {
			fn = lexically_normal(fn);
			url->host = fn.string();
			return std::make_pair(conv::to_string(*url), fn);
		}

		if (*fn.begin() == "." || *fn.begin() == "..") {
			auto tmp = lexically_normal(search.current.parent_path() / fn);
			if (fs::exists(tmp)) {
				url->host = tmp.string();
				return std::make_pair(conv::to_string(*url), tmp);
			}
			return error("Relative import not found");
		}

		if (fs::exists(fn))
			return std::make_pair(path, lexically_normal(fn));
		for (auto & prefix : search.search) {
			auto tmp = lexically_normal(prefix / fn);
			if (fs::exists(tmp)) {
				url->host = tmp.string();
				return std::make_pair(conv::to_string(*url), tmp);
			}
		}
		return error("File not found");
	}

	int parse_meta(tll::Config &cfg, const Search &search)
	{
		tll::Logger _log = {"tll.scheme"};
		auto o = Options::parse(cfg);
		if (!o) return _log.fail(EINVAL, "Failed to parse options");
		std::swap(options, *o);

		Message message;
		message.parent = this;

		if (Enum::parse_list(_log, cfg, enums))
			return _log.fail(EINVAL, "Failed to load enums");

		if (Union::parse_list(_log, message, cfg, unions))
			return _log.fail(EINVAL, "Failed to parse unions");

		for (auto & [unused, fc] : cfg.browse("aliases.*", true)) {
			auto n = fc.get("name");
			if (!n || !n->size())
				return _log.fail(EINVAL, "Alias without name");

			for (auto & a : aliases) {
				if (a.name == *n)
					return _log.fail(EEXIST, "Duplicate alias name {}", *n);
			}

			_log.trace("Loading alias {}", *n);
			auto f = Field::parse(message, fc, *n);
			if (!f)
				return _log.fail(EINVAL, "Failed to load alias {}", *n);
			if (message.enums.size())
				return _log.fail(EINVAL, "Failed to load alias {}: inline enums not allowed", *n);
			if (message.unions.size())
				return _log.fail(EINVAL, "Failed to load alias {}: inline unions not allowed", *n);
			aliases.push_back(*f);
		}

		for (auto & [path, ic] : cfg.browse("import.**")) {
			auto url = ic.get();
			if (!url) return _log.fail(EINVAL, "Unreadable url for import {}", path);
			auto r = Scheme::lookup(*url, search);
			if (!r)
				return _log.fail(EINVAL, "Failed to lookup import {} '{}': {}", path, *url, r.error());
			auto & lurl = r->first;
			if (imports.find(lurl) != imports.end()) {
				_log.debug("Scheme import {} already loaded", lurl);
				continue;
			}
			imports.emplace(lurl, *url);
			auto c = Config::load(lurl);
			if (!c)
				return _log.fail(EINVAL, "Failed to load config {}", lurl.substr(0, 64), lurl.size() > 64 ? "..." : "");
			_log.debug("Load scheme import from {}", lurl.substr(0, 64), lurl.size() > 64 ? "..." : "");
			Search s = search;
			s.current = r->second;
			if (parse(*c, s))
				return _log.fail(EINVAL, "Failed to load scheme {}", lurl.substr(0, 64), lurl.size() > 64 ? "..." : "");
		}
		return 0;
	}

	int parse(tll::Config &cfg, const Search &search)
	{
		tll::Logger _log = {"tll.scheme"};
		bool meta = false;

		std::set<std::string_view> names;
		for (auto & m : messages) names.insert(m.name);

		for (auto & [unused, mc] : cfg.browse("*", true)) {
			auto n = mc.get("name");
			if (!n || !n->size()) {
				if (meta)
					return _log.fail(EINVAL, "Duplicate meta block");
				if (parse_meta(mc, search))
					return _log.fail(EINVAL, "Failed to load meta block");
				meta = true;
				continue;
			}

			if (names.find(*n) != names.end())
				return _log.fail(EINVAL, "Duplicate message '{}'", *n);
			_log.debug("Loading message {}", *n);
			auto m = Message::parse(*this, mc, *n);
			if (!m)
				return _log.fail(EINVAL, "Failed to load message {}", *n);
			messages.push_back(*m);
			names.insert(messages.back().name);
		}
		return 0;
	}

	static std::optional<Scheme> load(tll::Config &cfg, const Search &search)
	{
		Scheme s;
		if (s.parse(cfg, search))
			return std::nullopt;
		return s;
	}
};

int Field::lookup(std::string_view type)
{
	for (auto & i : parent->parent->messages) {
		if (i.name == type) {
			type_msg = type;
			this->type = tll::scheme::Field::Message;
			return 0;
		}
	}
	for (auto & i : parent->enums) {
		if (i.name == type) {
			type_enum = type;
			this->sub_type = tll::scheme::Field::Enum;
			return 0;
		}
	}
	for (auto & i : parent->unions) {
		if (i.name == type) {
			type_union = type;
			this->type = tll::scheme::Field::Union;
			return 0;
		}
	}
	for (auto & i : parent->parent->enums) {
		if (i.name == type) {
			type_enum = type;
			this->sub_type = tll::scheme::Field::Enum;
			return 0;
		}
	}
	for (auto & i : parent->parent->unions) {
		if (i.name == type) {
			type_union = type;
			this->type = tll::scheme::Field::Union;
			return 0;
		}
	}
	for (auto & i : parent->parent->aliases) {
		if (i.name == type) {
			return alias(i);
		}
	}
	return ENOENT;
}

std::optional<Field> Field::parse(Message &m, tll::Config &cfg, std::string_view name)
{
	Field f;
	f.parent = &m;
	f.name = name;
	tll::Logger _log = {"tll.scheme.field." + f.name};

	auto o = Options::parse(cfg);
	if (!o) return _log.fail(std::nullopt, "Failed to parse options");
	std::swap(f.options, *o);

	o = Options::parse(cfg, "list-options");
	if (!o) return _log.fail(std::nullopt, "Failed to parse list-options");
	std::swap(f.list_options, *o);

	auto type = cfg.get("type");
	if (!type) return _log.fail(std::nullopt, "Type not found");
	if (f.parse_type(cfg, *type))
		return _log.fail(std::nullopt, "Failed to parse field type: {}", *type);

	auto max_count = cfg.get("max_count");
	if (max_count) {
		if (*max_count != "any") {
			_log.warning("Deprecated notation: max_count: {}, use {}[{}]", *max_count, *type, *max_count);
			auto c = conv::to_any<size_t>(*max_count);
			if (!c)
				return _log.fail(std::nullopt, "Invalid max_count {}: {}", *max_count, c.error());
			auto ct = f.options.get("count-type").value_or("int32");
			auto t = parse_type_int(ct);
			if (!t)
				return _log.fail(std::nullopt, "Invalid option count-type: {}", ct);
			f.nested.push_back(std::make_pair(*c, *t));
		} else {
			_log.warning("Deprecated notation: max_count: {}, use *{}", *max_count, *type);
			f.nested.push_back(TLL_SCHEME_OFFSET_PTR_LEGACY_SHORT);
		}
	}

	auto fsub = f.options.get("type");
	if (fsub) {
		if (f.parse_sub_type(cfg, *fsub))
			return _log.fail(std::nullopt, "Failed to parse sub-type {}", *fsub);
	}
	return f;
}

void Field::finalize(tll::scheme::Scheme *s, tll::scheme::Message *m, tll::scheme::Field *r)
{
	using tll::scheme::Field;

	r->name = strdup(name.c_str());
	r->options = options.finalize();
	r->type = type;
	r->sub_type = sub_type;
	if (size != -1)
		r->size = size;

	if (sub_type == Field::Enum) {
		r->type_enum = find_entry(type_enum, m->enums, s->enums);
		r->type = r->type_enum->type;
	} else if (sub_type == Field::TimePoint || sub_type == Field::Duration) {
		r->time_resolution = time_resolution;
	} else if (sub_type == Field::Fixed) {
		r->fixed_precision = fixed_precision;
	} else if (sub_type == Field::Bits) {
		auto last = &r->bitfields;
		size_t offset = 0;
		for (auto & f : bitfields) {
			*last = alloc_bit_field(f, 1, offset++);
			last = &(*last)->next;
		}
	} else if (type == Field::Message) {
		r->type_msg = find_entry(type_msg, s->messages);
	} else if (type == Field::Union) {
		r->type_union = find_entry(type_union, m->unions, s->unions);
	}

	bool bytestring = type == Field::Int8 && sub_type == Field::ByteString;
	for (auto n = nested.rbegin(); n != nested.rend(); n++) {
		if (!std::holds_alternative<array_t>(*n)) { // Pointer
			auto ptr = (Field *) malloc(sizeof(Field));
			*ptr = *r;
			*r = {};
			r->name = strdup(name.c_str());
			r->type = Field::Pointer;
			r->type_ptr = ptr;
			r->offset_ptr_version = std::get<tll_scheme_offset_ptr_version_t>(*n);
			r->options = list_options.finalize();

			if (bytestring) {
				ptr->sub_type = Field::SubNone;
				r->sub_type = Field::ByteString;
			}
		} else {
			auto ptr = (Field *) malloc(sizeof(Field));
			*ptr = *r;
			*r = {};
			r->name = strdup(name.c_str());
			r->type = Field::Array;
			auto & a = std::get<array_t>(*n);
			r->count = a.first;
			r->type_array = ptr;
			r->options = list_options.finalize();

			tll::scheme::internal::Field f;
			f.type = a.second; //Field::Int16;
			f.name = name + "_count";
			f.options["_auto"] = "count";
			r->count_ptr = f.finalize(s, m);
		}
		bytestring = false;
	}
}

int Field::parse_enum_inline(tll::Config &cfg)
{
	tll::Logger _log = {"tll.scheme.field." + name};
	if (find_entry(name, parent->enums))
		return _log.fail(EINVAL, "Can not create auto-enum {}, duplicate name", name);
	auto e = Enum::parse(cfg, name);
	if (!e)
		return _log.fail(EINVAL, "Failed to parse inline enum {}", name);

	e->options.clear();
	e->options["_auto"] = "inline";
	parent->enums.push_back(std::move(*e));
	type_enum = name;
	sub_type = tll::scheme::Field::Enum;
	return 0;
}

int Field::parse_union_inline(tll::Config &cfg)
{
	tll::Logger _log = {"tll.scheme.field." + name};

	if (find_entry(name, parent->unions))
		return _log.fail(EINVAL, "Can not create auto-union {}, duplicate name", name);
	auto u = Union::parse(*parent, cfg, name);

	if (!u)
		return _log.fail(EINVAL, "Failed to parse inline union {}", name);

	u->options.clear();
	u->options["_auto"] = "inline";
	parent->unions.push_back(std::move(*u));
	type_union = name;
	return 0;
}

namespace {
tll_scheme_field_type_t default_count_type(size_t size)
{
	if (size < 0x80u) return tll::scheme::Field::Int8;
	else if (size < 0x8000u) return tll::scheme::Field::Int16;
	else if (size < 0x80000000u) return tll::scheme::Field::Int32;
	else
		return tll::scheme::Field::Int32;
}
}

int Field::parse_type(tll::Config &cfg, std::string_view type)
{
	tll::Logger _log = {"tll.scheme.field." + name};
	if (!type.size()) return _log.fail(EINVAL, "Empty type");

	auto optr_type = TLL_SCHEME_OFFSET_PTR_DEFAULT;
	auto ot = list_options.get("offset-ptr-type");
	if (!ot || *ot == "default") {
		optr_type = TLL_SCHEME_OFFSET_PTR_DEFAULT;
	} else if (*ot == "legacy-short") {
		optr_type = TLL_SCHEME_OFFSET_PTR_LEGACY_SHORT;
	} else if (*ot == "legacy-long") {
		optr_type = TLL_SCHEME_OFFSET_PTR_LEGACY_LONG;
	} else
		return _log.fail(EINVAL, "Unknown offset-ptr-type: {}", *ot);
	while (type.size()) {
		if (type[0] != '*') break;
		nested.push_back(optr_type);
		type = type.substr(1);
	}
	if (!type.size()) return _log.fail(EINVAL, "Empty type");
	auto sep = type.find('[');
	if (sep != type.npos) {
		auto count = type.substr(sep + 1);
		type = type.substr(0, sep);
		if (!count.size() || count.back() != ']')
			return _log.fail(EINVAL, "Invalid array definition");
		count.remove_suffix(1);
		auto c = tll::conv::to_any<size_t>(count);
		if (!c)
			return _log.fail(EINVAL, "Invalid array size {}: {}", count, c.error());
		_log.debug("Field count {}", *c);
		auto count_type = default_count_type(*c);
		auto ct = list_options.get("count-type");
		if (ct) {
			auto t = parse_type_int(*ct);
			if (!t)
				return _log.fail(EINVAL, "Invalid option count-type: {}", *ct);
			count_type = *t;
		}
		nested.push_front(std::make_pair(*c, count_type));
	}
	if (type == "int8") this->type = tll::scheme::Field::Int8;
	else if (type == "int16") this->type = tll::scheme::Field::Int16;
	else if (type == "int32") this->type = tll::scheme::Field::Int32;
	else if (type == "int64") this->type = tll::scheme::Field::Int64;
	else if (type == "uint8") this->type = tll::scheme::Field::UInt8;
	else if (type == "uint16") this->type = tll::scheme::Field::UInt16;
	else if (type == "uint32") this->type = tll::scheme::Field::UInt32;
	else if (type == "double") this->type = tll::scheme::Field::Double;
	else if (type == "decimal128") this->type = tll::scheme::Field::Decimal128;
	else if (type == "string") {
		this->type = tll::scheme::Field::Int8;
		this->sub_type = tll::scheme::Field::ByteString;
		nested.push_back(optr_type);
	} else if (starts_with(type, "byte")) {
		this->type = tll::scheme::Field::Bytes;
		auto s = tll::conv::to_any<unsigned>(type.substr(4));
		if (!s)
			return _log.fail(EINVAL, "Invalid bytes count {}: {}", type.substr(4), s.error());
		this->size = *s;
	} else if (starts_with(type, "b") && tll::conv::to_any<unsigned>(type.substr(1))) {
		this->type = tll::scheme::Field::Bytes;
		auto s = tll::conv::to_any<unsigned>(type.substr(1));
		if (!s)
			return _log.fail(EINVAL, "Invalid bytes count {}: {}", type.substr(1), s.error());
		this->size = *s;
		_log.warning("Deprected notation: {}, use byte{}", type, *s);
	} else if (starts_with(type, "c") && tll::conv::to_any<unsigned>(type.substr(1))) {
		this->type = tll::scheme::Field::Bytes;
		this->sub_type = tll::scheme::Field::ByteString;
		auto s = tll::conv::to_any<unsigned>(type.substr(1));
		if (!s)
			return _log.fail(EINVAL, "Invalid bytes count {}: {}", type.substr(1), s.error());
		this->size = *s + 1;
		_log.warning("Deprected notation: {}, use byte{}, options.type: string", type, *s + 1);
	} else if (starts_with(type, "decimal")) {
		this->type = tll::scheme::Field::Int64;
		this->sub_type = tll::scheme::Field::Fixed;
		auto s = tll::conv::to_any<unsigned>(type.substr(7));
		if (!s)
			return _log.fail(EINVAL, "Invalid decimal precision {}: {}", type.substr(7), s.error());
		this->fixed_precision = *s;
		_log.warning("Deprected notation: {}, use fixed{}", type, *s);
	} else if (type == "enum1" || type == "enum2" || type == "enum4" || type == "enum8") {
		_log.warning("Deprected notation: {}, use options.type: enum", type);
		return parse_enum_inline(cfg);
	} else if (type == "union") {
		this->type = tll::scheme::Field::Union;
		return parse_union_inline(cfg);
	} else {
		if (lookup(type))
			return _log.fail(EINVAL, "Message or enum '{}' not found", type);
	}
	return 0;
}

} // namespace tll::scheme::internal

tll_scheme_t * tll_scheme_load(const char * curl, int ulen)
{
	tll::Logger _log = {"tll.scheme"};
	if (!curl) return _log.fail(nullptr, "Failed to load config: null string");

	tll::scheme::internal::Scheme::Search search;
	search.search = scheme_search_path();

	std::string_view url(curl, (ulen == -1)?strlen(curl):ulen);
	auto lurl = tll::scheme::internal::Scheme::lookup(url, search);
	if (!lurl)
		return _log.fail(nullptr, "Failed to lookup import '{}': {}", curl, lurl.error());
	auto cfg = tll::Config::load(lurl->first);
	if (!cfg) return _log.fail(nullptr, "Failed to load config: {}", lurl->first);

	search.current = lurl->second;
	auto s = tll::scheme::internal::Scheme::load(*cfg, search);
	if (!s)
		return _log.fail(nullptr, "Failed to load scheme");
	auto r = s->finalize();
	r->internal = new tll_scheme_internal_t;
	if (tll_scheme_fix(r)) {
		tll_scheme_free(r);
		return _log.fail(nullptr, "Failed to fix scheme");
	}
	return r;
}

namespace {
using namespace tll::scheme;

Option * copy_options(const Option *src)
{
	if (!src) return nullptr;
	auto r = (Option *) malloc(sizeof(Option));
	*r = *src;
	r->name = strdup(src->name);
	r->value = strdup(src->value);
	r->next = copy_options(src->next);
	return r;
}

EnumValue * copy_enum_values(const EnumValue *src)
{
	if (!src) return nullptr;
	auto r = (EnumValue *) malloc(sizeof(EnumValue));
	*r = *src;
	r->name = strdup(src->name);
	r->value = src->value;
	r->next = copy_enum_values(src->next);
	return r;
}

Enum * copy_enums(const Enum *src)
{
	if (!src) return nullptr;
	auto r = (Enum *) malloc(sizeof(Enum));
	*r = *src;
	r->name = strdup(src->name);
	r->options = copy_options(src->options);
	r->values = copy_enum_values(src->values);
	r->next = copy_enums(src->next);
	return r;
}

Field * copy_fields(Scheme *ds, Message *dm, Field **result, const Field *src);

void copy_field_body(Scheme *ds, Message *dm, Field *r, const Field *src)
{
	*r = *src;
	r->name = strdup(src->name);
	r->next = nullptr;
	r->user = nullptr;
	r->user_free = nullptr;
	r->options = copy_options(src->options);
	if (r->type == Field::Message) {
		r->type_msg = find_entry(src->type_msg->name, ds->messages);
	} else if (r->type == Field::Array) {
		r->count_ptr = copy_fields(ds, dm, &r->count_ptr, src->count_ptr);
		r->type_array = copy_fields(ds, dm, &r->type_array, src->type_array);
	} else if (r->type == Field::Pointer) {
		r->type_ptr = copy_fields(ds, dm, &r->type_ptr, src->type_ptr);
	} else if (r->type == Field::Union) {
		r->type_union = find_entry(src->type_union->name, dm->unions, ds->unions);
	} else if (r->sub_type == Field::Enum) {
		r->type_enum = find_entry(src->type_enum->name, dm->enums, ds->enums);
	} else if (r->sub_type == Field::Bits) {
		r->bitfields = nullptr;
		auto last = &r->bitfields;
		for (auto &f : list_wrap(src->bitfields)) {
			*last = alloc_bit_field(f.name, f.size, f.offset);
			last = &(*last)->next;
		}
	}
}

Field * copy_fields(Scheme *ds, Message *dm, Field **result, const Field *src)
{
	if (!src) return nullptr;
	auto r = (Field *) malloc(sizeof(Field));
	*result = r;
	copy_field_body(ds, dm, r, src);
	copy_fields(ds, dm, &r->next, src->next);
	return r;
}

Union * copy_unions(Scheme *ds, Message *dm, const Union *src)
{
	if (!src) return nullptr;
	auto r = (Union *) malloc(sizeof(Union));
	*r = *src;
	r->name = strdup(src->name);
	r->options = copy_options(src->options);
	r->fields = (tll_scheme_field_t *) malloc(sizeof(tll_scheme_field_t) * r->fields_size);
	r->type_ptr = copy_fields(ds, dm, &r->type_ptr, src->type_ptr);
	for (auto i = 0u; i < r->fields_size; i++)
		copy_field_body(ds, dm, r->fields + i, src->fields + i);
	r->next = copy_unions(ds, dm, src->next);
	return r;
}

Message * copy_messages(Scheme *ds, Message ** result, const Message *src)
{
	if (!src) return nullptr;
	auto r = (Message *) malloc(sizeof(Message));
	*result = r;
	*r = *src;
	r->name = strdup(src->name);
	r->next = nullptr;
	r->user = nullptr;
	r->user_free = nullptr;
	r->options = copy_options(src->options);
	r->enums = copy_enums(src->enums);
	r->unions = copy_unions(ds, r, src->unions);
	r->fields = nullptr;
	copy_fields(ds, r, &r->fields, src->fields);
	copy_messages(ds, &r->next, src->next);
	return r;
}
}

tll_scheme_t * tll_scheme_copy(const tll_scheme_t *src)
{
	tll_scheme_message_t m = {};

	if (!src) return nullptr;
	auto r = (tll::scheme::Scheme *) malloc(sizeof(tll::scheme::Scheme));
	*r = *src;
	r->internal = new tll_scheme_internal_t;
	r->user = nullptr;
	r->user_free = nullptr;
	r->options = copy_options(src->options);
	r->enums = copy_enums(src->enums);
	r->unions = copy_unions(r, &m, src->unions);
	r->aliases = nullptr;

	copy_fields(r, &m, &r->aliases, src->aliases);

	r->messages = nullptr;
	copy_messages(r, &r->messages, src->messages);
	return r;
}

const tll_scheme_t * tll_scheme_ref(const tll_scheme_t *s)
{
	/*
	if (s) {
		assert(s->internal);
		assert(s->internal->ref.load() >= 0);
	}
	printf("Scheme %p   ref %d\n", s, s?s->internal->ref.load():0);
	*/
	if (!s) return s;
	++s->internal->ref;
	return s;
}

void tll_scheme_unref(const tll_scheme_t *s)
{
	/*
	if (s) {
		assert(s->internal);
		assert(s->internal->ref.load() >= 0);
	}
	printf("Scheme %p unref %d\n", s, s?s->internal->ref.load():0);
	*/
	if (!s) return;
	if (!s->internal || --s->internal->ref == 0)
		tll_scheme_free((tll_scheme_t *) s);
}

void tll_scheme_option_free(tll_scheme_option_t *o)
{
	if (!o) return;
	if (o->name) free((char *) o->name);
	if (o->value) free((char *) o->value);
	tll_scheme_option_free(o->next);
	free(o);
}

void tll_scheme_enum_free(tll_scheme_enum_t *e)
{
	if (!e) return;
	tll_scheme_option_free(e->options);
	if (e->name) free((char *) e->name);
	for (auto v = e->values; v;) {
		auto tmp = v;
		v = v->next;
		free((char *) tmp->name);
		free(tmp);
	}
	tll_scheme_enum_free(e->next);
	free(e);
}

void tll_scheme_bit_field_free(tll_scheme_bit_field_t *f)
{
	if (!f) return;
	if (f->name) free((char *) f->name);
	tll_scheme_bit_field_free(f->next);
	free(f);
}

static void tll_scheme_field_free_body(tll_scheme_field_t *f)
{
	if (!f) return;
	if (f->user) {
		if (f->user_free)
			(*f->user_free)(f->user);
		else
			free(f->user);
	}
	tll_scheme_option_free(f->options);
	if (f->type == Field::Array) {
		tll_scheme_field_free(f->count_ptr);
		tll_scheme_field_free(f->type_array);
	} else if (f->type == Field::Pointer) {
		tll_scheme_field_free(f->type_ptr);
	} else if (f->sub_type == Field::Bits)
		tll_scheme_bit_field_free(f->bitfields);
	if (f->name) free((char *) f->name);
}

void tll_scheme_field_free(tll_scheme_field_t *f)
{
	if (!f) return;
	tll_scheme_field_free_body(f);
	free(f);
}

void tll_scheme_union_free(tll_scheme_union_t *u)
{
	if (!u) return;
	tll_scheme_option_free(u->options);
	if (u->name) free((char *) u->name);
	tll_scheme_field_free(u->type_ptr);
	for (auto f = u->fields; f != u->fields + u->fields_size; f++)
		tll_scheme_field_free_body(f);
	free(u->fields);
	tll_scheme_union_free(u->next);
	free(u);
}

void tll_scheme_message_free(tll_scheme_message_t *m)
{
	if (!m) return;
	if (m->user) {
		if (m->user_free)
			(*m->user_free)(m->user);
		else
			free(m->user);
	}
	tll_scheme_option_free(m->options);
	tll_scheme_enum_free(m->enums);
	tll_scheme_union_free(m->unions);
	if (m->name) free((char *) m->name);
	for (auto f = m->fields; f;) {
		auto tmp = f;
		f = f->next;
		tll_scheme_field_free(tmp);
	}
	free(m);
}

void tll_scheme_free(tll_scheme_t *s)
{
	if (!s) return;
	if (s->user) {
		if (s->user_free)
			(*s->user_free)(s->user);
		else
			free(s->user);
	}
	tll_scheme_option_free(s->options);
	tll_scheme_enum_free(s->enums);
	tll_scheme_union_free(s->unions);
	for (auto f = s->aliases; f;) {
		auto tmp = f;
		f = f->next;
		tll_scheme_field_free(tmp);
	}
	for (auto m = s->messages; m;) {
		auto tmp = m;
		m = m->next;
		tll_scheme_message_free(tmp);
	}
	if (s->internal)
		delete s->internal;
	free(s);
}

namespace {
std::string dump(const tll::scheme::Field * f);

std::string dump_type(tll_scheme_field_type_t t, const tll::scheme::Field * f)
{
	using tll::scheme::Field;
	if (f && f->sub_type == Field::Enum) {
		if (tll::getter::get(f->type_enum->options, "_auto").value_or("") != "inline")
			return f->type_enum->name;
	}
	switch (t) {
	case Field::Int8:  return "int8";
	case Field::Int16: return "int16";
	case Field::Int32: return "int32";
	case Field::Int64: return "int64";
	case Field::UInt8:  return "uint8";
	case Field::UInt16: return "uint16";
	case Field::UInt32: return "uint32";
	case Field::Double: return "double";
	case Field::Decimal128: return "decimal128";
	case Field::Message: if (!f) return "unknown"; return f->type_msg->name;
	case Field::Bytes: if (!f) return "unknown"; return fmt::format("byte{}", f->size);
	case Field::Array: if (!f) return "unknown"; return fmt::format("{}[{}]", dump_type(f->type_array->type, f->type_array), f->count);
	case Field::Pointer:
		if (!f) return "unknown";
		if (f->sub_type == Field::ByteString)
			return "string";
		return "*" + dump_type(f->type_ptr->type, f->type_ptr);
	case Field::Union: return "union";
	}
	return "unknown";
}

std::string dump(const tll::scheme::Option * options, std::string_view key = "options")
{
	if (!options) return "";
	std::string r;
	r += fmt::format("{}: {{", key);
	bool comma = false;
	for (auto &o : list_wrap(options)) {
		if (o.name == std::string_view("_auto")) continue;
		if (comma)
			r += ", ";
		comma = true;
		r += fmt::format("'{}': '{}'", o.name, o.value);
	}
	r += "}";
	return r;
}

std::string dump_body(const tll::scheme::Enum * e)
{
	std::string r = "enum: {";
	bool comma = false;
	for (auto &v : list_wrap(e->values)) {
		if (comma)
			r += ", ";
		comma = true;
		r += fmt::format("'{}': {}", v.name, v.value);
	}
	r += "}";
	return r;
}

std::string dump_body(const tll::scheme::Union * u)
{
	std::string r = "union: [";
	bool comma = false;
	for (auto uf = u->fields; uf != u->fields + u->fields_size; uf++) {
		if (comma)
			r += ", ";
		comma = true;
		r += dump(uf);
	}
	r += "]";
	return r;
}

std::string dump(const tll::scheme::Enum * e)
{
	std::string r;
	r += fmt::format("'{}': ", e->name);
	r += "{";
	r += fmt::format("type: {}, ", dump_type(e->type, nullptr));
	r += dump_body(e);
	if (e->options)
		r += ", " + dump(e->options);
	r += "}";
	return r;
}

std::string dump_options(const tll::scheme::Field * f, bool skip=false)
{
	std::string r;
	switch (f->type) {
	case tll::scheme::Field::Array:
		if (!skip && f->options)
			r += ", " + dump(f->options, "list-options");
		return r + dump_options(f->type_array, true);
	case tll::scheme::Field::Pointer:
		if (!skip && f->options)
			r += ", " + dump(f->options, "list-options");
		return r + dump_options(f->type_ptr, true);
	default:
		if (f->options)
			return ", " + dump(f->options);
	}
	return r;
}

std::string dump(const tll_scheme_bit_field_t * fields)
{
	if (!fields) return "";
	std::string r;
	r += "bits: [";
	bool comma = false;
	for (auto &o : list_wrap(fields)) {
		if (comma)
			r += ", ";
		comma = true;
		r += fmt::format("'{}'", o.name);
	}
	r += "]";
	return r;
}

std::string dump(const tll::scheme::Field * f)
{
	std::string r;
	r += "{";
	r += fmt::format("name: '{}', type: '{}'", f->name, dump_type(f->type, f));
	r += dump_options(f);

	if (f->type == tll::scheme::Field::Union)
		r += ", " + dump_body(f->type_union);
	if (f->sub_type == tll::scheme::Field::Enum) {
		if (tll::getter::get(f->type_enum->options, "_auto").value_or("") == "inline")
			r += ", " + dump_body(f->type_enum);
		//if (f->type_enum->options)
		//	r += ", " + dump(f->type_enum->options, "enum-options");
	}
	if (f->sub_type == tll::scheme::Field::Bits) {
		r += ", " + dump(f->bitfields);
	}
	r += "}";
	return r;
}

std::string dump(const tll::scheme::Message * m)
{
	std::string r;
	r += fmt::format("- name: '{}'\n", m->name);
	if (m->msgid)
		r += "  " + fmt::format("id: {}\n", m->msgid);
	if (m->options)
		r += "  " + dump(m->options) + "\n";
	if (m->enums) {
		r += "  enums:\n";
		for (auto &e : list_wrap(m->enums)) {
			if (tll::getter::get(e.options, "_auto").value_or("") == "inline")
				continue;
			r += "    " + dump(&e) + "\n" ;
		}
	}
	if (m->fields) {
		r += "  fields:\n";
		for (auto &f : list_wrap(m->fields))
			r += "    - " + dump(&f) + "\n" ;
	}
	return r;
}
}

char * tll_scheme_dump(const tll_scheme_t * s, const char * format)
{
	if (!s) return nullptr;

	constexpr std::string_view fdefault = "yamls";

	auto fmt = format == nullptr ? fdefault : std::string_view(format);
	if (fmt != "yamls" && fmt != "yamls+gz")
		return nullptr;
	std::string r;
	if (s->options || s->enums || s->aliases) {
		r += "- name: ''\n";
		if (s->options)
			r += "  " + dump(s->options) + "\n";
		if (s->enums) {
			r += "  enums:\n";
			for (auto &e : list_wrap(s->enums))
				r += "    " + dump(&e) + "\n" ;
		}
		if (s->aliases) {
			r += "  aliases:\n";
			for (auto &f : list_wrap(s->aliases))
				r += "    - " + dump(&f) + "\n" ;
		}
	}

	for (auto &m : list_wrap(s->messages))
		r += dump(&m);

	if (fmt == "yamls+gz") {
		auto z = tll::zlib::compress(r);
		if (!z)
			return nullptr;
		r = "yamls+gz://" + tll::util::b64_encode(*z);
	} else
		r = "yamls://" + r;

	return strdup(r.c_str());
}

int tll_scheme_field_fix(tll_scheme_field_t * f)
{
	if (!f) return EINVAL;
	using tll::scheme::Field;
	if (f->sub_type == Field::Enum)
		f->type = f->type_enum->type;
	switch (f->type) {
	case Field::Int8: f->size = 1; break;
	case Field::Int16: f->size = 2; break;
	case Field::Int32: f->size = 4; break;
	case Field::Int64: f->size = 8; break;
	case Field::UInt8: f->size = 1; break;
	case Field::UInt16: f->size = 2; break;
	case Field::UInt32: f->size = 4; break;
	case Field::Double: f->size = 8; break;
	case Field::Decimal128: f->size = 16; break;
	case Field::Bytes: if (f->size == 0) f->size = 1; break;
	case Field::Array:
		if (!f->count_ptr->name) f->count_ptr->name = strdup(fmt::format("{}_count", f->name).c_str());
		if (!f->type_array->name) f->type_array->name = strdup(f->name);
		tll_scheme_field_fix(f->count_ptr);
		tll_scheme_field_fix(f->type_array);
		f->type_array->offset = f->count_ptr->size;
		f->size = f->count_ptr->size + f->count * f->type_array->size;
		break;
	case Field::Pointer:
		switch (f->offset_ptr_version) {
		case TLL_SCHEME_OFFSET_PTR_DEFAULT: f->size = 8; break;
		case TLL_SCHEME_OFFSET_PTR_LEGACY_SHORT: f->size = 4; break;
		case TLL_SCHEME_OFFSET_PTR_LEGACY_LONG: f->size = 8; break;
		default: return EINVAL;
		}
		if (!f->type_ptr->name)
			f->type_ptr->name = strdup(f->name);
		if (fix_offset_ptr_options(f))
			return EINVAL;
		return tll_scheme_field_fix(f->type_ptr);
	case Field::Message:
		if (f->type_msg->size == 0) {
			if (tll_scheme_message_fix(f->type_msg))
				return EINVAL;
		}
		f->size = f->type_msg->size;
		break;
	case Field::Union: {
		size_t size = 0;
		for (auto uf = f->type_union->fields; uf != f->type_union->fields + f->type_union->fields_size; uf++) {
			size = std::max(size, uf->size);
		}
		f->type_union->union_size = size;
		f->size = f->type_union->type_ptr->size + size;
		break;
	}
	}

	if (f->sub_type != Field::SubNone && !tll::getter::has(f->options, "type")) {
		switch (f->sub_type) {
		case Field::ByteString:
			f->options = alloc_option("type", "string", f->options);
			break;
		case Field::Fixed:
			f->options = alloc_option("type", fmt::format("fixed{}", f->fixed_precision), f->options);
			break;
		case Field::Duration:
		case Field::TimePoint:
			if (f->sub_type == Field::Duration)
				f->options = alloc_option("type", "duration", f->options);
			else
				f->options = alloc_option("type", "time_point", f->options);
			if (!tll::getter::has(f->options, "resolution"))
				f->options = alloc_option("resolution", tll::scheme::time_resolution_str(f->time_resolution), f->options);
			break;
		default: break;
		}
	}

	return 0;
}

int tll_scheme_message_fix(tll_scheme_message_t * m)
{
	if (!m) return EINVAL;
	if (m->size) return 0;
	size_t offset = 0;

	for (auto &u : list_wrap(m->unions)) {
		if (tll_scheme_field_fix(u.type_ptr))
			return EINVAL;
		for (auto uf = u.fields; uf != u.fields + u.fields_size; uf++) {
			uf->offset = u.type_ptr->size;
			if (tll_scheme_field_fix(uf))
				return EINVAL;
		}
	}

	for (auto &f : list_wrap(m->fields)) {
		if (tll_scheme_field_fix(&f))
			return EINVAL;
		f.offset = offset;
		offset += f.size;
	}
	m->size = offset;
	return 0;
}

int tll_scheme_fix(tll_scheme_t * s)
{
	if (!s) return EINVAL;
	if (!s->internal)
		s->internal = new tll_scheme_internal_t;
	for (auto &m : list_wrap(s->messages)) {
		if (tll_scheme_message_fix(&m))
			return EINVAL;
	}
	return 0;
}
