#pragma once

#include <tll/scheme/binder.h>
#include <tll/util/conv.h>

namespace control_scheme {

static constexpr std::string_view scheme_string = R"(yamls+gz://eJydVE1TwjAQvfsrcusFZmjFqtwcUMcZPw5+jcfYLCVjSWqSogzDf3cTUtoitQ4X2CRv3nv7dqFPBJ3DiARjKaY8vQYTHBHC2YiEAyymHDKmR1gR0icrj82pmQU9Ypa5PWmjuEiD9VG/yfVCswJKtqid7QOWe8iamIXj6pS8FKwUPB5Ujw8f5e2wdnuplFTlw0m7P3C4P7QfwdzK9BYWkJV0saUDUcwdW0uKCqb8u7P1zPFuUQUX5gyPMjdcCj0iq8C+BNYo6gXrntO192PFDU9ohm8nPRJM4L1IsQ57Ze+YB9Y3YiqxjLB8UjSxVAOsX6kS1g9Gud71pCAplOYLOMjXvfQSb6Ctn3U9S0MNTIp5Xk3sPN55fs4Zfm4BYRjVwyYehcyrjTdnrZK/SIx1vul4nEkNzPuxB9+yywsnIpc+vXpiDzn4aEIXzf75JjMqRH10LQPWzu0WtTG/L5Ladg/D6KxC3IHWNHWJdOycG8lWCnMJ43/ukhQYRubXZ0INtZn9Wgz71d0wfDY8xMNfEMqYau7WHhCzLv74XfpYrqT6oqoW3Wncmg/DmXf6b+qW4df/jDaTd7tVyZ5HB6zKD7scpTI=)";

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

struct Ok
{
	static constexpr size_t meta_size() { return 0; }
	static constexpr std::string_view meta_name() { return "Ok"; }
	static constexpr int meta_id() { return 40; }

	template <typename Buf>
	struct binder_type : public tll::scheme::Binder<Buf>
	{
		using tll::scheme::Binder<Buf>::Binder;

		static constexpr auto meta_size() { return Ok::meta_size(); }
		static constexpr auto meta_name() { return Ok::meta_name(); }
		static constexpr auto meta_id() { return Ok::meta_id(); }
		void view_resize() { this->_view_resize(meta_size()); }
	};

	template <typename Buf>
	static binder_type<Buf> bind(Buf &buf, size_t offset = 0) { return binder_type<Buf>(tll::make_view(buf).view(offset)); }
};

struct Error
{
	static constexpr size_t meta_size() { return 8; }
	static constexpr std::string_view meta_name() { return "Error"; }
	static constexpr int meta_id() { return 50; }

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
	};

	template <typename Buf>
	static binder_type<Buf> bind(Buf &buf, size_t offset = 0) { return binder_type<Buf>(tll::make_view(buf).view(offset)); }
};

struct SetLogLevel
{
	static constexpr size_t meta_size() { return 10; }
	static constexpr std::string_view meta_name() { return "SetLogLevel"; }
	static constexpr int meta_id() { return 60; }

	enum class level: uint8_t
	{
		Trace = 0,
		Debug = 1,
		Info = 2,
		Warning = 3,
		Error = 4,
		Critical = 5,
	};

	enum class recursive: uint8_t
	{
		No = 0,
		Yes = 1,
	};

	template <typename Buf>
	struct binder_type : public tll::scheme::Binder<Buf>
	{
		using tll::scheme::Binder<Buf>::Binder;

		static constexpr auto meta_size() { return SetLogLevel::meta_size(); }
		static constexpr auto meta_name() { return SetLogLevel::meta_name(); }
		static constexpr auto meta_id() { return SetLogLevel::meta_id(); }
		void view_resize() { this->_view_resize(meta_size()); }

		std::string_view get_prefix() const { return this->template _get_string<tll_scheme_offset_ptr_t>(0); }
		void set_prefix(std::string_view v) { return this->template _set_string<tll_scheme_offset_ptr_t>(0, v); }

		using type_level = level;
		type_level get_level() const { return this->template _get_scalar<type_level>(8); }
		void set_level(type_level v) { return this->template _set_scalar<type_level>(8, v); }

		using type_recursive = recursive;
		type_recursive get_recursive() const { return this->template _get_scalar<type_recursive>(9); }
		void set_recursive(type_recursive v) { return this->template _set_scalar<type_recursive>(9, v); }
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

struct Message
{
	static constexpr size_t meta_size() { return 34; }
	static constexpr std::string_view meta_name() { return "Message"; }

	enum class type: int16_t
	{
		Data = 0,
		Control = 1,
	};

	template <typename Buf>
	struct binder_type : public tll::scheme::Binder<Buf>
	{
		using tll::scheme::Binder<Buf>::Binder;

		static constexpr auto meta_size() { return Message::meta_size(); }
		static constexpr auto meta_name() { return Message::meta_name(); }
		void view_resize() { this->_view_resize(meta_size()); }

		using type_type = type;
		type_type get_type() const { return this->template _get_scalar<type_type>(0); }
		void set_type(type_type v) { return this->template _set_scalar<type_type>(0, v); }

		std::string_view get_name() const { return this->template _get_string<tll_scheme_offset_ptr_t>(2); }
		void set_name(std::string_view v) { return this->template _set_string<tll_scheme_offset_ptr_t>(2, v); }

		using type_seq = int64_t;
		type_seq get_seq() const { return this->template _get_scalar<type_seq>(10); }
		void set_seq(type_seq v) { return this->template _set_scalar<type_seq>(10, v); }

		using type_addr = uint64_t;
		type_addr get_addr() const { return this->template _get_scalar<type_addr>(18); }
		void set_addr(type_addr v) { return this->template _set_scalar<type_addr>(18, v); }

		std::string_view get_data() const { return this->template _get_string<tll_scheme_offset_ptr_t>(26); }
		void set_data(std::string_view v) { return this->template _set_string<tll_scheme_offset_ptr_t>(26, v); }
	};

	template <typename Buf>
	static binder_type<Buf> bind(Buf &buf, size_t offset = 0) { return binder_type<Buf>(tll::make_view(buf).view(offset)); }
};

struct MessageForward
{
	static constexpr size_t meta_size() { return 42; }
	static constexpr std::string_view meta_name() { return "MessageForward"; }
	static constexpr int meta_id() { return 4176; }

	template <typename Buf>
	struct binder_type : public tll::scheme::Binder<Buf>
	{
		using tll::scheme::Binder<Buf>::Binder;

		static constexpr auto meta_size() { return MessageForward::meta_size(); }
		static constexpr auto meta_name() { return MessageForward::meta_name(); }
		static constexpr auto meta_id() { return MessageForward::meta_id(); }
		void view_resize() { this->_view_resize(meta_size()); }

		std::string_view get_dest() const { return this->template _get_string<tll_scheme_offset_ptr_t>(0); }
		void set_dest(std::string_view v) { return this->template _set_string<tll_scheme_offset_ptr_t>(0, v); }

		using type_data = Message::binder_type<Buf>;
		const type_data get_data() const { return this->template _get_binder<type_data>(8); }
		type_data get_data() { return this->template _get_binder<type_data>(8); }
	};

	template <typename Buf>
	static binder_type<Buf> bind(Buf &buf, size_t offset = 0) { return binder_type<Buf>(tll::make_view(buf).view(offset)); }
};

struct ChannelClose
{
	static constexpr size_t meta_size() { return 8; }
	static constexpr std::string_view meta_name() { return "ChannelClose"; }
	static constexpr int meta_id() { return 4192; }

	template <typename Buf>
	struct binder_type : public tll::scheme::Binder<Buf>
	{
		using tll::scheme::Binder<Buf>::Binder;

		static constexpr auto meta_size() { return ChannelClose::meta_size(); }
		static constexpr auto meta_name() { return ChannelClose::meta_name(); }
		static constexpr auto meta_id() { return ChannelClose::meta_id(); }
		void view_resize() { this->_view_resize(meta_size()); }

		std::string_view get_channel() const { return this->template _get_string<tll_scheme_offset_ptr_t>(0); }
		void set_channel(std::string_view v) { return this->template _set_string<tll_scheme_offset_ptr_t>(0, v); }
	};

	template <typename Buf>
	static binder_type<Buf> bind(Buf &buf, size_t offset = 0) { return binder_type<Buf>(tll::make_view(buf).view(offset)); }
};

} // namespace control_scheme

template <>
struct tll::conv::dump<control_scheme::SetLogLevel::level> : public to_string_from_string_buf<control_scheme::SetLogLevel::level>
{
	template <typename Buf>
	static inline std::string_view to_string_buf(const control_scheme::SetLogLevel::level &v, Buf &buf)
	{
		switch (v) {
		case control_scheme::SetLogLevel::level::Critical: return "Critical";
		case control_scheme::SetLogLevel::level::Debug: return "Debug";
		case control_scheme::SetLogLevel::level::Error: return "Error";
		case control_scheme::SetLogLevel::level::Info: return "Info";
		case control_scheme::SetLogLevel::level::Trace: return "Trace";
		case control_scheme::SetLogLevel::level::Warning: return "Warning";
		default: break;
		}
		return tll::conv::to_string_buf<uint8_t, Buf>((uint8_t) v, buf);
	}
};

template <>
struct tll::conv::dump<control_scheme::SetLogLevel::recursive> : public to_string_from_string_buf<control_scheme::SetLogLevel::recursive>
{
	template <typename Buf>
	static inline std::string_view to_string_buf(const control_scheme::SetLogLevel::recursive &v, Buf &buf)
	{
		switch (v) {
		case control_scheme::SetLogLevel::recursive::No: return "No";
		case control_scheme::SetLogLevel::recursive::Yes: return "Yes";
		default: break;
		}
		return tll::conv::to_string_buf<uint8_t, Buf>((uint8_t) v, buf);
	}
};

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

template <>
struct tll::conv::dump<control_scheme::Message::type> : public to_string_from_string_buf<control_scheme::Message::type>
{
	template <typename Buf>
	static inline std::string_view to_string_buf(const control_scheme::Message::type &v, Buf &buf)
	{
		switch (v) {
		case control_scheme::Message::type::Control: return "Control";
		case control_scheme::Message::type::Data: return "Data";
		default: break;
		}
		return tll::conv::to_string_buf<int16_t, Buf>((int16_t) v, buf);
	}
};
