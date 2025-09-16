#ifndef _CHANNEL_EMULATE_CONTROL_H
#define _CHANNEL_EMULATE_CONTROL_H

#include "tll/channel/base.h"

namespace tll::channel {

template <typename T, typename S = Base<T>>
class EmulateControl : public S
{
public:
	using Base = S;

	template <typename Reader>
	int _init_emulate_control(Reader &reader);

 protected:
	int _merge_control(std::string_view scheme, std::string_view name);
};

} // namespace tll::channel

#endif//_CHANNEL_EMULATE_CONTROL_H
