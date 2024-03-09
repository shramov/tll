#include <tll/util/bench.h>

#include <tll/ring.h>
#include <tll/cppring.h>

using namespace tll::bench;
using namespace std::chrono_literals;

static constexpr unsigned MSIZE = 37;

void fill(void * ptr, size_t size, size_t i)
{
#if 0
	static constexpr unsigned MDATA = 57;
	const auto c = 'A' + i % MDATA;

	auto data = (size_t *) ptr;
	*data = i;
	memset(data + 1, c, size);
#endif
}

void * ringpub(ringbuffer_t * ring, size_t * idx)
{
	void * ptr;
	const auto i = *idx++;

	const auto size = i % MSIZE;

	while (ring_write_begin(ring, &ptr, sizeof(size_t) + size))
		ring_shift(ring);
	fill(ptr, size, i);
	ring_write_end(ring, ptr, sizeof(size_t) + size);

	return ptr;
}

template <typename R>
void * ringpubcpp(R * ring, size_t * idx)
{
	void * ptr;
	const auto i = *idx++;

	const auto size = i % MSIZE;

	while (ring->write_begin(&ptr, sizeof(size_t) + size))
		ring->shift();
	fill(ptr, size, i);
	ring->write_end(ptr, sizeof(size_t) + size);

	return ptr;
}

int main(int argc, char *argv[])
{
	auto pub = tll::PubRing::allocate(1024 * 1024);

	auto rpp = tll::Ring::bind(pub.get());
	ringbuffer_t ring = { (ring_header_t *) pub.get() };

	const unsigned count = 10000000;
	size_t idx = 0;

	prewarm(100ms);
	timeit(count, "c", ringpub, &ring, &idx); idx = 0;
	timeit(count, "c++", ringpubcpp<tll::Ring>, rpp, &idx); idx = 0;
	timeit(count, "c++-pub", ringpubcpp<tll::PubRing>, pub.get(), &idx); idx = 0;
	timeit(count, "c", ringpub, &ring, &idx); idx = 0;

	return 0;
}
