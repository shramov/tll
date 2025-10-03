/*
 * Copyright (c) 2018-2021 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#include "tll/config.h"

#include "tll/logger.h"
#include "tll/util/bin2ascii.h"
#include "tll/util/conv.h"
#include "tll/util/result.h"

#include <errno.h>
#include <stdio.h>
#include <yaml.h>

#include <memory>
#include <optional>
#include <variant>

using tll::Config;

namespace {
struct state_t
{
	Config cfg;
	tll::Logger _log = {"tll.config.yaml.state"};

	using variant_t = std::variant<bool, size_t, std::string>;
	struct Frame {
		Config cfg;
		variant_t key;
		std::string path;
	};
	std::list<Frame> stack;
	variant_t key;

	std::map<std::string, Config, std::less<>> anchors;
	std::map<Config, bool> checked;

	int parse(const yaml_event_t &event);

	std::string key_full(std::string_view suffix)
	{
		std::string full;
		for (auto &i : stack) {
			if (full.empty())
				full = i.path;
			else
				full += "." + i.path;
		}
		if (suffix.size()) {
			if (full.empty())
				full = suffix;
			else
				full += "." + std::string(suffix);
		}
		return full;
	}

	std::string key_string()
	{
		if (std::holds_alternative<std::string>(key)) {
			std::string s = std::move(std::get<std::string>(key));
			key = false;
			return s;
		} else if (std::holds_alternative<size_t>(key)) {
			auto & idx = std::get<size_t>(key);
			char buf[64];
			snprintf(buf, sizeof(buf), "%04zd", idx++);
			return std::string(buf);
		} else
			return "";
	}

	void anchor(const yaml_char_t * name, Config cfg, std::string_view kind)
	{
		if (!name) return;
		_log.trace("New {} anchor {}", kind, (const char *) name);
		anchors[(const char *) name] = cfg;
	}

	std::optional<Config> anchor(std::string_view name)
	{
		auto it = anchors.find(name);
		if (it == anchors.end()) return std::nullopt;
		return it->second;
	}
};

int state_t::parse(const yaml_event_t &event)
{
	switch (event.type) {
	case YAML_STREAM_START_EVENT:
	case YAML_STREAM_END_EVENT:
	case YAML_DOCUMENT_START_EVENT:
	case YAML_DOCUMENT_END_EVENT:
	case YAML_NO_EVENT:
		return 0;

	case YAML_ALIAS_EVENT: {
		std::string_view name = (const char *) event.data.alias.anchor;
		auto alias = anchor(name);
		if (!alias)
			return _log.fail(ENOENT, "Alias {} not found", name);
		if (std::holds_alternative<bool>(key))
			return _log.fail(EINVAL, "Got alias event in invalid context");
		auto k = key_string();
		_log.trace("Alias: {} to {}", k, name);
		cfg.set(k, alias->copy());
		return 0;
	}

	case YAML_MAPPING_END_EVENT:
	case YAML_SEQUENCE_END_EVENT:
		if (stack.empty()) return 0; // Last mapping
		key = stack.back().key;
		cfg = stack.back().cfg;
		stack.pop_back();
		break;

	case YAML_MAPPING_START_EVENT:
	case YAML_SEQUENCE_START_EVENT: {
		auto c = cfg;
		std::string k;
		if (!std::holds_alternative<bool>(key)) {
			k = key_string();
			auto sub = cfg.sub(k, true);
			if (!sub)
				return _log.fail(EINVAL, "Failed to build path {}", k);
			c = *sub;
		}
		stack.emplace_back(Frame {cfg, std::move(key), k});
		cfg = c;
		if (event.type == YAML_SEQUENCE_START_EVENT) {
			anchor(event.data.mapping_start.anchor, cfg, "sequence");
			key = (size_t) 0;
		} else {
			anchor(event.data.sequence_start.anchor, cfg, "mapping");
			key = false;
		}
		break;
	}

	case YAML_SCALAR_EVENT: {
		std::string_view value = {(const char *) event.data.scalar.value, event.data.scalar.length};
		if (!std::holds_alternative<bool>(key)) {
			auto k = key_string();
			bool created = !cfg.sub(k, false);
			auto sub = cfg.sub(k, true);
			if (sub->value())
				return _log.fail(EINVAL, "Failed to set value {}: duplicate entry", k);
			if (created) {
				auto index = 0;
				for (auto p = sub->parent(); p; p = p->parent()) {
					index++;
					if (p->value()) {
						auto full = key_full(k);
						auto split = tll::split<'.'>(full);
						auto it = split.end();
						it--;
						for (; index; index--)
							it--;
						auto path = std::string_view(it.data_begin, it.end - it.data_begin);
						const auto & mark = event.start_mark;
						_log.error("Parent '{}' with value conflicts with new node '{}' at line {}", path, full, mark.line + 1);
						break;
					}
				}
			} else if (auto children = sub->list(); !children.empty()) {
				std::string names;
				for (auto &[c, _] : children) {
					if (names.empty())
						names = c;
					else
						names += ", " + c;
				}
				auto full = key_full(k);
				const auto & mark = event.start_mark;
				_log.error("Conflicting value at '{}', node has children [{}] at line {}", full, names, mark.line + 1);
			}
			if (event.data.scalar.tag) {
				std::string_view tag((const char *) event.data.scalar.tag);
				if (tag == "tag:yaml.org,2002:binary") {
					auto r = tll::util::b64_decode(value);
					if (!r)
						return _log.fail(EINVAL, "Invalid binary data for {}: {}", k, r.error());
					if (cfg.set(k, {r->data(), r->size()}))
						return _log.fail(EINVAL, "Failed to set value {}: {}", k, value);
				} else if (tag == "!link") {
					_log.trace("Link {} to {}", k, value);
					if (cfg.link(k, value))
						return _log.fail(EINVAL, "Failed to set link {}: {}", k, value);
				} else
					return _log.fail(EINVAL, "Unknown tag {}: '{}'", k, tag);
			} else if (cfg.set(k, value))
				return _log.fail(EINVAL, "Failed to set value {}: {}", k, value);
			anchor(event.data.scalar.anchor, *cfg.sub(k), "scalar");
		} else
			key = std::string(value);
		break;
	}
	}
	return 0;
}

}

namespace {
tll_config_t * yaml_parse(tll::Logger &_log, yaml_parser_t * parser)
{
	yaml_event_t event;
	state_t state = {};

	do {
		if (!yaml_parser_parse(parser, &event))
			return _log.fail(nullptr, "Failed to parse YAML at {}:{}: {} {}", parser->problem_mark.line + 1, parser->problem_mark.column + 1, parser->problem ? parser->problem : "null", parser->context ? parser->context : "null");
		std::unique_ptr<yaml_event_t, decltype(&yaml_event_delete)> _event = { &event, yaml_event_delete };
		if (event.type == YAML_STREAM_END_EVENT) break;
		if (state.parse(event))
			return _log.fail(nullptr, "Failed to parse event");
	} while (true);

	tll_config_ref(state.cfg);
	return state.cfg;
}
}

tll_config_t * yaml_load(std::string_view filename)
{
	tll::Logger _log = {"tll.config.yaml"};
	std::string fn(filename);
	auto fp = fopen(fn.c_str(), "r");
	if (!fp)
		return _log.fail(nullptr, "Failed to open file '{}': {}", filename, strerror(errno));
	std::unique_ptr<FILE, int (*)(FILE *)> _fp = { fp, fclose };
	yaml_parser_t parser;
	if (!yaml_parser_initialize(&parser))
		return _log.fail(nullptr, "Failed to init yaml parser");
	std::unique_ptr<yaml_parser_t, decltype(&yaml_parser_delete)> _parser = { &parser, yaml_parser_delete };
	yaml_parser_set_input_file(&parser, fp);

	return yaml_parse(_log, &parser);
}

tll_config_t * yaml_load_data(std::string_view data)
{
	if (data.empty())
		return tll_config_new();
	tll::Logger _log = {"tll.config.yaml"};
	yaml_parser_t parser;
	if (!yaml_parser_initialize(&parser))
		return _log.fail(nullptr, "Failed to init yaml parser");
	std::unique_ptr<yaml_parser_t, decltype(&yaml_parser_delete)> _parser = { &parser, yaml_parser_delete };
	yaml_parser_set_input_string(&parser, (const unsigned char *) data.data(), data.size());

	return yaml_parse(_log, &parser);
}
