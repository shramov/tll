#ifndef _TLL_COMPAT_FILESYSTEM_H
#define _TLL_COMPAT_FILESYSTEM_H

#include <list>

#if __has_include(<filesystem>)
# include <filesystem>

#else
# include <experimental/filesystem>

namespace std::filesystem {

using namespace ::std::experimental::filesystem;

} // namespace std::filesystem
#endif

namespace tll::filesystem {

inline std::filesystem::path compat_lexically_normal(const std::filesystem::path &p)
{
	if (p.empty())
		return p;
	std::list<std::filesystem::path> components = {p.begin(), p.end()};
	auto ri = components.rbegin();
	while (ri != components.rend()) {
		if (*ri != "." && *ri != "")
			break;
		if (++ri == components.rend()) break;
		if (*ri != "." && *ri != "..") break;
		components.pop_back();
		ri = components.rbegin();
	}

	auto i = components.begin();
	while (i != components.end()) {
		if (*i == "." || *i == "") {
			if (i == --components.end()) {
				break;
			}
			i = components.erase(i);
		} else if (*i == ".." && i != components.begin()) {
			auto prev = i;
			prev--;
			if (prev->is_absolute())
				i = components.erase(i);
			else if (*prev != "..")
				i = components.erase(components.erase(prev));
			else
				i++;
		} else
			i++;
	}

	if (components.empty())
		components.push_back(".");

	if (components.size() > 1 && (components.back() == "" || components.back() == ".")) {
		auto prev = ++components.rbegin();
		if (*prev == ".." || *prev == ".")
			components.pop_back();
		else
			components.back() = "";
	}

	std::filesystem::path r;
	for (auto & c : components)
		r /= c;
	return r;
}

inline std::filesystem::path lexically_normal(const std::filesystem::path &p)
{
#if __has_include(<filesystem>)
	return p.lexically_normal();
#else
	return compat_lexically_normal(p);
#endif
}

inline std::filesystem::path compat_relative_simple(const std::filesystem::path &_p, const std::filesystem::path &_base)
{
	auto p = tll::filesystem::lexically_normal(_p);
	auto base = tll::filesystem::lexically_normal(_base);

	if (!p.is_absolute() || !base.is_absolute()) // Not possible to compute relative path without CWD
		return p;

	auto pi = p.begin();
	auto bi = base.begin();
	for (; pi != p.end() && bi != base.end(); pi++, bi++) {
		if (*pi != *bi) break;
	}

	std::filesystem::path r;
	for (; bi != base.end(); bi++) {
		if (bi == --base.end() && *bi == "")
			break;
		r /= "..";
	}
	for (; pi != p.end(); pi++)
		r /= *pi;
	return r;
}

inline std::filesystem::path relative_simple(const std::filesystem::path &p, const std::filesystem::path &base)
{
#if __has_include(<filesystem>)
	return std::filesystem::relative(p, base);
#else
	return compat_relative_simple(p, base);
#endif
}

} // namespace tll::filesystem

#endif//_TLL_COMPAT_FILESYSTEM_H
