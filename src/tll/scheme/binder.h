/*
 * Copyright (c) 2021 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#ifndef _TLL_SCHEME_BINDER_H
#define _TLL_SCHEME_BINDER_H

#include <tll/scheme/types.h>
#include <tll/util/memoryview.h>

#include <stddef.h>
#include <string.h>

#include <optional>

namespace tll {

namespace scheme::binder {

template <typename Buf>
class Base
{
 protected:
	using view_type = tll::memoryview<Buf>;
	view_type _buf;

	template <typename T>
	T _get_scalar(size_t offset) const
	{
		return *_buf.view(offset).template dataT<T>();
	}

	template <typename T>
	void _set_scalar(size_t offset, T v)
	{
		*_buf.view(offset).template dataT<T>() = v;
	}

	template <size_t Size>
	const Bytes<Size> & _get_bytes(size_t offset) const
	{
		return *_buf.view(offset).template dataT<Bytes<Size>>();
	}

	template <size_t Size>
	void _set_bytes(size_t offset, const std::array<unsigned char, Size> &v)
	{
		auto ptr = _buf.view(offset).template dataT<unsigned char>();
		memcpy(ptr, v.data(), Size);
	}

	template <size_t Size>
	std::string_view _get_bytestring(size_t offset) const
	{
		auto data = _buf.view(offset).template dataT<char>();
		return {data, strnlen(data, Size)};
	}

	template <size_t Size>
	void _set_bytestring(size_t offset, std::string_view v)
	{
		auto ptr = _buf.view(offset).template dataT<char>();
		const auto size = std::min(v.size(), Size);
		memcpy(ptr, v.data(), size);
		memset(ptr + size, 0, Size - size);
	}

	template <typename Ptr>
	std::string_view _get_string(size_t offset) const
	{
		auto ptr = _buf.view(offset).template dataT<tll::scheme::String<Ptr>>();
		return *ptr;
	}

	template <typename Ptr>
	void _set_string(size_t offset, std::string_view v);

	template <typename T>
	T _get_binder(size_t offset) const
	{
		return T(_buf.view(offset));
	}

	template <typename T>
	T _get_binder(size_t offset)
	{
		return T(_buf.view(offset));
	}

	void _view_resize(size_t size)
	{
		_buf.resize(0);
		_buf.resize(size);
	}

 public:
	Base(view_type view) : _buf(view) {}

	view_type & view() { return _buf; }
	const view_type & view() const { return _buf; }
};

template <typename Buf, typename T>
class binder_iterator
{
	using view_type = tll::memoryview<Buf>;
	T _data;
	size_t _step = 0;

	void _shift(ptrdiff_t offset)
	{
		_data.view() = view_type(_data.view().memory(), _data.view().offset() + offset);
	}
 public:
	using poiner = T *;
	using value_type = T;
	using reference = T &;

	binder_iterator(view_type view, size_t step) : _data(view), _step(step) {}

	binder_iterator & operator ++ () { _shift(_step); return *this; }
	binder_iterator operator ++ (int) { auto tmp = *this; ++*this; return tmp; }

	binder_iterator & operator -- () { _shift(-_step); return *this; }
	binder_iterator operator -- (int) { auto tmp = *this; --*this; return tmp; }

	binder_iterator & operator += (size_t i) { _shift(i * _step); return *this; }
	binder_iterator operator + (size_t i) const { auto tmp = *this; tmp += i; return tmp; }

	binder_iterator & operator -= (size_t i) { _shift(-i * _step); return *this; }
	binder_iterator operator - (size_t i) const { auto tmp = *this; tmp -= i; return tmp; }

	bool operator == (const binder_iterator &rhs) const { return _data.view().data() == rhs._data.view().data(); }
	bool operator != (const binder_iterator &rhs) const { return _data.view().data() != rhs._data.view().data(); }

	T& operator * () { return _data; }
	const T& operator * () const { return _data; }

	T * operator -> () { return &_data; }
	const T * operator -> () const { return &_data; }
};

template <typename Buf, typename T, typename Ptr>
class List : public Base<Buf>
{
 protected:
	static constexpr bool is_binder = std::is_base_of_v<Base<Buf>, T>;
	using pointer_type = tll::scheme::offset_ptr_t<std::conditional_t<is_binder, char, T>, Ptr>;

	auto optr() { return this->_buf.template dataT<pointer_type>(); }
	auto optr() const { return this->_buf.template dataT<pointer_type>(); }

 public:
	using view_type = typename Base<Buf>::view_type;
	using iterator = std::conditional_t<is_binder, binder_iterator<Buf, T>, typename pointer_type::iterator>;
	using const_iterator = std::conditional_t<is_binder, binder_iterator<Buf, T>, typename pointer_type::const_iterator>;

	List(view_type view) : Base<Buf>(view) {}

	static constexpr size_t entity_size_static()
	{
		if constexpr (is_binder)
			return T::meta_size();
		else
			return sizeof(T);
	}

	size_t entity_size() const
	{
		if constexpr (std::is_same_v<Ptr, tll_scheme_offset_ptr_legacy_short_t>)
			return entity_size_static();
		else
			return optr()->entity;
	}

	size_t size() const { return optr()->size; }

	iterator begin()
	{
		if constexpr (is_binder)
			return iterator(this->_buf.view(optr()->offset), entity_size());
		else
			return optr()->begin();
	}

	const_iterator begin() const
	{
		if constexpr (is_binder)
			return const_iterator(this->_buf.view(optr()->offset), entity_size());
		else
			return optr()->begin();
	}

	iterator end() { return begin() + size(); }
	const_iterator end() const { return begin() + size(); }

	void resize(size_t size)
	{
		auto ptr = optr();
		if (size <= ptr->size) {
			ptr->size = size;
			return;
		}
		const size_t entity = entity_size_static();
		const auto data_end = ptr->offset + entity * ptr->size;
		const auto buf_end = this->_buf.size();

		if (ptr->size && buf_end == data_end) {
			auto dview = this->_buf.view(ptr->offset);
			if constexpr (is_binder)
				memset(dview.data(), 0, dview.size());
			dview.resize(entity * size);
			optr()->size = size;
		} else {
			this->_buf.resize(buf_end + entity * size);
			ptr = optr();
			ptr->size = size;
			ptr->offset = buf_end;
			ptr->entity = entity;
		}
	}

	std::conditional_t<is_binder, T, T &> operator [](size_t idx)
	{
		return *(begin() + idx);
	}

	const std::conditional_t<is_binder, T, T &> operator [](size_t idx) const
	{
		return *(begin() + idx);
	}

	/*
	void push_back(std::enable_if_t<Scalar, T> v)
	{
		resize(size() + 1);
		*--end() = v;
	}
	*/
};

template <typename Buf, typename Ptr>
class String : public List<Buf, char, Ptr>
{
 public:
	using view_type = typename Base<Buf>::view_type;
	using pointer_type = tll::scheme::offset_ptr_t<char, Ptr>;
	using iterator = typename pointer_type::iterator;
	using const_iterator = typename pointer_type::const_iterator;

	static constexpr size_t meta_size() { return sizeof(Ptr); }

	String(view_type view) : List<Buf, char, Ptr>(view) {}

	size_t size() const { return std::max(this->optr()->size, 1u) - 1; }

	iterator end() { return this->begin() + size(); }
	const_iterator end() const { return this->begin() + size(); }

	operator std::string_view () const
	{
		auto ptr = this->optr();
		if (ptr->size == 0)
			return "";
		return { ptr->data(), ptr->size - 1u };
	}

	String & operator = (std::string_view v)
	{
		this->resize(v.size() + 1);
		memcpy(&*this->begin(), v.data(), v.size());
		*this->end() = 0;
		return *this;
	}
};

template <typename Buf>
template <typename Ptr>
inline void Base<Buf>::_set_string(size_t offset, std::string_view v)
{
	auto l = _get_binder<String<Buf, Ptr>>(offset);
	l = v;
}

template <typename Buf, typename Type>
class Union : public Base<Buf>
{
 protected:
	void _set_type(Type v) { this->template _set_scalar<Type>(0, v); }

 public:
	using view_type = typename Base<Buf>::view_type;
	static constexpr size_t data_offset = sizeof(Type);

	Union(view_type view) : Base<Buf>(view) {}

	Type union_type() const { return this->template _get_scalar<Type>(0); }
};

} // namespace scheme::binder

namespace scheme {

template <typename Buf> using Binder = tll::scheme::binder::Base<Buf>;

template <template <typename B> typename T, typename Buf>
T<Buf> make_binder(Buf & buf)
{
	return T<Buf>(buf);
}

template <template <typename B> typename T, typename Buf>
T<Buf> make_binder_reset(Buf & buf)
{
	auto r = make_binder<T, Buf>(buf);
	r.view().resize(0);
	r.view().resize(r.meta_size());
	return r;
}

}

} // namespace tll

#endif//_TLL_SCHEME_BINDER_H
