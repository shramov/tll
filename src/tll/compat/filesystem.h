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
	auto i = components.begin();
	while (i != components.end()) {
		if (*i == "." || *i == "") {
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

} // namespace tll::filesystem

#endif//_TLL_COMPAT_FILESYSTEM_H
