#include <tll/scheme/binder.h>

namespace file_scheme {

static constexpr std::string_view scheme_string = R"(yamls+gz://eJyFkUEOgjAQRfeeYnZNjCStRqzslLVewSCO2khbQouJGu5ui4ogGnfN78u8n5kAVCIxArKwthDb0iIZAOwFZjsTuRdAALcnkjTICOwl95FxiTqQ6oM8J1n5jQpethXaxIvELgIWTqaUcRoyF6Aq5VNMYi3zAo0RWpEIbo9hpVCWj2rOZWStFbpPWlU/a0vn2hhxbRXyQ1jYr41FLetwvIelrV4NGndCnVv3ML6h/3cNiW9Mqs9Z20ynp65wMu4ZTXpE+X/tzYHMGx2+D9va/7LWvg7AKQtnMz6ngzuwHZ9V)";

template <typename Buf>
struct Attribute : public tll::scheme::Binder<Buf>
{
	using tll::scheme::Binder<Buf>::Binder;

	static constexpr size_t meta_size() { return 16; }
	static constexpr std::string_view meta_name() { return "Attribute"; }
	void view_resize() { this->_view_resize(meta_size()); }

	std::string_view get_attribute() const { return this->template _get_string<tll_scheme_offset_ptr_t>(0); }
	void set_attribute(std::string_view v) { return this->template _set_string<tll_scheme_offset_ptr_t>(0, v); }

	std::string_view get_value() const { return this->template _get_string<tll_scheme_offset_ptr_t>(8); }
	void set_value(std::string_view v) { return this->template _set_string<tll_scheme_offset_ptr_t>(8, v); }
};

template <typename Buf>
struct Meta : public tll::scheme::Binder<Buf>
{
	using tll::scheme::Binder<Buf>::Binder;

	static constexpr size_t meta_size() { return 24; }
	static constexpr std::string_view meta_name() { return "Meta"; }
	static constexpr int meta_id() { return 1635018061; }
	void view_resize() { this->_view_resize(meta_size()); }

	enum class Compression: uint8_t
	{
		None = 0,
	};

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

	using type_attributes = tll::scheme::binder::List<Buf, Attribute<Buf>, tll_scheme_offset_ptr_t>;
	const type_attributes get_attributes() const { return this->template _get_binder<type_attributes>(16); }
	type_attributes get_attributes() { return this->template _get_binder<type_attributes>(16); }
};

template <typename Buf>
struct Block : public tll::scheme::Binder<Buf>
{
	using tll::scheme::Binder<Buf>::Binder;

	static constexpr size_t meta_size() { return 0; }
	static constexpr std::string_view meta_name() { return "Block"; }
	static constexpr int meta_id() { return 1801677890; }
	void view_resize() { this->_view_resize(meta_size()); }
};

} // namespace file_scheme
