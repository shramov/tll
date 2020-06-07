/*
 * Copyright (c) 2018-2020 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#ifndef _TLL_CHANNEL_IMPL_H
#define _TLL_CHANNEL_IMPL_H

#include "tll/channel.h"
#include "tll/scheme.h"

#ifdef __cplusplus
extern "C" {
#endif//__cplusplus

struct tll_channel_impl_t
{
	int (*init)(tll_channel_t *, const char * str, size_t len, tll_channel_t * parent, tll_channel_context_t * ctx);
	void (*free)(tll_channel_t *);
	int (*open)(tll_channel_t *, const char * str, size_t len);
	int (*close)(tll_channel_t *);

	int (*process)(tll_channel_t *, long timeout, int flags);
	int (*post)(tll_channel_t *, const tll_msg_t *msg, int flags);

	const tll_scheme_t * (*scheme)(const tll_channel_t *, int);

	/// Protocol name
	const char * name;

	/// Is this impl refers to prefix channel or not
	int prefix;

	/// User defined data for impl
	void * data;
};

typedef struct tll_channel_callback_pair_t
{
	tll_channel_callback_t cb;
	void * user;
	unsigned mask;
} tll_channel_callback_pair_t;

typedef struct tll_channel_internal_t
{
	tll_state_t state;
	tll_channel_t * self;

	const char * name;

	unsigned caps;
	unsigned dcaps;
	int fd;
	tll_config_t * config;
	tll_channel_list_t * children;

	unsigned data_cb_size;
	tll_channel_callback_pair_t * data_cb;

	unsigned cb_size;
	tll_channel_callback_pair_t * cb;
} tll_channel_internal_t;

struct tll_channel_module_t
{
	/// Version of module symbol
	int version;
	/// Null terminated list of implementations
	tll_channel_impl_t ** impl;
	//int (*init)(tll_channel_context_t *ctx);
	//int (*fini)(tll_channel_context_t *ctx);
};

void tll_channel_list_free(tll_channel_list_t *l);
int tll_channel_list_add(tll_channel_list_t **l, tll_channel_t *c);
int tll_channel_list_del(tll_channel_list_t **l, const tll_channel_t *c);
void tll_channel_internal_init(tll_channel_internal_t *ptr);
void tll_channel_internal_clear(tll_channel_internal_t *ptr);

static inline int tll_channel_callback_data(const tll_channel_internal_t * in, const tll_msg_t * msg)
{
	unsigned size = in->data_cb_size;
	//if (size == 1) {
	//	(*in->data_cb_fixed[0].cb)(c, msg, in->data_cb_fixed[0].user);
	//	return 0;
	//}
	tll_channel_callback_pair_t * cb = in->data_cb;
	for (unsigned i = 0; i < size; i++)
		(*cb[i].cb)(in->self, msg, cb[i].user);
	return 0;
}

static inline int tll_channel_callback(const tll_channel_internal_t * in, const tll_msg_t * msg)
{
	if (msg->type == TLL_MESSAGE_DATA)
		return tll_channel_callback_data(in, msg);
	{
		tll_channel_callback_pair_t * cb = in->cb;
		for (unsigned i = 0; i < in->cb_size; i++) {
			if (cb[i].mask & (1u << msg->type))
				(*cb[i].cb)(in->self, msg, cb[i].user);
		}
	}
	return 0;
}

#ifdef __cplusplus
} // extern "C"
#endif//__cplusplus

#ifdef __cplusplus
namespace tll {

template <typename T, typename VBase = T>
class channel_impl : public tll_channel_impl_t
{
	std::string _name;

public:
	channel_impl(std::string_view name = "")
	{
		init = _init;
		free = _free;
		open = _open;
		close = _close;
		process = _process;
		post = _post;
		scheme = _scheme;
		prefix = T::impl_prefix_channel();
		if (name.size())
			_name = name;
		else
			_name = T::impl_protocol();
		this->name = _name.c_str();
	}

	static VBase * _dataT(tll_channel_t * c) { return (T *) c->data; }
	static const VBase * _dataT(const tll_channel_t * c) { return (T *) c->data; }

	static int _init(tll_channel_t *c, const char * str, size_t len, tll_channel_t * parent, tll_channel_context_t *ctx)
	{
		auto ptr = new T();
		c->data = ptr;
		c->internal = &_dataT(c)->internal;
		c->internal->self = c;
		int r = _dataT(c)->init(std::string_view(str, len), static_cast<tll::Channel *>(parent), ctx);
		if (c->data != ptr) {
			delete ptr;
			return r;
		}
		if (r) {
			delete _dataT(c);
			c->data = 0;
		}
		return r;
	}

	static void _free(tll_channel_t * c)
	{
		_dataT(c)->free();
		delete static_cast<T *>(_dataT(c));
	}

	static int _open(tll_channel_t * c, const char *str, size_t len) { return _dataT(c)->open(std::string_view(str, len)); }
	static int _close(tll_channel_t * c) { return _dataT(c)->close(); }

	static int _process(tll_channel_t *c, long timeout, int flags) { return _dataT(c)->process(timeout, flags); }
	static int _post(tll_channel_t *c, const tll_msg_t *msg, int flags) { return _dataT(c)->post(msg, flags); }

	static const tll_scheme_t * _scheme(const tll_channel_t *c, int type) { return _dataT(c)->scheme(type); }
};

template <size_t Size>
struct channel_module_t : public tll_channel_module_t
{
	std::array<tll_channel_impl_t *, Size + 1> channels_array;
	constexpr channel_module_t() { impl = channels_array.data(); }
	constexpr channel_module_t(channel_module_t &&m) : channels_array(std::move(m.channels_array)) { impl = channels_array.data(); }
	constexpr channel_module_t(const channel_module_t &m) : channels_array(m.channels_array) { impl = channels_array.data(); }

	template <typename ... T>
	constexpr channel_module_t(T ... args) : channels_array({static_cast<tll_channel_impl_t *>(args)..., nullptr})
	{
		impl = channels_array.data();
	}
};

template <typename ... Args>
constexpr channel_module_t<sizeof...(Args)> make_channel_module()
{
	return channel_module_t<sizeof...(Args)>(&Args::impl...);
}
} // namespace tll
#endif

#endif//_TLL_CHANNEL_IMPL_H
