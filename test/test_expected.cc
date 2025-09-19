#include "gtest/gtest.h"

#include "tll/compat/expected.h"

using namespace tll::compat;

enum class Error { Error };
expected<int, Error> fbool(bool ok)
{
	if (!ok)
		return unexpected(Error::Error);
	return 10;
}

expected<void, Error> fvoid(bool ok)
{
	if (!ok)
		return unexpected(Error::Error);
	return {};
}

TEST(Util, Expected)
{
	auto r = fbool(true);
	ASSERT_TRUE(r);
	ASSERT_EQ(*r, 10);

	r = fbool(false);
	ASSERT_FALSE(r);
	ASSERT_EQ(r.error(), Error::Error);

	auto rv = fvoid(true);
	ASSERT_TRUE(rv);

	rv = fvoid(false);
	ASSERT_FALSE(rv);
	ASSERT_EQ(rv.error(), Error::Error);
}
