#include <tll/scheme/binder.h>

namespace stream_scheme {

static constexpr std::string_view scheme_string = R"(yamls+gz://eJzTVchLzE21UlB3LCkpykwqLUlV51JQSMtMzUkptgKyFBR0FaqhShLhSnQUSioLQELFQJG8dPVaNJVliTml2FTpwmwLSi0sTS0uAdmVmWKlYGiA09Li1EKEQZl5JWYmGLYl52Sm5pUQdFRSTn5yNkFVcE8WI5RqIQIHxQ8FOZUwHxhR5IPi1KKy1CJ8AeZaVJRfBLPMGLdlqWB1hDyJ0z4A/xacug==)";

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
	static binder_type<Buf> bind(Buf &buf) { return binder_type<Buf>(buf); }
};

struct Request
{
	static constexpr size_t meta_size() { return 32; }
	static constexpr std::string_view meta_name() { return "Request"; }
	static constexpr int meta_id() { return 10; }

	template <typename Buf>
	struct binder_type : public tll::scheme::Binder<Buf>
	{
		using tll::scheme::Binder<Buf>::Binder;

		static constexpr auto meta_size() { return Request::meta_size(); }
		static constexpr auto meta_name() { return Request::meta_name(); }
		static constexpr auto meta_id() { return Request::meta_id(); }
		void view_resize() { this->_view_resize(meta_size()); }

		using type_seq = int64_t;
		type_seq get_seq() const { return this->template _get_scalar<type_seq>(0); }
		void set_seq(type_seq v) { return this->template _set_scalar<type_seq>(0, v); }

		std::string_view get_client() const { return this->template _get_string<tll_scheme_offset_ptr_t>(8); }
		void set_client(std::string_view v) { return this->template _set_string<tll_scheme_offset_ptr_t>(8, v); }

		std::string_view get_block() const { return this->template _get_string<tll_scheme_offset_ptr_t>(16); }
		void set_block(std::string_view v) { return this->template _set_string<tll_scheme_offset_ptr_t>(16, v); }

		using type_attributes = tll::scheme::binder::List<Buf, Attribute::binder_type<Buf>, tll_scheme_offset_ptr_t>;
		const type_attributes get_attributes() const { return this->template _get_binder<type_attributes>(24); }
		type_attributes get_attributes() { return this->template _get_binder<type_attributes>(24); }
	};

	template <typename Buf>
	static binder_type<Buf> bind(Buf &buf) { return binder_type<Buf>(buf); }
};

struct Reply
{
	static constexpr size_t meta_size() { return 16; }
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

		using type_seq = int64_t;
		type_seq get_seq() const { return this->template _get_scalar<type_seq>(0); }
		void set_seq(type_seq v) { return this->template _set_scalar<type_seq>(0, v); }

		std::string_view get_server() const { return this->template _get_string<tll_scheme_offset_ptr_t>(8); }
		void set_server(std::string_view v) { return this->template _set_string<tll_scheme_offset_ptr_t>(8, v); }
	};

	template <typename Buf>
	static binder_type<Buf> bind(Buf &buf) { return binder_type<Buf>(buf); }
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
	static binder_type<Buf> bind(Buf &buf) { return binder_type<Buf>(buf); }
};

} // namespace stream_scheme
