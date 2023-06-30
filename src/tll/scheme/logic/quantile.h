#pragma once

#include <tll/scheme/binder.h>
#include <tll/util/conv.h>

namespace quantile_scheme {

static constexpr std::string_view scheme_string = R"(yamls+gz://eJxdjDEKgDAQBHtfsd01CgoiktqPRIxyoEkwpyCSv5uAhVjtDLtsBas3o0CDFk0FwJNCUyeY2axTUImACvc7y0El5PLZxktMn9R5YWeDwk25oVQF2dkuFOPv4NTr8Xk42ErXUiwegFYoUQ==)";

struct Data
{
	static constexpr size_t meta_size() { return 16; }
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

		std::string_view get_name() const { return this->template _get_bytestring<8>(0); }
		void set_name(std::string_view v) { return this->template _set_bytestring<8>(0, v); }

		using type_value = uint64_t;
		type_value get_value() const { return this->template _get_scalar<type_value>(8); }
		void set_value(type_value v) { return this->template _set_scalar<type_value>(8, v); }
	};

	template <typename Buf>
	static binder_type<Buf> bind(Buf &buf, size_t offset = 0) { return binder_type<Buf>(tll::make_view(buf).view(offset)); }
};

} // namespace quantile_scheme
