#pragma once

#include <tll/scheme/binder.h>
#include <tll/util/conv.h>

namespace stream_control_scheme {

static constexpr std::string_view scheme_string = R"(yamls+gz://eJxFzDEKgDAMAMC9r8iWqaBrR8HZJ0iJEYttUmw38e/aQZ0PzoL4xA4QDYDmGlSKgxMpZ9ukZE+Mj5d6sE8zqdRD41xo48R4GfsGk8Qg3JqwOOi7X0ZZpnWISvunvbkBPt4lHQ==)";

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

	template <typename Buf>
	static binder_type<Buf> bind_reset(Buf &buf) { return tll::scheme::make_binder_reset<binder_type, Buf>(buf); }
};

struct EndOfBlock
{
	static constexpr size_t meta_size() { return 0; }
	static constexpr std::string_view meta_name() { return "EndOfBlock"; }
	static constexpr int meta_id() { return 11; }

	template <typename Buf>
	struct binder_type : public tll::scheme::Binder<Buf>
	{
		using tll::scheme::Binder<Buf>::Binder;

		static constexpr auto meta_size() { return EndOfBlock::meta_size(); }
		static constexpr auto meta_name() { return EndOfBlock::meta_name(); }
		static constexpr auto meta_id() { return EndOfBlock::meta_id(); }
		void view_resize() { this->_view_resize(meta_size()); }
	};

	template <typename Buf>
	static binder_type<Buf> bind(Buf &buf, size_t offset = 0) { return binder_type<Buf>(tll::make_view(buf).view(offset)); }

	template <typename Buf>
	static binder_type<Buf> bind_reset(Buf &buf) { return tll::scheme::make_binder_reset<binder_type, Buf>(buf); }
};

} // namespace stream_control_scheme
