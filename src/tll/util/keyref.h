#ifndef _TLL_UTIL_KEYREF_H
#define _TLL_UTIL_KEYREF_H

#include "tll/conv/base.h"
#include "tll/keyring.h"

namespace tll::util {

struct KeyRef
{
	enum Type {
		Undefined = 0,
		Key,
		Data,
	} type = Undefined;
	Secret payload;

	KeyRef() = default;
	KeyRef(Type t, std::string_view p): type(t), payload(p) {}
	KeyRef(Type t, Secret p): type(t), payload(std::move(p)) {}

	static expected<KeyRef, std::string> parse(std::string_view uri, bool compat = false)
	{
		auto sep = uri.find(':');
		if (sep == uri.npos) {
			if (compat)
				return KeyRef { Data, uri };
			return unexpected{std::string("Invalid keyref: no : separator")};
		}
		auto method = uri.substr(0, sep);
		auto body = uri.substr(sep+1);
		Type type;
		if (method == "data")
			type = Data;
		else if (method == "key")
			type = Key;
		else
			return unexpected{std::string("Invalid method: ") + std::string(method)};
		return KeyRef { type, body };
	}

	expected<Secret, Errno> read() const
	{
		if (type == Undefined)
			return unexpected{EINVAL};
		if (type == Key) {
			char * buf = nullptr;
			auto r = tll_keyring_read(payload.data(), &buf, 0);
			if (r < 0)
				return unexpected(Errno(-r));
			return Secret::consume(buf, r);
		} else if (type == Data)
			return Secret(payload);
		else
			return unexpected{ENOSYS};
	}
};

struct KeyRefCompat : public KeyRef
{
	using KeyRef::KeyRef;
};

} // namespace tll::util

namespace tll::conv {

template <>
struct parse<tll::util::KeyRef>
{
	static result_t<tll::util::KeyRef> to_any(std::string_view s)
	{
		return tll::util::KeyRef::parse(s, false);
	}
};

template <>
struct parse<tll::util::KeyRefCompat>
{
	static result_t<tll::util::KeyRefCompat> to_any(std::string_view s)
	{
		if (auto r = tll::util::KeyRef::parse(s, true); r)
			return tll::util::KeyRefCompat{r->type, std::move(r->payload)};
		else
			return unexpected(std::move(r.error()));
	}
};

} // namespace tll::conv

#endif//_TLL_UTIL_KEYREF_H
