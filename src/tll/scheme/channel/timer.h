#pragma once

#include <chrono>
#include <string_view>

namespace timer_scheme {
static constexpr std::string_view scheme_relative = R"(yamls://
- name: relative
  id: 1
  fields: [{name: ts, type: int64, options.type: duration, options.resolution: ns}]
)";

static constexpr std::string_view scheme_absolute = R"(yamls://
- name: relative
  id: 1
  fields: [{name: ts, type: int64, options.type: duration, options.resolution: ns}]
- name: absolute
  id: 2
  fields: [{name: ts, type: int64, options.type: time_point, options.resolution: ns}]
)";

struct relative {
	static constexpr int id = 1;
	std::chrono::duration<int64_t, std::nano> ts;
};

struct absolute {
	static constexpr int id = 2;
	std::chrono::time_point<std::chrono::system_clock, std::chrono::duration<int64_t, std::nano>> ts;
};
}
