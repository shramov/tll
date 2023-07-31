Processor
=========

Processor replaces ``main`` for user programs. It reads configuration, initializes objects, opens
them in order defined in config and runs processing loops. With processor user needs only channel
implementations and configuration file describing relations between them.

Initialization
--------------

During initialization processor builds directed acyclic graph of objects that depends on each other.
When channel ``A`` depends on ``B`` it means that ``A`` needs ``B`` for it's proper operation and
``B`` has to be opened before ``A``. If ``B`` fails then ``A`` also needs to be closed. For example
``ipc://`` server part has to be opened first to allow ``ipc://`` clients to connect.

Basic processor config for echo service looks like

.. code:: yaml

    processor.objects:
      tcp:
        url: 'tcp://*:8080;mode=server'
        depends: logic
      logic:
        url: 'forward://'
        type: logic
        channels: {input: tcp, output: tcp}

In this case ``tcp`` object is declared first but depends on ``logic`` and is is opened after it.
So incoming connections are only accepted when ``logic`` is fully functional.

If this configuration format is not suitable for user it's possible to create alternative dialects.
Processor is implemented as a TLL channel and created with its config file as ``tll_config_t``
object so configuration preprocessors can be added as prefix channels.

For example ``chains`` dialect can be used to declare dependencies in alternative way. Same
configuration would look like

.. code:: yaml

    processor.chain:
      root:
        - name: logic
          objects:
            logic:
              url: 'forward://'
              type: logic
              channels: {input: tcp, output: tcp}
        - name: input
          objects:
            tcp:
              url: 'tcp://*:8080;mode=server'

Dependency between ``tcp`` and ``logic`` is defined by order of levels in a chain. Level ``logic``
with ``logic`` object precedes ``input`` level with ``tcp`` object so every object in subsequent
level depends on previous one.

``root`` is a predefined name for root chain but additional chains can be defined for parallel
dependency paths. Chains can be spawned at any level and then joined back.

.. code:: yaml

    processor.chain:
      root:
        - name: spawn-a
          spawn: a
        - name: spawn-b
          spawn: b
        - name: join-a
          join: a
      a:
        - name: input
          objects:
            obj: {url: 'null://'}
        - name: join-b
          join: b
      b:
        - name: input
          objects:
            obj-b: {url: 'null://'}

State Changes
-------------

Loop
----

Channels can have arbitrary number of child objects organized in a tree so processing even one
object needs processing of it's childs. For example ``tcp://;mode=server`` channel exposes incoming
connections as child channels. This approach simplifies channel implementation but moves that
complexity outside.

Processor loop implements rules of channel polling and processing. Its exposed as a separate API so
same implementation is used inside processor workers and if user needs to work with channels in it's
own script or program.

Object Configuration
--------------------

Object parameters are passed to channel init method but they also can be used to control processor
behaviour. List of supported keys:

 * ``shutdown-on=[none|close|error]``: shutdown processor if this object is closed (``close``) or
   enters ``Error`` state (``error``), default is ``none``.
 * ``reopen-timeout=<duration>``: initial reopen timeout, on each failed attempt to open object timeout is
   doubled until it reach maximum timeout, default is 1 second;
 * ``reopen-timeout-max=<duration>``: maximum reopen timeout, default is 30 seconds;
 * ``tll.processor-verbose=<bool>``: report state change of this object with separate info log
   messages if set to true, default is false.

..
    vim: sts=4 sw=4 et tw=100
