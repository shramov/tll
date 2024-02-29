#include <functional>
#include <cstdio>

struct Channel {};
struct Message { unsigned size = 0; };

using Callback = int (*)(const Channel *, const Message *, void * data);

template <typename T, int (T::*F)(const Channel *, const Message *)>
static int _proxy_member(const Channel * c, const Message * msg, void * data)
{
	return (static_cast<T *>(data)->*F)(c, msg);
}

template <typename T, int (T::*F)(const Channel *, const Message *)>
static int _proxy_invoke(const Channel * c, const Message * msg, void * data)
{
	return std::invoke(F, static_cast<T *>(data), c, msg);
}

struct Counter
{
	unsigned count = 0;

	int method(const Channel *, const Message * msg)
	{
		count += msg->size;
		printf("%d\n", count);
		return count;
	}
};

int main()
{
	Callback cb0 = _proxy_member<Counter, &Counter::method>;
	Callback cb1 = _proxy_invoke<Counter, &Counter::method>;
	Counter c0, c1;
	Message m0 = { 10 };
	Message m1 = { 20 };

	for (auto i = 0; i < 1000; i++) {
		auto r0 = cb0(nullptr, &m0, &c0);
		auto r1 = cb1(nullptr, &m1, &c1);
		printf("%d %d\n", r0, r1);
	}

	return 0;
}
