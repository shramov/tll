#pragma once

#include <tll/scheme/binder.h>
#include <tll/util/conv.h>

namespace large_item_scheme {

static constexpr std::string_view scheme_string = R"(yamls+gz://eJzTVchLzE21UlD3LEnNVedSUEjLTM1JKbYCshQUdBWqobJJ+SmV6joKJZUFYF5lSaqRmRlQIL+gJDM/r9hKoVodJKcOlCwuKcrMS1evreXShZntkliSCDI7M8VKwdAApyU5mcUlCEu0wC6q5QIAEYYvHA==)";

struct Item
{
	static constexpr size_t meta_size() { return 266; }
	static constexpr std::string_view meta_name() { return "Item"; }

	template <typename Buf>
	struct binder_type : public tll::scheme::Binder<Buf>
	{
		using tll::scheme::Binder<Buf>::Binder;

		static constexpr auto meta_size() { return Item::meta_size(); }
		static constexpr auto meta_name() { return Item::meta_name(); }
		void view_resize() { this->_view_resize(meta_size()); }

		std::string_view get_body() const { return this->template _get_bytestring<266>(0); }
		void set_body(std::string_view v) { return this->template _set_bytestring<266>(0, v); }
	};

	template <typename Buf>
	static binder_type<Buf> bind(Buf &buf, size_t offset = 0) { return binder_type<Buf>(tll::make_view(buf).view(offset)); }

	template <typename Buf>
	static binder_type<Buf> bind_reset(Buf &buf) { return tll::scheme::make_binder_reset<binder_type, Buf>(buf); }
};

struct Data
{
	static constexpr size_t meta_size() { return 8; }
	static constexpr std::string_view meta_name() { return "Data"; }
	static constexpr int meta_id() { return 10; }

	template <typename Buf>
	struct binder_type : public tll::scheme::Binder<Buf>
	{
		using tll::scheme::Binder<Buf>::Binder;

		static constexpr auto meta_size() { return Data::meta_size(); }
		static constexpr auto meta_name() { return Data::meta_name(); }
		static constexpr auto meta_id() { return Data::meta_id(); }
		void view_resize() { this->_view_resize(meta_size()); }

		using type_list = tll::scheme::binder::List<Buf, Item::binder_type<Buf>, tll_scheme_offset_ptr_t>;
		const type_list get_list() const { return this->template _get_binder<type_list>(0); }
		type_list get_list() { return this->template _get_binder<type_list>(0); }
	};

	template <typename Buf>
	static binder_type<Buf> bind(Buf &buf, size_t offset = 0) { return binder_type<Buf>(tll::make_view(buf).view(offset)); }

	template <typename Buf>
	static binder_type<Buf> bind_reset(Buf &buf) { return tll::scheme::make_binder_reset<binder_type, Buf>(buf); }
};

} // namespace large_item_scheme
