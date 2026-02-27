#pragma once

#include <tll/scheme/binder.h>
#include <tll/util/conv.h>

namespace stream_server_control_scheme {

static constexpr std::string_view scheme_string = R"(yamls+gz://eJw1zDEKwCAMAMDdV2TLJNTVrS8RSQMVqgYTXEr/3jp0PjgPLVeOgOgAuljpTSPcSCJ+iUomxs/VBuealMfkkag3G/1KSidXxsf5/9nJyszG6ytHhBA29wKw1CDm)";

struct Activate
{
	static constexpr size_t meta_size() { return 0; }
	static constexpr std::string_view meta_name() { return "Activate"; }
	static constexpr int meta_id() { return 110; }

	template <typename Buf>
	struct binder_type : public tll::scheme::Binder<Buf>
	{
		using tll::scheme::Binder<Buf>::Binder;

		static constexpr auto meta_size() { return Activate::meta_size(); }
		static constexpr auto meta_name() { return Activate::meta_name(); }
		static constexpr auto meta_id() { return Activate::meta_id(); }
		void view_resize() { this->_view_resize(meta_size()); }

		template <typename RBuf>
		void copy(const binder_type<RBuf> &rhs)
		{
		}
	};

	template <typename Buf>
	static binder_type<Buf> bind(Buf &buf, size_t offset = 0) { return binder_type<Buf>(tll::make_view(buf).view(offset)); }

	template <typename Buf>
	static binder_type<Buf> bind_reset(Buf &buf) { return tll::scheme::make_binder_reset<binder_type, Buf>(buf); }
};

} // namespace stream_server_control_scheme
