#pragma once

#include <tll/scheme/binder.h>
#include <tll/util/conv.h>

namespace http_binder {

static constexpr std::string_view scheme_string = R"(yamls+gz://eJylUl1rwjAUffdX5C0gDahzzvkmNZuCtDLrs8Q22kBNShMHTvrfd2P9yKqywd7O7Tmcc+9JCZJsywcI4wZCKjdCST1ABxznObGMzlnMMfCpMflyJWTCC1yClsvdVg8AIIS33KQqWRrQHcw+BzshTd87aqyZHwYB9SOgex7CIzqlEYXhGYZ3aj+3AY3pcASwAzCcRZMwmMP0AtNsGPljwK8Wh3Orf7JwYVEXUPQx9K0fJOJFMKJvk4Baq1ZZNsj5vjFndnVYeC14lpxWJ+hw4tOK91B1ANamEHJzPNWVfbJsx++oLjmxkpLHxgaJBC57GFiVdrW6lFiPjFXiJEKz7d6NRouvn5pe90aTM5P+el9Vg77qmqfi6sLV3nBHZsf+fVEVVFN67s9mGezs5NSZCF1rtPOw0b8UxYtC3XvlS+BU6OvjtR5maeM8XdPZsh6YZcq93pV6KIMw4hSh1mvNDclNQc6dZHzD4j05upQ35jpVhfm/e2VzY69jljGnrea5028ESkAf)";

enum class method_t: int8_t
{
	UNDEFINED = 0,
	GET = 1,
	HEAD = 2,
	POST = 3,
	PUT = 4,
	DELETE = 5,
	CONNECT = 6,
	OPTIONS = 7,
	TRACE = 8,
	PATCH = 9,
};

struct Header
{
	static constexpr size_t meta_size() { return 16; }
	static constexpr std::string_view meta_name() { return "Header"; }
	static constexpr size_t offset_header = 0;
	static constexpr size_t offset_value = 8;

	template <typename Buf>
	struct binder_type : public tll::scheme::Binder<Buf>
	{
		using tll::scheme::Binder<Buf>::Binder;

		static constexpr auto meta_size() { return Header::meta_size(); }
		static constexpr auto meta_name() { return Header::meta_name(); }
		void view_resize() { this->_view_resize(meta_size()); }

		std::string_view get_header() const { return this->template _get_string<tll_scheme_offset_ptr_t>(offset_header); }
		void set_header(std::string_view v) { return this->template _set_string<tll_scheme_offset_ptr_t>(offset_header, v); }

		std::string_view get_value() const { return this->template _get_string<tll_scheme_offset_ptr_t>(offset_value); }
		void set_value(std::string_view v) { return this->template _set_string<tll_scheme_offset_ptr_t>(offset_value, v); }
	};

	template <typename Buf>
	static binder_type<Buf> bind(Buf &buf, size_t offset = 0) { return binder_type<Buf>(tll::make_view(buf).view(offset)); }

	template <typename Buf>
	static binder_type<Buf> bind_reset(Buf &buf) { return tll::scheme::make_binder_reset<binder_type, Buf>(buf); }
};

struct connect
{
	static constexpr size_t meta_size() { return 43; }
	static constexpr std::string_view meta_name() { return "connect"; }
	static constexpr int meta_id() { return 1; }
	static constexpr size_t offset_method = 0;
	static constexpr size_t offset_code = 1;
	static constexpr size_t offset_size = 3;
	static constexpr size_t offset_path = 11;
	static constexpr size_t offset_headers = 19;
	static constexpr size_t offset_bytes = 27;
	static constexpr size_t offset_bytestring = 35;

	template <typename Buf>
	struct binder_type : public tll::scheme::Binder<Buf>
	{
		using tll::scheme::Binder<Buf>::Binder;

		static constexpr auto meta_size() { return connect::meta_size(); }
		static constexpr auto meta_name() { return connect::meta_name(); }
		static constexpr auto meta_id() { return connect::meta_id(); }
		void view_resize() { this->_view_resize(meta_size()); }

		using type_method = method_t;
		type_method get_method() const { return this->template _get_scalar<type_method>(offset_method); }
		void set_method(type_method v) { return this->template _set_scalar<type_method>(offset_method, v); }

		using type_code = int16_t;
		type_code get_code() const { return this->template _get_scalar<type_code>(offset_code); }
		void set_code(type_code v) { return this->template _set_scalar<type_code>(offset_code, v); }

		using type_size = int64_t;
		type_size get_size() const { return this->template _get_scalar<type_size>(offset_size); }
		void set_size(type_size v) { return this->template _set_scalar<type_size>(offset_size, v); }

		std::string_view get_path() const { return this->template _get_string<tll_scheme_offset_ptr_t>(offset_path); }
		void set_path(std::string_view v) { return this->template _set_string<tll_scheme_offset_ptr_t>(offset_path, v); }

		using type_headers = tll::scheme::binder::List<Buf, Header::binder_type<Buf>, tll_scheme_offset_ptr_t>;
		const type_headers get_headers() const { return this->template _get_binder<type_headers>(offset_headers); }
		type_headers get_headers() { return this->template _get_binder<type_headers>(offset_headers); }

		const tll::scheme::Bytes<8> & get_bytes() const { return this->template _get_bytes<8>(offset_bytes); }
		void set_bytes(const tll::scheme::Bytes<8> &v) const { return this->template _set_bytes<8>(offset_bytes, {v.data(), v.size()}); }
		void set_bytes(std::string_view v) { return this->template _set_bytestring<8>(offset_bytes, v); }

		std::string_view get_bytestring() const { return this->template _get_bytestring<8>(offset_bytestring); }
		void set_bytestring(std::string_view v) { return this->template _set_bytestring<8>(offset_bytestring, v); }
	};

	template <typename Buf>
	static binder_type<Buf> bind(Buf &buf, size_t offset = 0) { return binder_type<Buf>(tll::make_view(buf).view(offset)); }

	template <typename Buf>
	static binder_type<Buf> bind_reset(Buf &buf) { return tll::scheme::make_binder_reset<binder_type, Buf>(buf); }
};

struct disconnect
{
	static constexpr size_t meta_size() { return 10; }
	static constexpr std::string_view meta_name() { return "disconnect"; }
	static constexpr int meta_id() { return 2; }
	static constexpr size_t offset_code = 0;
	static constexpr size_t offset_error = 2;

	template <typename Buf>
	struct binder_type : public tll::scheme::Binder<Buf>
	{
		using tll::scheme::Binder<Buf>::Binder;

		static constexpr auto meta_size() { return disconnect::meta_size(); }
		static constexpr auto meta_name() { return disconnect::meta_name(); }
		static constexpr auto meta_id() { return disconnect::meta_id(); }
		void view_resize() { this->_view_resize(meta_size()); }

		using type_code = int16_t;
		type_code get_code() const { return this->template _get_scalar<type_code>(offset_code); }
		void set_code(type_code v) { return this->template _set_scalar<type_code>(offset_code, v); }

		std::string_view get_error() const { return this->template _get_string<tll_scheme_offset_ptr_t>(offset_error); }
		void set_error(std::string_view v) { return this->template _set_string<tll_scheme_offset_ptr_t>(offset_error, v); }
	};

	template <typename Buf>
	static binder_type<Buf> bind(Buf &buf, size_t offset = 0) { return binder_type<Buf>(tll::make_view(buf).view(offset)); }

	template <typename Buf>
	static binder_type<Buf> bind_reset(Buf &buf) { return tll::scheme::make_binder_reset<binder_type, Buf>(buf); }
};

struct List
{
	static constexpr size_t meta_size() { return 28; }
	static constexpr std::string_view meta_name() { return "List"; }
	static constexpr int meta_id() { return 10; }
	static constexpr size_t offset_std = 0;
	static constexpr size_t offset_llong = 8;
	static constexpr size_t offset_lshort = 16;
	static constexpr size_t offset_scalar = 20;

	template <typename Buf>
	struct binder_type : public tll::scheme::Binder<Buf>
	{
		using tll::scheme::Binder<Buf>::Binder;

		static constexpr auto meta_size() { return List::meta_size(); }
		static constexpr auto meta_name() { return List::meta_name(); }
		static constexpr auto meta_id() { return List::meta_id(); }
		void view_resize() { this->_view_resize(meta_size()); }

		using type_std = tll::scheme::binder::List<Buf, disconnect::binder_type<Buf>, tll_scheme_offset_ptr_t>;
		const type_std get_std() const { return this->template _get_binder<type_std>(offset_std); }
		type_std get_std() { return this->template _get_binder<type_std>(offset_std); }

		using type_llong = tll::scheme::binder::List<Buf, disconnect::binder_type<Buf>, tll_scheme_offset_ptr_legacy_long_t>;
		const type_llong get_llong() const { return this->template _get_binder<type_llong>(offset_llong); }
		type_llong get_llong() { return this->template _get_binder<type_llong>(offset_llong); }

		using type_lshort = tll::scheme::binder::List<Buf, disconnect::binder_type<Buf>, tll_scheme_offset_ptr_legacy_short_t>;
		const type_lshort get_lshort() const { return this->template _get_binder<type_lshort>(offset_lshort); }
		type_lshort get_lshort() { return this->template _get_binder<type_lshort>(offset_lshort); }

		using type_scalar = tll::scheme::binder::List<Buf, int16_t, tll_scheme_offset_ptr_t>;
		const type_scalar get_scalar() const { return this->template _get_binder<type_scalar>(offset_scalar); }
		type_scalar get_scalar() { return this->template _get_binder<type_scalar>(offset_scalar); }
	};

	template <typename Buf>
	static binder_type<Buf> bind(Buf &buf, size_t offset = 0) { return binder_type<Buf>(tll::make_view(buf).view(offset)); }

	template <typename Buf>
	static binder_type<Buf> bind_reset(Buf &buf) { return tll::scheme::make_binder_reset<binder_type, Buf>(buf); }
};

} // namespace http_binder

template <>
struct tll::conv::dump<http_binder::method_t> : public to_string_from_string_buf<http_binder::method_t>
{
	template <typename Buf>
	static inline std::string_view to_string_buf(const http_binder::method_t &v, Buf &buf)
	{
		switch (v) {
		case http_binder::method_t::CONNECT: return "CONNECT";
		case http_binder::method_t::DELETE: return "DELETE";
		case http_binder::method_t::GET: return "GET";
		case http_binder::method_t::HEAD: return "HEAD";
		case http_binder::method_t::OPTIONS: return "OPTIONS";
		case http_binder::method_t::PATCH: return "PATCH";
		case http_binder::method_t::POST: return "POST";
		case http_binder::method_t::PUT: return "PUT";
		case http_binder::method_t::TRACE: return "TRACE";
		case http_binder::method_t::UNDEFINED: return "UNDEFINED";
		default: break;
		}
		return tll::conv::to_string_buf<int8_t, Buf>((int8_t) v, buf);
	}
};
