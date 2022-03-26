#include <tll/scheme/binder.h>
#include <tll/util/bench.h>

using namespace tll::bench;

struct __attribute__((packed)) caster
{
	tll::scheme::String<tll_scheme_offset_ptr_t> path;
};

template <typename Buf>
struct Binder : public tll::scheme::Binder<Buf>
{
	using tll::scheme::Binder<Buf>::Binder;

	static constexpr size_t meta_size() { return 8; }

	std::string_view get_path() const { return this->template _get_string<tll_scheme_offset_ptr_t>(0); }
	void set_path(std::string_view v) { return this->template _set_string<tll_scheme_offset_ptr_t>(0, v); }

	auto get_bpath() { return this->template _get_binder<tll::scheme::binder::String<Buf, tll_scheme_offset_ptr_t>>(0); }
	auto get_bpath() const { return this->template _get_binder<tll::scheme::binder::String<const Buf, tll_scheme_offset_ptr_t>>(0); }
};

std::string_view null() { return ""; }

std::string_view caster_get(const caster * data)
{
	return data->path;
}

std::string_view binder_data(const Binder<std::vector<char>> *data)
{
	return data->get_path();
}

template <typename T>
std::string_view binder_get(T &data)
{
	return data.get_path();
}

template <typename T>
std::string_view binder_binder(T &data)
{
	return data.get_bpath();
}

auto vector_data(const std::vector<char> &data)
{
	return data.data();
}

auto memoryview_data(const tll::memoryview<std::vector<char>> &data)
{
	return data.view(1).data();
}

template <typename Buf>
void bench(size_t count, std::string_view prefix, Buf & buf)
{
	auto binder = tll::scheme::make_binder<Binder>(buf);
	timeit(count, fmt::format("binder<{}>::string", prefix), binder_get<Binder<Buf>>, binder);
	timeit(count, fmt::format("binder<{}>::string const", prefix), binder_get<const Binder<Buf>>, binder);
	timeit(count, fmt::format("binder<{}>::binder", prefix), binder_binder<Binder<Buf>>, binder);
	timeit(count, fmt::format("binder<{}>::binder const", prefix), binder_binder<const Binder<Buf>>, binder);
}

int main(int argc, char *argv[])
{
	std::vector<char> buf;
	auto binder = tll::scheme::make_binder<Binder>(buf);
	buf.resize(binder.meta_size());
	binder.set_path("abcdef");
	tll::const_memory memory = { buf.data(), buf.size() };

	constexpr size_t count = 10000000;

	timeit(count, "prewarm", binder_data, &binder);
	timeit(count, "cast", caster_get, (const caster *) buf.data());
	timeit(count, "binder", &binder_data, &binder);
	timeit(count, "null", null);
	bench(count, "vector", buf);
	bench(count, "memory", memory);
	timeit(count, "vector<>::data", vector_data, buf);
	auto view = tll::make_view(buf);
	timeit(count, "memoryview<>::data", memoryview_data, view);

	return 0;
}
