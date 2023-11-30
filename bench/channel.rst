tll-channel-bench
=================

:Manual Section: 3
:Manual Group: TLL
:Subtitle: Run basic benchmarks for list of TLL channels

Synopsis
--------

``tll-channel-bench [--config CONFIG] [-m module] [--count COUNT] [--msgid ID] [--msgsize SIZE] URLS...``


Description
-----------

Measure time needed to post a message and (optionaly) processing time on a list of
channels. Default message has zeroed body but user can provide it's own with ``--payload`` option.

Options
-------

``URLS...`` list of channel urls to benchmark. If nothing is specified - measure TLL overhead for
post/callback functions using set of pirmitive channels:

  - ``null://`` measure post overhead with one function pointer call;
  - ``prefix+null://`` prefix overhead with two function pointer calls;
  - ``echo://`` posted message is echoed back, same as ``null://`` but with callback call;
  - ``prefix+echo://`` measure both paths of prefix channel, both posts and callbacks;

``-m | --module MODULE`` load additional channel module, can be specified multiple times.

``-C | --count COUNT`` run ``COUNT`` number of iterations, by default 10000000.

``--process`` benchmark ``tll::Channel::process`` function.

``--callback`` run tests with data callback added to channels.

``--msgid ID`` use ``ID`` for messages that are posted in benchmark.

``--msgsize SIZE`` size of message used in benchmark, defaults to 1024. Ignored if message body is loaded using
``--payload`` option.

``--payload URL`` read first message from channel ``URL`` and use it for benchmark. ``--msgid``
option can be used to override message id got from this channel.

``--config CONFIG``:  read options from YAML config file, command line has precedence over config
parameters. Supported variables are following:

  - ``msgid``: message id, see ``--msgid`` option.
  - ``msgsize``: message size, see ``--msgsize`` option.
  - ``module``: list of modules to load in format similar to processor config: ``module: [{module: tll-http}, {module: tll-lua}]``
  - ``channel``: channels to measure, concatenated with command line list. Configuration format of
    each channel is same as in processor:

    * scalar url string value: ``null://;stat=yes``
    * broken down parameters url: ``{tll.proto: null, stat: yes}``
    * mixed variant: ``{url: null://, stat: yes}``

  - ``payload``: channel that provides message body for benchmark (see ``--payload`` option), for
    configuration format see ``channel`` key.

Examples
--------

Measure relative overhead of statistic gathering

::

    tll-bench-channel 'null://' 'null://;stat=yes'

Read message body from yaml file and benchmark some lua code two times (to demonstrate syntax),
configuration file that should be used with ``--config`` option:


::

  module:
    - { module: tll-lua }
  payload: yaml://payload.yaml;scheme=yaml://scheme.yaml
  channel:
    - lua+null://;lua.code=file://code.lua;scheme=yaml://scheme.yaml
    - { url: lua+null://, lua.code: file://code.lua, scheme=yaml://scheme.yaml}

..
    vim: sts=4 sw=4 et tw=100
