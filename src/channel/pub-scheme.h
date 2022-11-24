#pragma once

#include <tll/scheme/binder.h>
#include <tll/util/conv.h>

namespace pub_scheme {

static constexpr std::string_view scheme_string = R"(yamls+gz://eJyVjrEOgyAQQHe/4rZbJJGlg2uXzh26m3paEgQCp4kx/nsPa2vSTp04uMfLU+CagWpALADIjUOqZQDAG8VkvMMaFp6DEMaxPpUbI294HmMkx7LX61qot+ZC1vrsMq1sqkqmzpBtd62CZQen3V/CS4+bH9cvLB8Hkzga18vdB5bPKYf4rkvEKnBU06cZLfXNfVbp4SPjT+CVgp2PSv1/5RPE0V4q)";

enum class Version: int16_t
{
	Current = 1,
};

struct Hello
{
	static constexpr size_t meta_size() { return 10; }
	static constexpr std::string_view meta_name() { return "Hello"; }
	static constexpr int meta_id() { return 100; }

	template <typename Buf>
	struct binder_type : public tll::scheme::Binder<Buf>
	{
		using tll::scheme::Binder<Buf>::Binder;

		static constexpr auto meta_size() { return Hello::meta_size(); }
		static constexpr auto meta_name() { return Hello::meta_name(); }
		static constexpr auto meta_id() { return Hello::meta_id(); }
		void view_resize() { this->_view_resize(meta_size()); }

		using type_version = int16_t;
		type_version get_version() const { return this->template _get_scalar<type_version>(0); }
		void set_version(type_version v) { return this->template _set_scalar<type_version>(0, v); }

		std::string_view get_name() const { return this->template _get_string<tll_scheme_offset_ptr_t>(2); }
		void set_name(std::string_view v) { return this->template _set_string<tll_scheme_offset_ptr_t>(2, v); }
	};

	template <typename Buf>
	static binder_type<Buf> bind(Buf &buf, size_t offset = 0) { return binder_type<Buf>(tll::make_view(buf).view(offset)); }
};

struct HelloReply
{
	static constexpr size_t meta_size() { return 2; }
	static constexpr std::string_view meta_name() { return "HelloReply"; }
	static constexpr int meta_id() { return 101; }

	template <typename Buf>
	struct binder_type : public tll::scheme::Binder<Buf>
	{
		using tll::scheme::Binder<Buf>::Binder;

		static constexpr auto meta_size() { return HelloReply::meta_size(); }
		static constexpr auto meta_name() { return HelloReply::meta_name(); }
		static constexpr auto meta_id() { return HelloReply::meta_id(); }
		void view_resize() { this->_view_resize(meta_size()); }

		using type_version = int16_t;
		type_version get_version() const { return this->template _get_scalar<type_version>(0); }
		void set_version(type_version v) { return this->template _set_scalar<type_version>(0, v); }
	};

	template <typename Buf>
	static binder_type<Buf> bind(Buf &buf, size_t offset = 0) { return binder_type<Buf>(tll::make_view(buf).view(offset)); }
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
