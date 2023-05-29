/*
 * Copyright (c) 2023 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#include "channel/blocks.h"
#include "channel/blocks.scheme.h"

#include <sys/fcntl.h>
#include <unistd.h>

using namespace tll::channel;

TLL_DEFINE_IMPL(Blocks);

int Blocks::_init(const tll::Channel::Url &url, tll::Channel *master)
{
	if ((internal.caps & caps::InOut) == 0) // Defaults to input
		internal.caps |= caps::Input;

	_scheme_control.reset(context().scheme_load(blocks_scheme::scheme_string));
	if (!_scheme_control.get())
		return _log.fail(EINVAL, "Failed to load control scheme");

	if (master) {
		_master = tll::channel_cast<Blocks>(master);
		if (!_master)
			return _log.fail(EINVAL, "Need blocks:// master, got invalid channel {}", master->name());
		if ((internal.caps & caps::InOut) != caps::Input)
			return _log.fail(EINVAL, "Slave channel can be only created in input mode for reading");
		return 0;
	}

	if ((internal.caps & caps::InOut) == caps::InOut)
		return _log.fail(EINVAL, "blocks:// can be either read-only or write-only, need proper dir in parameters");

	auto reader = channel_props_reader(url);
	_default_type = reader.getT<std::string>("default-type", "default");
	if (!reader)
		return _log.fail(EINVAL, "Invalid parameters");

	_filename = url.host();
	if (!_filename.size())
		return _log.fail(EINVAL, "Empty blocks filename");

	return 0;
}

int Blocks::_open(const tll::ConstConfig &cfg)
{
	_seq = -1;

	if (auto r = Base::_open(cfg); r)
		return r;

	if (!_master && !access(_filename.c_str(), R_OK)) {
		_log.info("Load data blocks from {}", _filename);
		auto blocks = Config::load("yaml", _filename);
		if (!blocks)
			return _log.fail(EINVAL, "Failed to load data blocks");
		for (auto [_, c] : blocks->browse("*", true)) {
			auto seq = c.getT<long long>("seq");
			auto type = c.getT<std::string>("type", "default");
			if (!seq)
				return _log.fail(EINVAL, "Failed to load data blocks: invalid seq: {}", seq.error());
			if (!type || type->empty())
				return _log.fail(EINVAL, "Invalid or empty data block type for seq {}", *seq);
			_create_block(*type, *seq, false);
		}

		for (auto & [k, v] : _blocks)
			_log.debug("Loaded {} '{}' blocks", v.size(), k);
	}

	for (auto & [k, v] : _blocks) {
		for (auto & s : v)
			_seq = std::max(_seq, s);
	}

	config_info().set_ptr("seq", &_seq);

	if (internal.caps & caps::Input)
		return _open_input(cfg);

	state(state::Active);
	return 0;
}

int Blocks::_open_input(const tll::ConstConfig &cfg)
{
	auto reader = tll::make_props_reader(cfg);
	auto block = reader.getT<unsigned>("block");
	auto type = reader.getT<std::string>("block-type", _default_type);
	if (!reader)
		return _log.fail(EINVAL, "Invalid open parameters: {}", reader.error());

	auto & blocks = _master ? _master->_blocks : _blocks;

	auto it = blocks.find(type);
	if (it == blocks.end())
		return _log.fail(EINVAL, "Unknown block type '{}'", type);
	ssize_t size = it->second.size();
	if (size == 0)
		return _log.fail(EINVAL, "No known blocks of type '{}'", type);
	if (block >= size)
		return _log.fail(EINVAL, "Requested block '{}' too large: {} > max {}", type, block, size - 1);
	auto i = it->second.rbegin();
	for (; i != it->second.rend() && block; i++)
		block--;
	_log.info("Translated block type '{}' number {} to seq {}", type, block, *i);

	std::array<char, blocks_scheme::BlockRange::meta_size()> buf;
	auto data = blocks_scheme::BlockRange::bind(buf);
	data.set_begin(*i + 1);
	data.set_end(*i + 1);

	tll_msg_t msg = { TLL_MESSAGE_CONTROL };
	msg.msgid = data.meta_id();
	msg.size = data.view().size();
	msg.data = data.view().data();
	msg.seq = *i + 1;
	_callback(&msg);

	return close();
}

int Blocks::_close()
{
	config_info().setT("seq", _seq);
	return Base::_close();
}

int Blocks::_post(const tll_msg_t *msg, int flags)
{
	if (msg->type == TLL_MESSAGE_DATA) {
		_seq = msg->seq;
		return 0;
	} else if (msg->type != TLL_MESSAGE_CONTROL)
		return 0;

	if (msg->msgid != blocks_scheme::Block::meta_id())
		return _log.fail(EINVAL, "Invalid control message {}", msg->msgid);
	if (_seq < 0)
		return _log.fail(EINVAL, "Failed to make block: no data in storage", _seq);

	auto data = blocks_scheme::Block::bind(*msg);
	if (data.meta_size() > msg->size)
		return _log.fail(EMSGSIZE, "Invalid Blocks message: size {} < min size {}", msg->size, data.meta_size());

	auto block = data.get_type();
	if (block.size() == 0) {
		if (_default_type.empty())
			return _log.fail(EINVAL, "Empty block name");
		block = _default_type;
	}

	return _create_block(block, _seq, true);
}

int Blocks::_create_block(std::string_view block, long long seq, bool store)
{
	_log.debug("Create block {} at {}", block, seq);
	auto it = _blocks.find(block);
	if (it == _blocks.end())
		it = _blocks.emplace(block, std::list<long long>{}).first;
	it->second.push_back(seq);

	if (store) {
		_log.info("Store seq type {} at {}", block, seq);

		auto s = fmt::format("- {{seq: {}, type: '{}'}}\n", seq, block);
		auto fd = ::open(_filename.c_str(), O_WRONLY | O_APPEND | O_CREAT, 0644);
		if (fd == -1)
			return _log.fail(EINVAL, "Failed to open data block file '{}': {}", _filename, strerror(errno));
		if (write(fd, s.data(), s.size()) != (ssize_t) s.size())
			return _log.fail(EINVAL, "Failed to write data block file '{}': {}", _filename, strerror(errno));
		::close(fd);
	}
	return 0;
}
