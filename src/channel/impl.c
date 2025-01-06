/*
 * Copyright (c) 2018-2021 Pavel Shramov <shramov@mexmat.net>
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

#if defined(__linux__) || defined(__FreeBSD__)
# if defined(__GNUC__) && !defined(__clang__) && __GNUC__ >= 10
#  define SYMVER(func, alias) __attribute__((symver(alias)))
# else
#  define SYMVER(func, alias) __asm__(".symver " #func "," alias);
# endif
#else
# define SYMVER(func, alias)
#endif

SYMVER(tll_channel_internal_init_v0, "tll_channel_internal_init@TLL_0.0.0")
void tll_channel_internal_init_v0(tll_channel_internal_t *ptr)
{
	memset(ptr, 0, offsetof(tll_channel_internal_t, logger));
	ptr->fd = -1;
}

SYMVER(tll_channel_internal_init_v1, "tll_channel_internal_init@@TLL_0.2.0")
void tll_channel_internal_init_v1(tll_channel_internal_t *ptr)
{
	memset(ptr, 0, offsetof(tll_channel_internal_t, reserved) + sizeof(ptr->reserved));
	ptr->version = TLL_CHANNEL_INTERNAL_V1;
	ptr->fd = -1;
}

#if !(defined(__linux__) || defined(__FreeBSD__))
void tll_channel_internal_init(tll_channel_internal_t *ptr) { tll_channel_internal_init_v1(ptr); }
#endif

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

	if (ptr->version >= 1)
		tll_logger_free(ptr->logger);
	ptr->logger = NULL;
}

static int _state_callback(const tll_channel_t * c, const tll_msg_t *msg, void * data)
{
	tll_channel_internal_t * ptr = (tll_channel_internal_t *) data;
	if (!ptr)
		return EINVAL;
	if (msg->type != TLL_MESSAGE_STATE || msg->msgid != TLL_STATE_DESTROY)
		return 0;
	return tll_channel_internal_child_del(ptr, c, NULL, 0);
}

int tll_channel_internal_child_add(tll_channel_internal_t *ptr, tll_channel_t *c, const char * tag, int len)
{
	//tll_logger_printf(ptr->logger, TLL_LOGGER_INFO, "Add child %s", tll_channel_name(c));
	int r = tll_channel_list_add(&ptr->children, c);
	if (r) {
		//tll_logger_printf(internal->logger, TLL_LOGGER_ERROR, "Failed to add child '%s': %s", tll_channel_name(c), strerror(r));
		return r;
	}

	if ((ptr->caps & TLL_CAPS_PARENT) == 0)
		tll_logger_printf(ptr->logger, TLL_LOGGER_WARNING, "Adding child '%s', but Parent cap is not set", tll_channel_name(c));

	tll_msg_t msg = {.type = TLL_MESSAGE_CHANNEL, .msgid = TLL_MESSAGE_CHANNEL_ADD};
	msg.data = &c;
	msg.size = sizeof(c);
	tll_channel_callback_add(c, _state_callback, ptr, TLL_MESSAGE_MASK_STATE);
	tll_channel_callback(ptr, &msg);
	if (tag && (len > 0 || strlen(tag) > 0))
		tll_config_set_config(ptr->config, tag, len, tll_channel_config(c), 1);
	return 0;
}

int tll_channel_internal_child_del(tll_channel_internal_t *ptr, const tll_channel_t *c, const char * tag, int len)
{
	//tll_logger_printf(log, TLL_LOGGER_INFO, "Remove child %s", tll_channel_name(c));
	int r = tll_channel_list_del(&ptr->children, c);
	if (r) {
		//tll_logger_printf(internal->logger, TLL_LOGGER_ERROR, "Failed to remove child '%s': %s", tll_channel_name(c), strerror(r));
		return r;
	}
	tll_msg_t msg = {.type = TLL_MESSAGE_CHANNEL, .msgid = TLL_MESSAGE_CHANNEL_DELETE};
	msg.data = &c;
	msg.size = sizeof(c);
	tll_channel_callback(ptr, &msg);
	if (tag && (len > 0 || strlen(tag) > 0))
		tll_config_remove(ptr->config, tag, len);
	tll_channel_callback_del((tll_channel_t *) c, _state_callback, ptr, TLL_MESSAGE_MASK_STATE);
	return 0;
}

static inline const char * tll_state_str(tll_state_t s)
{
	switch (s) {
	case TLL_STATE_CLOSED: return "Closed";
	case TLL_STATE_OPENING: return "Opening";
	case TLL_STATE_ACTIVE: return "Active";
	case TLL_STATE_ERROR: return "Error";
	case TLL_STATE_CLOSING: return "Closing";
	case TLL_STATE_DESTROY: return "Destroy";
	}
	return "Unknown";
}

int tll_channel_internal_set_state(tll_channel_internal_t * in, tll_state_t state)
{
	tll_state_t old = in->state;
	if (state == old)
		return 0;
	in->state_count++;
	tll_logger_printf(in->logger, TLL_LOGGER_INFO, "State change: %s -> %s", tll_state_str(old), tll_state_str(state));
	in->state = state;
	tll_config_set(in->config, "state", -1, tll_state_str(state), -1);
	tll_msg_t msg = {.type = TLL_MESSAGE_STATE, .msgid = state};
	tll_channel_callback(in, &msg);
	return 0;
}
