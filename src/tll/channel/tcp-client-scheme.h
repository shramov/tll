#include <tll/scheme/binder.h>

namespace tcp_client_scheme {

static constexpr std::string_view scheme_string = R"(yamls+gz://eJzTVchLzE21UlAPL8osSXUrzclR51JQyEyxUjA24NJFkQxKTUyphMmaGHABAAMnEIg=)";

struct WriteFull
{
	static constexpr size_t meta_size() { return 0; }
	static constexpr std::string_view meta_name() { return "WriteFull"; }
	static constexpr int meta_id() { return 30; }

	template <typename Buf>
	struct binder_type : public tll::scheme::Binder<Buf>
	{
		using tll::scheme::Binder<Buf>::Binder;

		static constexpr auto meta_size() { return WriteFull::meta_size(); }
		static constexpr auto meta_name() { return WriteFull::meta_name(); }
		static constexpr auto meta_id() { return WriteFull::meta_id(); }
		void view_resize() { this->_view_resize(meta_size()); }
	};

	template <typename Buf>
	static binder_type<Buf> bind(Buf &buf) { return binder_type<Buf>(buf); }
};

struct WriteReady
{
	static constexpr size_t meta_size() { return 0; }
	static constexpr std::string_view meta_name() { return "WriteReady"; }
	static constexpr int meta_id() { return 40; }

	template <typename Buf>
	struct binder_type : public tll::scheme::Binder<Buf>
	{
		using tll::scheme::Binder<Buf>::Binder;

		static constexpr auto meta_size() { return WriteReady::meta_size(); }
		static constexpr auto meta_name() { return WriteReady::meta_name(); }
		static constexpr auto meta_id() { return WriteReady::meta_id(); }
		void view_resize() { this->_view_resize(meta_size()); }
	};

	template <typename Buf>
	static binder_type<Buf> bind(Buf &buf) { return binder_type<Buf>(buf); }
};

} // namespace tcp_client_scheme
