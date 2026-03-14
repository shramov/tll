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
	using tns = std::chrono::time_point<std::chrono::system_clock, ns>;
	tns _last;
	ns _user;
	ns _sys;
	struct rusage _rusage = {};
	int _who = RUSAGE_SELF;

 public:
	static constexpr std::string_view channel_protocol() { return "rusage"; }
	static constexpr auto tagged_stat_policy() { return TaggedStatPolicy::Disable; }

	struct StatType
	{
		tll::stat::FloatGroup<tll::stat::Unknown, 'c', 'p', 'u'> cpu;
		tll::stat::Integer<tll::stat::Max, tll::stat::Unknown, 'c', 'p', 'u', 'u', 's', 'r'> cpu_user;
		tll::stat::Integer<tll::stat::Max, tll::stat::Unknown, 'c', 'p', 'u', 's', 'y', 's'> cpu_sys;
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
		_user = _sys = {};
		return 0;
	}

	int callback_tag(TaggedChannel<Timer> * c, const tll_msg_t *msg)
	{
		if (!this->_stat_enable)
			return 0;
		if (getrusage(_who, &_rusage))
			return _log.fail(EINVAL, "Failed to get rusage: {}", strerror(errno));
		tns now = tll::time::now();
		auto user = _tv2ns(_rusage.ru_utime);
		auto sys = _tv2ns(_rusage.ru_stime);
		if (_last != tll::time_point {}) {
			auto dt = now - _last;
			auto duser = 100 * (user - _user).count() / dt.count();
			auto dsys = 100 * (sys - _sys).count() / dt.count();
			if (auto page = this->channelT()->stat()->acquire(); page) {
				page->mem = _rusage.ru_maxrss * 1024; // ru_maxrss in kilobytes
				page->cpu = duser + dsys;
				page->cpu_user = duser;
				page->cpu_sys = dsys;
				this->channelT()->stat()->release(page);
			}
		}
		_last = now;
		_user = user;
		_sys = sys;
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
