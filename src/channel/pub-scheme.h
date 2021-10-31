#ifndef _TLL_IMPL_CHANNEL_PUB_SCHEME_H
#define _TLL_IMPL_CHANNEL_PUB_SCHEME_H

#include "tll/scheme/types.h"

namespace tll::pub {

static constexpr int version = 1;

struct client_hello
{
	static constexpr int id = 100;
	int16_t version;
	scheme::offset_ptr_t<char, tll_scheme_offset_ptr_legacy_short_t> name;
};

struct server_hello
{
	static constexpr int id = 101;
	int16_t version;
};

} // namespace tll::pub

#endif//_TLL_IMPL_CHANNEL_PUB_SCHEME_H
