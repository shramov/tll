#pragma once

#include <tll/scheme/binder.h>
#include <tll/util/conv.h>

namespace control_scheme {

static constexpr std::string_view scheme_string = R"(yamls+gz://eJyFUE2rwjAQvPdX7C2XFpq+KtqbPOUd30H0HsyqwXYb2lQo0v/u9ssvFC9hJjPMDhMAqQwTEL857c3hD53wAIxOQIYM9gZTXSaMAAK4DF6r3FH44GrbstIVhg6i8YLnrK1KKxzTos9pJ6zfhD17zl3W15Mr0uPBn/Aurp1yuKwyO4pxOJ++yBur+b0ZpIwYIlXZ0Ld3iQQufYfKkJv5nYP/xGLnzLmVI5/LpHmJmkk4kLYsV2K2RO6e18wmzFZFkReMY8b/Fqn3yab5ONbuqIgw/TpY2bW9ufryzZtJHiaLZTTzrla3mVk=)";

struct ConfigGet
{
	static constexpr size_t meta_size() { return 8; }
	static constexpr std::string_view meta_name() { return "ConfigGet"; }
	static constexpr int meta_id() { return 10; }

	template <typename Buf>
	struct binder_type : public tll::scheme::Binder<Buf>
	{
		using tll::scheme::Binder<Buf>::Binder;

		static constexpr auto meta_size() { return ConfigGet::meta_size(); }
		static constexpr auto meta_name() { return ConfigGet::meta_name(); }
		static constexpr auto meta_id() { return ConfigGet::meta_id(); }
		void view_resize() { this->_view_resize(meta_size()); }

		std::string_view get_path() const { return this->template _get_string<tll_scheme_offset_ptr_t>(0); }
		void set_path(std::string_view v) { return this->template _set_string<tll_scheme_offset_ptr_t>(0, v); }
	};

	template <typename Buf>
	static binder_type<Buf> bind(Buf &buf, size_t offset = 0) { return binder_type<Buf>(tll::make_view(buf).view(offset)); }
};

struct ConfigValue
{
	static constexpr size_t meta_size() { return 16; }
	static constexpr std::string_view meta_name() { return "ConfigValue"; }
	static constexpr int meta_id() { return 20; }

	template <typename Buf>
	struct binder_type : public tll::scheme::Binder<Buf>
	{
		using tll::scheme::Binder<Buf>::Binder;

		static constexpr auto meta_size() { return ConfigValue::meta_size(); }
		static constexpr auto meta_name() { return ConfigValue::meta_name(); }
		static constexpr auto meta_id() { return ConfigValue::meta_id(); }
		void view_resize() { this->_view_resize(meta_size()); }

		std::string_view get_key() const { return this->template _get_string<tll_scheme_offset_ptr_t>(0); }
		void set_key(std::string_view v) { return this->template _set_string<tll_scheme_offset_ptr_t>(0, v); }

		std::string_view get_value() const { return this->template _get_string<tll_scheme_offset_ptr_t>(8); }
		void set_value(std::string_view v) { return this->template _set_string<tll_scheme_offset_ptr_t>(8, v); }
	};

	template <typename Buf>
	static binder_type<Buf> bind(Buf &buf, size_t offset = 0) { return binder_type<Buf>(tll::make_view(buf).view(offset)); }
};

struct ConfigEnd
{
	static constexpr size_t meta_size() { return 0; }
	static constexpr std::string_view meta_name() { return "ConfigEnd"; }
	static constexpr int meta_id() { return 30; }

	template <typename Buf>
	struct binder_type : public tll::scheme::Binder<Buf>
	{
		using tll::scheme::Binder<Buf>::Binder;

		static constexpr auto meta_size() { return ConfigEnd::meta_size(); }
		static constexpr auto meta_name() { return ConfigEnd::meta_name(); }
		static constexpr auto meta_id() { return ConfigEnd::meta_id(); }
		void view_resize() { this->_view_resize(meta_size()); }
	};

	template <typename Buf>
	static binder_type<Buf> bind(Buf &buf, size_t offset = 0) { return binder_type<Buf>(tll::make_view(buf).view(offset)); }
};

struct StateDump
{
	static constexpr size_t meta_size() { return 0; }
	static constexpr std::string_view meta_name() { return "StateDump"; }
	static constexpr int meta_id() { return 4096; }

	template <typename Buf>
	struct binder_type : public tll::scheme::Binder<Buf>
	{
		using tll::scheme::Binder<Buf>::Binder;

		static constexpr auto meta_size() { return StateDump::meta_size(); }
		static constexpr auto meta_name() { return StateDump::meta_name(); }
		static constexpr auto meta_id() { return StateDump::meta_id(); }
		void view_resize() { this->_view_resize(meta_size()); }
	};

	template <typename Buf>
	static binder_type<Buf> bind(Buf &buf, size_t offset = 0) { return binder_type<Buf>(tll::make_view(buf).view(offset)); }
};

struct StateUpdate
{
	static constexpr size_t meta_size() { return 9; }
	static constexpr std::string_view meta_name() { return "StateUpdate"; }
	static constexpr int meta_id() { return 4112; }

	enum class State: uint8_t
	{
		Closed = 0,
		Opening = 1,
		Active = 2,
		Closing = 3,
		Error = 4,
		Destroy = 5,
	};

	template <typename Buf>
	struct binder_type : public tll::scheme::Binder<Buf>
	{
		using tll::scheme::Binder<Buf>::Binder;

		static constexpr auto meta_size() { return StateUpdate::meta_size(); }
		static constexpr auto meta_name() { return StateUpdate::meta_name(); }
		static constexpr auto meta_id() { return StateUpdate::meta_id(); }
		void view_resize() { this->_view_resize(meta_size()); }

		std::string_view get_channel() const { return this->template _get_string<tll_scheme_offset_ptr_t>(0); }
		void set_channel(std::string_view v) { return this->template _set_string<tll_scheme_offset_ptr_t>(0, v); }

		using type_state = State;
		type_state get_state() const { return this->template _get_scalar<type_state>(8); }
		void set_state(type_state v) { return this->template _set_scalar<type_state>(8, v); }
	};

	template <typename Buf>
	static binder_type<Buf> bind(Buf &buf, size_t offset = 0) { return binder_type<Buf>(tll::make_view(buf).view(offset)); }
};

struct StateDumpEnd
{
	static constexpr size_t meta_size() { return 0; }
	static constexpr std::string_view meta_name() { return "StateDumpEnd"; }
	static constexpr int meta_id() { return 4128; }

	template <typename Buf>
	struct binder_type : public tll::scheme::Binder<Buf>
	{
		using tll::scheme::Binder<Buf>::Binder;

		static constexpr auto meta_size() { return StateDumpEnd::meta_size(); }
		static constexpr auto meta_name() { return StateDumpEnd::meta_name(); }
		static constexpr auto meta_id() { return StateDumpEnd::meta_id(); }
		void view_resize() { this->_view_resize(meta_size()); }
	};

	template <typename Buf>
	static binder_type<Buf> bind(Buf &buf, size_t offset = 0) { return binder_type<Buf>(tll::make_view(buf).view(offset)); }
};

} // namespace control_scheme

template <>
struct tll::conv::dump<control_scheme::StateUpdate::State> : public to_string_from_string_buf<control_scheme::StateUpdate::State>
{
	template <typename Buf>
	static inline std::string_view to_string_buf(const control_scheme::StateUpdate::State &v, Buf &buf)
	{
		switch (v) {
		case control_scheme::StateUpdate::State::Active: return "Active";
		case control_scheme::StateUpdate::State::Closed: return "Closed";
		case control_scheme::StateUpdate::State::Closing: return "Closing";
		case control_scheme::StateUpdate::State::Destroy: return "Destroy";
		case control_scheme::StateUpdate::State::Error: return "Error";
		case control_scheme::StateUpdate::State::Opening: return "Opening";
		default: break;
		}
		return tll::conv::to_string_buf<uint8_t, Buf>((uint8_t) v, buf);
	}
};
