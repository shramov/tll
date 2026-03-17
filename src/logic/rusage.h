#ifndef _LOGIC_RUSAGE_H
#define _LOGIC_RUSAGE_H

// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Pavel Shramov <shramov@mexmat.net>

#include "tll/channel/tagged.h"
#include "tll/stat.h"

#include <sys/resource.h>

#ifndef RUSAGE_THREAD
#define RUSAGE_THREAD RUSAGE_SELF // TODO: Use clock_gettime(CLOCK_THREAD_CPUTIME_ID)
#endif

namespace tll::channel {

struct Timer : public Tag<TLL_MESSAGE_MASK_DATA> { static constexpr std::string_view name() { return "timer"; } };

class RUsage : public tll::channel::Tagged<RUsage, Timer>
{
	using Base = tll::channel::Tagged<RUsage, Timer>;
	using ns = std::chrono::nanoseconds;
	tll::time_point _last;
	ns _cpu;
	struct rusage _rusage = {};
	int _who = RUSAGE_SELF;

 public:
	static constexpr std::string_view channel_protocol() { return "rusage"; }
	static constexpr auto tagged_stat_policy() { return TaggedStatPolicy::Disable; }

	struct StatType
	{
		tll::stat::IntegerGroup<tll::stat::Unknown, 'c', 'p', 'u'> cpu;
		tll::stat::Integer<tll::stat::Max, tll::stat::Ns, 'c', 'p', 'u'> cpuns;
		tll::stat::Integer<tll::stat::Max, tll::stat::Bytes, 'm', 'e', 'm'> mem;
	};

	stat::BlockT<StatType> * stat() { return static_cast<stat::BlockT<StatType> *>(this->internal.stat); }

	int _init(const tll::Channel::Url &url, tll::Channel *master)
	{
		auto reader = channel_props_reader(url);
		if (reader.getT("thread", false))
			_who = RUSAGE_THREAD;
		if (!reader)
			return _log.fail(EINVAL, "Invalid url: {}", reader.error());
		if (check_channels_size<Timer>(1, 1))
			return EINVAL;
		return 0;
	}

	int _open(const tll::ConstConfig &)
	{
		_last =  {};
		_cpu = {};
		return 0;
	}

	int callback_tag(TaggedChannel<Timer> * c, const tll_msg_t *msg)
	{
		auto stat = this->channelT()->stat();
		if (!stat)
			return 0;
		if (getrusage(_who, &_rusage))
			return _log.fail(EINVAL, "Failed to get rusage: {}", strerror(errno));
		auto now = tll::time::now();
		auto cpu = _tv2ns(_rusage.ru_utime) + _tv2ns(_rusage.ru_stime);
		if (_last != tll::time_point {} && _last != now) {
			auto dt = now - _last;
			if (auto page = stat->acquire(); page) {
				page->mem = _rusage.ru_maxrss * 1024; // ru_maxrss in kilobytes
				page->cpu = 100 * (1. * (cpu - _cpu) / dt);
				page->cpuns = cpu.count();
				stat->release(page);
			}
		}
		_last = now;
		_cpu = cpu;
		return 0;
	}
 private:
	static constexpr ns _tv2ns(const struct timeval &tv)
	{
		using namespace std::chrono;
		return duration_cast<ns>(seconds(tv.tv_sec) + microseconds(tv.tv_usec));
	}
};

} // namespace tll::channel

#endif//_LOGIC_RUSAGE_H
