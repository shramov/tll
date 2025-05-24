#ifndef _TLL_SCHEME_MERGE_H
#define _TLL_SCHEME_MERGE_H

#include <tll/scheme.h>
#include <tll/util/listiter.h>
#include <tll/util/result.h>

#include <list>
#include <set>

namespace tll::scheme::_ {

template <typename T>
T * lookup(T * list, std::string_view name)
{
	for (auto ptr = list; ptr; ptr = ptr->next) {
		if (ptr->name == name)
			return ptr;
	}
	return nullptr;
}

template <typename T>
T ** find_tail(T ** list)
{
	if (!list)
		return nullptr;
	for (; *list; list = &(*list)->next) {}
	return list;
}

inline void depends(const tll::scheme::Field * f, std::set<const tll::scheme::Message *> &deps);
inline void depends(const tll::scheme::Union * u, std::set<const tll::scheme::Message *> &deps)
{
	for (auto & f : tll::util::list_wrap(u->fields)) {
		depends(&f, deps);
	}
}

inline void depends(const tll::scheme::Message * msg, std::set<const tll::scheme::Message *> &deps)
{
	deps.insert(msg);
	for (auto & f : tll::util::list_wrap(msg->fields)) {
		depends(&f, deps);
	}
}

inline void depends(const tll::scheme::Field * f, std::set<const tll::scheme::Message *> &deps)
{
	using Field = tll::scheme::Field;
	switch (f->type) {
	case Field::Message:
		return depends(f->type_msg, deps);
	case Field::Pointer:
		return depends(f->type_ptr, deps);
	case Field::Array:
		return depends(f->type_array, deps);
	case Field::Union:
		return depends(f->type_union, deps);
	default:
		break;
	}
}

} // namespace tll::scheme::_

namespace tll::scheme {

inline bool compare(const tll::scheme::Enum * lhs, const tll::scheme::Enum * rhs)
{
	if (lhs->type != rhs->type)
		return false;
	std::map<std::string_view, long long> lmap, rmap;
	for (auto v = lhs->values; v; v = v->next)
		lmap[v->name] = v->value;
	for (auto v = rhs->values; v; v = v->next)
		rmap[v->name] = v->value;
	if (lmap.size() != rmap.size())
		return false;
	for (auto &lv : lmap) {
		auto rv = rmap.find(lv.first);
		if (rv == rmap.end() || rv->second != lv.second)
			return false;
	}
	return true;
}

inline bool compare(const tll::scheme::Message * lhs, const tll::scheme::Message * rhs);
inline bool compare(const tll::scheme::Union * lhs, const tll::scheme::Union * rhs);
inline bool compare(const tll::scheme::Field * lhs, const tll::scheme::Field * rhs)
{
	if (lhs->type != rhs->type)
		return false;
	if (lhs->size != rhs->size)
		return false;
	if (lhs->sub_type != rhs->sub_type)
		return false;
	using Field = tll::scheme::Field;
	switch (lhs->type) {
	case Field::Message:
		return compare(lhs->type_msg, rhs->type_msg);
	case Field::Array:
		return compare(lhs->count_ptr, rhs->count_ptr) && compare(lhs->type_array, rhs->type_array);
	case Field::Pointer:
		return lhs->offset_ptr_version == rhs->offset_ptr_version && compare(lhs->type_ptr, rhs->type_ptr);
	case Field::Union:
		return compare(lhs->type_union, rhs->type_union);
	default:
		break;
	}
	switch (lhs->sub_type) {
	case Field::Duration:
	case Field::TimePoint:
		return lhs->time_resolution == rhs->time_resolution;
	case Field::Fixed:
		return lhs->fixed_precision == rhs->fixed_precision;
	case Field::Enum:
		return compare(lhs->type_enum, rhs->type_enum);
	default:
		break;
	}
	return true;
}

inline bool compare(const tll::scheme::Union * lhs, const tll::scheme::Union * rhs)
{
	if (std::string_view(lhs->name) != rhs->name)
		return false;
	if (!compare(lhs->type_ptr, rhs->type_ptr))
		return false;
	if (lhs->union_size != rhs->union_size)
		return false;
	if (lhs->fields_size != rhs->fields_size)
		return false;
	for (auto i = 0u; i < lhs->fields_size; i++) {
		if (!compare(lhs->fields + i, rhs->fields + i))
			return false;
	}
	return true;
}

inline bool compare(const tll::scheme::Message * lhs, const tll::scheme::Message * rhs)
{
	if (std::string_view(lhs->name) != rhs->name)
		return false;
	if (lhs->msgid != rhs->msgid)
		return false;
	if (lhs->size != rhs->size)
		return false;
	for (auto lf = lhs->fields, rf = rhs->fields; lf || rf; lf = lf->next, rf = rf->next) {
		if (!lf || !rf)
			return false;
		if (!compare(lf, rf))
			return false;
	}
	return true;
}

static inline bool compare(const tll::Scheme * lhs, const tll::Scheme * rhs)
{
	auto lcount = 0, rcount = 0;
	for (auto f = lhs->messages; f; f = f->next)
		lcount++;
	for (auto f = rhs->messages; f; f = f->next)
		rcount++;
	if (lcount != rcount)
		return false;
	for (auto & lm : tll::util::list_wrap(lhs->messages)) {
		auto rm = rhs->lookup(lm.name);
		if (!rm)
			return false;
		if (!compare(&lm, rm))
			return false;
	}
	return true;
}

inline tll::result_t<tll::Scheme *> merge(const std::list<const tll::Scheme *> &list)
{
	using namespace tll::scheme::_;
	std::unique_ptr<tll::Scheme> result;

	for (auto scheme : list) {
		if (!scheme)
			continue;
		std::unique_ptr<tll::Scheme> tmp { scheme->copy() };
		if (!result) {
			result.reset(tmp.release());
			continue;
		}

		for (auto & i : tll::util::list_wrap(tmp->enums)) {
			if (auto r = lookup(result->enums, i.name); r)
				return tll::error(fmt::format("Duplicate global enum {}", i.name));
		}

		std::swap(*find_tail(&result->enums), tmp->enums);

		for (auto & i : tll::util::list_wrap(tmp->unions)) {
			if (auto r = lookup(result->unions, i.name); r)
				return tll::error(fmt::format("Duplicate global union {}", i.name));
		}

		std::swap(*find_tail(&result->unions), tmp->unions);

		std::set<const tll::scheme::Message *> move;

		for (auto & m : tll::util::list_wrap(tmp->messages)) {
			if (!m.msgid)
				continue;
			if (auto r = result->lookup(m.name); r) {
				if (!compare(&m, r))
					return tll::error(fmt::format("Non-matching message {} {}", m.name, r->name));
				continue;
			} else if (auto r = result->lookup(m.msgid); r)
				return tll::error(fmt::format("Duplicate msgid {}: {} and {}", m.msgid, r->name, m.name));

			move.insert(&m);
			depends(&m, move);
		}

		auto ptr = &tmp->messages;
		while (*ptr) {
			if (move.find(*ptr) != move.end()) {
				auto m = *ptr;
				*ptr = m->next;
				m->next = nullptr;
				*find_tail(&result->messages) = m;
			} else
				ptr = &(*ptr)->next;
		}
	}


	return result.release();
}
} // namespace tll::scheme

#endif//_TLL_SCHEME_MERGE_H
