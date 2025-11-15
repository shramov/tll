#pragma once

#include <tll/scheme/binder.h>
#include <tll/util/conv.h>

namespace stream_scheme {

static constexpr std::string_view scheme_string = R"(yamls+gz://eJylk8tqwzAQRff5Cu0ExYY6DaF41yZdF7roppSg2NNWVJYdPUKD8b9Xkl9xYtWG7oR1NHfunXGIOMkgRhgvEMoLRXMuY1TipChCeyMLkgA291IJINlOJl+QAa4MDVxnMjYHhPArCGmeGrBUp8LUo1xF68AxttxGCwFcmfuoqhZhK/qglKB7rcCqf1BgaVMwRGWDkA4JUF3atkL5p+vhnDwSpseoTu0FDhqkemaplaOpaSby6h4bR1291uKlrIRDDxnb69UVkjBqzU8Z2LM8+Z6kukBkj970QV77fXRlvUbnqVKews+1z0utLtilOWjutqnekC1RxK6H+xijt9H4dFM38HU3sHSGUU4VJWzQ4D2u3qv/z3fm8CbHMsRTG0cHunAGaRbs1Ga5vPWaYESq3YwFFHVskM6BXeRzQAnCpPjXH/ckRC5aH3d+H+C4qYin9TZuVtucQyu68ouOOvwF70iI3g==)";

enum class Version: int16_t
{
	Current = 1,
};

struct Attribute
{
	static constexpr size_t meta_size() { return 16; }
	static constexpr std::string_view meta_name() { return "Attribute"; }
	static constexpr size_t offset_attribute = 0;
	static constexpr size_t offset_value = 8;

	template <typename Buf>
	struct binder_type : public tll::scheme::Binder<Buf>
	{
		using tll::scheme::Binder<Buf>::Binder;

		static constexpr auto meta_size() { return Attribute::meta_size(); }
		static constexpr auto meta_name() { return Attribute::meta_name(); }
		void view_resize() { this->_view_resize(meta_size()); }

		template <typename RBuf>
		void copy(const binder_type<RBuf> &rhs)
		{
			set_attribute(rhs.get_attribute());
			set_value(rhs.get_value());
		}

		std::string_view get_attribute() const { return this->template _get_string<tll_scheme_offset_ptr_t>(offset_attribute); }
		void set_attribute(std::string_view v) { return this->template _set_string<tll_scheme_offset_ptr_t>(offset_attribute, v); }

		std::string_view get_value() const { return this->template _get_string<tll_scheme_offset_ptr_t>(offset_value); }
		void set_value(std::string_view v) { return this->template _set_string<tll_scheme_offset_ptr_t>(offset_value, v); }
	};

	template <typename Buf>
	static binder_type<Buf> bind(Buf &buf, size_t offset = 0) { return binder_type<Buf>(tll::make_view(buf).view(offset)); }

	template <typename Buf>
	static binder_type<Buf> bind_reset(Buf &buf) { return tll::scheme::make_binder_reset<binder_type, Buf>(buf); }
};

struct RequestOld
{
	static constexpr size_t meta_size() { return 34; }
	static constexpr std::string_view meta_name() { return "RequestOld"; }
	static constexpr int meta_id() { return 11; }
	static constexpr size_t offset_version = 0;
	static constexpr size_t offset_seq = 2;
	static constexpr size_t offset_client = 10;
	static constexpr size_t offset_block = 18;
	static constexpr size_t offset_attributes = 26;

	template <typename Buf>
	struct binder_type : public tll::scheme::Binder<Buf>
	{
		using tll::scheme::Binder<Buf>::Binder;

		static constexpr auto meta_size() { return RequestOld::meta_size(); }
		static constexpr auto meta_name() { return RequestOld::meta_name(); }
		static constexpr auto meta_id() { return RequestOld::meta_id(); }
		void view_resize() { this->_view_resize(meta_size()); }

		template <typename RBuf>
		void copy(const binder_type<RBuf> &rhs)
		{
			set_version(rhs.get_version());
			set_seq(rhs.get_seq());
			set_client(rhs.get_client());
			set_block(rhs.get_block());
			get_attributes().copy(rhs.get_attributes());
		}

		using type_version = Version;
		type_version get_version() const { return this->template _get_scalar<type_version>(offset_version); }
		void set_version(type_version v) { return this->template _set_scalar<type_version>(offset_version, v); }

		using type_seq = int64_t;
		type_seq get_seq() const { return this->template _get_scalar<type_seq>(offset_seq); }
		void set_seq(type_seq v) { return this->template _set_scalar<type_seq>(offset_seq, v); }

		std::string_view get_client() const { return this->template _get_string<tll_scheme_offset_ptr_t>(offset_client); }
		void set_client(std::string_view v) { return this->template _set_string<tll_scheme_offset_ptr_t>(offset_client, v); }

		std::string_view get_block() const { return this->template _get_string<tll_scheme_offset_ptr_t>(offset_block); }
		void set_block(std::string_view v) { return this->template _set_string<tll_scheme_offset_ptr_t>(offset_block, v); }

		using type_attributes = tll::scheme::binder::List<Buf, Attribute::binder_type<Buf>, tll_scheme_offset_ptr_t>;
		using const_type_attributes = tll::scheme::binder::List<const Buf, Attribute::binder_type<const Buf>, tll_scheme_offset_ptr_t>;
		const_type_attributes get_attributes() const { return this->template _get_binder<const_type_attributes>(offset_attributes); }
		type_attributes get_attributes() { return this->template _get_binder<type_attributes>(offset_attributes); }
	};

	template <typename Buf>
	static binder_type<Buf> bind(Buf &buf, size_t offset = 0) { return binder_type<Buf>(tll::make_view(buf).view(offset)); }

	template <typename Buf>
	static binder_type<Buf> bind_reset(Buf &buf) { return tll::scheme::make_binder_reset<binder_type, Buf>(buf); }
};

struct RequestBlock
{
	static constexpr size_t meta_size() { return 16; }
	static constexpr std::string_view meta_name() { return "RequestBlock"; }
	static constexpr size_t offset_block = 0;
	static constexpr size_t offset_index = 8;

	template <typename Buf>
	struct binder_type : public tll::scheme::Binder<Buf>
	{
		using tll::scheme::Binder<Buf>::Binder;

		static constexpr auto meta_size() { return RequestBlock::meta_size(); }
		static constexpr auto meta_name() { return RequestBlock::meta_name(); }
		void view_resize() { this->_view_resize(meta_size()); }

		template <typename RBuf>
		void copy(const binder_type<RBuf> &rhs)
		{
			set_block(rhs.get_block());
			set_index(rhs.get_index());
		}

		std::string_view get_block() const { return this->template _get_string<tll_scheme_offset_ptr_t>(offset_block); }
		void set_block(std::string_view v) { return this->template _set_string<tll_scheme_offset_ptr_t>(offset_block, v); }

		using type_index = int64_t;
		type_index get_index() const { return this->template _get_scalar<type_index>(offset_index); }
		void set_index(type_index v) { return this->template _set_scalar<type_index>(offset_index, v); }
	};

	template <typename Buf>
	static binder_type<Buf> bind(Buf &buf, size_t offset = 0) { return binder_type<Buf>(tll::make_view(buf).view(offset)); }

	template <typename Buf>
	static binder_type<Buf> bind_reset(Buf &buf) { return tll::scheme::make_binder_reset<binder_type, Buf>(buf); }
};

struct Request
{
	static constexpr size_t meta_size() { return 35; }
	static constexpr std::string_view meta_name() { return "Request"; }
	static constexpr int meta_id() { return 12; }
	static constexpr size_t offset_version = 0;
	static constexpr size_t offset_client = 2;
	static constexpr size_t offset_attributes = 10;
	static constexpr size_t offset_data = 18;

	template <typename Buf>
	struct Data: public tll::scheme::binder::Union<Buf, int8_t>
	{
		using union_index_type = int8_t;
		using tll::scheme::binder::Union<Buf, union_index_type>::Union;

		static constexpr union_index_type index_seq = 0;
		using type_seq = uint64_t;
		std::optional<uint64_t> get_seq() const { if (this->union_type() != index_seq) return std::nullopt; return unchecked_seq(); }
		uint64_t unchecked_seq() const { return this->template _get_scalar<uint64_t>(1); }
		void set_seq(const uint64_t &v) { this->_set_type(index_seq); this->template _set_scalar<uint64_t>(1, v); }

		static constexpr union_index_type index_block = 1;
		using type_block = RequestBlock::binder_type<Buf>;
		std::optional<RequestBlock::binder_type<Buf>> get_block() const { if (this->union_type() != index_block) return std::nullopt; return unchecked_block(); }
		RequestBlock::binder_type<Buf> unchecked_block() { return this->template _get_binder<RequestBlock::binder_type<Buf>>(1); }
		RequestBlock::binder_type<Buf> unchecked_block() const { return this->template _get_binder<RequestBlock::binder_type<Buf>>(1); }
		RequestBlock::binder_type<Buf> set_block() { this->_set_type(index_block); return this->template _get_binder<RequestBlock::binder_type<Buf>>(1); }

		static constexpr union_index_type index_initial = 2;
		using type_initial = int8_t;
		std::optional<int8_t> get_initial() const { if (this->union_type() != index_initial) return std::nullopt; return unchecked_initial(); }
		int8_t unchecked_initial() const { return this->template _get_scalar<int8_t>(1); }
		void set_initial(const int8_t &v) { this->_set_type(index_initial); this->template _set_scalar<int8_t>(1, v); }
	};


	template <typename Buf>
	struct binder_type : public tll::scheme::Binder<Buf>
	{
		using tll::scheme::Binder<Buf>::Binder;

		static constexpr auto meta_size() { return Request::meta_size(); }
		static constexpr auto meta_name() { return Request::meta_name(); }
		static constexpr auto meta_id() { return Request::meta_id(); }
		void view_resize() { this->_view_resize(meta_size()); }

		template <typename RBuf>
		void copy(const binder_type<RBuf> &rhs)
		{
			set_version(rhs.get_version());
			set_client(rhs.get_client());
			get_attributes().copy(rhs.get_attributes());
			get_data().copy(rhs.get_data());
		}

		using type_version = Version;
		type_version get_version() const { return this->template _get_scalar<type_version>(offset_version); }
		void set_version(type_version v) { return this->template _set_scalar<type_version>(offset_version, v); }

		std::string_view get_client() const { return this->template _get_string<tll_scheme_offset_ptr_t>(offset_client); }
		void set_client(std::string_view v) { return this->template _set_string<tll_scheme_offset_ptr_t>(offset_client, v); }

		using type_attributes = tll::scheme::binder::List<Buf, Attribute::binder_type<Buf>, tll_scheme_offset_ptr_t>;
		using const_type_attributes = tll::scheme::binder::List<const Buf, Attribute::binder_type<const Buf>, tll_scheme_offset_ptr_t>;
		const_type_attributes get_attributes() const { return this->template _get_binder<const_type_attributes>(offset_attributes); }
		type_attributes get_attributes() { return this->template _get_binder<type_attributes>(offset_attributes); }

		using type_data = Data<Buf>;
		using const_type_data = Data<const Buf>;
		const_type_data get_data() const { return this->template _get_binder<const_type_data>(offset_data); }
		type_data get_data() { return this->template _get_binder<type_data>(offset_data); }
	};

	template <typename Buf>
	static binder_type<Buf> bind(Buf &buf, size_t offset = 0) { return binder_type<Buf>(tll::make_view(buf).view(offset)); }

	template <typename Buf>
	static binder_type<Buf> bind_reset(Buf &buf) { return tll::scheme::make_binder_reset<binder_type, Buf>(buf); }
};

struct Reply
{
	static constexpr size_t meta_size() { return 32; }
	static constexpr std::string_view meta_name() { return "Reply"; }
	static constexpr int meta_id() { return 20; }
	static constexpr size_t offset_last_seq = 0;
	static constexpr size_t offset_requested_seq = 8;
	static constexpr size_t offset_block_seq = 16;
	static constexpr size_t offset_server = 24;

	template <typename Buf>
	struct binder_type : public tll::scheme::Binder<Buf>
	{
		using tll::scheme::Binder<Buf>::Binder;

		static constexpr auto meta_size() { return Reply::meta_size(); }
		static constexpr auto meta_name() { return Reply::meta_name(); }
		static constexpr auto meta_id() { return Reply::meta_id(); }
		void view_resize() { this->_view_resize(meta_size()); }

		template <typename RBuf>
		void copy(const binder_type<RBuf> &rhs)
		{
			set_last_seq(rhs.get_last_seq());
			set_requested_seq(rhs.get_requested_seq());
			set_block_seq(rhs.get_block_seq());
			set_server(rhs.get_server());
		}

		using type_last_seq = int64_t;
		type_last_seq get_last_seq() const { return this->template _get_scalar<type_last_seq>(offset_last_seq); }
		void set_last_seq(type_last_seq v) { return this->template _set_scalar<type_last_seq>(offset_last_seq, v); }

		using type_requested_seq = int64_t;
		type_requested_seq get_requested_seq() const { return this->template _get_scalar<type_requested_seq>(offset_requested_seq); }
		void set_requested_seq(type_requested_seq v) { return this->template _set_scalar<type_requested_seq>(offset_requested_seq, v); }

		using type_block_seq = int64_t;
		type_block_seq get_block_seq() const { return this->template _get_scalar<type_block_seq>(offset_block_seq); }
		void set_block_seq(type_block_seq v) { return this->template _set_scalar<type_block_seq>(offset_block_seq, v); }

		std::string_view get_server() const { return this->template _get_string<tll_scheme_offset_ptr_t>(offset_server); }
		void set_server(std::string_view v) { return this->template _set_string<tll_scheme_offset_ptr_t>(offset_server, v); }
	};

	template <typename Buf>
	static binder_type<Buf> bind(Buf &buf, size_t offset = 0) { return binder_type<Buf>(tll::make_view(buf).view(offset)); }

	template <typename Buf>
	static binder_type<Buf> bind_reset(Buf &buf) { return tll::scheme::make_binder_reset<binder_type, Buf>(buf); }
};

struct Error
{
	static constexpr size_t meta_size() { return 16; }
	static constexpr std::string_view meta_name() { return "Error"; }
	static constexpr int meta_id() { return 30; }
	static constexpr size_t offset_error = 0;
	static constexpr size_t offset_server = 8;

	template <typename Buf>
	struct binder_type : public tll::scheme::Binder<Buf>
	{
		using tll::scheme::Binder<Buf>::Binder;

		static constexpr auto meta_size() { return Error::meta_size(); }
		static constexpr auto meta_name() { return Error::meta_name(); }
		static constexpr auto meta_id() { return Error::meta_id(); }
		void view_resize() { this->_view_resize(meta_size()); }

		template <typename RBuf>
		void copy(const binder_type<RBuf> &rhs)
		{
			set_error(rhs.get_error());
			set_server(rhs.get_server());
		}

		std::string_view get_error() const { return this->template _get_string<tll_scheme_offset_ptr_t>(offset_error); }
		void set_error(std::string_view v) { return this->template _set_string<tll_scheme_offset_ptr_t>(offset_error, v); }

		std::string_view get_server() const { return this->template _get_string<tll_scheme_offset_ptr_t>(offset_server); }
		void set_server(std::string_view v) { return this->template _set_string<tll_scheme_offset_ptr_t>(offset_server, v); }
	};

	template <typename Buf>
	static binder_type<Buf> bind(Buf &buf, size_t offset = 0) { return binder_type<Buf>(tll::make_view(buf).view(offset)); }

	template <typename Buf>
	static binder_type<Buf> bind_reset(Buf &buf) { return tll::scheme::make_binder_reset<binder_type, Buf>(buf); }
};

struct ClientDone
{
	static constexpr size_t meta_size() { return 8; }
	static constexpr std::string_view meta_name() { return "ClientDone"; }
	static constexpr int meta_id() { return 40; }
	static constexpr size_t offset_seq = 0;

	template <typename Buf>
	struct binder_type : public tll::scheme::Binder<Buf>
	{
		using tll::scheme::Binder<Buf>::Binder;

		static constexpr auto meta_size() { return ClientDone::meta_size(); }
		static constexpr auto meta_name() { return ClientDone::meta_name(); }
		static constexpr auto meta_id() { return ClientDone::meta_id(); }
		void view_resize() { this->_view_resize(meta_size()); }

		template <typename RBuf>
		void copy(const binder_type<RBuf> &rhs)
		{
			set_seq(rhs.get_seq());
		}

		using type_seq = int64_t;
		type_seq get_seq() const { return this->template _get_scalar<type_seq>(offset_seq); }
		void set_seq(type_seq v) { return this->template _set_scalar<type_seq>(offset_seq, v); }
	};

	template <typename Buf>
	static binder_type<Buf> bind(Buf &buf, size_t offset = 0) { return binder_type<Buf>(tll::make_view(buf).view(offset)); }

	template <typename Buf>
	static binder_type<Buf> bind_reset(Buf &buf) { return tll::scheme::make_binder_reset<binder_type, Buf>(buf); }
};

} // namespace stream_scheme

template <>
struct tll::conv::dump<stream_scheme::Version> : public to_string_from_string_buf<stream_scheme::Version>
{
	template <typename Buf>
	static inline std::string_view to_string_buf(const stream_scheme::Version &v, Buf &buf)
	{
		switch (v) {
		case stream_scheme::Version::Current: return "Current";
		default: break;
		}
		return tll::conv::to_string_buf<int16_t, Buf>((int16_t) v, buf);
	}
};
