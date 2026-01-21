#pragma once

#include <tll/scheme/binder.h>
#include <tll/util/conv.h>

namespace tcp_scheme {

static constexpr std::string_view scheme_string = R"(yamls+gz://eJxdj0sLwjAQhO/9FXvbi4X6RHoTRfAmXjyISE1XGqhJMFuxlPx3U7X2cdvkm9mZDUEld4oBMQDQhqVWNoYKhTFhTaxJBKHnLMzFiozuhC4IG9fxIZm2RZ7XdpnGMI0G8EBJWjZ0NqTbvLBZQ+cdutZKkeAGjSM/FOrTzk8AuNuvVOmLVZ/fGE7VzynNc4Yj4NLUr0Iqnk7QjaDDFy2/lkzjRZf7fa++f4nu7HzqTVKe/vLDvz7Tllv9t5YbaIx+cH9nndleu5FW9A+eRMEbyh16Aw==)";

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

		template <typename RBuf>
		void copy(const binder_type<RBuf> &rhs)
		{
		}
	};

	template <typename Buf>
	static binder_type<Buf> bind(Buf &buf, size_t offset = 0) { return binder_type<Buf>(tll::make_view(buf).view(offset)); }

	template <typename Buf>
	static binder_type<Buf> bind_reset(Buf &buf) { return tll::scheme::make_binder_reset<binder_type, Buf>(buf); }
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

		template <typename RBuf>
		void copy(const binder_type<RBuf> &rhs)
		{
		}
	};

	template <typename Buf>
	static binder_type<Buf> bind(Buf &buf, size_t offset = 0) { return binder_type<Buf>(tll::make_view(buf).view(offset)); }

	template <typename Buf>
	static binder_type<Buf> bind_reset(Buf &buf) { return tll::scheme::make_binder_reset<binder_type, Buf>(buf); }
};

struct WriteFlush
{
	static constexpr size_t meta_size() { return 0; }
	static constexpr std::string_view meta_name() { return "WriteFlush"; }
	static constexpr int meta_id() { return 50; }

	template <typename Buf>
	struct binder_type : public tll::scheme::Binder<Buf>
	{
		using tll::scheme::Binder<Buf>::Binder;

		static constexpr auto meta_size() { return WriteFlush::meta_size(); }
		static constexpr auto meta_name() { return WriteFlush::meta_name(); }
		static constexpr auto meta_id() { return WriteFlush::meta_id(); }
		void view_resize() { this->_view_resize(meta_size()); }

		template <typename RBuf>
		void copy(const binder_type<RBuf> &rhs)
		{
		}
	};

	template <typename Buf>
	static binder_type<Buf> bind(Buf &buf, size_t offset = 0) { return binder_type<Buf>(tll::make_view(buf).view(offset)); }

	template <typename Buf>
	static binder_type<Buf> bind_reset(Buf &buf) { return tll::scheme::make_binder_reset<binder_type, Buf>(buf); }
};

struct Connect
{
	static constexpr size_t meta_size() { return 19; }
	static constexpr std::string_view meta_name() { return "Connect"; }
	static constexpr int meta_id() { return 10; }
	static constexpr size_t offset_host = 0;
	static constexpr size_t offset_port = 17;

	template <typename Buf>
	struct IPAny: public tll::scheme::binder::Union<Buf, int8_t>
	{
		using union_index_type = int8_t;
		using tll::scheme::binder::Union<Buf, union_index_type>::Union;

		static constexpr union_index_type index_ipv4 = 0;
		using type_ipv4 = uint32_t;
		std::optional<uint32_t> get_ipv4() const { if (this->union_type() != index_ipv4) return std::nullopt; return unchecked_ipv4(); }
		uint32_t unchecked_ipv4() const { return this->template _get_scalar<uint32_t>(1); }
		void set_ipv4(const uint32_t &v) { this->_set_type(index_ipv4); this->template _set_scalar<uint32_t>(1, v); }

		static constexpr union_index_type index_ipv6 = 1;
		using type_ipv6 = tll::scheme::Bytes<16>;
		std::optional<tll::scheme::Bytes<16>> get_ipv6() const { if (this->union_type() != index_ipv6) return std::nullopt; return unchecked_ipv6(); }
		tll::scheme::Bytes<16> unchecked_ipv6() const { return this->template _get_bytes<16>(1); }
		void set_ipv6(const tll::scheme::Bytes<16> &v) const { this->_set_type(index_ipv6); return this->template _set_bytes<16>(1, {v.data(), v.size()}); }
		void set_ipv6(std::string_view v) { this->_set_type(index_ipv6); return this->template _set_bytestring<16>(1, v); }

		static constexpr union_index_type index_unix = 2;
		using type_unix = uint8_t;
		std::optional<uint8_t> get_unix() const { if (this->union_type() != index_unix) return std::nullopt; return unchecked_unix(); }
		uint8_t unchecked_unix() const { return this->template _get_scalar<uint8_t>(1); }
		void set_unix(const uint8_t &v) { this->_set_type(index_unix); this->template _set_scalar<uint8_t>(1, v); }
	};


	template <typename Buf>
	struct binder_type : public tll::scheme::Binder<Buf>
	{
		using tll::scheme::Binder<Buf>::Binder;

		static constexpr auto meta_size() { return Connect::meta_size(); }
		static constexpr auto meta_name() { return Connect::meta_name(); }
		static constexpr auto meta_id() { return Connect::meta_id(); }
		void view_resize() { this->_view_resize(meta_size()); }

		template <typename RBuf>
		void copy(const binder_type<RBuf> &rhs)
		{
			get_host().copy(rhs.get_host());
			set_port(rhs.get_port());
		}

		using type_host = IPAny<Buf>;
		using const_type_host = IPAny<const Buf>;
		const_type_host get_host() const { return this->template _get_binder<const_type_host>(offset_host); }
		type_host get_host() { return this->template _get_binder<type_host>(offset_host); }

		using type_port = uint16_t;
		type_port get_port() const { return this->template _get_scalar<type_port>(offset_port); }
		void set_port(type_port v) { return this->template _set_scalar<type_port>(offset_port, v); }
	};

	template <typename Buf>
	static binder_type<Buf> bind(Buf &buf, size_t offset = 0) { return binder_type<Buf>(tll::make_view(buf).view(offset)); }

	template <typename Buf>
	static binder_type<Buf> bind_reset(Buf &buf) { return tll::scheme::make_binder_reset<binder_type, Buf>(buf); }
};

struct Disconnect
{
	static constexpr size_t meta_size() { return 0; }
	static constexpr std::string_view meta_name() { return "Disconnect"; }
	static constexpr int meta_id() { return 20; }

	template <typename Buf>
	struct binder_type : public tll::scheme::Binder<Buf>
	{
		using tll::scheme::Binder<Buf>::Binder;

		static constexpr auto meta_size() { return Disconnect::meta_size(); }
		static constexpr auto meta_name() { return Disconnect::meta_name(); }
		static constexpr auto meta_id() { return Disconnect::meta_id(); }
		void view_resize() { this->_view_resize(meta_size()); }

		template <typename RBuf>
		void copy(const binder_type<RBuf> &rhs)
		{
		}
	};

	template <typename Buf>
	static binder_type<Buf> bind(Buf &buf, size_t offset = 0) { return binder_type<Buf>(tll::make_view(buf).view(offset)); }

	template <typename Buf>
	static binder_type<Buf> bind_reset(Buf &buf) { return tll::scheme::make_binder_reset<binder_type, Buf>(buf); }
};

} // namespace tcp_scheme
