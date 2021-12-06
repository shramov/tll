/*
 * Copyright (c) 2021 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#ifndef _TLL_CHANNEL_LASTSEQ_H
#define _TLL_CHANNEL_LASTSEQ_H

#include "tll/channel/base.h"

namespace tll::channel {

namespace lastseq {
enum LastSeqMode {
	Rx = 1,
	Tx = 2,
	RxTx = Rx | Tx,
};

template <LastSeqMode Mode> struct StatT;

template <> struct StatT<Rx>
{
	template <typename T>
	struct type : public T
	{
		tll::stat::Integer<tll::stat::Last, tll::stat::Unknown, 'r', 'x', 's', 'e', 'q'> rxseq;
	};
};

template <> struct StatT<Tx>
{
	template <typename T>
	struct type : public T
	{
		tll::stat::Integer<tll::stat::Last, tll::stat::Unknown, 't', 'x', 's', 'e', 'q'> txseq;
	};
};

template <> struct StatT<RxTx>
{
	template <typename T>
	struct type : public T
	{
		tll::stat::Integer<tll::stat::Last, tll::stat::Unknown, 'r', 'x', 's', 'e', 'q'> rxseq;
		tll::stat::Integer<tll::stat::Last, tll::stat::Unknown, 't', 'x', 's', 'e', 'q'> txseq;
	};
};

template <LastSeqMode Mode, typename T>
using Stat = typename StatT<Mode>::template type<T>;
} // namespace lastseq

using lastseq::LastSeqMode;

/// Channel mixin that records last post/processed seq in stat
template <LastSeqMode Mode, typename T, typename S = Base<T>>
class LastSeq : public S
{
public:
	using Base = S;

	using StatType = typename lastseq::Stat<Mode, typename Base::StatType>;

	stat::BlockT<StatType> * stat() { return static_cast<stat::BlockT<StatType> *>(this->internal.stat); }

	void _last_seq_rx(long long seq) { _last_seq_mode<LastSeqMode::Rx>(seq); }
	void _last_seq_tx(long long seq) { _last_seq_mode<LastSeqMode::Tx>(seq); }

	int _post(const tll_msg_t *msg, int flags)
	{
		auto r = Base::_post(msg, flags);
		if constexpr (Mode & LastSeqMode::Tx) {
			if (r == 0 && msg->type == TLL_MESSAGE_DATA)
				_last_seq_tx(msg->seq);
		}
		return r;
	}

	inline int _callback_data(const tll_msg_t * msg)
	{
		if constexpr (Mode & LastSeqMode::Rx)
			_last_seq_rx(msg->seq);
		return Base::_callback_data(msg);
	}

 private:
	template <LastSeqMode M>
	void _last_seq_mode(long long seq)
	{
		if (this->_stat_enable) {
			auto page = this->channelT()->stat()->acquire();
			if (page) {
				if constexpr (M == LastSeqMode::Rx)
					page->rxseq = seq;
				else
					page->txseq = seq;
				this->channelT()->stat()->release(page);
			}
		}
	}

};

template <typename T, typename S = Base<T>>
using LastSeqRx = LastSeq<LastSeqMode::Rx, T, S>;

template <typename T, typename S = Base<T>>
using LastSeqTx = LastSeq<LastSeqMode::Tx, T, S>;

template <typename T, typename S = Base<T>>
using LastSeqRxTx = LastSeq<LastSeqMode::RxTx, T, S>;

} // namespace tll::channel

#endif//_TLL_CHANNEL_LASTSEQ_H
