#pragma once

#include <tll/scheme/binder.h>
#include <tll/util/conv.h>

namespace processor_scheme {

static constexpr std::string_view scheme_string = R"(yamls+gz://eJyFkctugzAQRfd8xey8ASkQQlN2VZPsoiyqfoAVT6klMMQ2raLI/57h0VAeVTfWXPvYc+c6AMULTIG9WW5xVxcV8wCkSCFePSdeMDp+rwStDyAMIypR1YVJqYCeYinc7LWiW7VUduu3BO2xl7OVX81x5AN7zUuDgsSqF1JlpNakdmisLq+kNqT2Wpea6pjqU4Wq40LnqOWHxFz0zQO49V7Pn1wpzJkPnQ1GzzW33IQzrdsH1Zl3k5mbSPZKDENH24E4ojE8axNZttI8PXSgOMJkZqMwmRQjaB3NveJlhCTxDOFC6IGplyH6QL4QzHSiQ6m/uf419VPy54yCvuvfrMd9f3Jz3h2eA7ne)";

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
	static constexpr size_t meta_size() { return 30; }
	static constexpr std::string_view meta_name() { return "Message"; }

	template <typename Buf>
	struct binder_type : public tll::scheme::Binder<Buf>
	{
		using tll::scheme::Binder<Buf>::Binder;

		static constexpr auto meta_size() { return Message::meta_size(); }
		static constexpr auto meta_name() { return Message::meta_name(); }
		void view_resize() { this->_view_resize(meta_size()); }

		using type_type = int16_t;
		type_type get_type() const { return this->template _get_scalar<type_type>(0); }
		void set_type(type_type v) { return this->template _set_scalar<type_type>(0, v); }

		using type_msgid = int32_t;
		type_msgid get_msgid() const { return this->template _get_scalar<type_msgid>(2); }
		void set_msgid(type_msgid v) { return this->template _set_scalar<type_msgid>(2, v); }

		using type_seq = int64_t;
		type_seq get_seq() const { return this->template _get_scalar<type_seq>(6); }
		void set_seq(type_seq v) { return this->template _set_scalar<type_seq>(6, v); }

		using type_addr = uint64_t;
		type_addr get_addr() const { return this->template _get_scalar<type_addr>(14); }
		void set_addr(type_addr v) { return this->template _set_scalar<type_addr>(14, v); }

		std::string_view get_data() const { return this->template _get_string<tll_scheme_offset_ptr_t>(22); }
		void set_data(std::string_view v) { return this->template _set_string<tll_scheme_offset_ptr_t>(22, v); }
	};

	template <typename Buf>
	static binder_type<Buf> bind(Buf &buf, size_t offset = 0) { return binder_type<Buf>(tll::make_view(buf).view(offset)); }
};

struct MessageForward
{
	static constexpr size_t meta_size() { return 38; }
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
