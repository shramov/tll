/*
 * Copyright (c) 2021 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#ifndef _TLL_UTIL_CPPRING_H
#define _TLL_UTIL_CPPRING_H

#include <type_traits>
#include <vector>
#include <cerrno>
#include <fmt/format.h>

namespace tll::util {

template <typename T, bool Const = false>
class circular_iterator
{
	using container_type = std::conditional_t<Const, const std::vector<T>, std::vector<T>>;
	container_type * _data = nullptr;
	unsigned _idx = 0;

	void shift()
	{
		if (++_idx == _data->size())
			_idx = 0;
	}

public:
	using iterator_category = std::forward_iterator_tag;

	using value_type = typename container_type::value_type;
	using reference = std::conditional_t<Const, typename container_type::const_reference, typename container_type::reference>;
	using pointer = std::conditional_t<Const, typename container_type::const_pointer, typename container_type::pointer>;
	using const_reference = typename container_type::const_reference;
	using const_pointer = typename container_type::const_pointer;

	circular_iterator() = default;
	circular_iterator(container_type &data, unsigned idx) : _data(&data), _idx(idx) {}

	circular_iterator operator ++ () { shift(); return *this; }
	circular_iterator operator ++ (int) { circular_iterator tmp = *this; shift(); return tmp; }

	pointer ptr() { return &(*_data)[_idx]; }
	const_pointer ptr() const { return &(*_data)[_idx]; }

	reference operator * () { return *ptr(); }
	const_reference operator * () const { return *ptr(); }
	pointer operator -> () { return ptr(); }
	const_pointer operator -> () const { return ptr(); }

	bool operator == (const circular_iterator& rhs) const { return ptr() == rhs.ptr(); }
	bool operator != (const circular_iterator& rhs) const { return ptr() != rhs.ptr(); }
};

template <typename T>
class Ring
{
	using vector_t = std::vector<T>;

	vector_t _data;
	unsigned _head = 0;
	unsigned _tail = 0;

	unsigned shift(unsigned v)
	{
		if (++v == _data.size())
			return 0;
		return v;
	}

	unsigned prev(unsigned v)
	{
		if (v == 0)
			return _data.size() - 1;
		return v - 1;
	}

public:
	using iterator = circular_iterator<T>;
	using const_iterator = circular_iterator<T, true>;

	using value_type = typename std::vector<T>::value_type;
	using reference = typename std::vector<T>::reference;
	using pointer = typename std::vector<T>::pointer;
	using const_reference = typename std::vector<T>::const_reference;
	using const_pointer = typename std::vector<T>::const_pointer;

	Ring() = default;
	Ring(size_t size) : _data(size) {}

	void resize(size_t size) { _head = _tail = 0; _data.resize(size); }
	void clear() { _head = _tail = 0; }

	iterator begin() { return iterator(_data, _head); }
	const_iterator begin() const { return const_iterator(_data, _head); }
	iterator end() { return iterator(_data, _tail); }
	const_iterator end() const { return const_iterator(_data, _tail); }

	reference front() { return _data[_head]; }
	const_reference front() const { return _data[_head]; }

	reference back() { return _data[prev(_tail)]; }
	const_reference back() const { return _data[prev(_tail)]; }

	size_t size() const {
		if (_head <= _tail)
			return _tail - _head;
		return _tail + _data.size() - _head;
	}

	size_t capacity() const { return _data.size() - 1; }
	bool empty() const { return _head == _tail; }

	T * push_back(T value)
	{
		auto t = shift(_tail);
		if (t == _head)
			return nullptr;
		_data[_tail] = std::move(value);
		std::swap(_tail, t);
		return &_data[t];
	}

	void pop_front()
	{
		if (_head == _tail)
			return;
		_head = shift(_head);
	}
};

template <typename T>
struct framed_data_t
{
	static constexpr size_t frame_size = sizeof(T);
	T * frame = nullptr;
	size_t size = 0;

	void * data() { return (void *) (frame + 1); }
	const void * data() const { return (const void *) (frame + 1); }
	void * end() { return ((char *) data()) + size; }
	const void * end() const { return ((const char *) data()) + size; }
};

template <>
struct framed_data_t<void>
{
	static constexpr size_t frame_size = 0;
	void * frame = nullptr;
	size_t size = 0;

	void * data() { return frame; }
	const void * data() const { return frame; }
	void * end() { return ((char *) data()) + size; }
	const void * end() const { return ((const char *) data()) + size; }
};

template <typename T>
class DataRing : public Ring<framed_data_t<T>>
{
	std::vector<uint8_t> _data;

	using Ring<framed_data_t<T>>::push_back;

	uint8_t * data_head()
	{
		if (this->empty())
			return data_end();
		return (uint8_t *) this->front().frame;
	}

	uint8_t * data_tail()
	{
		if (this->empty())
			return data_begin();
		return (uint8_t *) this->back().end();
	}

	uint8_t * data_begin()
	{
		return _data.data();
	}

	uint8_t * data_end()
	{
		return _data.data() + _data.size();
	}

public:
	using value_type = typename Ring<framed_data_t<T>>::value_type;

	DataRing() = default;
	DataRing(size_t size, size_t data_size) : Ring<framed_data_t<T>>(size), _data(data_size) {}

	void data_resize(size_t size) { this->clear(); _data.resize(size); }
	size_t data_capacity() const { return _data.size(); }

	value_type * push_back(const void * data, size_t size)
	{
		if (this->size() == this->capacity())
			return nullptr;

		size_t full = value_type::frame_size + size;
		if (full > _data.size())
			return nullptr;
		auto tail = data_tail();
		auto head = data_head();
		if (tail > head) {
			size_t free = data_end() - tail;
			if (free < full) {
				free = head - data_begin();
				if (free < full) // No space in front of head
					return nullptr;
				tail = data_begin(); // Wrap, data fits
			}
		} else {
			size_t free = head - tail;
			if (free < full)
				return nullptr;
		}

		value_type v = { (T *) tail, size };
		memcpy(v.data(), data, size);
		return push_back(std::move(v));
	}

	template <typename U>
	value_type * push_back(U value, const void * data, size_t size)
	{
		auto ptr = push_back(data, size);
		if (ptr) {
			if constexpr (std::is_rvalue_reference_v<U> && !std::is_lvalue_reference_v<U>)
				*(ptr->frame) = std::move(value);
			else
				*(ptr->frame) = value;
		}
		return ptr;
	}
};

} // namespace tll::util

#endif//_TLL_UTIL_CPPRING_H
