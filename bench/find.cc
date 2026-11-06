#include <tll/util/bench.h>

#include <fmt/format.h>

#include <algorithm>
#include <set>
#include <type_traits>
#include <unordered_set>
#include <vector>

using namespace tll::bench;
using namespace std::chrono_literals;

template <typename T>
int find(const T &c, int v)
{
	typename T::const_iterator it;
	if constexpr (std::is_same_v<T, std::vector<int>>) {
		it = std::find(c.begin(), c.end(), v);
	} else {
		it = c.find(v);
	}
	if (it != c.end())
		return *it;
	return 0;
}

int main(int argc, char *argv[])
{
	std::set<int> s;
	std::vector<int> v;
	std::unordered_set<int> u;

	unsigned count = 1000000;

	prewarm(100ms);
	for (auto j = 10u; j <= 1000; j *= 10) {
		for (auto i = v.size(); i < j; i++) {
			v.push_back(i);
			s.insert(i);
			u.insert(i);
		}
		for (auto i = 0u; i <= v.size(); i += v.size() / 2) {
			timeit(count, fmt::format("vector[{}]: {}", v.size(), i), find<decltype(v)>, v, i);
			timeit(count, fmt::format("   set[{}]: {}", s.size(), i), find<decltype(s)>, s, i);
			timeit(count, fmt::format("  uset[{}]: {}", u.size(), i), find<decltype(u)>, u, i);
		}
	}

	return 0;
}
