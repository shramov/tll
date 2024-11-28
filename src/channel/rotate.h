/*
 * Copyright (c) 2023 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#ifndef _TLL_CHANNEL_ROTATE_H
#define _TLL_CHANNEL_ROTATE_H

#include "tll/channel/prefix.h"

#include <mutex>

namespace tll::channel {

class Rotate : public Prefix<Rotate>
{
 private:
	Rotate * _master = nullptr;
	long long _seq_first = -1;
	long long _seq_last = -1;

	struct Files
	{
		struct File
		{
			std::string filename;
			long long last;
		};

		using Map = std::map<long long, File>;
		Map files; // First seq -> filename map
		std::list<Rotate *> listeners;

		long long * seq_first = nullptr;
		long long * seq_last = nullptr;

		std::mutex _lock;
		auto lock() { return std::lock_guard<std::mutex>(_lock); }
	};

	std::shared_ptr<Files> _files;
	Files::Map::const_iterator _current_file;
	bool _current_empty = false;
	bool _end_of_data = false;
	bool _autoclose = false;

	std::string _fileprefix;
	std::string _directory;
	std::string _last_filename;
	std::string_view _filename_key = "info.seq";

	tll::Config _open_cfg;
	int _control_eod_msgid = 0;

	enum class State { Closed, Build, Seek, Read, Write } _state = State::Closed;

 public:
	using Base = Prefix<Rotate>;
	static constexpr std::string_view channel_protocol() { return "rotate+"; }
	static constexpr auto prefix_config_policy() { return PrefixConfigPolicy::Manual; }

	const Scheme * scheme(int type) const
	{
		switch (type) {
		case TLL_MESSAGE_DATA:
			return _scheme.get();
		case TLL_MESSAGE_CONTROL:
			return _scheme_control.get();
		default:
			return Base::scheme(type);
		}
	}

	std::optional<const tll_channel_impl_t *> _init_replace(const tll::Channel::Url &url, tll::Channel *master);
	int _on_init(tll::Channel::Url &curl, const tll::Channel::Url &, tll::Channel *master);
	int _open(const tll::ConstConfig &params);
	int _close(bool force = false);

	int _post(const tll_msg_t *msg, int flags);
	int _process(long timeout, int flags);

	int _on_data(const tll_msg_t *msg);
	int _on_other(const tll_msg_t *msg);
	int _on_active();
	int _on_closing();
	int _on_closed();

	auto files() { return _files; }
	void notify()
	{
		if (state() != tll::state::Active)
			return;
		_update_dcaps(dcaps::Process | dcaps::Pending);
	}

 private:
	int _build_map();

	int _post_rotate(const tll_msg_t *msg);
	int _seek(long long seq);

	bool _current_last()
	{
		auto lock = _files->lock();
		return _current_file == --_files->files.end();
	}
};

} // namespace tll::channel

#endif//_TLL_CHANNEL_ROTATE_H
