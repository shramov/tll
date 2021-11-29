/*
 * Copyright (c) 2021 Pavel Shramov <shramov@mexmat.net>
 *
 * tll is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 *
 * Command line parser inspired by python argparse and used an idea from Simon Schneegans
 *
 * http://schneegans.github.io/tutorials/2019/08/06/commandline
 */

#ifndef _TLL_UTIL_ARGPARSE_H
#define _TLL_UTIL_ARGPARSE_H

#include <fmt/format.h>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include <tll/util/result.h>

namespace tll::util
{

class ArgumentParser
{
 public:
	using value_type = std::variant<bool *, std::string *, std::vector<std::string> *>;

	ArgumentParser(const ArgumentParser &) = delete;
	ArgumentParser(ArgumentParser &&) = delete;

	explicit ArgumentParser(std::string_view description) : _description(description)
	{
		add_argument({"-h", "--help"}, "display help and exit", &help);
	}

	template <typename T>
	void add_argument(const std::vector<std::string> &flags, std::string_view help, T * value)
	{
		_arguments.emplace_back(argument_type {flags, std::string(help), value});
	}

	std::string format_help() const;
	expected<int, std::string> parse(int argc, char *argv[]) const;

	bool help = false;

 private:
	struct argument_type {
		std::vector<std::string> flags;
		std::string help;
		value_type value;
	};

	std::string _description;
	std::vector<argument_type> _arguments;
};

inline std::string ArgumentParser::format_help() const
{

	std::ostringstream os;

	os << _description << std::endl;

	size_t align = 0;

	for (auto &arg : _arguments) {
		size_t size = 0;
		for (auto &flag : arg.flags)
			size += flag.size() + 2;

		align = std::max(align, size);
	}

	for (auto const &arg : _arguments) {
		std::string flags;
		size_t size = 0;
		os << "  ";
		for (auto &flag : arg.flags) {
			if (size) {
				os << ", ";
				size += 2;
			}
			os << flag;
			size += flag.size();
		}
		for (; size < align; size++)
			os << ' ';
		os << arg.help << std::endl;
	}
	return os.str();
}

inline expected<int, std::string> ArgumentParser::parse(int argc, char *argv[]) const
{
	std::map<std::string_view, const argument_type *> flags;
	std::vector<const argument_type *> positional;
	for (auto &arg : _arguments) {
		for (auto &f : arg.flags) {
			if (f.size() && f[0] == '-')
				flags[f] = &arg;
			else
				positional.push_back(&arg);
		}
	}

	auto posit = positional.begin();

	int i = 1; // argv[0] is program name
	while (i < argc) {
		std::string_view flag(argv[i++]);
		std::optional<std::string_view> value;

		if (flag.size() < 1)
			return error("Flag size < 2: '" + std::string(flag) + "'");

		size_t sep = flag.find('=');

		if (flag == "--")
			return i;

		const argument_type * arg = nullptr;
		if (flag.size() > 1 && flag[0] == '-') { // Normal argument
			if (flag[1] != '-') {
				if (flag.size() > 2) { // Short flag
					value = flag.substr(2);
					flag = flag.substr(0, 2);
				}
			} else if (sep != flag.npos) {
				value = flag.substr(sep + 1);
				flag = flag.substr(0, sep);
			}

			auto it = flags.find(flag);
			if (it == flags.end())
				return error("Invalid flag: '" + std::string(flag) + "'");

			arg = it->second;

			if (!std::holds_alternative<bool *>(arg->value) && !value) {
				if (i >= argc)
					return error("No value for flag '" + std::string(flag) + "'");
				value = argv[i++];
			}

		} else { // Positional argument
			if (posit == positional.end())
				return error("No positional arguments defined for '" + std::string(flag) + "'");
			arg = *posit;
			if (!std::holds_alternative<std::vector<std::string> *>(arg->value))
				posit++;
			value = flag;
		}

		if (std::holds_alternative<bool *>(arg->value)) {
			auto ptr = std::get<bool *>(arg->value);

			if (value)
				*ptr = (*value == "true");
			else
				*ptr = true;
		} else if (std::holds_alternative<std::string *>(arg->value)) {
			*std::get<std::string *>(arg->value) = std::string(*value);
		} else if (std::holds_alternative<std::vector<std::string> *>(arg->value)) {
			std::get<std::vector<std::string> *>(arg->value)->push_back(std::string(*value));
		}
	}
	return i;
}

} // namespace tll::util

#endif // _TLL_UTIL_ARGPARSE_H
