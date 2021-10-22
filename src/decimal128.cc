#include "tll/util/decimal128.h"

int tll_decimal128_pack(tll_decimal128_t * d, const tll_decimal128_unpacked_t * u)
{
	return static_cast<tll::util::Decimal128 *>(d)->pack(*u);
}

int tll_decimal128_unpack(tll_decimal128_unpacked_t * u, const tll_decimal128_t * d)
{
	static_cast<const tll::util::Decimal128 *>(d)->unpack(*u);
	return 0;
}
