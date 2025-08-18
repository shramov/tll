#pragma once

#include <tll/scheme/binder.h>
#include <tll/util/conv.h>

namespace processor_scheme {

static constexpr std::string_view scheme_string = R"(yamls+gz://eJydkj1vgzAQhvf8Cm8sIAVCacJW5WOrOlSdqg5ufFBLYKhtWqUR/71nIOYrUaUu6A4/9nvv3XlE0Bxi4jxrqmFX5aWzIISzmITLTbTwRscvJcOvBXw/wBBElasYA9JRTkzO+lTirYoLvXYbAv85D0fNv8xx4BJnmxUKGCbLLuEixWyF2Q6UlsUJszvM9lIWEuMQ46cSRMv5dY2S71xfpA8ZTdVY2o/cliCv586G0jQFxyVFkijQjbjiP2Dec4mFKoU6bID5PfZmdBMOGeuUPXvv+EGFgAzvtTWgmjTV1hNONV2yVNu0KZQ0fizU2qsnAzHz2mOldiLBuiceQSnj9ma95uleoWnYrIxcpZyNoFUwNwSfIyQKZwhlTPZMdR3C7aJXujd1dCjkN5UD1/fRTY8Md+nPgYx1L30bCG/byTY728tugn+swi+FCgLh)";

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

	template <typename Buf>
	static binder_type<Buf> bind_reset(Buf &buf) { return tll::scheme::make_binder_reset<binder_type, Buf>(buf); }
};

struct StateUpdate
{
	static constexpr size_t meta_size() { return 11; }
	static constexpr std::string_view meta_name() { return "StateUpdate"; }
	static constexpr int meta_id() { return 4112; }
	static constexpr size_t offset_channel = 0;
	static constexpr size_t offset_state = 8;
	static constexpr size_t offset_flags = 9;

	enum class State: uint8_t
	{
		Closed = 0,
		Opening = 1,
		Active = 2,
		Closing = 3,
		Error = 4,
		Destroy = 5,
	};

	struct Flags: public tll::scheme::Bits<uint16_t>
	{
		using tll::scheme::Bits<uint16_t>::Bits;
		constexpr auto stage() const { return get(0, 1); };
		constexpr Flags & stage(bool v) { set(0, 1, v); return *this; };
		constexpr auto suspend() const { return get(1, 1); };
		constexpr Flags & suspend(bool v) { set(1, 1, v); return *this; };
		static std::map<std::string_view, value_type> bits_descriptor()
		{
			return {
				{ "stage", static_cast<value_type>(Bits::mask(1)) << 0 },
				{ "suspend", static_cast<value_type>(Bits::mask(1)) << 1 },
			};
		}
	};

	template <typename Buf>
	struct binder_type : public tll::scheme::Binder<Buf>
	{
		using tll::scheme::Binder<Buf>::Binder;

		static constexpr auto meta_size() { return StateUpdate::meta_size(); }
		static constexpr auto meta_name() { return StateUpdate::meta_name(); }
		static constexpr auto meta_id() { return StateUpdate::meta_id(); }
		void view_resize() { this->_view_resize(meta_size()); }

		std::string_view get_channel() const { return this->template _get_string<tll_scheme_offset_ptr_t>(offset_channel); }
		void set_channel(std::string_view v) { return this->template _set_string<tll_scheme_offset_ptr_t>(offset_channel, v); }

		using type_state = State;
		type_state get_state() const { return this->template _get_scalar<type_state>(offset_state); }
		void set_state(type_state v) { return this->template _set_scalar<type_state>(offset_state, v); }

		using type_flags = Flags;
		type_flags get_flags() const { return this->template _get_scalar<type_flags>(offset_flags); }
		void set_flags(type_flags v) { return this->template _set_scalar<type_flags>(offset_flags, v); }
	};

	template <typename Buf>
	static binder_type<Buf> bind(Buf &buf, size_t offset = 0) { return binder_type<Buf>(tll::make_view(buf).view(offset)); }

	template <typename Buf>
	static binder_type<Buf> bind_reset(Buf &buf) { return tll::scheme::make_binder_reset<binder_type, Buf>(buf); }
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

	template <typename Buf>
	static binder_type<Buf> bind_reset(Buf &buf) { return tll::scheme::make_binder_reset<binder_type, Buf>(buf); }
};

struct Message
{
	static constexpr size_t meta_size() { return 30; }
	static constexpr std::string_view meta_name() { return "Message"; }
	static constexpr size_t offset_type = 0;
	static constexpr size_t offset_msgid = 2;
	static constexpr size_t offset_seq = 6;
	static constexpr size_t offset_addr = 14;
	static constexpr size_t offset_data = 22;

	template <typename Buf>
	struct binder_type : public tll::scheme::Binder<Buf>
	{
		using tll::scheme::Binder<Buf>::Binder;

		static constexpr auto meta_size() { return Message::meta_size(); }
		static constexpr auto meta_name() { return Message::meta_name(); }
		void view_resize() { this->_view_resize(meta_size()); }

		using type_type = int16_t;
		type_type get_type() const { return this->template _get_scalar<type_type>(offset_type); }
		void set_type(type_type v) { return this->template _set_scalar<type_type>(offset_type, v); }

		using type_msgid = int32_t;
		type_msgid get_msgid() const { return this->template _get_scalar<type_msgid>(offset_msgid); }
		void set_msgid(type_msgid v) { return this->template _set_scalar<type_msgid>(offset_msgid, v); }

		using type_seq = int64_t;
		type_seq get_seq() const { return this->template _get_scalar<type_seq>(offset_seq); }
		void set_seq(type_seq v) { return this->template _set_scalar<type_seq>(offset_seq, v); }

		using type_addr = uint64_t;
		type_addr get_addr() const { return this->template _get_scalar<type_addr>(offset_addr); }
		void set_addr(type_addr v) { return this->template _set_scalar<type_addr>(offset_addr, v); }

		std::string_view get_data() const { return this->template _get_string<tll_scheme_offset_ptr_t>(offset_data); }
		void set_data(std::string_view v) { return this->template _set_string<tll_scheme_offset_ptr_t>(offset_data, v); }
	};

	template <typename Buf>
	static binder_type<Buf> bind(Buf &buf, size_t offset = 0) { return binder_type<Buf>(tll::make_view(buf).view(offset)); }

	template <typename Buf>
	static binder_type<Buf> bind_reset(Buf &buf) { return tll::scheme::make_binder_reset<binder_type, Buf>(buf); }
};

struct MessageForward
{
	static constexpr size_t meta_size() { return 38; }
	static constexpr std::string_view meta_name() { return "MessageForward"; }
	static constexpr int meta_id() { return 4176; }
	static constexpr size_t offset_dest = 0;
	static constexpr size_t offset_data = 8;

	template <typename Buf>
	struct binder_type : public tll::scheme::Binder<Buf>
	{
		using tll::scheme::Binder<Buf>::Binder;

		static constexpr auto meta_size() { return MessageForward::meta_size(); }
		static constexpr auto meta_name() { return MessageForward::meta_name(); }
		static constexpr auto meta_id() { return MessageForward::meta_id(); }
		void view_resize() { this->_view_resize(meta_size()); }

		std::string_view get_dest() const { return this->template _get_string<tll_scheme_offset_ptr_t>(offset_dest); }
		void set_dest(std::string_view v) { return this->template _set_string<tll_scheme_offset_ptr_t>(offset_dest, v); }

		using type_data = Message::binder_type<Buf>;
		const type_data get_data() const { return this->template _get_binder<type_data>(offset_data); }
		type_data get_data() { return this->template _get_binder<type_data>(offset_data); }
	};

	template <typename Buf>
	static binder_type<Buf> bind(Buf &buf, size_t offset = 0) { return binder_type<Buf>(tll::make_view(buf).view(offset)); }

	template <typename Buf>
	static binder_type<Buf> bind_reset(Buf &buf) { return tll::scheme::make_binder_reset<binder_type, Buf>(buf); }
};

struct ChannelClose
{
	static constexpr size_t meta_size() { return 8; }
	static constexpr std::string_view meta_name() { return "ChannelClose"; }
	static constexpr int meta_id() { return 4192; }
	static constexpr size_t offset_channel = 0;

	template <typename Buf>
	struct binder_type : public tll::scheme::Binder<Buf>
	{
		using tll::scheme::Binder<Buf>::Binder;

		static constexpr auto meta_size() { return ChannelClose::meta_size(); }
		static constexpr auto meta_name() { return ChannelClose::meta_name(); }
		static constexpr auto meta_id() { return ChannelClose::meta_id(); }
		void view_resize() { this->_view_resize(meta_size()); }

		std::string_view get_channel() const { return this->template _get_string<tll_scheme_offset_ptr_t>(offset_channel); }
		void set_channel(std::string_view v) { return this->template _set_string<tll_scheme_offset_ptr_t>(offset_channel, v); }
	};

	template <typename Buf>
	static binder_type<Buf> bind(Buf &buf, size_t offset = 0) { return binder_type<Buf>(tll::make_view(buf).view(offset)); }

	template <typename Buf>
	static binder_type<Buf> bind_reset(Buf &buf) { return tll::scheme::make_binder_reset<binder_type, Buf>(buf); }
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
