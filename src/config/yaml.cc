/*
 * Copyright (c) 2018-2021 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#include "tll/config.h"

#include "tll/logger.h"
#include "tll/util/conv.h"
#include "tll/util/result.h"

#include <errno.h>
#include <stdio.h>
#include <yaml.h>

#include <optional>
#include <variant>

using tll::Config;

namespace {
struct state_t
{
	Config cfg;
	tll::Logger _log = {"tll.config.yaml.state"};

	using variant_t = std::variant<bool, size_t, std::string>;
	std::list<std::pair<Config, variant_t>> stack;
	variant_t key;

	std::map<std::string, Config, std::less<>> anchors;

	int parse(const yaml_event_t &event);

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

	std::optional<Config> anchor(const yaml_char_t * name)
	{
		auto it = anchors.find((const char *) name);
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
		auto alias = anchor(event.data.alias.anchor);
		if (!alias)
			return _log.fail(ENOENT, "Alias {} not found", event.data.alias.anchor);
		if (std::holds_alternative<bool>(key))
			return _log.fail(EINVAL, "Got alias event in invalid context");
		auto k = key_string();
		_log.trace("Alias: {} to {}", k, event.data.alias.anchor);
		cfg.set(k, alias->copy());
		return 0;
	}

	case YAML_MAPPING_END_EVENT:
	case YAML_SEQUENCE_END_EVENT:
		if (stack.empty()) return 0; // Last mapping
		key = stack.back().second;
		cfg = stack.back().first;
		stack.pop_back();
		break;

	case YAML_MAPPING_START_EVENT:
	case YAML_SEQUENCE_START_EVENT: {
		auto c = cfg;
		if (!std::holds_alternative<bool>(key)) {
			auto k = key_string();
			auto sub = cfg.sub(k, true);
			if (!sub)
				return _log.fail(EINVAL, "Failed to build path {}", k);
			c = *sub;
		}
		stack.emplace_back(cfg, std::move(key));
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
			if (cfg.has(k))
				return _log.fail(EINVAL, "Failed to set value {}: duplicate entry", k);
			if (event.data.scalar.tag) {
				std::string_view tag((const char *) event.data.scalar.tag);
				if (tag != "!link")
					return _log.fail(EINVAL, "Unknown tag {}: '{}'", k, tag);
				_log.info("Link {} to {}", k, value);
				if (cfg.link(k, value))
					return _log.fail(EINVAL, "Failed to set link {}: {}", k, value);
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
	std::unique_ptr<FILE, decltype(&fclose)> _fp = { fp, fclose };
	yaml_parser_t parser;
	if (!yaml_parser_initialize(&parser))
		return _log.fail(nullptr, "Failed to init yaml parser");
	std::unique_ptr<yaml_parser_t, decltype(&yaml_parser_delete)> _parser = { &parser, yaml_parser_delete };
	yaml_parser_set_input_file(&parser, fp);

	return yaml_parse(_log, &parser);
}

tll_config_t * yaml_load_data(std::string_view data)
{
	tll::Logger _log = {"tll.config.yaml"};
	yaml_parser_t parser;
	if (!yaml_parser_initialize(&parser))
		return _log.fail(nullptr, "Failed to init yaml parser");
	std::unique_ptr<yaml_parser_t, decltype(&yaml_parser_delete)> _parser = { &parser, yaml_parser_delete };
	yaml_parser_set_input_string(&parser, (const unsigned char *) data.data(), data.size());

	return yaml_parse(_log, &parser);
}
