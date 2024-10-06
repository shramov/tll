Logger
======

Motivation for building logging subsystem is to have one place where logs from different components (that may
be implemented in different languages) can be handled.

It's design resembles Python ``logging`` or any other hierarcical system - each ``Logger`` object has it's name,
(for example ``tll.channel.stat``) and current level (for example ``Info``). Messages with level less then current
are ignored (for example ``Debug`` will be discarded). Level of ``Logger`` object can be changed during program
execution.

Since there are many logging libraries in the wild there is no reason to write yet another one so only
primitive stderr printing backend is implemented. Actual logging is handled either by spdlog_ library or by
user provided callback. For example in Python scripts it's good idea to use native python logging subsystem.

C++ API
-------

Non-macro logging API similar to Python logging.

Uses fmt_ for formatting.

.. code:: c

  tll::Logger log("a.b.c");
  log.info("Float: {.2f}", 1.1);

Configuration
-------------

When new ``Logger`` is created its level is derived from current configuration
by searching for longest prefix of its name (exact match is a longest one that can be found).

For example for name ``aaa.bbb.ccc`` following keys will have descending priority:

 - ``aaa.bbb.ccc``
 - ``aaa.bbb.c*``
 - ``aaa.bbb``
 - ``aaa.b*``

Logging levels can be set one by one or from ``Config`` object in a single call.

Levels are located in ``levels`` subtree and are specified either in simple
``prefix: LEVEL`` form or in form of ``{name: prefix, level: LEVEL}`` subtree.

.. code:: yaml

  levels:
    tll._: {name: tll, level: INFO}
    tll.processor.context: DEBUG
    tll.channel.stat: INFO

``type`` key controls backend type, currently only ``spdlog`` is supported.

spdlog
------

Backend is using only sinks from spdlog_ library. For each logging prefix set of sinks
with own levels and formats can be defined.

Each sink has following common parameters:

 * ``type``: type of sink, see list below;
 * ``level=debug``: ignore messages with level less then this parameter;
 * ``flush-level=info``: flush sink when message with this or higher level is logged;
 * ``format``: specify message format for this particular sink object;
 * ``prefix``: only log messages with logger name matching prefix are affected;
 * ``additivity=true``: propogate message to parent set of sinks;

Implemented sinks and possible parameters (with default values for optional ones):

 * ``console``: no parameters;
 * ``file``: ``filename``, optional ``truncate=false``;
 * ``rotating-file``: ``filename``, optional ``max-files=5``, ``max-size=64mb`` and ``rotate-on-open=false``;
 * ``daily-file``: ``filename``, optional ``max-files=5``, ``rotate-hour=0``, ``rotate-minute=0`` and ``truncate=false``;
 * ``syslog``: optional ``ident=``

All configuration is located in ``spdlog`` subtree. Default values for sink types can be provided in ``spdlog.defaults.{type}`` block.

.. code:: yaml

  type: spdlog
  spdlog:
    format: "%^%Y-%m-%d %H:%M:%S.%e %l %n%$: %v" # Default format
    sinks:
      - {type: console, level: debug}
      - {type: syslog, level: critical}
      - {prefix: tll.channel.stat, type: file, level: debug, filename: stat.log, additivity: false}
    defaults:
      rotating-file: {max-files: 10, max-size: 10mb}

For exact meaning of parameters see spdlog-sinks_ documentation of sinks.

.. _fmt: http://fmtlib.net/
.. _spdlog: https://github.com/gabime/spdlog
.. _spdlog-sinks: https://github.com/gabime/spdlog/wiki/4.-Sinks
