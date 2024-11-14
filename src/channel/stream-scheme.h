#pragma once

#include <tll/scheme/binder.h>
#include <tll/util/conv.h>

namespace stream_scheme {

static constexpr std::string_view scheme_string = R"(yamls+gz://eJylkztvgzAUhff8Cm9IFUgljTKwtUnnSh26VFXkwG1r1RjHj6gR4r/XNq8ScEHqhvDHOfccXyLEcA4JCoIVQgVXpGAyQWWQch7ZE8lxCoE5l0oAzg8y/YQcgsrQwHQuE/OAUPACQppPDViqCzd6hKl4GzrGyu20EMCUOY+rahW1pvdKCXLUCqz7OwGaNYIRKhsEd0iIamk7CmEfbobf5BlTPUV1bs9w0iDVE82sHcnMMLHX99wk6vTaiNe2Ek49ZGJvNyMkpcSGnwtwpEX6NUt1hcgevemLHOd9cLLeoMtcCcvge5zz2qsrdm0eNHPbVG/IHits18O9TNDrZH260Q190w0iVW/V/69v4d3Mtj7EM5u2A132QVmcXtqq1rfeEBRLdViwX6JuBbIlsGt0CShBmBb/+qEehShEm+POnwMcN1fxvN/O3dW+YNCabvymkwl/AAWxfi4=)";

enum class Version: int16_t
{
	Current = 1,
};

struct Attribute
{
	static constexpr size_t meta_size() { return 16; }
	static constexpr std::string_view meta_name() { return "Attribute"; }

	template <typename Buf>
	struct binder_type : public tll::scheme::Binder<Buf>
	{
		using tll::scheme::Binder<Buf>::Binder;

		static constexpr auto meta_size() { return Attribute::meta_size(); }
		static constexpr auto meta_name() { return Attribute::meta_name(); }
		void view_resize() { this->_view_resize(meta_size()); }

		std::string_view get_attribute() const { return this->template _get_string<tll_scheme_offset_ptr_t>(0); }
		void set_attribute(std::string_view v) { return this->template _set_string<tll_scheme_offset_ptr_t>(0, v); }

		std::string_view get_value() const { return this->template _get_string<tll_scheme_offset_ptr_t>(8); }
		void set_value(std::string_view v) { return this->template _set_string<tll_scheme_offset_ptr_t>(8, v); }
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

	template <typename Buf>
	struct binder_type : public tll::scheme::Binder<Buf>
	{
		using tll::scheme::Binder<Buf>::Binder;

		static constexpr auto meta_size() { return RequestOld::meta_size(); }
		static constexpr auto meta_name() { return RequestOld::meta_name(); }
		static constexpr auto meta_id() { return RequestOld::meta_id(); }
		void view_resize() { this->_view_resize(meta_size()); }

		using type_version = Version;
		type_version get_version() const { return this->template _get_scalar<type_version>(0); }
		void set_version(type_version v) { return this->template _set_scalar<type_version>(0, v); }

		using type_seq = int64_t;
		type_seq get_seq() const { return this->template _get_scalar<type_seq>(2); }
		void set_seq(type_seq v) { return this->template _set_scalar<type_seq>(2, v); }

		std::string_view get_client() const { return this->template _get_string<tll_scheme_offset_ptr_t>(10); }
		void set_client(std::string_view v) { return this->template _set_string<tll_scheme_offset_ptr_t>(10, v); }

		std::string_view get_block() const { return this->template _get_string<tll_scheme_offset_ptr_t>(18); }
		void set_block(std::string_view v) { return this->template _set_string<tll_scheme_offset_ptr_t>(18, v); }

		using type_attributes = tll::scheme::binder::List<Buf, Attribute::binder_type<Buf>, tll_scheme_offset_ptr_t>;
		const type_attributes get_attributes() const { return this->template _get_binder<type_attributes>(26); }
		type_attributes get_attributes() { return this->template _get_binder<type_attributes>(26); }
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

	template <typename Buf>
	struct binder_type : public tll::scheme::Binder<Buf>
	{
		using tll::scheme::Binder<Buf>::Binder;

		static constexpr auto meta_size() { return RequestBlock::meta_size(); }
		static constexpr auto meta_name() { return RequestBlock::meta_name(); }
		void view_resize() { this->_view_resize(meta_size()); }

		std::string_view get_block() const { return this->template _get_string<tll_scheme_offset_ptr_t>(0); }
		void set_block(std::string_view v) { return this->template _set_string<tll_scheme_offset_ptr_t>(0, v); }

		using type_index = int64_t;
		type_index get_index() const { return this->template _get_scalar<type_index>(8); }
		void set_index(type_index v) { return this->template _set_scalar<type_index>(8, v); }
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
	};


	template <typename Buf>
	struct binder_type : public tll::scheme::Binder<Buf>
	{
		using tll::scheme::Binder<Buf>::Binder;

		static constexpr auto meta_size() { return Request::meta_size(); }
		static constexpr auto meta_name() { return Request::meta_name(); }
		static constexpr auto meta_id() { return Request::meta_id(); }
		void view_resize() { this->_view_resize(meta_size()); }

		using type_version = Version;
		type_version get_version() const { return this->template _get_scalar<type_version>(0); }
		void set_version(type_version v) { return this->template _set_scalar<type_version>(0, v); }

		std::string_view get_client() const { return this->template _get_string<tll_scheme_offset_ptr_t>(2); }
		void set_client(std::string_view v) { return this->template _set_string<tll_scheme_offset_ptr_t>(2, v); }

		using type_attributes = tll::scheme::binder::List<Buf, Attribute::binder_type<Buf>, tll_scheme_offset_ptr_t>;
		const type_attributes get_attributes() const { return this->template _get_binder<type_attributes>(10); }
		type_attributes get_attributes() { return this->template _get_binder<type_attributes>(10); }

		using type_data = Data<Buf>;
		const type_data get_data() const { return this->template _get_binder<type_data>(18); }
		type_data get_data() { return this->template _get_binder<type_data>(18); }
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

	template <typename Buf>
	struct binder_type : public tll::scheme::Binder<Buf>
	{
		using tll::scheme::Binder<Buf>::Binder;

		static constexpr auto meta_size() { return Reply::meta_size(); }
		static constexpr auto meta_name() { return Reply::meta_name(); }
		static constexpr auto meta_id() { return Reply::meta_id(); }
		void view_resize() { this->_view_resize(meta_size()); }

		using type_last_seq = int64_t;
		type_last_seq get_last_seq() const { return this->template _get_scalar<type_last_seq>(0); }
		void set_last_seq(type_last_seq v) { return this->template _set_scalar<type_last_seq>(0, v); }

		using type_requested_seq = int64_t;
		type_requested_seq get_requested_seq() const { return this->template _get_scalar<type_requested_seq>(8); }
		void set_requested_seq(type_requested_seq v) { return this->template _set_scalar<type_requested_seq>(8, v); }

		using type_block_seq = int64_t;
		type_block_seq get_block_seq() const { return this->template _get_scalar<type_block_seq>(16); }
		void set_block_seq(type_block_seq v) { return this->template _set_scalar<type_block_seq>(16, v); }

		std::string_view get_server() const { return this->template _get_string<tll_scheme_offset_ptr_t>(24); }
		void set_server(std::string_view v) { return this->template _set_string<tll_scheme_offset_ptr_t>(24, v); }
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

	template <typename Buf>
	struct binder_type : public tll::scheme::Binder<Buf>
	{
		using tll::scheme::Binder<Buf>::Binder;

		static constexpr auto meta_size() { return Error::meta_size(); }
		static constexpr auto meta_name() { return Error::meta_name(); }
		static constexpr auto meta_id() { return Error::meta_id(); }
		void view_resize() { this->_view_resize(meta_size()); }

		std::string_view get_error() const { return this->template _get_string<tll_scheme_offset_ptr_t>(0); }
		void set_error(std::string_view v) { return this->template _set_string<tll_scheme_offset_ptr_t>(0, v); }

		std::string_view get_server() const { return this->template _get_string<tll_scheme_offset_ptr_t>(8); }
		void set_server(std::string_view v) { return this->template _set_string<tll_scheme_offset_ptr_t>(8, v); }
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

	template <typename Buf>
	struct binder_type : public tll::scheme::Binder<Buf>
	{
		using tll::scheme::Binder<Buf>::Binder;

		static constexpr auto meta_size() { return ClientDone::meta_size(); }
		static constexpr auto meta_name() { return ClientDone::meta_name(); }
		static constexpr auto meta_id() { return ClientDone::meta_id(); }
		void view_resize() { this->_view_resize(meta_size()); }

		using type_seq = int64_t;
		type_seq get_seq() const { return this->template _get_scalar<type_seq>(0); }
		void set_seq(type_seq v) { return this->template _set_scalar<type_seq>(0, v); }
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
