/*
 * Copyright (c) 2018-2021 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#ifndef _TLL_SCHEME_H
#define _TLL_SCHEME_H

#include <stddef.h>

#ifdef __cplusplus
#include <string>
#include <string_view>
#include <memory>

#include <tll/util/cstring.h>

namespace tll::scheme {
template <typename T>
T * lookup_name(T * list, std::string_view name)
{
	for (auto i = list; i; i = i->next) {
		if (i->name && i->name == name)
			return i;
	}
	return nullptr;
}

template <typename T>
T * lookup_msgid(T * list, int msgid)
{
	for (auto i = list; i; i = i->next) {
		if (i->msgid && i->msgid == msgid)
			return i;
	}
	return nullptr;
}
} // namespace tll::scheme

extern "C" {
#endif

struct tll_scheme_t;
struct tll_scheme_t * tll_scheme_load(const char * url, int ulen);
/**
 * Deep copy scheme structure except user fields
 */
struct tll_scheme_t * tll_scheme_copy(const struct tll_scheme_t *);

/**
 * Increment scheme reference count
 */
const struct tll_scheme_t * tll_scheme_ref(const struct tll_scheme_t *);

/**
 * Decrement scheme reference count
 */
void tll_scheme_unref(const struct tll_scheme_t *);

/**
 * Dump scheme into string. Supported formats:
 *
 *  - yamls - yaml representation
 *  - yamls+gz - compressed yaml, base64(gzip(yaml))
 *  - sha256 - sha256 hash of yaml representation
 *
 * On error and for unsupported format nullptr is returned.
 * Returned string should be free'd by caller
 */
char * tll_scheme_dump(const struct tll_scheme_t *s, const char * format);

typedef enum tll_scheme_field_type_t
{
	TLL_SCHEME_FIELD_INT8,
	TLL_SCHEME_FIELD_INT16,
	TLL_SCHEME_FIELD_INT32,
	TLL_SCHEME_FIELD_INT64,
	TLL_SCHEME_FIELD_UINT8,
	TLL_SCHEME_FIELD_UINT16,
	TLL_SCHEME_FIELD_UINT32,
	TLL_SCHEME_FIELD_DOUBLE,
	TLL_SCHEME_FIELD_DECIMAL128,
	TLL_SCHEME_FIELD_BYTES,
	TLL_SCHEME_FIELD_MESSAGE,
	TLL_SCHEME_FIELD_ARRAY,
	TLL_SCHEME_FIELD_POINTER,
	TLL_SCHEME_FIELD_UNION,
	TLL_SCHEME_FIELD_UINT64,
} tll_scheme_field_type_t;

typedef enum tll_scheme_sub_type_t
{
	TLL_SCHEME_SUB_NONE,
	TLL_SCHEME_SUB_ENUM,
	TLL_SCHEME_SUB_BYTE_STRING,
	TLL_SCHEME_SUB_FIXED_POINT,
	TLL_SCHEME_SUB_TIME_POINT,
	TLL_SCHEME_SUB_DURATION,
	TLL_SCHEME_SUB_BITS,
} tll_scheme_sub_type_t;

typedef struct tll_scheme_option_t
{
	struct tll_scheme_option_t * next;
	const char * name;
	const char * value;
} tll_scheme_option_t;

/// Enum value descriptor
typedef struct tll_scheme_enum_value_t
{
	/// Pointer to the next element in a linked list
	struct tll_scheme_enum_value_t * next;
	/// Name of the enum value
	const char * name;
	/// Value
	long long value;
} tll_scheme_enum_value_t;

/// Enum descriptor
typedef struct tll_scheme_enum_t
{
	/// Pointer to the next element in a linked list
	struct tll_scheme_enum_t * next;
	/// Name of enum type
	const char * name;
	/// Primitive type of enum values
	tll_scheme_field_type_t type;
	/// Size of enum type
	size_t size;
	/// Linked list of enum values
	struct tll_scheme_enum_value_t * values;
	/// Linked list of options
	struct tll_scheme_option_t * options;
} tll_scheme_enum_t;

/// Union descriptor
typedef struct tll_scheme_union_t
{
	/// Pointer to the next element in a linked list
	struct tll_scheme_union_t * next;
	/// Union type name
	const char * name;
	/// Integer union type field
	struct tll_scheme_field_t * type_ptr;
	/// Array of union variants
	struct tll_scheme_field_t * fields;
	/// Size of union fields list (amount of fields)
	size_t fields_size;
	/// Size in bytes of union part (without type_ptr)
	size_t union_size;
	/// Linked list of options
	struct tll_scheme_option_t * options;
} tll_scheme_union_t;

/// Descriptor of bit field in bits
typedef struct tll_scheme_bit_field_t
{
	/// Pointer to the next element in a linked list
	struct tll_scheme_bit_field_t * next;
	/// Name of bit
	const char * name;
	/// Offset in bits
	unsigned offset;
	/// Size in bits
	unsigned size;
} tll_scheme_bit_field_t;

/// Bits descriptor
typedef struct tll_scheme_bits_t
{
	/// Pointer to the next element in a linked list
	struct tll_scheme_bits_t * next;
	/// Name of bits type
	const char * name;
	/// Integer type used to hold bits
	tll_scheme_field_type_t type;
	/// Size in bytes of bits field
	size_t size;
	/// List of bit values
	struct tll_scheme_bit_field_t * values;
	/// Linked list of options
	struct tll_scheme_option_t * options;
} tll_scheme_bits_t;

struct tll_scheme_message_t;

/*
typedef struct tll_scheme_time_resolution_t
{
	unsigned mul;
	unsigned div;
} tll_scheme_time_resolution_t;
*/

typedef enum tll_scheme_time_resolution_t {
	TLL_SCHEME_TIME_NS,
	TLL_SCHEME_TIME_US,
	TLL_SCHEME_TIME_MS,
	TLL_SCHEME_TIME_SECOND,
	TLL_SCHEME_TIME_MINUTE,
	TLL_SCHEME_TIME_HOUR,
	TLL_SCHEME_TIME_DAY,
} tll_scheme_time_resolution_t;

/// Type of offset pointer structure
typedef enum tll_scheme_offset_ptr_version_t {
	/// Default offset pointer, 8 bytes with entity size @see tll_scheme_offset_ptr_t
	TLL_SCHEME_OFFSET_PTR_DEFAULT = 0,
	/// Shoft offset pointer, 4 bytes without entity size @see tll_scheme_offset_ptr_legacy_short_t
	TLL_SCHEME_OFFSET_PTR_LEGACY_SHORT,
	/// Long deprecated offset pointer, 8 bytes with entity size @see tll_scheme_offset_ptr_legacy_long_t
	TLL_SCHEME_OFFSET_PTR_LEGACY_LONG,
} tll_scheme_offset_ptr_version_t;

/// Field descriptor
typedef struct tll_scheme_field_t
{
	/// Pointer to the next entity
	struct tll_scheme_field_t * next;

	/// Linked list of options
	struct tll_scheme_option_t * options;

	/// Name of the field
	const char * name;
	/// Offset in the message
	size_t offset;
	/// Field type
	tll_scheme_field_type_t type;
	/// Field sub type
	tll_scheme_sub_type_t sub_type;
	/// Size of field data
	size_t size;

	/// Additional fields for different types and sub types
	union {
		/// Message descriptor for TLL_SCHEME_FIELD_MESSAGE fields
		struct tll_scheme_message_t * type_msg;

		/// Sub-field descriptor for TLL_SCHEME_FIELD_POINTER fields
		struct {
			/// Type of element
			struct tll_scheme_field_t * type_ptr;
			/// Type of offset structure
			tll_scheme_offset_ptr_version_t offset_ptr_version;
		};

		/// Sub-field descrioptor for TLL_SCHEME_FIELD_ARRAY fields
		struct {
			/// Type of element, offset from start of array field. Usually count_ptr->size
			struct tll_scheme_field_t * type_array;
			/// Type of count field, offset from start of array. Usually 0
			struct tll_scheme_field_t * count_ptr;
			/// Maximum number of elements in array
			size_t count;
		};

		/// Enum descriptor for TLL_SCHEME_SUB_ENUM fields
		struct tll_scheme_enum_t * type_enum;
		/// Fixed point precision (number of digits) for TLL_SCHEME_SUB_FIXED_POINT fields
		unsigned fixed_precision;
		/// Time resolution (from 2^32/1 to 1/2^32) for TLL_SCHEME_SUB_TIME_POINT/DURATION fields
		tll_scheme_time_resolution_t time_resolution;

		/// List of bit fields with corresponding offsets for TLL_SCHEME_SUB_BITS
		struct {
			struct tll_scheme_bit_field_t * bitfields;
			struct tll_scheme_bits_t * type_bits;
		};

		/// Union descriptors for TLL_SCHEME_FIELD_UNION
		struct tll_scheme_union_t * type_union;
	};

	/// User defined data
	void * user;
	/// Function to destroy user defined data, if not specified standard ``free`` is used
	void (*user_free)(void *);

	/// Field index, negative if not defined (auto or mandatory fields)
	int index;

#ifdef __cplusplus
	static constexpr auto Int8 = TLL_SCHEME_FIELD_INT8;
	static constexpr auto Int16 = TLL_SCHEME_FIELD_INT16;
	static constexpr auto Int32 = TLL_SCHEME_FIELD_INT32;
	static constexpr auto Int64 = TLL_SCHEME_FIELD_INT64;
	static constexpr auto UInt8 = TLL_SCHEME_FIELD_UINT8;
	static constexpr auto UInt16 = TLL_SCHEME_FIELD_UINT16;
	static constexpr auto UInt32 = TLL_SCHEME_FIELD_UINT32;
	static constexpr auto UInt64 = TLL_SCHEME_FIELD_UINT64;
	static constexpr auto Double = TLL_SCHEME_FIELD_DOUBLE;
	static constexpr auto Decimal128 = TLL_SCHEME_FIELD_DECIMAL128;
	static constexpr auto Bytes = TLL_SCHEME_FIELD_BYTES;
	static constexpr auto Message = TLL_SCHEME_FIELD_MESSAGE;
	static constexpr auto Array = TLL_SCHEME_FIELD_ARRAY;
	static constexpr auto Pointer = TLL_SCHEME_FIELD_POINTER;
	static constexpr auto Union = TLL_SCHEME_FIELD_UNION;

	static constexpr auto SubNone = TLL_SCHEME_SUB_NONE;
	static constexpr auto Enum = TLL_SCHEME_SUB_ENUM;
	static constexpr auto ByteString = TLL_SCHEME_SUB_BYTE_STRING;
	static constexpr auto TimePoint = TLL_SCHEME_SUB_TIME_POINT;
	static constexpr auto Duration = TLL_SCHEME_SUB_DURATION;
	static constexpr auto Fixed = TLL_SCHEME_SUB_FIXED_POINT;
	static constexpr auto Bits = TLL_SCHEME_SUB_BITS;
#endif
} tll_scheme_field_t;

/// Message descriptor
typedef struct tll_scheme_message_t
{
	/// Pointer to the next element in a linked list
	struct tll_scheme_message_t * next;

	/// Linked list of options
	struct tll_scheme_option_t * options;

	/// Message id, may be zero if not defined
	int msgid;
	/// Name of the message
	const char * name;
	/// Size of fixed part of the message
	size_t size;
	/// Linked list of fields
	struct tll_scheme_field_t * fields;
	/// Linked list of enum types defined in the message
	struct tll_scheme_enum_t * enums;
	/// Linked list of union types defined in the message
	struct tll_scheme_union_t * unions;

	/// User defined data
	void * user;
	/// Function to destroy user defined data, if not specified standard ``free`` is used
	void (*user_free)(void *);

	/// Linked list of bits types defined in the message
	struct tll_scheme_bits_t * bits;

	/// Presence map field (if defined)
	struct tll_scheme_field_t * pmap;

#ifdef __cplusplus
	tll_scheme_field_t * lookup(std::string_view name) { return tll::scheme::lookup_name(fields, name); }
	const tll_scheme_field_t * lookup(std::string_view name) const { return tll::scheme::lookup_name(fields, name); }
#endif
} tll_scheme_message_t;

typedef struct tll_scheme_import_t
{
	struct tll_scheme_import_t * next;
	const char * url;
	const char * filename;
} tll_scheme_import_t;

struct tll_scheme_internal_t;

/// Scheme descriptor
typedef struct tll_scheme_t
{
	/// Internal structure, do not use
	struct tll_scheme_internal_t * internal;

	/// Linked list of options
	struct tll_scheme_option_t * options;
	/// Linked list of messages
	struct tll_scheme_message_t * messages;
	/// Linked list of global enum types
	struct tll_scheme_enum_t * enums;
	/// Linked list of aliases
	struct tll_scheme_field_t * aliases;
	/// Linked list of global union types
	struct tll_scheme_union_t * unions;

	/// User defined data
	void * user;
	/// Function to destroy user defined data, if not specified standard ``free`` is used
	void (*user_free)(void *);

	/// Linked list of global bits types
	struct tll_scheme_bits_t * bits;

#ifdef __cplusplus
	static tll_scheme_t * load(std::string_view url) { return tll_scheme_load(url.data(), url.size()); }
	tll_scheme_t * copy() const { return tll_scheme_copy(this); }

	tll_scheme_t * ref() { return const_cast<tll_scheme_t *>(tll_scheme_ref(this)); }
	const tll_scheme_t * ref() const { return tll_scheme_ref(this); }

	tll::util::cstring dump(const std::string &format) const { return tll::util::cstring(tll_scheme_dump(this, format.c_str())); }

	tll_scheme_message_t * lookup(int id) { return tll::scheme::lookup_msgid(messages, id); }
	const tll_scheme_message_t * lookup(int id) const { return tll::scheme::lookup_msgid(messages, id); }

	tll_scheme_message_t * lookup(std::string_view name) { return tll::scheme::lookup_name(messages, name); }
	const tll_scheme_message_t * lookup(std::string_view name) const { return tll::scheme::lookup_name(messages, name); }
#endif
} tll_scheme_t;


void tll_scheme_option_free(tll_scheme_option_t *);
void tll_scheme_bits_free(tll_scheme_bits_t *);
void tll_scheme_enum_free(tll_scheme_enum_t *);
void tll_scheme_field_free(tll_scheme_field_t *);
void tll_scheme_message_free(tll_scheme_message_t *);
void tll_scheme_union_free(tll_scheme_union_t *);
//void tll_scheme_free(tll_scheme_t *);

int tll_scheme_fix(tll_scheme_t *);
int tll_scheme_message_fix(tll_scheme_message_t *);
int tll_scheme_field_fix(tll_scheme_field_t *);

static inline int tll_scheme_pmap_get(const void * data, int index)
{
	if (index < 0)
		return 1;
	const unsigned char * udata = (const unsigned char *) data;
	return (udata[index / 8] & (1 << (index % 8))) != 0;
}

inline void tll_scheme_pmap_set(void * data, int index)
{
	if (index < 0)
		return;
	unsigned char * udata = (unsigned char *) data;
	udata[index / 8] |= (1 << (index % 8));
}

static inline void tll_scheme_pmap_unset(void * data, int index)
{
	if (index < 0)
		return;
	unsigned char * udata = (unsigned char *) data;
	udata[index / 8] &= 0xffu ^ (1 << (index % 8));
}

#ifdef __cplusplus
} // extern "C"
#endif

#ifdef __cplusplus
#include "tll/util/getter.h"
#include "tll/util/url.h"

constexpr auto format_as(tll_scheme_field_type_t v) noexcept { return static_cast<int>(v); }
constexpr auto format_as(tll_scheme_sub_type_t v) noexcept { return static_cast<int>(v); }
constexpr auto format_as(tll_scheme_time_resolution_t v) noexcept { return static_cast<int>(v); }
constexpr auto format_as(tll_scheme_offset_ptr_version_t v) noexcept { return static_cast<int>(v); }

namespace tll {
using Scheme = tll_scheme_t;

namespace getter {
template <>
struct getter_api<tll_scheme_option_t *>
{
	using string_type = std::string_view;
	static std::optional<std::string_view> get(const tll_scheme_option_t *o, std::string_view key)
	{
		for (; o; o = o->next) {
			if (key == o->name) {
				if (o->value)
					return o->value;
				return std::nullopt;
			}
		}
		return std::nullopt;
	}

	static bool has(const tll_scheme_option_t *o, std::string_view key)
	{
		return !!get(o, key);
	}
};

}

namespace scheme {
inline PropsView options_map(const tll_scheme_option_t * o)
{
	PropsView r;
	for (; o; o = o->next)
		r.emplace(o->name, o->value);
	return r;
}

using Option = tll_scheme_option_t;
using EnumValue = tll_scheme_enum_value_t;
using Enum = tll_scheme_enum_t;
using Union = tll_scheme_union_t;
using BitFields = tll_scheme_bits_t;
using Field = tll_scheme_field_t;
using Message = tll_scheme_message_t;
using Scheme = tll_scheme_t;

using ConstSchemePtr = std::unique_ptr<const Scheme>;
using SchemePtr = std::unique_ptr<Scheme>;

using time_resolution_t = tll_scheme_time_resolution_t;

inline bool pmap_get(const void * data, int index) { return tll_scheme_pmap_get(data, index); }
inline void pmap_set(void * data, int index) { tll_scheme_pmap_set(data, index); }
inline void pmap_unset(void * data, int index) { tll_scheme_pmap_unset(data, index); }

constexpr std::string_view time_resolution_str(time_resolution_t r)
{
	switch (r) {
	case TLL_SCHEME_TIME_NS: return "ns";
	case TLL_SCHEME_TIME_US: return "us";
	case TLL_SCHEME_TIME_MS: return "ms";
	case TLL_SCHEME_TIME_SECOND: return "s";
	case TLL_SCHEME_TIME_MINUTE: return "m";
	case TLL_SCHEME_TIME_HOUR: return "h";
	case TLL_SCHEME_TIME_DAY: return "d";
	}
	return "";
}

} // namespace scheme
} // namespace tll

namespace std {
template <> struct default_delete<tll::scheme::Scheme> { void operator ()(tll::scheme::Scheme *ptr) const { tll_scheme_unref(ptr); } };
template <> struct default_delete<const tll::scheme::Scheme> { void operator ()(const tll::scheme::Scheme *ptr) const { tll_scheme_unref(ptr); } };

template <> struct default_delete<tll::scheme::Message> { void operator ()(tll::scheme::Message *ptr) const { tll_scheme_message_free(ptr); } };
template <> struct default_delete<tll::scheme::Field> { void operator ()(tll::scheme::Field *ptr) const { tll_scheme_field_free(ptr); } };
template <> struct default_delete<tll::scheme::BitFields> { void operator ()(tll::scheme::BitFields *ptr) const { tll_scheme_bits_free(ptr); } };
template <> struct default_delete<tll::scheme::Enum> { void operator ()(tll::scheme::Enum *ptr) const { tll_scheme_enum_free(ptr); } };
template <> struct default_delete<tll::scheme::Union> { void operator ()(tll::scheme::Union *ptr) const { tll_scheme_union_free(ptr); } };
} // namespace std

#endif //__cplusplus

#endif//_TLL_SCHEME_H
