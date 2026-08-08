#include "gtest/gtest.h"

#include "tll/config.h"
#include "tll/keyring.h"
#include "tll/util/keyref.h"

#include <chrono>
#include <fmt/format.h>

using namespace tll::keyring;

inline std::string _random_name()
{
	return fmt::format("tll:test:{:x}", std::chrono::steady_clock::now().time_since_epoch().count());
}

TEST(Keyring, Test)
{
#ifndef __linux__
	GTEST_SKIP() << "Keyring not available";
#endif
	auto name = _random_name();

	ASSERT_FALSE(read(name));

	ASSERT_EQ(write(Process, name, "test-body"), 0);

	auto r = read(name);
	ASSERT_TRUE(r);
	ASSERT_EQ(r->str(), "test-body");
}

TEST(Keyring, KeyRef)
{
	const auto data = KeyRef { "data:body" };

	auto r = data.load();
	ASSERT_TRUE(r);
	ASSERT_EQ(r->str(), "body");

	tll::Config cfg;
	cfg.set("ref", "invalid");
	ASSERT_FALSE(cfg.getT("ref", KeyRef {}));
	cfg.set("ref", "data:body");
	ASSERT_TRUE(cfg.getT("ref", KeyRef {}));

#ifndef __linux__
	return;
#endif

	auto name = _random_name();
	const auto ref = KeyRef { "key:" + name };

	ASSERT_FALSE(ref.load());

	ASSERT_EQ(write(Process, name, "test-body"), 0);

	r = ref.load();
	ASSERT_TRUE(r);
	ASSERT_EQ(r->str(), "test-body");

	char * buf = nullptr;
	ASSERT_EQ(tll_keyring_read_ref(ref.uri.data(), -1, &buf), 9);
	ASSERT_STREQ(buf, "test-body");
	free(buf);
}
