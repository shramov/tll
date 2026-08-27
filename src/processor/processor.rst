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
  - definitions of stage objects
  - additional scheme search paths
  - keyring files

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

Worker is a separate thread that runs processor loop on objects that belongs to it. Loop implements
rules of object polling and processing:

 - Object in ``Error`` and ``Closed`` states is not processed.
 - On suspend channel with all children is marked inactive and is not processed until resumed.
 - All channels are divided into two lists: objects with polling capabilities (e.g. with file
   descriptor on Linux) and without. Objects that can not be polled are processed periodically.
 - If object sets ``Pending`` dcap to indicate pending data it is added to special list and is
   processed in next immediate step. Number of pending steps is limited so other channels will get
   some attention too.

Workers are declared implicitly with ``worker`` keyword in object definition, without keyword
``default`` worker is assumed. Worker parameters are defined in ``processor.worker.{name}`` subtree:

  - ``cpu: <list>``, default empty: bind worker thread to the specified list of CPU cores using
    ``sched_setaffinity(2)`` call (list of core indices is converted internally into bitmask).
    Initialization (therefore resource allocation) and processing of all objects in this worker will
    be performed only on this cpuset.
  - ``poll: <bool>``, default ``yes``: if enabled - worker use ``epoll`` (or ``kqueue`` for BSD
    platforms) to wait for objects to become ready for processing. Otherwise spin mode is used, where
    all active objects (with ``Process`` dcap enabled) are processed continuously in the loop.
  - ``poll-interval: <duration>``, default ``100ms``: timeout passed to system polling function, not
    used in spin mode.
  - ``nofd-interval: <duration>``, default ``100ms`` on Linux and ``10ms`` on other platforms:
    interval between processing of objects that do not export a pollable file descriptor. Such
    objects can not be passed to OS polling functions and are thus processed periodically. Not used
    in spin mode.
  - ``time-cache: <bool>``, default ``true``: on each iteration call ``tll_time_now`` and store result
    in TLS variable, so subsequent calls to ``tll_time_now_cached`` return correct value. If disabled
    cached variant behaves like normal function.
  - ``pending-steps: <unsigned>``, default ``8``: after each OS poll function and if list of pending
    objects (with ``Pending`` dcap set) is not empty then next ``pending-steps`` loop steps do not
    call OS poll and only pending objects are processed. Not used in spin mode.

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

Number of channel init parameters (under ``init`` subtree) are handled by processor:
 - ``shutdown-on: {none|error|close}``, default ``none``: shutdown processor if channel fails
   (``error`` or ``close``) or closed (if ``close``).
 - ``open-timeout: <duration>``, default ``300s``: open timeout, close object if it sits in
   ``Opening`` state for too long.
 - ``close-timeout: <duration>``, default ``10s``: close timeout, force close object if it sits in
   ``Closing`` state for too long.
 - ``reopen-active-min: <duration>``, default ``1ms``: if channel lives in ``Active`` state (before
   transitioning to ``Closing`` or ``Error``) for less than specified time then reopen it with
   delay.
 - ``reopen-timeout: <duration>``, default ``1s``: starting reopen interval, on each step it is
   doubled but limited by maximum value (see ``reopen-timeout-max``), reset to starting value when
   channel successfully opened and stays in ``Active`` for ``reopen-active-min`` time.
 - ``reopen-timeout-max: <duration>``, default ``30s``: maximum reopen interval
 - ``tll.processor.active-on-control: <string>``, default empty: wait for control message with
   specified name to consider that object is active and its dependencies should be activated. For
   example for ``stream`` clients ``Online`` can be used to wait for receiving old data.
 - ``tll.processor-verbose: <bool>``, default ``no``: print log message with level info from
   processor context about state changes of this object.

Stages
~~~~~~

Stage is a state of processor graph that becomes active when subset of objects are active.
Proccessor have at least one stage with name ``active`` that is either defined by user or created
implicitly in which case it depends on all leaf nodes. It can be used to determine graph is fully
open. Additional stages can be added in ``stages.**`` subtree, for example ``middle`` that depends
on 3 objects, ``a``, ``b`` and ``c``::

 stages:
   middle: [a, b, c]

Scheme search paths
~~~~~~~~~~~~~~~~~~~

List of values read from the subtree ``processor.scheme-path`` (as a yaml list or mapping) is added
into scheme search path.

Keyring files
~~~~~~~~~~~~~

List of files from the subtree ``processor.keyring`` (as a yaml list or mapping) that are loaded
into context keyring. File format is very simple: plain ``key: value`` lines without any sublevels.

Statistics
----------

Processor exports following stat variables:
 - ``cpu``: human readable CPU load of the process, in percent;
 - ``cpu/ns``: cumulative CPU time (user and system) used by the process, duplicates ``cpu`` metric
   for monitoring systems that should calculate ``(cpu(ts1) - cpu(ts0)) / (ts1 - ts0)``
 - ``mem/b``: total memory used in bytes
 - ``state``: number of state transitions of objects
 - ``error``: number of ``Error`` state transitions of objects

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
