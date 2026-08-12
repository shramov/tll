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

	static expected<KeyRef, std::string> parse(std::string_view uri)
	{
		auto sep = uri.find(':');
		if (sep == uri.npos)
			return unexpected{std::string("Invalid keyref: no : separator")};
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
			return unexpected{-EINVAL};
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

} // namespace tll::util

namespace tll::conv {

template <>
struct parse<tll::util::KeyRef>
{
	static result_t<tll::util::KeyRef> to_any(std::string_view s)
	{
		auto sep = s.find(':');
		if (sep == s.npos)
			return error("Invalid keyref: no ':' separator");
		return tll::util::KeyRef::parse(s);
	}
};

} // namespace tll::conv

#endif//_TLL_UTIL_KEYREF_H
