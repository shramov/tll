#pragma once

#include <tll/scheme/binder.h>
#include <tll/util/conv.h>

namespace stream_control_scheme {

static constexpr std::string_view scheme_string = R"(yamls+gz://eJzTVchLzE21UlD3z8vJzEtV51JQyEyxUjA04AIAZKoG2A==)";

struct Online
{
	static constexpr size_t meta_size() { return 0; }
	static constexpr std::string_view meta_name() { return "Online"; }
	static constexpr int meta_id() { return 10; }

	template <typename Buf>
	struct binder_type : public tll::scheme::Binder<Buf>
	{
		using tll::scheme::Binder<Buf>::Binder;

		static constexpr auto meta_size() { return Online::meta_size(); }
		static constexpr auto meta_name() { return Online::meta_name(); }
		static constexpr auto meta_id() { return Online::meta_id(); }
		void view_resize() { this->_view_resize(meta_size()); }
	};

	template <typename Buf>
	static binder_type<Buf> bind(Buf &buf, size_t offset = 0) { return binder_type<Buf>(tll::make_view(buf).view(offset)); }
};

} // namespace stream_control_scheme
