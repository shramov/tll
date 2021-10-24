#include <tll/channel.h>
#include <tll/logger.h>

#include "timeit.h"

using namespace std::chrono;
int post(tll::Channel * c, tll_msg_t * msg)
{
	return c->post(msg);
}

int main(int argc, char *argv[])
{
	constexpr size_t count = 100000;
	tll::Logger::set("tll", tll::Logger::Warning, true);

	std::string_view url = "null://;name=null";
	if (argc > 1)
		url = argv[1];
	auto c = tll::Channel::init(url);
	if (!c)
		return -1;
	if (c->open())
		return -1;

	char data[1024] = {};
	tll_msg_t msg = {};
	msg.data = data;
	msg.size = 128;

	timeit(count, "post", post, c.get(), &msg);

	return 0;
}

