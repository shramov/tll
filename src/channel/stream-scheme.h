#pragma once

#include <tll/scheme/binder.h>
#include <tll/util/conv.h>

namespace stream_scheme {

static constexpr std::string_view scheme_string = R"(yamls+gz://eJyVkcsOgjAQRfd+xexIjCQ+iAt2Rv0Bf8AUGE1jLdAZSIjx3wW0Eh9Y3DWTk7k9d3zQ4owheCtmI6OC0RsBHCSqhML6BeDD5YGIJzIBrrJmRPVEH73rG1kKVXyjfJu2w7xA4iZLJiHMpr2hhHm3SGpeBh9psZKo2fmpSKXxyUk9JalDx105Lw6ZqqzBvN9ACeL9AA1z7wSTIXArMwQkNCWaX7fYGpMa67Ho98CWc/Xnzlu359qkGm1o8Of5b/jgziE=)";

struct Attribute
{
	static constexpr size_t meta_size() { return 16; }
	static constexpr std::string_view meta_name() { return "Attribute"; }

	template <typename Buf>
	struct binder_type : public tll::scheme::Binder<Buf>
	{
		using tll::scheme::Binder<Buf>::Binder;

		static constexpr auto meta_size() { return Attribute::meta_size(); }
		static constexpr auto meta_name() { return Attribute::meta_name(); }
		void view_resize() { this->_view_resize(meta_size()); }

		std::string_view get_attribute() const { return this->template _get_string<tll_scheme_offset_ptr_t>(0); }
		void set_attribute(std::string_view v) { return this->template _set_string<tll_scheme_offset_ptr_t>(0, v); }

		std::string_view get_value() const { return this->template _get_string<tll_scheme_offset_ptr_t>(8); }
		void set_value(std::string_view v) { return this->template _set_string<tll_scheme_offset_ptr_t>(8, v); }
	};

	template <typename Buf>
	static binder_type<Buf> bind(Buf &buf, size_t offset = 0) { return binder_type<Buf>(tll::make_view(buf).view(offset)); }
};

struct Request
{
	static constexpr size_t meta_size() { return 32; }
	static constexpr std::string_view meta_name() { return "Request"; }
	static constexpr int meta_id() { return 10; }

	template <typename Buf>
	struct binder_type : public tll::scheme::Binder<Buf>
	{
		using tll::scheme::Binder<Buf>::Binder;

		static constexpr auto meta_size() { return Request::meta_size(); }
		static constexpr auto meta_name() { return Request::meta_name(); }
		static constexpr auto meta_id() { return Request::meta_id(); }
		void view_resize() { this->_view_resize(meta_size()); }

		using type_seq = int64_t;
		type_seq get_seq() const { return this->template _get_scalar<type_seq>(0); }
		void set_seq(type_seq v) { return this->template _set_scalar<type_seq>(0, v); }

		std::string_view get_client() const { return this->template _get_string<tll_scheme_offset_ptr_t>(8); }
		void set_client(std::string_view v) { return this->template _set_string<tll_scheme_offset_ptr_t>(8, v); }

		std::string_view get_block() const { return this->template _get_string<tll_scheme_offset_ptr_t>(16); }
		void set_block(std::string_view v) { return this->template _set_string<tll_scheme_offset_ptr_t>(16, v); }

		using type_attributes = tll::scheme::binder::List<Buf, Attribute::binder_type<Buf>, tll_scheme_offset_ptr_t>;
		const type_attributes get_attributes() const { return this->template _get_binder<type_attributes>(24); }
		type_attributes get_attributes() { return this->template _get_binder<type_attributes>(24); }
	};

	template <typename Buf>
	static binder_type<Buf> bind(Buf &buf, size_t offset = 0) { return binder_type<Buf>(tll::make_view(buf).view(offset)); }
};

struct Reply
{
	static constexpr size_t meta_size() { return 32; }
	static constexpr std::string_view meta_name() { return "Reply"; }
	static constexpr int meta_id() { return 20; }

	template <typename Buf>
	struct binder_type : public tll::scheme::Binder<Buf>
	{
		using tll::scheme::Binder<Buf>::Binder;

		static constexpr auto meta_size() { return Reply::meta_size(); }
		static constexpr auto meta_name() { return Reply::meta_name(); }
		static constexpr auto meta_id() { return Reply::meta_id(); }
		void view_resize() { this->_view_resize(meta_size()); }

		using type_last_seq = int64_t;
		type_last_seq get_last_seq() const { return this->template _get_scalar<type_last_seq>(0); }
		void set_last_seq(type_last_seq v) { return this->template _set_scalar<type_last_seq>(0, v); }

		using type_requested_seq = int64_t;
		type_requested_seq get_requested_seq() const { return this->template _get_scalar<type_requested_seq>(8); }
		void set_requested_seq(type_requested_seq v) { return this->template _set_scalar<type_requested_seq>(8, v); }

		using type_block_seq = int64_t;
		type_block_seq get_block_seq() const { return this->template _get_scalar<type_block_seq>(16); }
		void set_block_seq(type_block_seq v) { return this->template _set_scalar<type_block_seq>(16, v); }

		std::string_view get_server() const { return this->template _get_string<tll_scheme_offset_ptr_t>(24); }
		void set_server(std::string_view v) { return this->template _set_string<tll_scheme_offset_ptr_t>(24, v); }
	};

	template <typename Buf>
	static binder_type<Buf> bind(Buf &buf, size_t offset = 0) { return binder_type<Buf>(tll::make_view(buf).view(offset)); }
};

struct Error
{
	static constexpr size_t meta_size() { return 16; }
	static constexpr std::string_view meta_name() { return "Error"; }
	static constexpr int meta_id() { return 30; }

	template <typename Buf>
	struct binder_type : public tll::scheme::Binder<Buf>
	{
		using tll::scheme::Binder<Buf>::Binder;

		static constexpr auto meta_size() { return Error::meta_size(); }
		static constexpr auto meta_name() { return Error::meta_name(); }
		static constexpr auto meta_id() { return Error::meta_id(); }
		void view_resize() { this->_view_resize(meta_size()); }

		std::string_view get_error() const { return this->template _get_string<tll_scheme_offset_ptr_t>(0); }
		void set_error(std::string_view v) { return this->template _set_string<tll_scheme_offset_ptr_t>(0, v); }

		std::string_view get_server() const { return this->template _get_string<tll_scheme_offset_ptr_t>(8); }
		void set_server(std::string_view v) { return this->template _set_string<tll_scheme_offset_ptr_t>(8, v); }
	};

	template <typename Buf>
	static binder_type<Buf> bind(Buf &buf, size_t offset = 0) { return binder_type<Buf>(tll::make_view(buf).view(offset)); }
};

struct ClientDone
{
	static constexpr size_t meta_size() { return 8; }
	static constexpr std::string_view meta_name() { return "ClientDone"; }
	static constexpr int meta_id() { return 40; }

	template <typename Buf>
	struct binder_type : public tll::scheme::Binder<Buf>
	{
		using tll::scheme::Binder<Buf>::Binder;

		static constexpr auto meta_size() { return ClientDone::meta_size(); }
		static constexpr auto meta_name() { return ClientDone::meta_name(); }
		static constexpr auto meta_id() { return ClientDone::meta_id(); }
		void view_resize() { this->_view_resize(meta_size()); }

		using type_seq = int64_t;
		type_seq get_seq() const { return this->template _get_scalar<type_seq>(0); }
		void set_seq(type_seq v) { return this->template _set_scalar<type_seq>(0, v); }
	};

	template <typename Buf>
	static binder_type<Buf> bind(Buf &buf, size_t offset = 0) { return binder_type<Buf>(tll::make_view(buf).view(offset)); }
};

} // namespace stream_scheme
