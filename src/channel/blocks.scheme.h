#pragma once

#include <tll/scheme/binder.h>
#include <tll/util/conv.h>

namespace blocks_scheme {

static constexpr std::string_view scheme_string = R"(yamls+gz://eJx1zTEKAjEQBdA+p5gujYGsiEVK0VrwBlkzGwbXyWLSLEvu7ogSUbGa/5kPzwD7KzrQuzGdL1oBUHDQWStpIBxDdpIADCyvYZkn1Ct4HGn9XHC7kZ6mQomzg+W5kF8uN+Koa1XmQzl5jtio7j/VYyR+W8RFqPo1Qg6/kwYeOByHvS++eWur7t+vRdI=)";

struct Block
{
	static constexpr size_t meta_size() { return 64; }
	static constexpr std::string_view meta_name() { return "Block"; }
	static constexpr int meta_id() { return 100; }

	template <typename Buf>
	struct binder_type : public tll::scheme::Binder<Buf>
	{
		using tll::scheme::Binder<Buf>::Binder;

		static constexpr auto meta_size() { return Block::meta_size(); }
		static constexpr auto meta_name() { return Block::meta_name(); }
		static constexpr auto meta_id() { return Block::meta_id(); }
		void view_resize() { this->_view_resize(meta_size()); }

		std::string_view get_type() const { return this->template _get_bytestring<64>(0); }
		void set_type(std::string_view v) { return this->template _set_bytestring<64>(0, v); }
	};

	template <typename Buf>
	static binder_type<Buf> bind(Buf &buf, size_t offset = 0) { return binder_type<Buf>(tll::make_view(buf).view(offset)); }
};

struct BlockRange
{
	static constexpr size_t meta_size() { return 16; }
	static constexpr std::string_view meta_name() { return "BlockRange"; }
	static constexpr int meta_id() { return 110; }

	template <typename Buf>
	struct binder_type : public tll::scheme::Binder<Buf>
	{
		using tll::scheme::Binder<Buf>::Binder;

		static constexpr auto meta_size() { return BlockRange::meta_size(); }
		static constexpr auto meta_name() { return BlockRange::meta_name(); }
		static constexpr auto meta_id() { return BlockRange::meta_id(); }
		void view_resize() { this->_view_resize(meta_size()); }

		using type_begin = int64_t;
		type_begin get_begin() const { return this->template _get_scalar<type_begin>(0); }
		void set_begin(type_begin v) { return this->template _set_scalar<type_begin>(0, v); }

		using type_end = int64_t;
		type_end get_end() const { return this->template _get_scalar<type_end>(8); }
		void set_end(type_end v) { return this->template _set_scalar<type_end>(8, v); }
	};

	template <typename Buf>
	static binder_type<Buf> bind(Buf &buf, size_t offset = 0) { return binder_type<Buf>(tll::make_view(buf).view(offset)); }
};

struct EndOfData
{
	static constexpr size_t meta_size() { return 0; }
	static constexpr std::string_view meta_name() { return "EndOfData"; }
	static constexpr int meta_id() { return 120; }

	template <typename Buf>
	struct binder_type : public tll::scheme::Binder<Buf>
	{
		using tll::scheme::Binder<Buf>::Binder;

		static constexpr auto meta_size() { return EndOfData::meta_size(); }
		static constexpr auto meta_name() { return EndOfData::meta_name(); }
		static constexpr auto meta_id() { return EndOfData::meta_id(); }
		void view_resize() { this->_view_resize(meta_size()); }
	};

	template <typename Buf>
	static binder_type<Buf> bind(Buf &buf, size_t offset = 0) { return binder_type<Buf>(tll::make_view(buf).view(offset)); }
};

} // namespace blocks_scheme
