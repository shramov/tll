tll-processor
=============

:Manual Section: 1
:Manual Group: TLL
:Subtitle: Run TLL channel graph from config file

Synopsis
--------

``tll-processor CONFIG [-Dkey=value] [-Dkey=value...]``

Description
-----------

Processor initializes object graph from configuration and runs it in one or more separate worker
threads (as defined in config).

Options
-------

``CONFIG`` config file.

``-Dkey=value`` override parameter in config file, can be specified multiple times.

Configuration format
--------------------

Config file have following parts:

  - logger configuration
  - list of modules with channel implementation
  - default values for channel parameters
  - worker settings: polling mode, affinity and others
  - list of objects to create and process

Logger configuration
~~~~~~~~~~~~~~~~~~~~

Logger uses configuration subtree ``logger`` that is passed to ``tll_logger_config`` function, all
following keys are placed in it. Backend type is defined by ``type`` key:

 - ``stdio``: print messages to stderr;
 - ``spdlog``: use ``spdlog`` logging library, additional configuration is defined in
   ``spdlog`` subtree;

Levels are located in ``levels`` subtree and are specified either in simple ``prefix: LEVEL`` form
or in form of ``{name: prefix, level: LEVEL}`` dict (this variant should be used when key conflicts
with child nodes, for example ``levels: {tll: INFO, tll.channel: DEBUG}`` is not valid TLL config).

.. code:: yaml

  logger:
    levels:
      tll._: {name: tll, level: INFO}
      tll.processor.context: DEBUG
      tll.channel.stat: INFO

List of modules
~~~~~~~~~~~~~~~

Modules are defined in ``processor.module`` subtree as a list (or dict) of blocks with following
variables:

  - ``module: <string>`` name of shared object (without ``lib`` prefix and ``.so`` suffix). If ``/``
    symbol is present in the string then it is split into path and object name parts. If path is
    non-empty then library is loaded from exact path of form ``{path}/lib{name}.so``. On macOS
    suffix is ``.dylib``.
  - ``config: <subtree>`` passed to module init function.

When module list is contained in one file list syntax can be used::

  processor.module:
    - module: tll-logic-forward
    - module: tll-logic-stat

But when they are defined in several files it is better to use dict syntax::

  processor.module:
    forward: {module: tll-logic-forward}
    stat: {module: tll-logic-stat}

Default values
~~~~~~~~~~~~~~

Subtree ``processor.defaults`` is passed to TLL context where all objects (including processor
itself) will be created.

Worker settings
~~~~~~~~~~~~~~~

Workers are declared implicitly with ``worker`` keyword in object definition, without keyword
``default`` worker is assumed. Worker parameters are defined in ``processor.worker.{name}`` subtree:

  - ``poll: <bool>``, default ``yes``: if enabled - worker use ``epoll`` (or ``kqueue`` for BSD
    platforms) to wait for objects to become ready for processing. Otherwise spin mode is used, where
    all active objects (with ``Process`` dcap enabled) are processed continuously in the loop.
  - ``poll-interval: <duration>``, default ``100us``: timeout passed to system polling function, not
    used in spin mode.
  - ``nofd-interval: <duration>``, default ``100ms``: interval between processing of objects that do
    not export a pollable file descriptor. Such objects can not be passed to OS polling functions and
    thus processed periodically. Not used in spin mode.
  - ``time-cache: <bool>``, default ``true``: on each iteration call ``tll_time_now`` and store result
    in TLS variable, so subsequent calls to ``tll_time_now_cached`` return correct value. If disabled
    cached variant behaves like normal function.

List of objects
~~~~~~~~~~~~~~~

List of objects created by processor are defined in ``processor.objects`` subtree. Each key defines
object with that name with following values in subtree:

  - ``init: <subtree>``: init parameters for channel, either in a string form or as a subtree. ``name``
    parameter is set to object name. If there is no ``fd`` parameter and polling is disabled on the
    worker then ``fd`` parameter is set to ``no``.
  - ``open: <subtree>``: open parameters for channel, passed to ``tll_channel_open`` call.
  - ``worker: <string>``: name of the worker on which this channel would be processed. By default
    all channels are in ``default`` worker.
  - ``depends: <list-of-names>``: comma separated list of object dependencies. If object ``A``
    depends on object ``B`` then it can be activated if and only if ``B`` is active. If ``B`` is
    closed then ``A`` is deactivated too.
  - ``channels: <subtree>``: ``channels.{name}: {value}`` is added to init parameters as
    ``tll.channel.{name}: {value}`` and later is used by logic channels.
  - ``disable: <bool>`` - disable this object and do not parse any parameters.

Examples
--------

Create TCP server and send back everything received from the client::

  processor.module:
    - module: tll-logic-forward

  processor.objects:
    tcp:
      init: tcp://*:8080;mode=server;dump=text;frame=none
      depends: echo
    echo:
      init: forward://
      channels: {input: tcp, output: tcp}

``tcp`` channel declares dependency on ``echo`` forwarding logic and it is opened only after
``echo`` becomes active. Without this dependency ``tcp`` can become active before ``echo`` is ready.
See also
--------

``tll-channel-common(7)``
