#pragma once

#include <string_view>
#include "tll/scheme/types.h"

namespace curl_scheme {

static constexpr std::string_view scheme = R"(yamls://
- name:
  enums:
    method_t: { type: int8, enum: { UNDEFINED: -1, GET: 0, HEAD: 1, POST: 2, PUT: 3, DELETE: 4, CONNECT: 5, OPTIONS: 6, TRACE: 7, PATCH: 8 } }

- name: header
  fields:
    - { name: header, type: string }
    - { name: value, type: string }

- name: connect
  id: 1
  fields:
    - { name: method, type: method_t }
    - { name: code, type: int16 }
    - { name: size, type: int64 }
    - { name: path, type: string }
    - { name: headers, type: '*header' }

- name: disconnect
  id: 2
  fields:
    - { name: code, type: int16 }
    - { name: error, type: string }
)";

enum class method_t : int8_t { UNDEFINED = -1, GET = 0, HEAD = 1, POST = 2, PUT = 3, DELETE = 4, CONNECT = 5, OPTIONS = 6, TRACE = 7, PATCH = 8 };

struct __attribute__((packed)) header {
	tll::scheme::offset_ptr_t<char> header;
	tll::scheme::offset_ptr_t<char> value;
};

struct __attribute__((packed)) connect {
	static constexpr int id = 1;
	method_t method;
	int16_t code;
	int64_t size;
	tll::scheme::offset_ptr_t<char> path;
	tll::scheme::offset_ptr_t<header> headers;
};

struct __attribute__((packed)) disconnect {
	static constexpr int id = 2;
	int16_t code;
	tll::scheme::offset_ptr_t<char> error;
};

}
