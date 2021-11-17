/*
 * Copyright (c) 2018-2021 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#ifndef _TLL_CHANNEL_IMPL_H
#define _TLL_CHANNEL_IMPL_H

#include "tll/channel.h"
#include "tll/logger.h"
#include "tll/scheme.h"
#include "tll/stat.h"

#ifdef __cplusplus
extern "C" {
#endif//__cplusplus

struct tll_channel_impl_t
{
	int (*init)(tll_channel_t *, const tll_config_t * url, tll_channel_t * parent, tll_channel_context_t * ctx);
	void (*free)(tll_channel_t *);
	int (*open)(tll_channel_t *, const char * str, size_t len);
	int (*close)(tll_channel_t *, int);

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

#ifdef __cplusplus
#define TLL_DECLARE_STAT(...) tll::stat::FieldT<__VA_ARGS__>
#else
#define TLL_DECLARE_STAT(...) tll_stat_field_t
#endif

typedef struct tll_channel_stat_t
{
	TLL_DECLARE_STAT(tll_stat_int_t, tll::stat::Sum, tll::stat::Unknown, 'r', 'x') rx;
	TLL_DECLARE_STAT(tll_stat_int_t, tll::stat::Sum, tll::stat::Bytes, 'r', 'x') rxb;

	TLL_DECLARE_STAT(tll_stat_int_t, tll::stat::Sum, tll::stat::Unknown, 't', 'x') tx;
	TLL_DECLARE_STAT(tll_stat_int_t, tll::stat::Sum, tll::stat::Bytes, 't', 'x') txb;
} tll_channel_stat_t;

#undef TLL_DECLARE_STAT

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

	tll_stat_block_t * stat;
} tll_channel_internal_t;

/// Flags for tll_channel_module_t structure
typedef enum tll_channel_module_flags_t
{
	/// Load module with RTLD_GLOBAL. Is needed when module is linked with some libraries
	/// that are needed for symbol resolution of additional plugins like python modules
	TLL_CHANNEL_MODULE_DLOPEN_GLOBAL = 1,
} tll_channel_module_flags_t;

typedef struct tll_channel_module_t
{
	/// Version of module symbol
	int version;
	/// Null terminated list of implementations
	tll_channel_impl_t ** impl;
	/// Init function, may be NULL. Non-zero return value indicates error and aborts module loading
	int (*init)(struct tll_channel_module_t * m, tll_channel_context_t *ctx);
	/// Free function, called on context destruction as many times as init function.
	int (*free)(struct tll_channel_module_t * m, tll_channel_context_t *ctx);
	/// Flags, @see tll_channel_module_flags_t
	unsigned flags;
} tll_channel_module_t;

void tll_channel_list_free(tll_channel_list_t *l);
int tll_channel_list_add(tll_channel_list_t **l, tll_channel_t *c);
int tll_channel_list_del(tll_channel_list_t **l, const tll_channel_t *c);

void tll_channel_internal_init(tll_channel_internal_t *ptr);
void tll_channel_internal_clear(tll_channel_internal_t *ptr);
int tll_channel_internal_child_add(tll_channel_internal_t *ptr, tll_channel_t *c, const char * tag, int len);
int tll_channel_internal_child_del(tll_channel_internal_t *ptr, const tll_channel_t *c, const char * tag, int len);

static inline int tll_channel_callback_data(const tll_channel_internal_t * in, const tll_msg_t * msg)
{
	if (in->stat) {
		tll_stat_page_t * p = tll_stat_page_acquire(in->stat);
		if (p) {
			tll_channel_stat_t *f = (tll_channel_stat_t *) p->fields;
#ifdef __cplusplus
			f->rx.update(1);
			f->rxb.update(msg->size);
#else
			f->rx.value += 1;
			f->rxb.value += msg->size;
#endif
			tll_stat_page_release(in->stat, p);
		}
	}

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

/// Message flags for @ref tll_channel_log_msg function
typedef enum {
	TLL_MESSAGE_LOG_DISABLE = 0,	///< Disable logging
	TLL_MESSAGE_LOG_FRAME = 1,	///< Log only frame data (msgid, seq, size, ...)
	TLL_MESSAGE_LOG_TEXT = 2,	///< Log body as ASCII text (replacing unprintable symbols)
	TLL_MESSAGE_LOG_TEXT_HEX = 3,	///< Log body as ASCII text and hex representation
	TLL_MESSAGE_LOG_SCHEME = 4,	///< Log decomposed body as fields from scheme
} tll_channel_log_msg_format_t;

/** Format message and write it to logger
 *
 * @param c channel object
 * @param log name logger object that is used to write result
 * @param level logging level
 * @param format desired format
 * @param msg message object
 * @param text additional text
 * @param tlen length of additional text or -1 if it has trailing \0
 */
int tll_channel_log_msg(const tll_channel_t * c, const char * log, tll_logger_level_t level, tll_channel_log_msg_format_t format, const tll_msg_t * msg, const char * text, int tlen);

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
		if (name.size())
			_name = name;
		else
			_name = T::channel_protocol();
		this->name = _name.c_str();
	}

	static VBase * _dataT(tll_channel_t * c) { return (T *) c->data; }
	static const VBase * _dataT(const tll_channel_t * c) { return (T *) c->data; }

	static int _init(tll_channel_t *c, const tll_config_t *curl, tll_channel_t * parent, tll_channel_context_t *ctx)
	{
		auto ptr = new T();
		c->data = ptr;
		c->internal = &_dataT(c)->internal;
		c->internal->self = c;
		const Channel::Url url(const_cast<tll_config_t *>(curl));
		int r = _dataT(c)->init(url, static_cast<tll::Channel *>(parent), ctx);
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
	static int _close(tll_channel_t * c, int force) { return _dataT(c)->close(force); }

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

#define TLL_DEFINE_MODULE(...) \
auto channel_module = tll::make_channel_module<__VA_ARGS__>()

namespace channel::log_msg_format {

static constexpr auto Disable = TLL_MESSAGE_LOG_DISABLE;
static constexpr auto Frame = TLL_MESSAGE_LOG_FRAME;
static constexpr auto Text = TLL_MESSAGE_LOG_TEXT;
static constexpr auto TextHex = TLL_MESSAGE_LOG_TEXT_HEX;
static constexpr auto Scheme = TLL_MESSAGE_LOG_SCHEME;

} // namespace channel::log_msg_format

} // namespace tll
#endif

#endif//_TLL_CHANNEL_IMPL_H
