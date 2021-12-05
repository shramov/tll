#pragma once

#include <tll/scheme/binder.h>
#include <tll/util/conv.h>

namespace processor_scheme {

static constexpr std::string_view scheme_string = R"(yamls+gz://eJxdkEEOgjAQRfecYnbdQAKIBrszwtqF8QCEjtoECmmLCSG9uwM0GNk0/3fedP40AlW1yIHdbWWxGNqeBQBScMji8ymI/sqPXtC5AUmSkkQ1tIaTAE8xDpMde+oapLJ5uBB0xy61lZ+5nIbArk1nUJCJvZHqRe5ArkBjdTeSO5Irte406Yz0rUe1colzNPIpsRF+eASTz1q/K6WwYSGsMRg9N3e5HWeWtBu1hne7necvKZX4LZ3mwRfpaVmS)";

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

} // namespace processor_scheme

template <>
struct tll::conv::dump<processor_scheme::StateUpdate::State> : public to_string_from_string_buf<processor_scheme::StateUpdate::State>
{
	template <typename Buf>
	static inline std::string_view to_string_buf(const processor_scheme::StateUpdate::State &v, Buf &buf)
	{
		switch (v) {
		case processor_scheme::StateUpdate::State::Active: return "Active";
		case processor_scheme::StateUpdate::State::Closed: return "Closed";
		case processor_scheme::StateUpdate::State::Closing: return "Closing";
		case processor_scheme::StateUpdate::State::Destroy: return "Destroy";
		case processor_scheme::StateUpdate::State::Error: return "Error";
		case processor_scheme::StateUpdate::State::Opening: return "Opening";
		default: break;
		}
		return tll::conv::to_string_buf<uint8_t, Buf>((uint8_t) v, buf);
	}
};
