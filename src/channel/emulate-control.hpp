#ifndef _CHANNEL_EMULATE_CONTROL_HPP
#define _CHANNEL_EMULATE_CONTROL_HPP

#include "channel/emulate-control.h"

#include "tll/scheme/merge.h"

#include "channel/stream-control-server.h"
#include "channel/stream-control.h"
#include "tll/channel/tcp-client-scheme.h"
#include "tll/channel/tcp-scheme.h"

namespace {
inline std::optional<std::string_view> lookup_scheme(std::string_view name)
{
	if (name == "stream-server")
		return stream_server_control_scheme::scheme_string;
	else if (name == "stream-client")
		return stream_control_scheme::scheme_string;
	else if (name == "tcp-server")
		return tcp_scheme::scheme_string;
	else if (name == "tcp-client")
		return tcp_client_scheme::scheme_string;
	return std::nullopt;
}
}

namespace tll::channel {

template <typename T, typename S>
template <typename Reader>
int EmulateControl<T, S>::_init_emulate_control(Reader &reader)
{
	auto control = reader.get("scheme-control");
	auto emulate = reader.getT("emulate-control", std::vector<std::string>{});

	if (!reader)
		return this->_log.fail(EINVAL, "Invalid url: {}", reader.error());

	if (control && _merge_control(*control, *control))
		return EINVAL;

	for (auto name : emulate) {
		auto string = lookup_scheme(name);
		if (!string)
			return this->_log.fail(EINVAL, "Unknown control emulation tag: {}", name);
		this->_log.info("Add control scheme for {}", name);
		if (_merge_control(*string, name))
			return EINVAL;
	}
	return 0;
}

template <typename T, typename S>
int EmulateControl<T, S>::_merge_control(std::string_view scheme_string, std::string_view name)
{
	this->_log.trace("Loading control scheme {}...", name.substr(0, 64));

	std::unique_ptr<const tll::Scheme> ptr { this->context().scheme_load(scheme_string) };
	if (!ptr)
		return this->_log.fail(EINVAL, "Failed to load control scheme from {}...", name.substr(0, 64));

	if (this->_scheme_control) {
		auto control = tll::scheme::merge({this->_scheme_control.get(), ptr.get()});
		if (!control)
			return this->_log.fail(EINVAL, "Failed to merge control scheme: {}", control.error());
		this->_scheme_control.reset(*control);
	} else
		this->_scheme_control = std::move(ptr);

	return 0;
}
} // namespace tll::channel

#endif//_CHANNEL_EMULATE_CONTROL_HPP
