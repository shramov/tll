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
	auto ri = fbool(true);
	ASSERT_TRUE(ri);
	ASSERT_EQ(*ri, 10);

	ri = fbool(false);
	ASSERT_FALSE(ri);
	ASSERT_EQ(ri.error(), Error::Error);

	auto rv = fvoid(true);
	ASSERT_TRUE(rv);

	rv = fvoid(false);
	ASSERT_FALSE(rv);
	ASSERT_EQ(rv.error(), Error::Error);
}
