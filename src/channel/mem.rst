tll-channel-mem
===============

:Manual Section: 7
:Manual Group: TLL
:Subtitle: In-process ring buffer channel

Synopsis
--------

``mem://;size=SIZE;fd=<bool>``

``mem://;master=MASTER``


Description
-----------

Channel implements bidirectional communication between master and slave object using two fixed ring
buffers. Signalling file descriptor can be disabled to reduce latency when loop is running with
disabled polling.

Init parameters
~~~~~~~~~~~~~~~

Slave inherits all parameters from master channel.


``size=<SIZE>`` - default ``64kb``: size of ring buffers.

``fd=<bool>`` - default ``yes``: enable or disable signalling fd (``eventfd(2)``). If polling is
disabled in processor, then all channels are automatically created with ``fd=no`` option.

``frame={normal | full}`` - default ``normal``: pass only ``seq`` and ``msgid`` in ``normal`` mode
and all message metainfo in ``full`` mode through the channel.  ``full`` frame mode should be used
only for testing purposes.

Examples
--------

Create pair of ``mem://`` channels:

::

    mem://;size=256kb;name=mem-master
    mem://;master=mem-master


See also
--------

``tll-channel-common(7)``

..
    vim: sts=4 sw=4 et tw=100
