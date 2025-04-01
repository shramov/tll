#pragma once

#include <tll/scheme/binder.h>
#include <tll/util/conv.h>

namespace pub_scheme {

static constexpr std::string_view scheme_string = R"(yamls+gz://eJyljrEOgjAQQHe+4rZbaAKJYWB1cXZwJ3Jgk9LW9iAhhH/3QJSoo1OvvdeXp8BWHZWAmACQ7btYygCAFwpRO4slTDx6IbTlvEhXRt7w2IdAlmWfz3OiXpoTGeMWl65lk2UyNZpMvWkVTBs4bP4Unnpc/Th/YcuxM5GDtq3cnWf5HJcQ1zSRWHkOang3o6G2uo4q3lxg/Ak8kzfjXpn/Wxnp/oEUB0EeTstn9g==)";

enum class Version: int16_t
{
	Current = 1,
};

struct Hello
{
	static constexpr size_t meta_size() { return 10; }
	static constexpr std::string_view meta_name() { return "Hello"; }
	static constexpr int meta_id() { return 100; }
	static constexpr size_t offset_version = 0;
	static constexpr size_t offset_name = 2;

	template <typename Buf>
	struct binder_type : public tll::scheme::Binder<Buf>
	{
		using tll::scheme::Binder<Buf>::Binder;

		static constexpr auto meta_size() { return Hello::meta_size(); }
		static constexpr auto meta_name() { return Hello::meta_name(); }
		static constexpr auto meta_id() { return Hello::meta_id(); }
		void view_resize() { this->_view_resize(meta_size()); }

		using type_version = int16_t;
		type_version get_version() const { return this->template _get_scalar<type_version>(offset_version); }
		void set_version(type_version v) { return this->template _set_scalar<type_version>(offset_version, v); }

		std::string_view get_name() const { return this->template _get_string<tll_scheme_offset_ptr_t>(offset_name); }
		void set_name(std::string_view v) { return this->template _set_string<tll_scheme_offset_ptr_t>(offset_name, v); }
	};

	template <typename Buf>
	static binder_type<Buf> bind(Buf &buf, size_t offset = 0) { return binder_type<Buf>(tll::make_view(buf).view(offset)); }

	template <typename Buf>
	static binder_type<Buf> bind_reset(Buf &buf) { return tll::scheme::make_binder_reset<binder_type, Buf>(buf); }
};

struct HelloReply
{
	static constexpr size_t meta_size() { return 10; }
	static constexpr std::string_view meta_name() { return "HelloReply"; }
	static constexpr int meta_id() { return 101; }
	static constexpr size_t offset_version = 0;
	static constexpr size_t offset_seq = 2;

	template <typename Buf>
	struct binder_type : public tll::scheme::Binder<Buf>
	{
		using tll::scheme::Binder<Buf>::Binder;

		static constexpr auto meta_size() { return HelloReply::meta_size(); }
		static constexpr auto meta_name() { return HelloReply::meta_name(); }
		static constexpr auto meta_id() { return HelloReply::meta_id(); }
		void view_resize() { this->_view_resize(meta_size()); }

		using type_version = int16_t;
		type_version get_version() const { return this->template _get_scalar<type_version>(offset_version); }
		void set_version(type_version v) { return this->template _set_scalar<type_version>(offset_version, v); }

		using type_seq = int64_t;
		type_seq get_seq() const { return this->template _get_scalar<type_seq>(offset_seq); }
		void set_seq(type_seq v) { return this->template _set_scalar<type_seq>(offset_seq, v); }
	};

	template <typename Buf>
	static binder_type<Buf> bind(Buf &buf, size_t offset = 0) { return binder_type<Buf>(tll::make_view(buf).view(offset)); }

	template <typename Buf>
	static binder_type<Buf> bind_reset(Buf &buf) { return tll::scheme::make_binder_reset<binder_type, Buf>(buf); }
};

} // namespace pub_scheme

template <>
struct tll::conv::dump<pub_scheme::Version> : public to_string_from_string_buf<pub_scheme::Version>
{
	template <typename Buf>
	static inline std::string_view to_string_buf(const pub_scheme::Version &v, Buf &buf)
	{
		switch (v) {
		case pub_scheme::Version::Current: return "Current";
		default: break;
		}
		return tll::conv::to_string_buf<int16_t, Buf>((int16_t) v, buf);
	}
};
