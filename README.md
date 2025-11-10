# TLL

TLL is a cross-language framework for building fast data processing services. It consists of several
parts:

 * object abstraction with very low overhead (several nanoseconds) that is suitable both for IO and
   data processing;
 * runner that can execute object graph from text configuration for unified service deployment;
 * set of lighweight APIs, like logging, configuration parsing and statistics gathering;
 * number of basic network (TCP, UDP), local (shared memory), transform (JSON encoding, LZ4
   compression) or storage (file) channel implementations included in the core;
 * bindings for different programming languages like C++ (header-only over C API), Python,
   [Rust](https://github.com/shramov/tll-rs), [Go](https://github.com/shramov/tll-go) and
   [Lua](https://github.com/shramov/tll-lua).

## Good points

 * scalability: from small standalone applications to complex systems consisting of lots of
   different services;
 * easy testing: isolate components in unit tests or build complex integration tests by feeding them
   with prepared data;
 * language support: components written in Python, C++, Rust or Lua work with each other without
   issues;
 * small minimal footprint (when stripped) and limited number of external dependencies for core.
 * permissive license: covered by MIT license that imposes very little restrictions on how TLL can
   be used;

## Why should I use it?

 * Compatibility layer: it is common issue that libraries used for communication have to be changed
   during development or when system is already deployed, TLL simplifies such transitions with
   relatively low overhead.
 * Easy prototyping: build proof of concept version in Python and then iteratively rewrite parts in
   other languages for better performance. Communication between modules is defined by data scheme
   so, with good test coverage, replacing them one by one is not a problem.

## Getting Started

TLL is supported on Linux, FreeBSD and macOS.

### Packages

Packages for recent Debian and Ubuntu LTS releases are available in the <https://psha.org.ru/debian/>
repository.

### Building

List of actual dependencies is listed in `debian/control` file in `Build-Depends` field that can be
copied and pasted to `apt satisfy` command. For non-debian systems package names can differ (for
example standard suffix for RHEL/CentOS is `-devel` instead of `-dev`). Compilation of TLL core is
straightforward:

```
meson setup build
ninja -vC build
```

To build Python extensions path to the compiled library should be specified:

```
cd python
LDFLAGS=-L`pwd`/../build/ python3 setup.py build
```

It is possible to build extensions with Meson, in this case `-Dwith_cython_build=true` parameter
have to be passed to `meson setup`. However module files are created in build directory and should
be symlinked into `python/tll/` subtree.

For debian based systems easier way is to call `dpkg-buildpackage -uc -b -rfakeroot` and then
install result using `dpkg -i`.
