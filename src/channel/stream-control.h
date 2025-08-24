#pragma once

#include <tll/scheme/binder.h>
#include <tll/util/conv.h>

namespace stream_control_scheme {

static constexpr std::string_view scheme_string = R"(yamls+gz://eJxdzrEKwjAQBuC9T3FbFgNWxCFjwbmPUEJy1WByib1bSvHdTUAtuP3c9/NzGsgmNKBUB5CLhExsYFOuFN2Ei3WoqrMsaNPkMsmS48TujgnVq9PfgZFiIGwzwRvoj7tcyY/zELN7/LTfdcBboH8/1TAHjJ5NTQAatk89WpaJ8akOIGtpl0ByOddP3vZ7PPA=)";

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

struct BeginOfBlock
{
	static constexpr size_t meta_size() { return 8; }
	static constexpr std::string_view meta_name() { return "BeginOfBlock"; }
	static constexpr int meta_id() { return 12; }
	static constexpr size_t offset_last_seq = 0;

	template <typename Buf>
	struct binder_type : public tll::scheme::Binder<Buf>
	{
		using tll::scheme::Binder<Buf>::Binder;

		static constexpr auto meta_size() { return BeginOfBlock::meta_size(); }
		static constexpr auto meta_name() { return BeginOfBlock::meta_name(); }
		static constexpr auto meta_id() { return BeginOfBlock::meta_id(); }
		void view_resize() { this->_view_resize(meta_size()); }

		using type_last_seq = int64_t;
		type_last_seq get_last_seq() const { return this->template _get_scalar<type_last_seq>(offset_last_seq); }
		void set_last_seq(type_last_seq v) { return this->template _set_scalar<type_last_seq>(offset_last_seq, v); }
	};

	template <typename Buf>
	static binder_type<Buf> bind(Buf &buf, size_t offset = 0) { return binder_type<Buf>(tll::make_view(buf).view(offset)); }

	template <typename Buf>
	static binder_type<Buf> bind_reset(Buf &buf) { return tll::scheme::make_binder_reset<binder_type, Buf>(buf); }
};

} // namespace stream_control_scheme
