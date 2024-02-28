/*
 * Copyright (c) 2018-2021 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

/** @file
 * Communication abstraction subsystem.
 */

#ifndef _TLL_CHANNEL_H
#define _TLL_CHANNEL_H

#include <stdlib.h>
#include <stdint.h>

#include "tll/config.h"

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

/**
 * Channel states
 */
typedef enum {
	TLL_STATE_CLOSED = 0,  ///< Closed, changes to Opening, Destroy
	TLL_STATE_OPENING = 1, ///< Opening, changes to Active, Error
	TLL_STATE_ACTIVE = 2,  ///< Active, changes to Sleep, Closing, Error
	TLL_STATE_CLOSING = 3, ///< Closing, changes to Closed, Error(?)
	TLL_STATE_ERROR = 4,   ///< Error, changes to Closed
	TLL_STATE_DESTROY = 5, ///< Terminal state before object is destroyed
} tll_state_t;

/// Message type
typedef enum {
	/// Data, normal message with data payload.
	TLL_MESSAGE_DATA = 0,
	/// Control messages, like cache flushing or file seek, if supported by channel.
	TLL_MESSAGE_CONTROL = 1,
	/**
	 * State update message.
	 *
	 * On state change message is emitted with ``msgid`` field equal to new state (@ref tll_state_t).
	 */
	TLL_MESSAGE_STATE = 2,
	/// Channel internal state updates, like dynamic caps or child list changes.
	TLL_MESSAGE_CHANNEL = 3,
} tll_msg_type_t;

/// Message ids for @ref TLL_MESSAGE_CHANNEL messages.
typedef enum {
	TLL_MESSAGE_CHANNEL_UPDATE = 0,	///< Update dcaps, data == ``NULL``
	TLL_MESSAGE_CHANNEL_ADD = 1,	///< Add new sub-channel
	TLL_MESSAGE_CHANNEL_DELETE = 2,	///< Delete sub-channel
	TLL_MESSAGE_CHANNEL_UPDATE_FD = 3, ///< Update fd, data is old fd
} tll_msg_channel_t;

typedef union {
	uint64_t u64;
	int64_t  i64;
	void *   ptr;
} tll_addr_t;

/// Message object
typedef struct {
	short type;		///< tll_msg_type_t
	int msgid;		///< Message id
	long long seq;		///< Sequence number
	short flags;		///< User defined message flags
	const void * data; 	///< Data pointer
	size_t size;		///< Data size
	tll_addr_t addr;	///< Peer address
	long long time;	        ///< Timestamp in nanoseconds
} tll_msg_t;

/// Copy meta info from one message to another
static inline void tll_msg_copy_info(tll_msg_t * dest, const tll_msg_t * src)
{
	dest->type = src->type;
	dest->msgid = src->msgid;
	dest->seq = src->seq;
	dest->addr = src->addr;
	dest->time = src->time;
}

typedef struct tll_channel_context_t tll_channel_context_t;
typedef struct tll_channel_impl_t tll_channel_impl_t;

/// Channel static capabilities, fixed on initialization.
typedef enum {
	TLL_CAPS_PARENT   = 0x1, ///< Channel can create child objects
	TLL_CAPS_PROXY    = 0x2, ///< Proxy channel with exactly one child object

	TLL_CAPS_INPUT    = 0x4, ///< Channel is providing data
	TLL_CAPS_OUTPUT   = 0x8, ///< Channel is consuming data (messages can be posted)
	TLL_CAPS_INOUT    = TLL_CAPS_INPUT | TLL_CAPS_OUTPUT,

	TLL_CAPS_CUSTOM   = 0x10, ///< Runtime created subchannel, not registered in context
	TLL_CAPS_LONG_CLOSE = 0x20, ///< Channel close routine can be long
} tll_channel_cap_t;

/// Channel dynamic capabilities, may change.
typedef enum {
	TLL_DCAPS_ZERO     = 0x0, ///< Zero value
	TLL_DCAPS_POLLIN   = 0x1, ///< Channel fd needs poll for incoming data
	TLL_DCAPS_POLLOUT  = 0x2, ///< Channel fd needs poll for outgoing data
	TLL_DCAPS_POLLMASK = 0x3, ///< Mask for POLLIN/POLLOUT bits

	TLL_DCAPS_PROCESS  = 0x10, ///< Call process for this object, don't call if cap is not set.
	TLL_DCAPS_PENDING  = 0x20, ///< Pending data, process without polling
	TLL_DCAPS_SUSPEND  = 0x40, ///< Channel is suspended
	TLL_DCAPS_SUSPEND_PERMANENT = 0x80, ///< Channel is suspended explicitly.

	/**
	 * Mask to check if channel needs process or not
	 *
	 * Channel process is called only when PROCESS dcap is set and SUSPEND is not set.
	 * Such check can be done in one comparison:
	 * @code{.c}
	 * dcaps & TLL_DCAPS_PROCESS_MASK == TLL_DCAPS_PROCESS
	 * @endcode
	 */
	TLL_DCAPS_PROCESS_MASK = TLL_DCAPS_PROCESS | TLL_DCAPS_SUSPEND,
} tll_channel_dcap_t;

/**
 * Helper function to check if channel need process or not.
 */
static inline int tll_channel_dcap_need_process(unsigned dcap)
{
	return (dcap & TLL_DCAPS_PROCESS_MASK) == TLL_DCAPS_PROCESS;
}

typedef struct tll_channel_internal_t tll_channel_internal_t;

typedef struct tll_channel_t tll_channel_t;

typedef struct tll_channel_list_t
{
	tll_channel_t * channel;
	struct tll_channel_list_t * next;
} tll_channel_list_t;

/**
 * Structure representing channel object.
 * @note Consider it as forward declaration and never create this structure by hand.
 * It is not forward declaration for only purpose to allow casting to C++ @ref tll::Channel object.
 */
typedef struct tll_channel_t {
	const tll_channel_impl_t * impl;
	void * data;
	tll_channel_internal_t * internal;
	tll_channel_context_t * context;
	struct tll_channel_t * parent;
} tll_channel_t;

/// Mask values to select different message types
typedef enum {
	TLL_MESSAGE_MASK_DATA    = 1u << TLL_MESSAGE_DATA, ///< Data messages
	TLL_MESSAGE_MASK_CONTROL = 1u << TLL_MESSAGE_CONTROL, ///< Control messages
	TLL_MESSAGE_MASK_STATE   = 1u << TLL_MESSAGE_STATE, ///< State messsages
	TLL_MESSAGE_MASK_CHANNEL = 1u << TLL_MESSAGE_CHANNEL, ///< Child channel updates
	TLL_MESSAGE_MASK_ALL     = 0xffffffffu ^ TLL_MESSAGE_MASK_CHANNEL, ///< Mask for all messages excluding Channel updates
} tll_message_mask_t;

/**
 * Type of message callback function.
 *
 * @param channel Pointer to the message source channel
 * @param msg Message object
 * @param user User supplied data
 */

typedef int (*tll_channel_callback_t)(const tll_channel_t *channel, const tll_msg_t * msg, void * user);

/**
 * Add new callback to channel or update existing with new mask
 *
 * If ``(cb, user)`` pair already exists in channel then mask is updated with new bits. To stop receiving
 * message of some type see @ref tll_channel_callback_del call.
 *
 * @param cb Callback function.
 * @param user User data passed to callback.
 * @param mask Bitwise OR of message types (@ref tll_message_mask_t) that callback will be called on.
 *
 * @return 0 if callback was added, non-zero value on error.
 */
int tll_channel_callback_add(tll_channel_t *, tll_channel_callback_t cb, void * user, unsigned mask);

/**
 * Remove callback from channel
 *
 * @param cb Callback function. Pointer must be same as was passed to @ref tll_channel_callback_add call.
 * @param user User data. Pointer must be same as was passed to @ref tll_channel_callback_add call.
 * @param mask Bitwise OR of message types (@ref tll_message_mask_t) that callback will not be called on. ``mask`` bits are cleared
 *             from current mask and if is equal to 0 then callback is removed.
 *
 * @return 0 if ``(cb, user)`` pair was found, ``ENOENT`` otherwise.
 */
int tll_channel_callback_del(tll_channel_t *, tll_channel_callback_t cb, void * user, unsigned mask);

/**
 * Create new channel
 *
 * @param ctx Channel context. ``NULL`` may be used as an alias for default context, but it's not recommended and suitable
 *            only for simple script-like programs.
 * @param str Channel initialization url in format ``proto://host;param-a=value=a;param-b=value-b;...``.
 *            May be without trailing zero byte.
 * @param len Length of ``str``. For null-terminated strings can be set to -1
 * @param master Pointer to master object. May be ``NULL``.
 * @param impl Pointer to channel implementation, May be ``NULL`` otherwise protocol lookup is disabled and provided impl is used.
 *
 * @return New channel object or ``NULL`` if error occured.
 */
tll_channel_t * tll_channel_new(tll_channel_context_t * ctx, const char *str, size_t len, tll_channel_t *master, const tll_channel_impl_t *impl);

/**
 * Create new channel from parameters in config object.
 *
 * Same as @ref tll_channel_new but all parameters are in ``config`` object.
 *
 * @param ctx @ref tll_channel_new
 * @param url
 * @param master @ref tll_channel_new
 * @param impl @ref tll_channel_new
 */
tll_channel_t * tll_channel_new_url(tll_channel_context_t * ctx, const tll_config_t *url, tll_channel_t *master, const tll_channel_impl_t *impl);

/// Destroy channel
void tll_channel_free(tll_channel_t *);

/**
 * Open channel.
 * Same as @ref tll_channel_open_cfg but all parameters are in string format
 *
 * @param str Parameter string in format ``param-a=value=a;param-b=value-b;...``.
 *            May be without trailing zero byte.
 * @param len Length of ``str``. For null-terminated strings can be set to -1
 * @return 0 on success, non-zero value on error.
 */
int tll_channel_open(tll_channel_t *, const char * str, size_t len);

/**
 * Open channel.
 * Starts transition from ``Closed`` state to ``Opening``.
 *
 * @param cfg Open parameters. May be NULL
 * @return 0 on success, non-zero value on error.
 */
int tll_channel_open_cfg(tll_channel_t *, const tll_config_t * cfg);

/// Close channel
int tll_channel_close(tll_channel_t *, int force);

int tll_channel_process(tll_channel_t *c, long timeout, int flags);

/// Flags for tll_channel_post call
typedef enum {
	TLL_POST_MORE = 1, ///< More messages will be sent, equivalent of MSG_MORE flag for send(2)
} tll_channel_post_flag_t;

int tll_channel_post(tll_channel_t *c, const tll_msg_t *msg, int flags);

/// Suspend channel and all children
int tll_channel_suspend(tll_channel_t *c);
/// Resume channel and all children
int tll_channel_resume(tll_channel_t *c);

/// Get state
tll_state_t tll_channel_state(const tll_channel_t *c);
/// Get name
const char * tll_channel_name(const tll_channel_t *c);
/// Get capabilities
unsigned tll_channel_caps(const tll_channel_t *c);
/// Get dynamic capabilities
unsigned tll_channel_dcaps(const tll_channel_t *c);
/// Get associate file descriptor
int tll_channel_fd(const tll_channel_t *c);

/**
 * Get context of the channel.
 *
 * @return New reference to channel context. Caller must free it with @ref tll_channel_context_free.
 */
tll_channel_context_t * tll_channel_context(const tll_channel_t *c);

/**
 * Get config representing state of the channel.
 *
 * @return New reference to config object. Caller must free it with @ref tll_config_unref.
 */
tll_config_t * tll_channel_config(tll_channel_t *c);

/// Get list of channel child objects
const tll_channel_list_t * tll_channel_children(const tll_channel_t *c);

struct tll_scheme_t;
/**
 * Get channel's scheme object
 *
 * @param type Request scheme for specific message type, see @ref tll_msg_type_t for possible values.
 *
 * @return Pointer to scheme that is owned by channel. Caller don't need to free it.
 */
const struct tll_scheme_t * tll_channel_scheme(const tll_channel_t *c, int type);

tll_channel_t * tll_channel_get(const tll_channel_context_t *ctx, const char *name, int len);

tll_channel_context_t * tll_channel_context_new(tll_config_t * defaults);
tll_channel_context_t * tll_channel_context_ref(tll_channel_context_t *);

/**
 * Get default channel context.
 *
 * After initialization of default context it lives until program termination. There is no way to explictly destroy it.
 *
 * @return Borrowed reference to default context. Not needed to call @ref tll_channel_context_free on it.
 */
tll_channel_context_t * tll_channel_context_default();

/// Decrease reference count to context object and destroy it when it's not used anymore.
void tll_channel_context_free(tll_channel_context_t *);

/**
 * Get config representing state of all channels in current context.
 *
 * @return New reference to config object. Caller must free it with @ref tll_config_unref.
 */
tll_config_t * tll_channel_context_config(tll_channel_context_t *);

/**
 * Get config representing state of all channels in current context.
 *
 * @return New reference to config object. Caller must free it with @ref tll_config_unref.
 */
tll_config_t * tll_channel_context_config_defaults(tll_channel_context_t *);

struct tll_stat_list_t;

/**
 * Get list of all registered stat pages in this context
 */
struct tll_stat_list_t * tll_channel_context_stat_list(tll_channel_context_t *);

/**
 * Scheme load function with optional caching.
 *
 * Extends @ref tll_scheme_load with caching and ``channel://{name}`` protocol.
 * If cache is enabled then scheme is loaded only once and then reused. With disabled cache it's just
 * wrapper around @ref tll_scheme_load.
 *
 * If called with ``channel://{name}`` url then function tries to find channel with given name and get
 * scheme for ``TLL_MESSAGE_DATA`` from it (@ref tll_channel_scheme). If there is no channel or no scheme ``NULL`` is returned.
 *
 * @param ctx Pointer to channel context
 * @param url Scheme url string. May be without terminating null byte if ``len`` is specified.
 * @param len Length of url string. For null terminated strings can be set to ``-1``.
 * @param cache Enable (non zero value) or disable (0) caching.
 *
 * @return New reference to scheme object. Caller must free it with @ref tll_scheme_unref.
 */
const struct tll_scheme_t * tll_channel_context_scheme_load(tll_channel_context_t *ctx, const char *url, int len, int cache);

int tll_channel_impl_register(tll_channel_context_t *ctx, const tll_channel_impl_t *impl, const char *name);
int tll_channel_impl_unregister(tll_channel_context_t *ctx, const tll_channel_impl_t *impl, const char *name);
const tll_channel_impl_t * tll_channel_impl_get(const tll_channel_context_t *ctx, const char *name);

int tll_channel_alias_register_url(tll_channel_context_t *ctx, const char *name, const tll_config_t *cfg);
int tll_channel_alias_unregister_url(tll_channel_context_t *ctx, const char *name, const tll_config_t *cfg);

int tll_channel_alias_register(tll_channel_context_t *ctx, const char *name, const char *url, int len);
int tll_channel_alias_unregister(tll_channel_context_t *ctx, const char *name, const char *url, int len);

/**
 * Load channels from external module into context.
 *
 * @param ctx Pointer to channel context to which module will be loaded
 * @param module name of module without 'lib' prefix and '.so' suffix (or other platform specific parts).
 * @param symbol name of entry point, if string is empty or null default value is used
 * @param config additional configuration parameters passed to module init function
 *
 * Module is in format [path/]name. Name is converted into platform specific library name - for linux it is lib{name}.so.
 * If path is not given then global library search path is used.
 */
int tll_channel_module_load_cfg(tll_channel_context_t *ctx, const char *module, const char * symbol, const tll_config_t * cfg);
/// Version of tll_channel_module_load_cfg without config parameter
int tll_channel_module_load(tll_channel_context_t *ctx, const char *module, const char * symbol);
int tll_channel_module_unload(tll_channel_context_t *ctx, const char *module);

#ifdef __cplusplus
} // extern "C"
#endif // __cplusplus

#ifdef __cplusplus

#include <string_view>
#include <memory>

namespace tll {

namespace channel { class Context; }

namespace state {
	constexpr auto Closed = TLL_STATE_CLOSED;
	constexpr auto Opening = TLL_STATE_OPENING;
	constexpr auto Active = TLL_STATE_ACTIVE;
	constexpr auto Error = TLL_STATE_ERROR;
	constexpr auto Closing = TLL_STATE_CLOSING;
	constexpr auto Destroy = TLL_STATE_DESTROY;
}

namespace caps {
	constexpr auto Input = TLL_CAPS_INPUT;
	constexpr auto Output = TLL_CAPS_OUTPUT;
	constexpr auto InOut = TLL_CAPS_INOUT;

	constexpr auto Custom = TLL_CAPS_CUSTOM;
	constexpr auto Parent = TLL_CAPS_PARENT;
	constexpr auto Proxy = TLL_CAPS_PROXY;
	constexpr auto LongClose = TLL_CAPS_LONG_CLOSE;
}

namespace dcaps {
	constexpr auto Zero = TLL_DCAPS_ZERO;
	constexpr auto CPOLLIN = TLL_DCAPS_POLLIN;
	constexpr auto CPOLLOUT = TLL_DCAPS_POLLOUT;
	constexpr auto CPOLLMASK = TLL_DCAPS_POLLMASK;

	constexpr auto Process = TLL_DCAPS_PROCESS;
	constexpr auto Pending = TLL_DCAPS_PENDING;
	constexpr auto Suspend = TLL_DCAPS_SUSPEND;
	constexpr auto SuspendPermanent = TLL_DCAPS_SUSPEND_PERMANENT;
	constexpr auto ProcessMask = TLL_DCAPS_PROCESS_MASK;

	/// Helper function to check, if channel need process or not
	constexpr bool need_process(unsigned dcaps) { return (dcaps & ProcessMask) == Process; }
}

template <typename T>
struct CallbackT
{
	template <typename ... Args> static int call(T *ptr, Args ... args) { return ptr->callback(args...); }
};

/// C++ wrapper around @ref tll_channel_t object.
class Channel : public tll_channel_t
{
	Channel() = delete;

public:
	using Scheme = tll_scheme_t;
	using Url = ConfigUrl;

	/// Create new channel. See @ref tll_channel_new
	static std::unique_ptr<Channel> init(std::string_view params, Channel * master = nullptr);

	static void operator delete (void *ptr) { tll_channel_free((Channel *) ptr); }

	/// Open channel. See @ref tll_channel_open
	int open(std::string_view params) { return tll_channel_open(this, params.data(), params.size()); }
	/// Open channel. See @ref tll_channel_open_cfg
	int open(const Config &cfg) { return tll_channel_open_cfg(this, cfg); }
	int open(const ConstConfig &cfg) { return tll_channel_open_cfg(this, cfg); }
	int open() { return tll_channel_open_cfg(this, nullptr); }
	/// Close channel. See @ref tll_channel_close
	int close(bool force = false) { return tll_channel_close(this, force); }

	/// Get channel name. See @ref tll_channel_name
	const char * name() const { return tll_channel_name(this); }
	/// Get channel state. See @ref tll_channel_state
	tll_state_t state() const { return tll_channel_state(this); }
	/// Get channel caps. See @ref tll_channel_caps
	unsigned long long caps() const { return tll_channel_caps(this); }
	/// Get channel dynamic caps. See @ref tll_channel_dcaps
	unsigned long long dcaps() const { return tll_channel_dcaps(this); }
	/// Get channel file descriptor. See @ref tll_channel_fd
	int fd() const { return tll_channel_fd(this); }
	/// Get channel context. See @ref tll_channel_context
	channel::Context context() const;

	Config config() { return Config::consume(tll_channel_config(this)); }
	const Config config() const { return Config::consume(tll_channel_config((tll_channel_t *) this)); }

	const tll_channel_list_t * children() const { return tll_channel_children(this); }

	/// Get channel scheme. See @ref tll_channel_scheme
	const Scheme * scheme(int type = TLL_MESSAGE_DATA) const { return tll_channel_scheme(this, type); }

	/// Process channel. See @ref tll_channel_process
	int process(long timeout = 0, int flags = 0) { return tll_channel_process(this, timeout, flags); }
	/// Post message to channel. See @ref tll_channel_post
	int post(const tll_msg_t *msg, int flags = 0) { return tll_channel_post(this, msg, flags); }

	/// Suspend channel. See @ref tll_channel_suspend
	int suspend() { return tll_channel_suspend(this); }
	/// Resume channel. See @ref tll_channel_resume
	int resume() { return tll_channel_resume(this); }

	/// Add new callback. See @ref tll_channel_callback_add
	int callback_add(tll_channel_callback_t cb, void * user, unsigned mask = TLL_MESSAGE_MASK_ALL)
	{
		return tll_channel_callback_add(this, cb, user, mask);
	}

	/// Remove callback. See @ref tll_channel_callback_del
	int callback_del(tll_channel_callback_t cb, void * user, unsigned mask = TLL_MESSAGE_MASK_ALL)
	{
		return tll_channel_callback_del(this, cb, user, mask);
	}

	template <typename T>
	struct proxy {
		static int call(const tll_channel_t * c, const tll_msg_t * m, void * user)
		{
			return CallbackT<T>::call(static_cast<T *>(user), static_cast<const tll::Channel *>(c), m);
		}
	};

	template <typename T>
	int callback_add(T *obj, unsigned mask = TLL_MESSAGE_MASK_ALL)
	{
		return tll_channel_callback_add(this, proxy<T>::call, obj, mask);
	}

	template <typename T, int (*F)(T *, const tll::Channel *, const tll_msg_t * msg)>
	int callback_add(T *obj, unsigned mask = TLL_MESSAGE_MASK_ALL)
	{
		return tll_channel_callback_add(this, _proxy_func<T, F>, obj, mask);
	}

	template <typename T, int (T::*F)(const tll::Channel *, const tll_msg_t * msg)>
	int callback_add(T *obj, unsigned mask = TLL_MESSAGE_MASK_ALL)
	{
		return tll_channel_callback_add(this, _proxy_member<T, F>, obj, mask);
	}

	template <typename T, int (T::*F)(const tll::Channel *, const tll_msg_t * msg) const>
	int callback_add(const T *obj, unsigned mask = TLL_MESSAGE_MASK_ALL)
	{
		return tll_channel_callback_add(this, _proxy_member_const<T, F>, (void *) obj, mask);
	}

	template <typename T>
	int callback_del(T *obj, unsigned mask = TLL_MESSAGE_MASK_ALL)
	{
		return tll_channel_callback_del(this, proxy<T>::call, obj, mask);
	}

	template <typename T, int (*F)(T *, const tll::Channel *, const tll_msg_t * msg)>
	int callback_del(T *obj, unsigned mask = TLL_MESSAGE_MASK_ALL)
	{
		return tll_channel_callback_del(this, _proxy_func<T, F>, obj, mask);
	}

	template <typename T, int (T::*F)(const tll::Channel *, const tll_msg_t * msg)>
	int callback_del(T *obj, unsigned mask = TLL_MESSAGE_MASK_ALL)
	{
		return tll_channel_callback_del(this, _proxy_member<T, F>, obj, mask);
	}

	template <typename T, int (T::*F)(const tll::Channel *, const tll_msg_t * msg) const>
	int callback_del(const T *obj, unsigned mask = TLL_MESSAGE_MASK_ALL)
	{
		return tll_channel_callback_del(this, _proxy_member_const<T, F>, (void *) obj, mask);
	}

	int add_callback(tll_channel_callback_t cb, void * user, unsigned mask = TLL_MESSAGE_MASK_ALL) { return callback_add(cb, user, mask); }
	int del_callback(tll_channel_callback_t cb, void * user, unsigned mask = TLL_MESSAGE_MASK_ALL) { return callback_del(cb, user, mask); }

 private:

	template <typename T, int (*F)(T *, const tll::Channel *, const tll_msg_t *)>
	static int _proxy_func(const tll_channel_t * c, const tll_msg_t * msg, void * data)
	{
		return std::invoke(F, static_cast<T *>(data), static_cast<const tll::Channel *>(c), msg);
	}

	template <typename T, int (T::*F)(const tll::Channel *, const tll_msg_t *)>
	static int _proxy_member(const tll_channel_t * c, const tll_msg_t * msg, void * data)
	{
#ifdef __clang__
		return std::invoke(F, static_cast<T *>(data), static_cast<const tll::Channel *>(c), msg);
#else
		return (static_cast<T *>(data)->*F)(static_cast<const tll::Channel *>(c), msg);
#endif
	}

	template <typename T, int (T::*F)(const tll::Channel *, const tll_msg_t *) const>
	static int _proxy_member_const(const tll_channel_t * c, const tll_msg_t * msg, void * data)
	{
#ifdef __clang__
		return std::invoke(F, static_cast<const T *>(data), static_cast<const tll::Channel *>(c), msg);
#else
		return (static_cast<const T *>(data)->*F)(static_cast<const tll::Channel *>(c), msg);
#endif
	}
};

namespace channel {

class Context
{
	tll_channel_context_t * _ptr = nullptr;

	struct consume_t {};
	Context(tll_channel_context_t * ctx, consume_t) : _ptr(ctx) {}

 public:
	explicit Context(Config &cfg) : _ptr(tll_channel_context_new(cfg)) {}
	explicit Context(Config &&cfg) : _ptr(tll_channel_context_new(cfg)) {}

	Context(Context &rhs) : _ptr(tll_channel_context_ref(rhs._ptr)) {}
	Context(const Context &rhs) = delete;
	Context(Context &&rhs) { std::swap(_ptr, rhs._ptr); }
	Context(tll_channel_context_t * ctx) : _ptr(tll_channel_context_ref(ctx)) {}
	~Context() { tll_channel_context_free(_ptr); }

	static Context consume(tll_channel_context_t *ctx) { return Context(ctx, consume_t()); }
	static Context default_context() { return Context(tll_channel_context_default()); }

	Context & operator = (Context ctx) { std::swap(_ptr, ctx._ptr); return *this; }

	operator tll_channel_context_t * () { return _ptr; }
	operator const tll_channel_context_t * () const { return _ptr; }

	std::unique_ptr<Channel> channel(std::string_view params, Channel * master = nullptr, const tll_channel_impl_t * impl = nullptr)
	{
		auto c = static_cast<Channel *>(tll_channel_new(_ptr, params.data(), params.size(), master, impl));
		return std::unique_ptr<Channel>(c);
	}

	std::unique_ptr<Channel> channel(const ConstConfig &url, Channel * master = nullptr, const tll_channel_impl_t * impl = nullptr)
	{
		auto c = static_cast<Channel *>(tll_channel_new_url(_ptr, url, master, impl));
		return std::unique_ptr<Channel>(c);
	}

	Channel * get(std::string_view name) const { return static_cast<Channel *>(tll_channel_get(_ptr, name.data(), name.size())); }

	int reg(const tll_channel_impl_t * impl, const std::string &name = "") { return tll_channel_impl_register(_ptr, impl,  name.c_str()); }
	int unreg(const tll_channel_impl_t * impl, const std::string &name = "") { return tll_channel_impl_unregister(_ptr, impl, name.c_str()); }
	const tll_channel_impl_t * impl_get(const std::string &name) const { return tll_channel_impl_get(_ptr, name.c_str()); }

	int alias_reg(const std::string &name, tll::ConstConfig &cfg) { return tll_channel_alias_register_url(_ptr, name.c_str(), cfg); }
	int alias_unreg(const std::string &name, tll::ConstConfig &cfg) { return tll_channel_alias_unregister_url(_ptr, name.c_str(), cfg); }

	int alias_reg(const std::string &name, std::string_view url) { return tll_channel_alias_register(_ptr, name.c_str(), url.data(), url.size()); }
	int alias_unreg(const std::string &name, std::string_view url) { return tll_channel_alias_unregister(_ptr, name.c_str(), url.data(), url.size()); }

	int load(const std::string &path, const std::string &symbol = "") { return load(path, tll::ConstConfig(), symbol); }
	int load(const std::string &path, const tll::ConstConfig &cfg, const std::string &symbol = "") { return tll_channel_module_load_cfg(_ptr, path.c_str(), symbol.c_str(), cfg); }

	Config config() { return Config::consume(tll_channel_context_config(_ptr)); }
	const Config config() const { return Config::consume(tll_channel_context_config((tll_channel_context_t *) _ptr)); }

	Config config_defaults() { return Config::consume(tll_channel_context_config_defaults(_ptr)); }
	const Config config_defaults() const { return Config::consume(tll_channel_context_config_defaults((tll_channel_context_t *) _ptr)); }

	tll_stat_list_t * stat_list() { return tll_channel_context_stat_list(_ptr); }

	const tll_scheme_t * scheme_load(std::string_view url, bool cache = true)
	{
		return tll_channel_context_scheme_load(_ptr, url.data(), url.size(), cache);
	}
};

} // namespace channel

inline channel::Context Channel::context() const { return channel::Context::consume(tll_channel_context(this)); }
inline std::unique_ptr<Channel> Channel::init(std::string_view url, Channel * master) { return channel::Context::default_context().channel(url, master); }

} // namespace tll
#endif

#endif//_TLL_CHANNEL_H
