/*
 * Copyright (c) 2019-2020 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#ifndef _PROCESSOR_SCHEME_H
#define _PROCESSOR_SCHEME_H

namespace tll { class Channel; }
namespace tll::processor::_ { struct Worker; }
namespace tll::processor::_ { struct Processor; }

namespace tll::processor::_::scheme {

struct Exit
{
	static constexpr int id = 1;
	int code;
	tll::Channel * channel = nullptr;
};

struct State
{
	static constexpr int id = 2;

	tll_state_t state;
	const Channel * channel;
	Worker * worker;
};

struct WorkerState
{
	static constexpr int id = 3;

	tll_state_t state;
	Worker * worker;
};

struct Activate
{
	static constexpr int id = 5;
	tll::processor::_::Object * obj;
};

struct Deactivate
{
	static constexpr int id = 6;
	tll::processor::_::Object * obj;
};

} // namespace tll::processor

#endif//_PROCESSOR_SCHEME_H
