#pragma once

#include <tll/scheme/binder.h>
#include <tll/util/conv.h>

namespace http_binder {

static constexpr std::string_view scheme_string = R"(yamls+gz://eJylVE1vwjAMvfMrcotUtdJgjDFuqHQDCRU0yhmVNtBIoYmaMIkh/vscCiX0Y0zaza6f3rOfnTooDXdkgDBuIcSFojyVA3TEkRCOrkgRRgRDPVFKrNY0jUmGT4Al6X4nBxAghHdEJTxeKcAd1UEAHU1V3z5jNJk7833PDaDcsxEeeVMv8CB5geTD05/bEI294QjCDoSzeTCZ+QvIXiGbDwN3DPGbjmcLjX/W4VJHXYiCz6Gr+UARL/2R9z7xPU31dDq1nOt8YxLq1qHhDSUsvrTuoOOlnuR1G+UDYKkymm7Po5qwr5DtSQ2q0Il4mpJIaSEaw2SNgrlpN6rCxLJkxGNDEZxt9yoYSb/vMb1uBSNClTycL7dB3nDWxbgycH1QxIDptF8PyoVKSNs8Nl3BRk+GnTGVJUc7jY7+xSiSZbxuy4XglMrb8p4ataQyVmcZXZYFGePm9CbURgzEHMMIvtlIohyhMufqCSPbMDo4Z5ZThVwmPFP/Z89pKvQyCllouGVdPS3ccrk4FItpdqv8uBpuisLVPjjijQmJ+X7NSPUxmBh9buf813srL/fxj4Dt5PbxK2F3I1n1M7E7PasQ/AGe1KbL)";

enum class method_t: int8_t
{
	UNDEFINED = 0,
	GET = 1,
	HEAD = 2,
	POST = 3,
	PUT = 4,
	DELETE = 5,
	CONNECT = 6,
	OPTIONS = 7,
	TRACE = 8,
	PATCH = 9,
};

struct Header
{
	static constexpr size_t meta_size() { return 16; }
	static constexpr std::string_view meta_name() { return "Header"; }
	static constexpr size_t offset_header = 0;
	static constexpr size_t offset_value = 8;

	template <typename Buf>
	struct binder_type : public tll::scheme::Binder<Buf>
	{
		using tll::scheme::Binder<Buf>::Binder;

		static constexpr auto meta_size() { return Header::meta_size(); }
		static constexpr auto meta_name() { return Header::meta_name(); }
		void view_resize() { this->_view_resize(meta_size()); }

		template <typename RBuf>
		void copy(const binder_type<RBuf> &rhs)
		{
			set_header(rhs.get_header());
			set_value(rhs.get_value());
		}

		std::string_view get_header() const { return this->template _get_string<tll_scheme_offset_ptr_t>(offset_header); }
		void set_header(std::string_view v) { return this->template _set_string<tll_scheme_offset_ptr_t>(offset_header, v); }

		std::string_view get_value() const { return this->template _get_string<tll_scheme_offset_ptr_t>(offset_value); }
		void set_value(std::string_view v) { return this->template _set_string<tll_scheme_offset_ptr_t>(offset_value, v); }
	};

	template <typename Buf>
	static binder_type<Buf> bind(Buf &buf, size_t offset = 0) { return binder_type<Buf>(tll::make_view(buf).view(offset)); }

	template <typename Buf>
	static binder_type<Buf> bind_reset(Buf &buf) { return tll::scheme::make_binder_reset<binder_type, Buf>(buf); }
};

struct connect
{
	static constexpr size_t meta_size() { return 43; }
	static constexpr std::string_view meta_name() { return "connect"; }
	static constexpr int meta_id() { return 1; }
	static constexpr size_t offset_method = 0;
	static constexpr size_t offset_code = 1;
	static constexpr size_t offset_size = 3;
	static constexpr size_t offset_path = 11;
	static constexpr size_t offset_headers = 19;
	static constexpr size_t offset_bytes = 27;
	static constexpr size_t offset_bytestring = 35;

	template <typename Buf>
	struct binder_type : public tll::scheme::Binder<Buf>
	{
		using tll::scheme::Binder<Buf>::Binder;

		static constexpr auto meta_size() { return connect::meta_size(); }
		static constexpr auto meta_name() { return connect::meta_name(); }
		static constexpr auto meta_id() { return connect::meta_id(); }
		void view_resize() { this->_view_resize(meta_size()); }

		template <typename RBuf>
		void copy(const binder_type<RBuf> &rhs)
		{
			set_method(rhs.get_method());
			set_code(rhs.get_code());
			set_size(rhs.get_size());
			set_path(rhs.get_path());
			get_headers().copy(rhs.get_headers());
			get_bytes().copy(rhs.get_bytes());
			set_bytestring(rhs.get_bytestring());
		}

		using type_method = method_t;
		type_method get_method() const { return this->template _get_scalar<type_method>(offset_method); }
		void set_method(type_method v) { return this->template _set_scalar<type_method>(offset_method, v); }

		using type_code = int16_t;
		type_code get_code() const { return this->template _get_scalar<type_code>(offset_code); }
		void set_code(type_code v) { return this->template _set_scalar<type_code>(offset_code, v); }

		using type_size = int64_t;
		type_size get_size() const { return this->template _get_scalar<type_size>(offset_size); }
		void set_size(type_size v) { return this->template _set_scalar<type_size>(offset_size, v); }

		std::string_view get_path() const { return this->template _get_string<tll_scheme_offset_ptr_t>(offset_path); }
		void set_path(std::string_view v) { return this->template _set_string<tll_scheme_offset_ptr_t>(offset_path, v); }

		using type_headers = tll::scheme::binder::List<Buf, Header::binder_type<Buf>, tll_scheme_offset_ptr_t>;
		using const_type_headers = tll::scheme::binder::List<const Buf, Header::binder_type<const Buf>, tll_scheme_offset_ptr_t>;
		const_type_headers get_headers() const { return this->template _get_binder<const_type_headers>(offset_headers); }
		type_headers get_headers() { return this->template _get_binder<type_headers>(offset_headers); }

		const tll::scheme::Bytes<8> & get_bytes() const { return this->template _get_bytes<8>(offset_bytes); }
		void set_bytes(const tll::scheme::Bytes<8> &v) const { return this->template _set_bytes<8>(offset_bytes, {v.data(), v.size()}); }
		void set_bytes(std::string_view v) { return this->template _set_bytestring<8>(offset_bytes, v); }

		std::string_view get_bytestring() const { return this->template _get_bytestring<8>(offset_bytestring); }
		void set_bytestring(std::string_view v) { return this->template _set_bytestring<8>(offset_bytestring, v); }
	};

	template <typename Buf>
	static binder_type<Buf> bind(Buf &buf, size_t offset = 0) { return binder_type<Buf>(tll::make_view(buf).view(offset)); }

	template <typename Buf>
	static binder_type<Buf> bind_reset(Buf &buf) { return tll::scheme::make_binder_reset<binder_type, Buf>(buf); }
};

struct disconnect
{
	static constexpr size_t meta_size() { return 10; }
	static constexpr std::string_view meta_name() { return "disconnect"; }
	static constexpr int meta_id() { return 2; }
	static constexpr size_t offset_code = 0;
	static constexpr size_t offset_error = 2;

	template <typename Buf>
	struct binder_type : public tll::scheme::Binder<Buf>
	{
		using tll::scheme::Binder<Buf>::Binder;

		static constexpr auto meta_size() { return disconnect::meta_size(); }
		static constexpr auto meta_name() { return disconnect::meta_name(); }
		static constexpr auto meta_id() { return disconnect::meta_id(); }
		void view_resize() { this->_view_resize(meta_size()); }

		template <typename RBuf>
		void copy(const binder_type<RBuf> &rhs)
		{
			set_code(rhs.get_code());
			set_error(rhs.get_error());
		}

		using type_code = int16_t;
		type_code get_code() const { return this->template _get_scalar<type_code>(offset_code); }
		void set_code(type_code v) { return this->template _set_scalar<type_code>(offset_code, v); }

		std::string_view get_error() const { return this->template _get_string<tll_scheme_offset_ptr_t>(offset_error); }
		void set_error(std::string_view v) { return this->template _set_string<tll_scheme_offset_ptr_t>(offset_error, v); }
	};

	template <typename Buf>
	static binder_type<Buf> bind(Buf &buf, size_t offset = 0) { return binder_type<Buf>(tll::make_view(buf).view(offset)); }

	template <typename Buf>
	static binder_type<Buf> bind_reset(Buf &buf) { return tll::scheme::make_binder_reset<binder_type, Buf>(buf); }
};

struct List
{
	static constexpr size_t meta_size() { return 28; }
	static constexpr std::string_view meta_name() { return "List"; }
	static constexpr int meta_id() { return 10; }
	static constexpr size_t offset_std = 0;
	static constexpr size_t offset_llong = 8;
	static constexpr size_t offset_lshort = 16;
	static constexpr size_t offset_scalar = 20;

	template <typename Buf>
	struct binder_type : public tll::scheme::Binder<Buf>
	{
		using tll::scheme::Binder<Buf>::Binder;

		static constexpr auto meta_size() { return List::meta_size(); }
		static constexpr auto meta_name() { return List::meta_name(); }
		static constexpr auto meta_id() { return List::meta_id(); }
		void view_resize() { this->_view_resize(meta_size()); }

		template <typename RBuf>
		void copy(const binder_type<RBuf> &rhs)
		{
			get_std().copy(rhs.get_std());
			get_llong().copy(rhs.get_llong());
			get_lshort().copy(rhs.get_lshort());
			get_scalar().copy(rhs.get_scalar());
		}

		using type_std = tll::scheme::binder::List<Buf, disconnect::binder_type<Buf>, tll_scheme_offset_ptr_t>;
		using const_type_std = tll::scheme::binder::List<const Buf, disconnect::binder_type<const Buf>, tll_scheme_offset_ptr_t>;
		const_type_std get_std() const { return this->template _get_binder<const_type_std>(offset_std); }
		type_std get_std() { return this->template _get_binder<type_std>(offset_std); }

		using type_llong = tll::scheme::binder::List<Buf, disconnect::binder_type<Buf>, tll_scheme_offset_ptr_legacy_long_t>;
		using const_type_llong = tll::scheme::binder::List<const Buf, disconnect::binder_type<const Buf>, tll_scheme_offset_ptr_legacy_long_t>;
		const_type_llong get_llong() const { return this->template _get_binder<const_type_llong>(offset_llong); }
		type_llong get_llong() { return this->template _get_binder<type_llong>(offset_llong); }

		using type_lshort = tll::scheme::binder::List<Buf, disconnect::binder_type<Buf>, tll_scheme_offset_ptr_legacy_short_t>;
		using const_type_lshort = tll::scheme::binder::List<const Buf, disconnect::binder_type<const Buf>, tll_scheme_offset_ptr_legacy_short_t>;
		const_type_lshort get_lshort() const { return this->template _get_binder<const_type_lshort>(offset_lshort); }
		type_lshort get_lshort() { return this->template _get_binder<type_lshort>(offset_lshort); }

		using type_scalar = tll::scheme::binder::List<Buf, int16_t, tll_scheme_offset_ptr_t>;
		using const_type_scalar = tll::scheme::binder::List<const Buf, int16_t, tll_scheme_offset_ptr_t>;
		const_type_scalar get_scalar() const { return this->template _get_binder<const_type_scalar>(offset_scalar); }
		type_scalar get_scalar() { return this->template _get_binder<type_scalar>(offset_scalar); }
	};

	template <typename Buf>
	static binder_type<Buf> bind(Buf &buf, size_t offset = 0) { return binder_type<Buf>(tll::make_view(buf).view(offset)); }

	template <typename Buf>
	static binder_type<Buf> bind_reset(Buf &buf) { return tll::scheme::make_binder_reset<binder_type, Buf>(buf); }
};

struct Copy
{
	static constexpr size_t meta_size() { return 128; }
	static constexpr std::string_view meta_name() { return "Copy"; }
	static constexpr int meta_id() { return 20; }
	static constexpr size_t offset_header = 0;
	static constexpr size_t offset_i64 = 16;
	static constexpr size_t offset_f64 = 24;
	static constexpr size_t offset_s64 = 32;
	static constexpr size_t offset_str = 96;
	static constexpr size_t offset_lmsg = 104;
	static constexpr size_t offset_li64 = 112;
	static constexpr size_t offset_lstr = 120;

	template <typename Buf>
	struct binder_type : public tll::scheme::Binder<Buf>
	{
		using tll::scheme::Binder<Buf>::Binder;

		static constexpr auto meta_size() { return Copy::meta_size(); }
		static constexpr auto meta_name() { return Copy::meta_name(); }
		static constexpr auto meta_id() { return Copy::meta_id(); }
		void view_resize() { this->_view_resize(meta_size()); }

		template <typename RBuf>
		void copy(const binder_type<RBuf> &rhs)
		{
			get_header().copy(rhs.get_header());
			set_i64(rhs.get_i64());
			set_f64(rhs.get_f64());
			set_s64(rhs.get_s64());
			set_str(rhs.get_str());
			get_lmsg().copy(rhs.get_lmsg());
			get_li64().copy(rhs.get_li64());
			get_lstr().copy(rhs.get_lstr());
		}

		using type_header = Header::binder_type<Buf>;
		using const_type_header = Header::binder_type<const Buf>;
		const_type_header get_header() const { return this->template _get_binder<const_type_header>(offset_header); }
		type_header get_header() { return this->template _get_binder<type_header>(offset_header); }

		using type_i64 = int64_t;
		type_i64 get_i64() const { return this->template _get_scalar<type_i64>(offset_i64); }
		void set_i64(type_i64 v) { return this->template _set_scalar<type_i64>(offset_i64, v); }

		using type_f64 = double;
		type_f64 get_f64() const { return this->template _get_scalar<type_f64>(offset_f64); }
		void set_f64(type_f64 v) { return this->template _set_scalar<type_f64>(offset_f64, v); }

		std::string_view get_s64() const { return this->template _get_bytestring<64>(offset_s64); }
		void set_s64(std::string_view v) { return this->template _set_bytestring<64>(offset_s64, v); }

		std::string_view get_str() const { return this->template _get_string<tll_scheme_offset_ptr_t>(offset_str); }
		void set_str(std::string_view v) { return this->template _set_string<tll_scheme_offset_ptr_t>(offset_str, v); }

		using type_lmsg = tll::scheme::binder::List<Buf, Header::binder_type<Buf>, tll_scheme_offset_ptr_t>;
		using const_type_lmsg = tll::scheme::binder::List<const Buf, Header::binder_type<const Buf>, tll_scheme_offset_ptr_t>;
		const_type_lmsg get_lmsg() const { return this->template _get_binder<const_type_lmsg>(offset_lmsg); }
		type_lmsg get_lmsg() { return this->template _get_binder<type_lmsg>(offset_lmsg); }

		using type_li64 = tll::scheme::binder::List<Buf, int64_t, tll_scheme_offset_ptr_t>;
		using const_type_li64 = tll::scheme::binder::List<const Buf, int64_t, tll_scheme_offset_ptr_t>;
		const_type_li64 get_li64() const { return this->template _get_binder<const_type_li64>(offset_li64); }
		type_li64 get_li64() { return this->template _get_binder<type_li64>(offset_li64); }

		using type_lstr = tll::scheme::binder::List<Buf, tll::scheme::binder::String<Buf, tll_scheme_offset_ptr_t>, tll_scheme_offset_ptr_t>;
		using const_type_lstr = tll::scheme::binder::List<const Buf, tll::scheme::binder::String<const Buf, tll_scheme_offset_ptr_t>, tll_scheme_offset_ptr_t>;
		const_type_lstr get_lstr() const { return this->template _get_binder<const_type_lstr>(offset_lstr); }
		type_lstr get_lstr() { return this->template _get_binder<type_lstr>(offset_lstr); }
	};

	template <typename Buf>
	static binder_type<Buf> bind(Buf &buf, size_t offset = 0) { return binder_type<Buf>(tll::make_view(buf).view(offset)); }

	template <typename Buf>
	static binder_type<Buf> bind_reset(Buf &buf) { return tll::scheme::make_binder_reset<binder_type, Buf>(buf); }
};

} // namespace http_binder

template <>
struct tll::conv::dump<http_binder::method_t> : public to_string_from_string_buf<http_binder::method_t>
{
	template <typename Buf>
	static inline std::string_view to_string_buf(const http_binder::method_t &v, Buf &buf)
	{
		switch (v) {
		case http_binder::method_t::CONNECT: return "CONNECT";
		case http_binder::method_t::DELETE: return "DELETE";
		case http_binder::method_t::GET: return "GET";
		case http_binder::method_t::HEAD: return "HEAD";
		case http_binder::method_t::OPTIONS: return "OPTIONS";
		case http_binder::method_t::PATCH: return "PATCH";
		case http_binder::method_t::POST: return "POST";
		case http_binder::method_t::PUT: return "PUT";
		case http_binder::method_t::TRACE: return "TRACE";
		case http_binder::method_t::UNDEFINED: return "UNDEFINED";
		default: break;
		}
		return tll::conv::to_string_buf<int8_t, Buf>((int8_t) v, buf);
	}
};
