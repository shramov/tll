#ifndef _TLL_UTIL_KEYREF_H
#define _TLL_UTIL_KEYREF_H

#include "tll/conv/base.h"
#include "tll/keyring.h"

namespace tll::keyring {

struct KeyRef
{
	std::string uri;

	static expected<Secret, Errno> load_view(std::string_view uri)
	{
		auto sep = uri.find(':');
		if (sep == uri.npos)
			return unexpected{EINVAL};
		auto method = uri.substr(0, sep);
		auto body = uri.substr(sep+1);
		if (method == "data")
			return Secret(body.data(), body.size());
		else if (method == "key")
			return read(std::string(body));
		else
			return unexpected{ENOSYS};
	}

	auto load() const { return load_view(uri); }

	operator std::string_view () const { return uri; }
};

} // namespace tll::keyring

namespace tll::conv {

template <>
struct parse<tll::keyring::KeyRef>
{
	static result_t<tll::keyring::KeyRef> to_any(std::string_view s)
	{
		auto sep = s.find(':');
		if (sep == s.npos)
			return error("Invalid keyref: no ':' separator");
		return tll::keyring::KeyRef { std::string(s) };
	}
};

} // namespace tll::conv

#endif//_TLL_UTIL_KEYREF_H
