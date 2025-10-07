#ifndef _TLL_UTIL_VALUE_TREE_CHECK_H
#define _TLL_UTIL_VALUE_TREE_CHECK_H

#include <tll/config.h>

#include <list>
#include <set>
#include <string>

namespace tll::util {

inline std::list<std::string> check_value_tree_nodes(const std::set<std::string> &keys)
{
	std::list<std::string> r;
	for (auto it = keys.begin(); it != keys.end(); it++) {
		auto off = it->size();
		auto j = it;
		for (j++; j != keys.end(); j++) {
			if (j->size() <= off)
				break;
			auto c = (*j)[off];
			if (c < '.')
				continue;
			if (c == '.')
				r.emplace_back(*it);
			break;
		}
	}

	return r;
}

} // namespace tll::util

#endif//_TLL_UTIL_VALUE_TREE_CHECK_H
