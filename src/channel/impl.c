/*
 * Copyright (c) 2018-2020 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#include "tll/channel/impl.h"

#include <errno.h>
#include <string.h>

void tll_channel_list_free(tll_channel_list_t *l)
{
	if (!l) return;
	tll_channel_list_free(l->next);
	free(l);
}

int tll_channel_list_add(tll_channel_list_t **l, tll_channel_t *c)
{
	tll_channel_list_t ** i = l;
	if (!l) return EINVAL;
	for (; *i; i = &(*i)->next) {
		if ((*i)->channel == c)
			return EEXIST;
	}
	*i = (tll_channel_list_t *) malloc(sizeof(tll_channel_list_t));
	memset(*i, 0, sizeof(tll_channel_list_t));
	(*i)->channel = c;
	return 0;
}

int tll_channel_list_del(tll_channel_list_t **l, const tll_channel_t *c)
{
	tll_channel_list_t ** i = l;
	if (!l) return EINVAL;
	for (; *i; i = &(*i)->next) {
		if ((*i)->channel == c) {
			tll_channel_list_t * tmp = *i;
			*i = (*i)->next;
			free(tmp);
			return 0;
		}
	}
	return ENOENT;
}

void tll_channel_internal_init(tll_channel_internal_t *ptr)
{
	memset(ptr, 0, sizeof(tll_channel_internal_t));
	ptr->fd = -1;
}

void tll_channel_internal_clear(tll_channel_internal_t *ptr)
{
	tll_channel_list_free(ptr->children);
	ptr->children = NULL;

	free(ptr->cb);
	ptr->cb = NULL;
	ptr->cb_size = 0;

	free(ptr->data_cb);
	ptr->data_cb = NULL;
	ptr->data_cb_size = 0;
}
