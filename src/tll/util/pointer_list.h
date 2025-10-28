#ifndef _TLL_UTIL_POINTER_LIST_H
#define _TLL_UTIL_POINTER_LIST_H

#include <vector>

namespace tll::util {

template <typename T>
struct PointerList
{
	std::vector<T *> list;
	unsigned _size = 0;

	using storage_type = std::vector<T *>;
	using iterator = typename storage_type::iterator;
	using const_iterator = typename storage_type::const_iterator;
	using reference = typename storage_type::reference;

	iterator begin() { return list.begin(); }
	const_iterator begin() const { return list.begin(); }
	iterator end() { return list.begin() + _size; }
	const_iterator end() const { return list.begin() + _size; }

	reference operator [] (size_t i) { return list[i]; }

	constexpr size_t size() const { return _size; }
	constexpr bool empty() const { return _size == 0; }

	void rebuild()
	{
		auto to = begin();
		for (auto & i : *this) {
			if (!i) continue;
			std::swap(i, *to++);
		}
		_size = to - begin();
	}

	void insert(T * v)
	{
		for (auto & i : *this) {
			if (i == v)
				return;
			if (!i) {
				i = v;
				return;
			}
		}
		if (_size < list.size()) {
			list[_size++] = v;
			return;
		}
		list.push_back(v);
		_size++;
	}

	void erase(const T * v)
	{
		for (unsigned i = 0; i < _size; i++) {
			if (list[i] == v) {
				list[i] = nullptr;
				break;
			}
		}
	}

	void erase_shrink(const T * v)
	{
		erase(v);

		for (; _size > 0; _size--) {
			if (list[_size - 1] != nullptr)
				break;
		}
	}

	void add(T * v) { insert(v); }
	void del(const T * v) { erase_shrink(v); }
};

} // namespace tll::util

#endif//_TLL_UTIL_POINTER_LIST_H
