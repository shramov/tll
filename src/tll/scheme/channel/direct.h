#pragma once

#include <tll/scheme/binder.h>
#include <tll/util/conv.h>

namespace direct_scheme {

static constexpr std::string_view scheme_string = R"(yamls+gz://eJwtjs0KgzAQhO8+xd5yUVBjf8hNas89lD6AmG0JaCLJWhDx3buNuSzz7Q47U4DtJ1QgOuNxoCf1hK9Z8xQZgNEK6kqWl/LcSMkLtMsUFAsAEb1CwUbrzB8WY+maRwfvRDuQ+f7PdQ7iNrqAmqFMYOyHSTJ1GMi7lenEdPfeedYN68eM9vBV+86Rb4OjTuEFbKl3iC1yOEqkUnv2A6VOPts=)";

struct DirectStateUpdate
{
	static constexpr size_t meta_size() { return 1; }
	static constexpr std::string_view meta_name() { return "DirectStateUpdate"; }
	static constexpr int meta_id() { return 2130706433; }

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

		static constexpr auto meta_size() { return DirectStateUpdate::meta_size(); }
		static constexpr auto meta_name() { return DirectStateUpdate::meta_name(); }
		static constexpr auto meta_id() { return DirectStateUpdate::meta_id(); }
		void view_resize() { this->_view_resize(meta_size()); }

		using type_state = State;
		type_state get_state() const { return this->template _get_scalar<type_state>(0); }
		void set_state(type_state v) { return this->template _set_scalar<type_state>(0, v); }
	};

	template <typename Buf>
	static binder_type<Buf> bind(Buf &buf, size_t offset = 0) { return binder_type<Buf>(tll::make_view(buf).view(offset)); }

	template <typename Buf>
	static binder_type<Buf> bind_reset(Buf &buf) { return tll::scheme::make_binder_reset<binder_type, Buf>(buf); }
};

} // namespace direct_scheme

template <>
struct tll::conv::dump<direct_scheme::DirectStateUpdate::State> : public to_string_from_string_buf<direct_scheme::DirectStateUpdate::State>
{
	template <typename Buf>
	static inline std::string_view to_string_buf(const direct_scheme::DirectStateUpdate::State &v, Buf &buf)
	{
		switch (v) {
		case direct_scheme::DirectStateUpdate::State::Active: return "Active";
		case direct_scheme::DirectStateUpdate::State::Closed: return "Closed";
		case direct_scheme::DirectStateUpdate::State::Closing: return "Closing";
		case direct_scheme::DirectStateUpdate::State::Destroy: return "Destroy";
		case direct_scheme::DirectStateUpdate::State::Error: return "Error";
		case direct_scheme::DirectStateUpdate::State::Opening: return "Opening";
		default: break;
		}
		return tll::conv::to_string_buf<uint8_t, Buf>((uint8_t) v, buf);
	}
};
