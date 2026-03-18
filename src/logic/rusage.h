#ifndef _LOGIC_RUSAGE_H
#define _LOGIC_RUSAGE_H

// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Pavel Shramov <shramov@mexmat.net>

#include "tll/channel/tagged.h"
#include "tll/stat.h"
#include "tll/util/rusage.h"

#ifndef RUSAGE_THREAD
#define RUSAGE_THREAD RUSAGE_SELF // TODO: Use clock_gettime(CLOCK_THREAD_CPUTIME_ID)
#endif

namespace tll::channel {

struct Timer : public Tag<TLL_MESSAGE_MASK_DATA> { static constexpr std::string_view name() { return "timer"; } };

class RUsage : public tll::channel::Tagged<RUsage, Timer>
{
	using Base = tll::channel::Tagged<RUsage, Timer>;
	using RU = tll::util::RUsage;
	RU _rusage { RU::Process };

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
		auto type = reader.getT("type", RU::Process, {{"process", RU::Process}, {"thread", RU::Thread}});
		_rusage = RU { type };
		if (!reader)
			return _log.fail(EINVAL, "Invalid url: {}", reader.error());
		if (check_channels_size<Timer>(1, 1))
			return EINVAL;
		return 0;
	}

	int _open(const tll::ConstConfig &)
	{
		_rusage.reset();
		return 0;
	}

	int callback_tag(TaggedChannel<Timer> * c, const tll_msg_t *msg)
	{
		auto stat = this->channelT()->stat();
		if (!stat)
			return 0;
		_rusage.update();

		if (auto page = stat->acquire(); page) {
			page->mem = _rusage.memory;
			page->cpu = 100 * _rusage.cpu_ratio;
			page->cpuns = _rusage.cpu.count();
			stat->release(page);
		}
		return 0;
	}
};

} // namespace tll::channel

#endif//_LOGIC_RUSAGE_H
