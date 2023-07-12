#pragma once

#include <tll/scheme/binder.h>
#include <tll/util/conv.h>

namespace stat_scheme {

static constexpr std::string_view scheme_string = R"(yamls+gz://eJy1kz1PwzAQhvf+Cm+WUCqVDwHKyBBUCRBSVRbE4BI3WCTnqLELVeX/ztlxPuSSZIHJ5/j1e4/vLnMCrOAxoXRGCAddVDEGhNBHrj5kSmNyVIcSBVqAuo2cBL/RB1YpPLyMUMm+MbqwkQCMzjFa6QKjhTG12RqEGrS6Oyhe+XtPK2+1hk+QX+BN5g3l8oXlmlvWreB56mHn5OjPi5o6InWm5hUmkO2dS6tCnusr2kuT/EuaVOpNzvt5lvc7qcvhPO9Sg+oMdAMa0GDZT94SSLBHExK2z8ZQk79HbZOMsA5opmAtomXVICQ41t+p7dIZbXASb3ArS+Wu4XDaExxCWqmdgIyaEETbyW4N3JxPTIFDwq1bY/LayESg86NuotZoGyiSE4XIXJM6j7ppfY9A4dtq3nrVe2aZm32R4k+5GK6dTHswTYFGCzwgqv072VndPzP7Acq9XcQ=)";

enum class Method: uint8_t
{
	Sum = 0,
	Min = 1,
	Max = 2,
	Last = 3,
};

enum class Unit: uint8_t
{
	Unknown = 0,
	Bytes = 1,
	NS = 2,
};

struct IValue
{
	static constexpr size_t meta_size() { return 9; }
	static constexpr std::string_view meta_name() { return "IValue"; }

	template <typename Buf>
	struct binder_type : public tll::scheme::Binder<Buf>
	{
		using tll::scheme::Binder<Buf>::Binder;

		static constexpr auto meta_size() { return IValue::meta_size(); }
		static constexpr auto meta_name() { return IValue::meta_name(); }
		void view_resize() { this->_view_resize(meta_size()); }

		using type_method = Method;
		type_method get_method() const { return this->template _get_scalar<type_method>(0); }
		void set_method(type_method v) { return this->template _set_scalar<type_method>(0, v); }

		using type_value = int64_t;
		type_value get_value() const { return this->template _get_scalar<type_value>(1); }
		void set_value(type_value v) { return this->template _set_scalar<type_value>(1, v); }
	};

	template <typename Buf>
	static binder_type<Buf> bind(Buf &buf, size_t offset = 0) { return binder_type<Buf>(tll::make_view(buf).view(offset)); }
};

struct FValue
{
	static constexpr size_t meta_size() { return 9; }
	static constexpr std::string_view meta_name() { return "FValue"; }

	template <typename Buf>
	struct binder_type : public tll::scheme::Binder<Buf>
	{
		using tll::scheme::Binder<Buf>::Binder;

		static constexpr auto meta_size() { return FValue::meta_size(); }
		static constexpr auto meta_name() { return FValue::meta_name(); }
		void view_resize() { this->_view_resize(meta_size()); }

		using type_method = Method;
		type_method get_method() const { return this->template _get_scalar<type_method>(0); }
		void set_method(type_method v) { return this->template _set_scalar<type_method>(0, v); }

		using type_value = double;
		type_value get_value() const { return this->template _get_scalar<type_value>(1); }
		void set_value(type_value v) { return this->template _set_scalar<type_value>(1, v); }
	};

	template <typename Buf>
	static binder_type<Buf> bind(Buf &buf, size_t offset = 0) { return binder_type<Buf>(tll::make_view(buf).view(offset)); }
};

struct IGroup
{
	static constexpr size_t meta_size() { return 32; }
	static constexpr std::string_view meta_name() { return "IGroup"; }

	template <typename Buf>
	struct binder_type : public tll::scheme::Binder<Buf>
	{
		using tll::scheme::Binder<Buf>::Binder;

		static constexpr auto meta_size() { return IGroup::meta_size(); }
		static constexpr auto meta_name() { return IGroup::meta_name(); }
		void view_resize() { this->_view_resize(meta_size()); }

		using type_count = uint64_t;
		type_count get_count() const { return this->template _get_scalar<type_count>(0); }
		void set_count(type_count v) { return this->template _set_scalar<type_count>(0, v); }

		using type_min = int64_t;
		type_min get_min() const { return this->template _get_scalar<type_min>(8); }
		void set_min(type_min v) { return this->template _set_scalar<type_min>(8, v); }

		using type_max = int64_t;
		type_max get_max() const { return this->template _get_scalar<type_max>(16); }
		void set_max(type_max v) { return this->template _set_scalar<type_max>(16, v); }

		using type_avg = double;
		type_avg get_avg() const { return this->template _get_scalar<type_avg>(24); }
		void set_avg(type_avg v) { return this->template _set_scalar<type_avg>(24, v); }
	};

	template <typename Buf>
	static binder_type<Buf> bind(Buf &buf, size_t offset = 0) { return binder_type<Buf>(tll::make_view(buf).view(offset)); }
};

struct FGroup
{
	static constexpr size_t meta_size() { return 32; }
	static constexpr std::string_view meta_name() { return "FGroup"; }

	template <typename Buf>
	struct binder_type : public tll::scheme::Binder<Buf>
	{
		using tll::scheme::Binder<Buf>::Binder;

		static constexpr auto meta_size() { return FGroup::meta_size(); }
		static constexpr auto meta_name() { return FGroup::meta_name(); }
		void view_resize() { this->_view_resize(meta_size()); }

		using type_count = uint64_t;
		type_count get_count() const { return this->template _get_scalar<type_count>(0); }
		void set_count(type_count v) { return this->template _set_scalar<type_count>(0, v); }

		using type_min = double;
		type_min get_min() const { return this->template _get_scalar<type_min>(8); }
		void set_min(type_min v) { return this->template _set_scalar<type_min>(8, v); }

		using type_max = double;
		type_max get_max() const { return this->template _get_scalar<type_max>(16); }
		void set_max(type_max v) { return this->template _set_scalar<type_max>(16, v); }

		using type_avg = double;
		type_avg get_avg() const { return this->template _get_scalar<type_avg>(24); }
		void set_avg(type_avg v) { return this->template _set_scalar<type_avg>(24, v); }
	};

	template <typename Buf>
	static binder_type<Buf> bind(Buf &buf, size_t offset = 0) { return binder_type<Buf>(tll::make_view(buf).view(offset)); }
};

struct Field
{
	static constexpr size_t meta_size() { return 41; }
	static constexpr std::string_view meta_name() { return "Field"; }

	template <typename Buf>
	struct value: public tll::scheme::binder::Union<Buf, int8_t>
	{
		using union_index_type = int8_t;
		using tll::scheme::binder::Union<Buf, union_index_type>::Union;

		static constexpr union_index_type index_ivalue = 0;
		using type_ivalue = IValue::binder_type<Buf>;
		std::optional<IValue::binder_type<Buf>> get_ivalue() const { if (this->union_type() != index_ivalue) return std::nullopt; return unchecked_ivalue(); }
		IValue::binder_type<Buf> unchecked_ivalue() { return this->template _get_binder<IValue::binder_type<Buf>>(1); }
		IValue::binder_type<Buf> unchecked_ivalue() const { return this->template _get_binder<IValue::binder_type<Buf>>(1); }
		IValue::binder_type<Buf> set_ivalue() { this->_set_type(index_ivalue); return this->template _get_binder<IValue::binder_type<Buf>>(1); }

		static constexpr union_index_type index_fvalue = 1;
		using type_fvalue = FValue::binder_type<Buf>;
		std::optional<FValue::binder_type<Buf>> get_fvalue() const { if (this->union_type() != index_fvalue) return std::nullopt; return unchecked_fvalue(); }
		FValue::binder_type<Buf> unchecked_fvalue() { return this->template _get_binder<FValue::binder_type<Buf>>(1); }
		FValue::binder_type<Buf> unchecked_fvalue() const { return this->template _get_binder<FValue::binder_type<Buf>>(1); }
		FValue::binder_type<Buf> set_fvalue() { this->_set_type(index_fvalue); return this->template _get_binder<FValue::binder_type<Buf>>(1); }

		static constexpr union_index_type index_igroup = 2;
		using type_igroup = IGroup::binder_type<Buf>;
		std::optional<IGroup::binder_type<Buf>> get_igroup() const { if (this->union_type() != index_igroup) return std::nullopt; return unchecked_igroup(); }
		IGroup::binder_type<Buf> unchecked_igroup() { return this->template _get_binder<IGroup::binder_type<Buf>>(1); }
		IGroup::binder_type<Buf> unchecked_igroup() const { return this->template _get_binder<IGroup::binder_type<Buf>>(1); }
		IGroup::binder_type<Buf> set_igroup() { this->_set_type(index_igroup); return this->template _get_binder<IGroup::binder_type<Buf>>(1); }

		static constexpr union_index_type index_fgroup = 3;
		using type_fgroup = FGroup::binder_type<Buf>;
		std::optional<FGroup::binder_type<Buf>> get_fgroup() const { if (this->union_type() != index_fgroup) return std::nullopt; return unchecked_fgroup(); }
		FGroup::binder_type<Buf> unchecked_fgroup() { return this->template _get_binder<FGroup::binder_type<Buf>>(1); }
		FGroup::binder_type<Buf> unchecked_fgroup() const { return this->template _get_binder<FGroup::binder_type<Buf>>(1); }
		FGroup::binder_type<Buf> set_fgroup() { this->_set_type(index_fgroup); return this->template _get_binder<FGroup::binder_type<Buf>>(1); }
	};


	template <typename Buf>
	struct binder_type : public tll::scheme::Binder<Buf>
	{
		using tll::scheme::Binder<Buf>::Binder;

		static constexpr auto meta_size() { return Field::meta_size(); }
		static constexpr auto meta_name() { return Field::meta_name(); }
		void view_resize() { this->_view_resize(meta_size()); }

		std::string_view get_name() const { return this->template _get_bytestring<7>(0); }
		void set_name(std::string_view v) { return this->template _set_bytestring<7>(0, v); }

		using type_unit = Unit;
		type_unit get_unit() const { return this->template _get_scalar<type_unit>(7); }
		void set_unit(type_unit v) { return this->template _set_scalar<type_unit>(7, v); }

		using type_value = value<Buf>;
		const type_value get_value() const { return this->template _get_binder<type_value>(8); }
		type_value get_value() { return this->template _get_binder<type_value>(8); }
	};

	template <typename Buf>
	static binder_type<Buf> bind(Buf &buf, size_t offset = 0) { return binder_type<Buf>(tll::make_view(buf).view(offset)); }
};

struct Page
{
	static constexpr size_t meta_size() { return 24; }
	static constexpr std::string_view meta_name() { return "Page"; }
	static constexpr int meta_id() { return 10; }

	template <typename Buf>
	struct binder_type : public tll::scheme::Binder<Buf>
	{
		using tll::scheme::Binder<Buf>::Binder;

		static constexpr auto meta_size() { return Page::meta_size(); }
		static constexpr auto meta_name() { return Page::meta_name(); }
		static constexpr auto meta_id() { return Page::meta_id(); }
		void view_resize() { this->_view_resize(meta_size()); }

		std::string_view get_node() const { return this->template _get_string<tll_scheme_offset_ptr_t>(0); }
		void set_node(std::string_view v) { return this->template _set_string<tll_scheme_offset_ptr_t>(0, v); }

		std::string_view get_name() const { return this->template _get_string<tll_scheme_offset_ptr_t>(8); }
		void set_name(std::string_view v) { return this->template _set_string<tll_scheme_offset_ptr_t>(8, v); }

		using type_fields = tll::scheme::binder::List<Buf, Field::binder_type<Buf>, tll_scheme_offset_ptr_t>;
		const type_fields get_fields() const { return this->template _get_binder<type_fields>(16); }
		type_fields get_fields() { return this->template _get_binder<type_fields>(16); }
	};

	template <typename Buf>
	static binder_type<Buf> bind(Buf &buf, size_t offset = 0) { return binder_type<Buf>(tll::make_view(buf).view(offset)); }
};

} // namespace stat_scheme

template <>
struct tll::conv::dump<stat_scheme::Method> : public to_string_from_string_buf<stat_scheme::Method>
{
	template <typename Buf>
	static inline std::string_view to_string_buf(const stat_scheme::Method &v, Buf &buf)
	{
		switch (v) {
		case stat_scheme::Method::Last: return "Last";
		case stat_scheme::Method::Max: return "Max";
		case stat_scheme::Method::Min: return "Min";
		case stat_scheme::Method::Sum: return "Sum";
		default: break;
		}
		return tll::conv::to_string_buf<uint8_t, Buf>((uint8_t) v, buf);
	}
};

template <>
struct tll::conv::dump<stat_scheme::Unit> : public to_string_from_string_buf<stat_scheme::Unit>
{
	template <typename Buf>
	static inline std::string_view to_string_buf(const stat_scheme::Unit &v, Buf &buf)
	{
		switch (v) {
		case stat_scheme::Unit::Bytes: return "Bytes";
		case stat_scheme::Unit::NS: return "NS";
		case stat_scheme::Unit::Unknown: return "Unknown";
		default: break;
		}
		return tll::conv::to_string_buf<uint8_t, Buf>((uint8_t) v, buf);
	}
};
