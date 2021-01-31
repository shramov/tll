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

const tll::scheme::Message * lookup(const tll::Scheme * s, int msgid)
{
	if (!s) return nullptr;
	for (auto m = s->messages; m; m = m->next) {
		if (m->msgid == msgid)
			return m;
	}
	return nullptr;
}

int tll_channel_log_msg(const tll_channel_t * c, const char * _log, tll_logger_level_t level, tll_channel_log_msg_format_t format, const tll_msg_t * msg, const char * _text, int tlen)
{
	using namespace tll::channel;

	if (format == log_msg_format::Disable)
		return 0;

	auto text = tll::string_view_from_c(_text, tlen);
	tll::Logger log(_log);

	if (msg->type == TLL_MESSAGE_STATE) {
		log.log(level, "{} message: type: {}, msgid: {}", text, "State", tll_state_str((tll_state_t) msg->msgid));
		return 0;
	} else if (msg->type == TLL_MESSAGE_CHANNEL) {
		log.log(level, "{} message: type: {}, msgid: {}, seq: {}, size: {}, addr: {:016x}",
				text, "Channel", msg->msgid, msg->seq, msg->size, msg->addr.u64);
		return 0;
	} else if (msg->type != TLL_MESSAGE_DATA && msg->type != TLL_MESSAGE_CONTROL) {
		log.log(level, "{} message: type: {}, msgid: {}, seq: {}, size: {}, addr: {:016x}",
				text, msg->type, msg->msgid, msg->seq, msg->size, msg->addr.u64);
		return 0;
	}

	auto scheme = tll_channel_scheme(c, msg->type);
	auto message = lookup(scheme, msg->msgid);

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

	std::string header = fmt::format("{} message: type: {}, msgid: {}{}, seq: {}, size: {}, addr: {:016x}",
			text, msg->type == TLL_MESSAGE_DATA ? "Data" : "Control", msg->msgid, name, msg->seq, msg->size, msg->addr.u64);

	if (format == log_msg_format::Frame) {
		log.log(level, "{}", header);
		return 0;
	}

	std::string body = "";
	const std::string prefix = "  ";
	if (format == log_msg_format::Text || format == log_msg_format::TextHex) {
		body.reserve(msg->size);

		auto data = (const char *) msg->data; 
		for (auto c = data; c != data + msg->size; c++) {
			if (tll::util::printable(*c))
				body.push_back(*c);
			else
				body.push_back('.');
		}
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
			}
		}
	}

	log.log(level, "{}\n{}{}", header, prefix, body);

	return 0;
}
