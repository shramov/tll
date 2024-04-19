#pragma once

#include <tll/scheme/binder.h>
#include <tll/util/conv.h>

namespace file_scheme {

static constexpr std::string_view scheme_string = R"(yamls+gz://eJyFkktPwzAMx+/7FL5FQqmUsNFlvfEQJ+DCDYRQ2rkjok1HkyLB1O9O0q5dHyBulv3z628HoGWOEZBLa0sVVxbJAiBVmG1N5CyAAA5HRPYIBfu19y7jPHpH6gn5KbPqNyrout2jlb6R2kbAw+UF44KF3DlQV/mxMbku8n2JxqhCkwgObbFKaStowzkfuXtauRinQB4Kjc5ktR8mVrarcpvJnRnnhyvaEvDcTXyDmZWP+OGGLtLUoHWVKBj17YK8fqn/FCV3m7x67rSub8HDuShYNquMODHDksHWPTqUYpoQZ0XyPq66PJ9RJnnD/P/LpY1aPdWKN4X6RxiQZ6cHGtz5qpmtO7RgPFyvxYYtfgC4Z7zr)";

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

struct Meta
{
	static constexpr size_t meta_size() { return 32; }
	static constexpr std::string_view meta_name() { return "Meta"; }
	static constexpr int meta_id() { return 1635018061; }

	enum class Compression: uint8_t
	{
		None = 0,
		LZ4 = 1,
	};

	struct Flags: public tll::scheme::Bits<uint64_t>
	{
		using tll::scheme::Bits<uint64_t>::Bits;
		constexpr auto DeltaSeq() const { return get(0, 1); };
		constexpr Flags & DeltaSeq(bool v) { set(0, 1, v); return *this; };
		static std::map<std::string_view, value_type> bits_descriptor()
		{
			return {
				{ "DeltaSeq", static_cast<value_type>(Bits::mask(1)) << 0 },
			};
		}
	};

	template <typename Buf>
	struct binder_type : public tll::scheme::Binder<Buf>
	{
		using tll::scheme::Binder<Buf>::Binder;

		static constexpr auto meta_size() { return Meta::meta_size(); }
		static constexpr auto meta_name() { return Meta::meta_name(); }
		static constexpr auto meta_id() { return Meta::meta_id(); }
		void view_resize() { this->_view_resize(meta_size()); }

		using type_meta_size = uint16_t;
		type_meta_size get_meta_size() const { return this->template _get_scalar<type_meta_size>(0); }
		void set_meta_size(type_meta_size v) { return this->template _set_scalar<type_meta_size>(0, v); }

		using type_version = uint8_t;
		type_version get_version() const { return this->template _get_scalar<type_version>(2); }
		void set_version(type_version v) { return this->template _set_scalar<type_version>(2, v); }

		using type_compression = Compression;
		type_compression get_compression() const { return this->template _get_scalar<type_compression>(3); }
		void set_compression(type_compression v) { return this->template _set_scalar<type_compression>(3, v); }

		using type_block = uint32_t;
		type_block get_block() const { return this->template _get_scalar<type_block>(4); }
		void set_block(type_block v) { return this->template _set_scalar<type_block>(4, v); }

		std::string_view get_scheme() const { return this->template _get_string<tll_scheme_offset_ptr_t>(8); }
		void set_scheme(std::string_view v) { return this->template _set_string<tll_scheme_offset_ptr_t>(8, v); }

		using type_flags = Flags;
		type_flags get_flags() const { return this->template _get_scalar<type_flags>(16); }
		void set_flags(type_flags v) { return this->template _set_scalar<type_flags>(16, v); }

		using type_attributes = tll::scheme::binder::List<Buf, Attribute::binder_type<Buf>, tll_scheme_offset_ptr_t>;
		const type_attributes get_attributes() const { return this->template _get_binder<type_attributes>(24); }
		type_attributes get_attributes() { return this->template _get_binder<type_attributes>(24); }
	};

	template <typename Buf>
	static binder_type<Buf> bind(Buf &buf, size_t offset = 0) { return binder_type<Buf>(tll::make_view(buf).view(offset)); }

	template <typename Buf>
	static binder_type<Buf> bind_reset(Buf &buf) { return tll::scheme::make_binder_reset<binder_type, Buf>(buf); }
};

struct Block
{
	static constexpr size_t meta_size() { return 0; }
	static constexpr std::string_view meta_name() { return "Block"; }
	static constexpr int meta_id() { return 1801677890; }

	template <typename Buf>
	struct binder_type : public tll::scheme::Binder<Buf>
	{
		using tll::scheme::Binder<Buf>::Binder;

		static constexpr auto meta_size() { return Block::meta_size(); }
		static constexpr auto meta_name() { return Block::meta_name(); }
		static constexpr auto meta_id() { return Block::meta_id(); }
		void view_resize() { this->_view_resize(meta_size()); }
	};

	template <typename Buf>
	static binder_type<Buf> bind(Buf &buf, size_t offset = 0) { return binder_type<Buf>(tll::make_view(buf).view(offset)); }

	template <typename Buf>
	static binder_type<Buf> bind_reset(Buf &buf) { return tll::scheme::make_binder_reset<binder_type, Buf>(buf); }
};

} // namespace file_scheme

template <>
struct tll::conv::dump<file_scheme::Meta::Compression> : public to_string_from_string_buf<file_scheme::Meta::Compression>
{
	template <typename Buf>
	static inline std::string_view to_string_buf(const file_scheme::Meta::Compression &v, Buf &buf)
	{
		switch (v) {
		case file_scheme::Meta::Compression::LZ4: return "LZ4";
		case file_scheme::Meta::Compression::None: return "None";
		default: break;
		}
		return tll::conv::to_string_buf<uint8_t, Buf>((uint8_t) v, buf);
	}
};
