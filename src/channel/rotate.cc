/*
 * Copyright (c) 2023 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#include "tll/scheme/merge.h"
#include "channel/rotate.h"

#include <filesystem>

using namespace tll::channel;

static constexpr std::string_view control_scheme_read = R"(yamls://
- name: Seek
  id: 10
- name: EndOfData
  id: 20
- name: Rotate
  id: 150
)";
static constexpr std::string_view control_scheme_write = R"(yamls://
- name: Rotate
  id: 150
)";
static constexpr int control_seek_msgid = 10;
static constexpr int control_eod_msgid = 20;
static constexpr int control_rotate_msgid = 150;

TLL_DEFINE_IMPL(Rotate);

std::optional<const tll_channel_impl_t *> Rotate::_init_replace(const tll::Channel::Url &url, tll::Channel *master)
{
	return nullptr;
	/*
	auto reader = channel_props_reader(url);
	enum rw_t { None = 0, R = 1, W = 2, RW = R | W };
	auto dir = reader.getT("dir", None, {{"r", R}, {"w", W}, {"rw", RW}, {"in", R}, {"out", W}, {"inout", RW}});
	if (!reader)
		return _log.fail(std::nullopt, "Invalid url: {}", reader.error());

	if (dir == RW)
		return _log.fail(std::nullopt, "rotate+ can be either read or write, not read-write");
	if (dir == None || dir == R)
		return &RotateClient::impl;
	else
		return nullptr;
		*/
}

int Rotate::_on_init(tll::Channel::Url &curl, const tll::Channel::Url &url, tll::Channel *master)
{
	auto reader = channel_props_reader(url);
	_autoclose = reader.getT("autoclose", true);
	_convert_enable = reader.getT("convert", false);
	auto key_first = reader.getT("filename-key", true, {{"first", true}, {"last", false}});
	if (!reader)
		return this->_log.fail(EINVAL, "Invalid url: {}", reader.error());

	_filename_key = key_first ? "info.seq-begin" : "info.seq";

	auto path = std::filesystem::path(curl.host());
	_directory = path.parent_path();
	if (_directory.empty())
		_directory = ".";
	_fileprefix = path.filename();

	if (_fileprefix.empty())
		return _log.fail(EINVAL, "Empty filename");
	if (_fileprefix.back() == '.')
		return _log.fail(EINVAL, "Filename with . in the end");
	_last_filename = std::string(path.parent_path() / _fileprefix) + ".current.dat";

	curl.host("");

	if ((internal.caps & caps::InOut) == 0) // Defaults to input
		internal.caps |= caps::Input;
	if ((internal.caps & caps::InOut) == caps::InOut)
		return _log.fail(EINVAL, "rotate+:// can be either read-only or write-only, need proper dir in parameters");

	_scheme_control.reset(context().scheme_load((internal.caps & caps::Input) ? control_scheme_read : control_scheme_write));
	if (!_scheme_control.get())
		return _log.fail(EINVAL, "Failed to load control scheme");

	if (internal.caps & caps::Input)
		curl.remove("scheme");

	_master = tll::channel_cast<Rotate>(master);

	return 0;
}

int Rotate::_open(const tll::ConstConfig &cfg)
{
	_control_eod_msgid = 0;
	_end_of_data = false;
	_seq_first = -1;
	_seq_last = -1;
	_open_cfg = tll::Config();
	_state = State::Closed;
	_convert.reset();

	auto reader = tll::make_props_reader(cfg);

	if (!_master) {
		if (_build_map())
			return _log.fail(EINVAL, "Failed to build seq map");
	} else {
		_files = _master->_files;

		auto lock = _files->lock();
		_current_file = _files->files.end();
		_files->listeners.push_back(this);
		_seq_first = *_files->seq_first;
	}

	scheme::ConstSchemePtr scheme;
	{
		auto lock = _files->lock();
		scheme.reset(_files->scheme->ref());
	}

	if (_scheme_url) {
		_log.debug("Loading scheme from {}...", _scheme_url->substr(0, 64));
		_scheme.reset(context().scheme_load(*_scheme_url, _scheme_cache));
		if (!_scheme)
			return state_fail(EINVAL, "Failed to load scheme from {}...", _scheme_url->substr(0, 64));
	}

	config_info().set_ptr("seq-begin", _files->seq_first);
	config_info().set_ptr("seq", _files->seq_last);

	if (internal.caps & caps::Input) {
		_state = State::Read;
		{
			auto lock = _files->lock();
			if (_files->files.empty())
				return _log.fail(EINVAL, "No files found, can not open for reading");
		}

		if (!_scheme)
			_scheme.reset(scheme->ref());

		auto seq = reader.getT<long long>("seq", -1);
		if (!reader)
			return _log.fail(EINVAL, "Invalid params: {}", reader.error());
		if (seq != -1)
			return _seek(seq);

		{
			auto lock = _files->lock();
			_current_file = _files->files.begin();
			_open_cfg.set("filename", _current_file->second.filename);
		}

		if (_seq_first != -1)
			_open_cfg.set("seq", conv::to_string(_seq_first));
	} else {
		_open_cfg.set("filename", _last_filename);
		_state = State::Write;
	}

	if ((internal.caps & caps::Output) && _scheme_url) {
		auto h0 = scheme ? scheme->dump("sha256") : "NULL";
		auto h1 = _scheme->dump("sha256");
		_log.debug("Scheme hash: {}, last hash: {}", *h1, *h0);
		if (*h0 != *h1) {
			_log.info("Scheme changed, force rotation");
			if (_current_empty) {
				_log.info("Last file without data, overwrite");
				auto cfg = _open_cfg.copy();
				cfg.set("overwrite", "yes");
				return _child->open(cfg);
			} else {
				_state = State::Closed;
				if (auto r = _child->open(_open_cfg); r)
					return r;
				_state = State::Write;
				return _rotate();
			}
		}
	}

	return _child->open(_open_cfg);
}

int Rotate::_close(bool force)
{
	if (_files) {
		config_info().setT("seq-begin", *_files->seq_first);
		config_info().setT("seq", *_files->seq_last);
		auto lock = _files->lock();
		_files->listeners.remove(this);
	}

	_current_file = {};
	_files.reset();
	_scheme.reset();

	_state = State::Closed;
	if (_child->state() != tll::state::Closed)
		return _child->close(true);
	state(tll::state::Closed);
	return 0;
}

int Rotate::_post_rotate(const tll_msg_t *msg)
{
	if (internal.caps & caps::Input)
		return _log.fail(EINVAL, "Can not rotate input channel");
	if (_current_file == _files->files.end()) {
		_log.info("Skip rotating empty file");
		return 0;
	}
	return _rotate();
}

int Rotate::_rotate()
{
	auto key = _child->config().get(_filename_key);
	if (!key)
		return _log.fail(EINVAL, "File {} has no filename key '{}' in config", _current_file->second.filename, _filename_key);
	_child->close();

	auto next = std::filesystem::path(_directory) / fmt::format("{}.{}.dat", _fileprefix, *key);
	_log.info("Rename current file {} to {}", _current_file->second.filename, next.string());
	std::filesystem::rename(_current_file->second.filename, next);
	{
		auto lock = _files->lock();
		for (auto & p : _files->listeners) {
			if (p->_current_file == _current_file)
				p->notify();
		}
		auto file = const_cast<Files::File *>(&_current_file->second); // It's safe to const cast
		file->filename = next;
		file->last = _seq_last;
		_current_file = _files->files.end();
	}
	_current_empty = true;
	_child->open(_open_cfg);

	auto s = _child->scheme();
	{
		auto lock = _files->lock();
		_files->scheme.reset(s->ref());
	}
	return 0;
}

int Rotate::_seek(long long seq)
{
	if (internal.caps & caps::Output)
		return _log.fail(EINVAL, "Can not seek in write-only rotate+");

	Files::Map::const_iterator it;
	{
		auto lock = _files->lock();

		if (_files->files.empty())
			return _log.fail(EINVAL, "Can not seek in empty rotating files");

		it = _files->files.upper_bound(seq);
		if (it == _files->files.begin())
			return _log.fail(EINVAL, "Requested seq {} is less then first seq {}", seq, _files->files.begin()->first);
		it--;
	}

	_state = State::Seek;
	if (_child->state() != tll::state::Closed)
		_child->close(true);
	_current_file = it;
	_open_cfg.set("filename", it->second.filename);
	_open_cfg.set("seq", tll::conv::to_string(seq));
	_state = State::Read;
	return _child->open(_open_cfg);
}

int Rotate::_post(const tll_msg_t *msg, int flags)
{
	if (msg->type == TLL_MESSAGE_CONTROL) {
		if (msg->msgid == control_rotate_msgid && (internal.caps & caps::Output))
			return _post_rotate(msg);
		else if (msg->msgid == control_seek_msgid && (internal.caps & caps::Input))
			return _seek(msg->seq);
	}
	if (msg->type != TLL_MESSAGE_DATA)
		return _child->post(msg, flags);

	if (internal.caps & caps::Input)
		return _log.fail(EINVAL, "Can post to input channel");
	if (auto r = _child->post(msg, flags); r)
		return r;
	_seq_last = msg->seq;
	if (_current_empty) {
		if (_seq_first == -1)
			_seq_first = msg->seq;
		auto lock = _files->lock();
		_current_file = _files->files.emplace(_seq_last, Files::File {_last_filename, _seq_last}).first;
		_current_empty = false;
	}
	return 0;
}

int Rotate::_on_active()
{
	if (_state == State::Read && _convert_enable) {
		if (auto scheme = _child->scheme(); scheme) {
			if (auto r = _convert.init(_log, scheme, _scheme.get()); r)
				return _log.fail(r, "Can not initialize converter from the file");
		} else
			_convert.reset();
	}

	if (state() == tll::state::Active)
		return 0;
	if (_state != State::Read && _state != State::Write)
		return 0;

	auto scheme = _child->scheme(TLL_MESSAGE_CONTROL);
	if (scheme) {
		auto message = scheme->lookup("EndOfData");
		if (message)
			_control_eod_msgid = message->msgid;
	}
	return Base::_on_active();
}

int Rotate::_on_closing()
{
	if (_state == State::Closed)
		return Base::_on_closing();
	return 0;
}

int Rotate::_on_closed()
{
	if (_state == State::Closed)
		return Base::_on_closed();
	if (_state != State::Read)
		return 0;
	if (_current_last()) {
		close();
		return 0;
	}
	{
		auto lock = _files->lock();
		_open_cfg.set("filename", (++_current_file)->second.filename);
		_open_cfg.set("seq", conv::to_string(_current_file->first));
	}

	notify();

	if (_state == State::Read) {
		tll_msg_t msg = { .type = TLL_MESSAGE_CONTROL, .msgid = control_rotate_msgid };
		_callback(&msg);
	}
	return 0;
}

int Rotate::_on_data(const tll_msg_t *msg)
{
	if (_state != State::Read)
		return 0;
	_seq_last = msg->seq;
	if (_convert.scheme_from) {
		_log.debug("Try convert");
		if (auto m = _convert.convert(msg); m)
			return Base::_on_data(m);
		return _log.fail(EINVAL, "Failed to convert message {} at {}: {}", msg->msgid, _convert.format_stack(), _convert.error);
	}
	return Base::_on_data(msg);
}

int Rotate::_on_other(const tll_msg_t *msg)
{
	if (msg->type != TLL_MESSAGE_CONTROL)
		return 0;
	if (_state != State::Read)
		return 0;
	_log.trace("Got control message {}, eod {}", msg->msgid, _control_eod_msgid);
	if (_control_eod_msgid && msg->msgid == _control_eod_msgid) {
		// Switch to next
		if (_current_last()) {
			if (!_end_of_data) {
				_end_of_data = true;
				tll_msg_t m = *msg;
				m.msgid = control_eod_msgid;
				_callback(&m);
			}
			if (!_autoclose)
				return 0;
		}
		_child->close();
	}
	return 0;
}

int Rotate::_build_map()
{
	_state = State::Build;

	auto channel = _child.get();
	std::unique_ptr<tll::Channel> read;
	if (internal.caps & caps::Output) {
		auto cfg = _child->config().sub("init");
		if (!cfg)
			return _log.fail(EINVAL, "Can not create reading child channel: child init parameters not available");
		auto url = tll::Channel::Url(*cfg);
		child_url_fill(url, "index");
		url.set("dir", "r");
		url.remove("scheme");
		read = this->context().channel(url);
		if (!read)
			return _log.fail(EINVAL, "Can not create reading child channel");
		channel = read.get();
	}

	std::shared_ptr<Files> files(new Files);
	Files::Map & map = files->files;
	files->seq_first = &_seq_first;
	files->seq_last = &_seq_last;
	std::optional<Files::Map::const_iterator> current = std::nullopt;

	std::error_code ec;
	for (auto & e : std::filesystem::directory_iterator { _directory, ec }) {
		auto filename = e.path().filename();
		_log.debug("Check file {}", filename.string());
		if (filename.stem().stem() != _fileprefix) // File name format: {prefix}.seq.dat
			continue;
		if (filename.extension() != ".dat")
			continue;

		auto path = e.path().string();
		_open_cfg.set("filename", path);
		channel->open(_open_cfg);

		if (channel->state() != tll::state::Active)
			return _log.fail(EINVAL, "Can not open file {}", path);
		auto cfg = channel->config();

		auto reader = tll::make_props_reader(cfg);
		auto first = reader.getT<long long>("info.seq-begin", -1);
		auto last = reader.getT<long long>("info.seq", -1);
		scheme::ConstSchemePtr scheme { channel->scheme()->ref() };
		channel->close();

		if (!reader)
			return _log.fail(EINVAL, "Invalid seq in config: {}", reader.error());

		if (first < 0 || last < 0) {
			if (e.path() == _last_filename) {
				_log.info("Last file without data");
				current = map.end();
				files->scheme = std::move(scheme);
				continue;
			}
			return _log.fail(EINVAL, "File {} has no first/last seq", path);
		}

		_log.debug("File {}: first seq: {}, last seq: {}", path, first, last);
		_seq_last = std::max(last, _seq_last);
		if (_seq_first == -1)
			_seq_first = first;
		_seq_first = std::min(first, _seq_first);

		auto emplace = map.emplace(first, Files::File { path, last });
		if (!emplace.second) {
			auto it = map.find(first);
			return _log.fail(EINVAL, "Duplicate seq {}: files {} and {}", last, it->second.filename, path);
		}

		if (e.path() == _last_filename) {
			current = emplace.first;
			files->scheme = std::move(scheme);
		}

		if (!current && emplace.first == --map.end())
			files->scheme = std::move(scheme);
	}

	if (ec)
		return _log.fail(EINVAL, "Failed to scan directory '{}': {}", _directory, ec.message());

	_current_file = map.end(); // Can not use value_or due to gcc 9.4.0 bug
	if (current)
		_current_file = *current;
	_current_empty = _current_file == map.end();
	_files = files;
	_state = State::Closed;

	return 0;
}

int Rotate::_process(long timeout, int flags)
{
	_update_dcaps(0, dcaps::Process | dcaps::Pending);
	if (_child->state() == state::Closed)
		return _child->open(_open_cfg);
	{
		auto lock = _files->lock();
		if (_seq_last < _current_file->second.last)
			return 0;
		if (_current_file == --_files->files.end())
			return 0;
	}
	_child->close();
	return 0;
}
