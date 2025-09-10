// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Pavel Shramov <shramov@mexmat.net>

#ifndef _TLL_COMPAT_JTHREAD_H
#define _TLL_COMPAT_JTHREAD_H

#include <thread>

namespace tll::compat {

struct stop_token
{
	bool * _source = nullptr;

	constexpr bool stop_requested() const { return !_source || *_source; }
};

struct jthread
{
	std::thread _thread;
	bool _stop = false;

	jthread() = delete;
	jthread(const jthread &) = delete;
	jthread(jthread &&) = delete;

	template <class F, class ...Args>
	explicit jthread(F&& f, Args&&... args)
		: _thread(std::forward<F>(f), stop_token { &_stop }, std::forward<Args>(args)...)
	{
	}

	~jthread()
	{
		_stop = true;
		_thread.join();
	}
};

} // namespace tll::compat

#ifndef __cpp_lib_jthread
namespace std {
using tll::compat::stop_token;
using tll::compat::jthread;
}
#endif//__cpp_lib_jthread

#endif//_TLL_COMPAT_JTHREAD_H
