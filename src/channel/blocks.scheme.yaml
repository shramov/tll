#pragma once

#include <tll/scheme/binder.h>
#include <tll/util/conv.h>

namespace blocks_scheme {

static constexpr std::string_view scheme_string = R"(yamls+gz://eJzTVchLzE21UlB3yslPzlbnUlDITLFSMDQwALLSMlNzUoqtgCwFBV2FaqjCksqCVHUdBRAF5CVVlqSamQD5+QUlmfl5xVYK1RAVQLnikqLMvHT12louALk0HjA=)";

struct Block
{
	static constexpr size_t meta_size() { return 64; }
	static constexpr std::string_view meta_name() { return "Block"; }
	static constexpr int meta_id() { return 100; }

	template <typename Buf>
	struct binder_type : public tll::scheme::Binder<Buf>
	{
		using tll::scheme::Binder<Buf>::Binder;

		static constexpr auto meta_size() { return Block::meta_size(); }
		static constexpr auto meta_name() { return Block::meta_name(); }
		static constexpr auto meta_id() { return Block::meta_id(); }
		void view_resize() { this->_view_resize(meta_size()); }

		std::string_view get_type() const { return this->template _get_bytestring<64>(0); }
		void set_type(std::string_view v) { return this->template _set_bytestring<64>(0, v); }
	};

	template <typename Buf>
	static binder_type<Buf> bind(Buf &buf, size_t offset = 0) { return binder_type<Buf>(tll::make_view(buf).view(offset)); }
};

} // namespace blocks_scheme
