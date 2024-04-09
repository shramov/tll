/*
 * Copyright (c)2020-2021 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#include "tll/channel/base.h"
#include "tll/channel/impl.h"
#include "tll/scheme/format.h"
#include "tll/util/memoryview.h"
#include "tll/util/string.h"

std::string msg2hex(const tll_msg_t * msg)
{
	std::string body;
	body.reserve(msg->size * 4 + (msg->size / 16) * 18);

	auto data = std::string_view((const char *) msg->data, msg->size);
	for (auto l = 0u; l < msg->size; l += 16) {
		auto line = data.substr(l, 16);
		body += fmt::format("  {:08x}:  ", l);

		for (auto i = 0u; i < 16; i++) {
			if (line.size() > i)
				body += fmt::format("{:02x} ", (unsigned char) line[i]);
			else
				body += "   ";
			if (i % 4 == 3)
				body.push_back(' ');
		}

		for (auto c : line) {
			if (tll::util::printable(c))
				body.push_back(c);
			else
				body.push_back('.');
		}
		body.push_back('\n');
	}
	return body;
}

tll::Logger _logger_create(const tll_channel_t * c, const char * name)
{
	if (c->internal->version >= 1 && c->internal->logger)
		return { c->internal->logger };
	return { name };
}

int tll_channel_log_msg(const tll_channel_t * c, const char * _log, tll_logger_level_t level, tll_channel_log_msg_format_t format, const tll_msg_t * msg, const char * _text, int tlen)
{
	using namespace tll::channel;

	if (format == log_msg_format::Disable)
		return 0;

	auto text = tll::string_view_from_c(_text, tlen);
	auto log = _logger_create(c, _log);

	std::string addr;
	if (msg->addr.u64)
		addr = fmt::format(", addr: 0x{:x}", msg->addr.u64);

	if (msg->type == TLL_MESSAGE_STATE) {
		log.log(level, "{} message: type: {}, msgid: {}", text, "State", tll_state_str((tll_state_t) msg->msgid));
		return 0;
	} else if (msg->type == TLL_MESSAGE_CHANNEL) {
		if (level <= TLL_LOGGER_INFO)
			level = TLL_LOGGER_TRACE;
		log.log(level, "{} message: type: {}, msgid: {}, seq: {}, size: {}{}",
				text, "Channel", msg->msgid, msg->seq, msg->size, addr);
		return 0;
	} else if (msg->type != TLL_MESSAGE_DATA && msg->type != TLL_MESSAGE_CONTROL) {
		log.log(level, "{} message: type: {}, msgid: {}, seq: {}, size: {}{}",
				text, msg->type, msg->msgid, msg->seq, msg->size, addr);
		return 0;
	}

	auto scheme = tll_channel_scheme(c, msg->type);
	auto message = (scheme && msg->msgid) ? scheme->lookup(msg->msgid) : nullptr;

	if (format == log_msg_format::Auto) {
		if (message)
			format = log_msg_format::Scheme;
		else
			format = log_msg_format::TextHex;
	}

	std::string name;
	if (!scheme)
		name = "(no scheme)";
	else if (!message)
		name = "(no message)";
	else
		name = message->name;
	name = ", name: " + name;
	if (!scheme && format == log_msg_format::Frame)
		name = "";

	std::string header = fmt::format("{} message: type: {}, msgid: {}{}, seq: {}, size: {}{}",
			text, msg->type == TLL_MESSAGE_DATA ? "Data" : "Control", msg->msgid, name, msg->seq, msg->size, addr);

	if (format == log_msg_format::Frame) {
		log.log(level, "{}", header);
		return 0;
	}

	std::string body = "";
	std::string prefix = "  ";
	if (format == log_msg_format::Text) {
		body.reserve(msg->size);

		auto data = std::string_view((const char *) msg->data, msg->size);
		for (auto c : data) {
			if (tll::util::printable(c))
				body.push_back(c);
			else
				body.push_back('.');
		}
	} else if (format == log_msg_format::TextHex) {
		prefix = "";
		body = msg2hex(msg);
	} else if (format == log_msg_format::Scheme) {
		if (!scheme) {
			body = "(no scheme)";
		} else if (!message) {
			level = log.Warning;
			body = "(message not found)";
		} else {
			auto r = tll::scheme::to_strings(message, tll::make_view(*msg));
			if (r) {
				for (auto & i : *r) {
					if (body.size())
						body += prefix;
					body += i + '\n';
				}
			} else {
				auto & e = r.error();
				if (!e.first.size())
					body = fmt::format("Failed to format message {}: {}", message->name, e.second);
				else
					body = fmt::format("Failed to format message {} field {}: {}", message->name, e.first, e.second);
				body += "\n";
				body += msg2hex(msg);
			}
		}
	}

	log.log(level, "{}\n{}{}", header, prefix, body);

	return 0;
}
