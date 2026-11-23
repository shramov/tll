#ifndef _SRC_STAT_INTERNAL_H
#define _SRC_STAT_INTERNAL_H

#include "tll/stat.h"

extern "C" {

typedef enum { TLL_STAT_BLOCK_V0, TLL_STAT_BLOCK_V1 } tll_stat_block_version_t;

int tll_stat_list_add_internal(tll_stat_list_t * list, tll_stat_block_t * b, tll_stat_block_version_t version);

} // extern "C"

#endif//_SRC_STAT_INTERNAL_H
