#pragma once

#include <tll/scheme/binder.h>
#include <tll/util/conv.h>

namespace resolve_scheme {

static constexpr std::string_view scheme_string = R"(yamls+gz://eJzFkksLwjAQhO/+ir0FxILiA+lVPXlT9N7Htg3WpCZpsYj/3VrSKJEqKMXbLgzzzSzrAPOO6AJZY7n30hxJDyCimIbSrSYABy5accCSDECV2X2RSlAWk6ulKWqLV5XTYFbnjAu1RVHQoGbR0IXRsBUqtfITWHmxfIj6LaqES/Uu3VLwzMo2/iabVXeReIxh2lhOfq8baMdOz2IhOYto/GRmPsY6oNV2+v+2Jt4GTzlWXXW0WZfRDHTHZO7LQFDfPNW8S/ANQGol5A==)";

struct KeyValue
{
	static constexpr size_t meta_size() { return 16; }
	static constexpr std::string_view meta_name() { return "KeyValue"; }

	template <typename Buf>
	struct binder_type : public tll::scheme::Binder<Buf>
	{
		using tll::scheme::Binder<Buf>::Binder;

		static constexpr auto meta_size() { return KeyValue::meta_size(); }
		static constexpr auto meta_name() { return KeyValue::meta_name(); }
		void view_resize() { this->_view_resize(meta_size()); }

		std::string_view get_key() const { return this->template _get_string<tll_scheme_offset_ptr_t>(0); }
		void set_key(std::string_view v) { return this->template _set_string<tll_scheme_offset_ptr_t>(0, v); }

		std::string_view get_value() const { return this->template _get_string<tll_scheme_offset_ptr_t>(8); }
		void set_value(std::string_view v) { return this->template _set_string<tll_scheme_offset_ptr_t>(8, v); }
	};

	template <typename Buf>
	static binder_type<Buf> bind(Buf &buf, size_t offset = 0) { return binder_type<Buf>(tll::make_view(buf).view(offset)); }

	template <typename Buf>
	static binder_type<Buf> bind_reset(Buf &buf) { return tll::scheme::make_binder_reset<binder_type, Buf>(buf); }
};

struct ExportService
{
	static constexpr size_t meta_size() { return 24; }
	static constexpr std::string_view meta_name() { return "ExportService"; }
	static constexpr int meta_id() { return 10; }

	template <typename Buf>
	struct binder_type : public tll::scheme::Binder<Buf>
	{
		using tll::scheme::Binder<Buf>::Binder;

		static constexpr auto meta_size() { return ExportService::meta_size(); }
		static constexpr auto meta_name() { return ExportService::meta_name(); }
		static constexpr auto meta_id() { return ExportService::meta_id(); }
		void view_resize() { this->_view_resize(meta_size()); }

		std::string_view get_service() const { return this->template _get_string<tll_scheme_offset_ptr_t>(0); }
		void set_service(std::string_view v) { return this->template _set_string<tll_scheme_offset_ptr_t>(0, v); }

		using type_tags = tll::scheme::binder::List<Buf, tll::scheme::binder::String<Buf, tll_scheme_offset_ptr_t>, tll_scheme_offset_ptr_t>;
		const type_tags get_tags() const { return this->template _get_binder<type_tags>(8); }
		type_tags get_tags() { return this->template _get_binder<type_tags>(8); }

		std::string_view get_host() const { return this->template _get_string<tll_scheme_offset_ptr_t>(16); }
		void set_host(std::string_view v) { return this->template _set_string<tll_scheme_offset_ptr_t>(16, v); }
	};

	template <typename Buf>
	static binder_type<Buf> bind(Buf &buf, size_t offset = 0) { return binder_type<Buf>(tll::make_view(buf).view(offset)); }

	template <typename Buf>
	static binder_type<Buf> bind_reset(Buf &buf) { return tll::scheme::make_binder_reset<binder_type, Buf>(buf); }
};

struct DropService
{
	static constexpr size_t meta_size() { return 8; }
	static constexpr std::string_view meta_name() { return "DropService"; }
	static constexpr int meta_id() { return 30; }

	template <typename Buf>
	struct binder_type : public tll::scheme::Binder<Buf>
	{
		using tll::scheme::Binder<Buf>::Binder;

		static constexpr auto meta_size() { return DropService::meta_size(); }
		static constexpr auto meta_name() { return DropService::meta_name(); }
		static constexpr auto meta_id() { return DropService::meta_id(); }
		void view_resize() { this->_view_resize(meta_size()); }

		std::string_view get_service() const { return this->template _get_string<tll_scheme_offset_ptr_t>(0); }
		void set_service(std::string_view v) { return this->template _set_string<tll_scheme_offset_ptr_t>(0, v); }
	};

	template <typename Buf>
	static binder_type<Buf> bind(Buf &buf, size_t offset = 0) { return binder_type<Buf>(tll::make_view(buf).view(offset)); }

	template <typename Buf>
	static binder_type<Buf> bind_reset(Buf &buf) { return tll::scheme::make_binder_reset<binder_type, Buf>(buf); }
};

struct ExportChannel
{
	static constexpr size_t meta_size() { return 40; }
	static constexpr std::string_view meta_name() { return "ExportChannel"; }
	static constexpr int meta_id() { return 40; }

	template <typename Buf>
	struct binder_type : public tll::scheme::Binder<Buf>
	{
		using tll::scheme::Binder<Buf>::Binder;

		static constexpr auto meta_size() { return ExportChannel::meta_size(); }
		static constexpr auto meta_name() { return ExportChannel::meta_name(); }
		static constexpr auto meta_id() { return ExportChannel::meta_id(); }
		void view_resize() { this->_view_resize(meta_size()); }

		std::string_view get_service() const { return this->template _get_string<tll_scheme_offset_ptr_t>(0); }
		void set_service(std::string_view v) { return this->template _set_string<tll_scheme_offset_ptr_t>(0, v); }

		std::string_view get_channel() const { return this->template _get_string<tll_scheme_offset_ptr_t>(8); }
		void set_channel(std::string_view v) { return this->template _set_string<tll_scheme_offset_ptr_t>(8, v); }

		using type_tags = tll::scheme::binder::List<Buf, tll::scheme::binder::String<Buf, tll_scheme_offset_ptr_t>, tll_scheme_offset_ptr_t>;
		const type_tags get_tags() const { return this->template _get_binder<type_tags>(16); }
		type_tags get_tags() { return this->template _get_binder<type_tags>(16); }

		std::string_view get_host() const { return this->template _get_string<tll_scheme_offset_ptr_t>(24); }
		void set_host(std::string_view v) { return this->template _set_string<tll_scheme_offset_ptr_t>(24, v); }

		using type_config = tll::scheme::binder::List<Buf, KeyValue::binder_type<Buf>, tll_scheme_offset_ptr_t>;
		const type_config get_config() const { return this->template _get_binder<type_config>(32); }
		type_config get_config() { return this->template _get_binder<type_config>(32); }
	};

	template <typename Buf>
	static binder_type<Buf> bind(Buf &buf, size_t offset = 0) { return binder_type<Buf>(tll::make_view(buf).view(offset)); }

	template <typename Buf>
	static binder_type<Buf> bind_reset(Buf &buf) { return tll::scheme::make_binder_reset<binder_type, Buf>(buf); }
};

struct DropChannel
{
	static constexpr size_t meta_size() { return 24; }
	static constexpr std::string_view meta_name() { return "DropChannel"; }
	static constexpr int meta_id() { return 50; }

	template <typename Buf>
	struct binder_type : public tll::scheme::Binder<Buf>
	{
		using tll::scheme::Binder<Buf>::Binder;

		static constexpr auto meta_size() { return DropChannel::meta_size(); }
		static constexpr auto meta_name() { return DropChannel::meta_name(); }
		static constexpr auto meta_id() { return DropChannel::meta_id(); }
		void view_resize() { this->_view_resize(meta_size()); }

		std::string_view get_service() const { return this->template _get_string<tll_scheme_offset_ptr_t>(0); }
		void set_service(std::string_view v) { return this->template _set_string<tll_scheme_offset_ptr_t>(0, v); }

		std::string_view get_channel() const { return this->template _get_string<tll_scheme_offset_ptr_t>(8); }
		void set_channel(std::string_view v) { return this->template _set_string<tll_scheme_offset_ptr_t>(8, v); }

		using type_tags = tll::scheme::binder::List<Buf, tll::scheme::binder::String<Buf, tll_scheme_offset_ptr_t>, tll_scheme_offset_ptr_t>;
		const type_tags get_tags() const { return this->template _get_binder<type_tags>(16); }
		type_tags get_tags() { return this->template _get_binder<type_tags>(16); }
	};

	template <typename Buf>
	static binder_type<Buf> bind(Buf &buf, size_t offset = 0) { return binder_type<Buf>(tll::make_view(buf).view(offset)); }

	template <typename Buf>
	static binder_type<Buf> bind_reset(Buf &buf) { return tll::scheme::make_binder_reset<binder_type, Buf>(buf); }
};

struct Request
{
	static constexpr size_t meta_size() { return 16; }
	static constexpr std::string_view meta_name() { return "Request"; }
	static constexpr int meta_id() { return 60; }

	template <typename Buf>
	struct binder_type : public tll::scheme::Binder<Buf>
	{
		using tll::scheme::Binder<Buf>::Binder;

		static constexpr auto meta_size() { return Request::meta_size(); }
		static constexpr auto meta_name() { return Request::meta_name(); }
		static constexpr auto meta_id() { return Request::meta_id(); }
		void view_resize() { this->_view_resize(meta_size()); }

		std::string_view get_service() const { return this->template _get_string<tll_scheme_offset_ptr_t>(0); }
		void set_service(std::string_view v) { return this->template _set_string<tll_scheme_offset_ptr_t>(0, v); }

		std::string_view get_channel() const { return this->template _get_string<tll_scheme_offset_ptr_t>(8); }
		void set_channel(std::string_view v) { return this->template _set_string<tll_scheme_offset_ptr_t>(8, v); }
	};

	template <typename Buf>
	static binder_type<Buf> bind(Buf &buf, size_t offset = 0) { return binder_type<Buf>(tll::make_view(buf).view(offset)); }

	template <typename Buf>
	static binder_type<Buf> bind_reset(Buf &buf) { return tll::scheme::make_binder_reset<binder_type, Buf>(buf); }
};

struct Unsubscribe
{
	static constexpr size_t meta_size() { return 16; }
	static constexpr std::string_view meta_name() { return "Unsubscribe"; }
	static constexpr int meta_id() { return 80; }

	template <typename Buf>
	struct binder_type : public tll::scheme::Binder<Buf>
	{
		using tll::scheme::Binder<Buf>::Binder;

		static constexpr auto meta_size() { return Unsubscribe::meta_size(); }
		static constexpr auto meta_name() { return Unsubscribe::meta_name(); }
		static constexpr auto meta_id() { return Unsubscribe::meta_id(); }
		void view_resize() { this->_view_resize(meta_size()); }

		std::string_view get_service() const { return this->template _get_string<tll_scheme_offset_ptr_t>(0); }
		void set_service(std::string_view v) { return this->template _set_string<tll_scheme_offset_ptr_t>(0, v); }

		std::string_view get_channel() const { return this->template _get_string<tll_scheme_offset_ptr_t>(8); }
		void set_channel(std::string_view v) { return this->template _set_string<tll_scheme_offset_ptr_t>(8, v); }
	};

	template <typename Buf>
	static binder_type<Buf> bind(Buf &buf, size_t offset = 0) { return binder_type<Buf>(tll::make_view(buf).view(offset)); }

	template <typename Buf>
	static binder_type<Buf> bind_reset(Buf &buf) { return tll::scheme::make_binder_reset<binder_type, Buf>(buf); }
};

} // namespace resolve_scheme
