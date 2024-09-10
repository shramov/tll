/*
 * Copyright (c) 2021 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#ifndef _TLL_CHANNEL_MODULE_H
#define _TLL_CHANNEL_MODULE_H

#include "tll/channel/impl.h"

#ifdef __cplusplus
extern "C" {
#endif//__cplusplus

/// Flags for tll_channel_module_t structure
typedef enum tll_channel_module_flags_t
{
	/// Load module with RTLD_GLOBAL. Is needed when module is linked with some libraries
	/// that are needed for symbol resolution of additional plugins like python modules
	TLL_CHANNEL_MODULE_DLOPEN_GLOBAL = 1,
} tll_channel_module_flags_t;

#define TLL_CHANNEL_MODULE_VERSION 2


typedef struct tll_channel_module_t
{
	/// Version of module symbol
	int version;
	/// Null terminated list of implementations
	tll_channel_impl_t ** impl;
	/// Init function, may be NULL. Non-zero return value indicates error and aborts module loading
	int (*init)(struct tll_channel_module_t * m, tll_channel_context_t *ctx, const tll_config_t *cfg);
	/// Free function, called on context destruction as many times as init function.
	int (*free)(struct tll_channel_module_t * m, tll_channel_context_t *ctx);
	/// Flags, @see tll_channel_module_flags_t
	unsigned flags;
} tll_channel_module_t;

/// First version of init function, without config parameter
typedef int (*tll_channel_module_init_v1_t)(struct tll_channel_module_t * m, tll_channel_context_t *ctx);

typedef tll_channel_module_t * (*tll_channel_module_func_t)();

#ifdef __cplusplus
} // extern "C"
#endif//__cplusplus

#ifdef __cplusplus

#include <array>

namespace tll {

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
		version = TLL_CHANNEL_MODULE_VERSION;
	}
};

template <typename ... Args>
constexpr channel_module_t<sizeof...(Args)> make_channel_module()
{
	return channel_module_t<sizeof...(Args)>(&Args::impl...);
}

#define TLL_DEFINE_MODULE(...) \
extern "C" tll_channel_module_t * tll_channel_module() \
{ \
	static auto mod = tll::make_channel_module<__VA_ARGS__>(); \
	return &mod; \
}

} // namespace tll

#endif//__cplusplus

#endif//_TLL_CHANNEL_MODULE_H
