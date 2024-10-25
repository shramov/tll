tll-channel-direct
==================

:Manual Section: 7
:Manual Group: TLL
:Subtitle: Synchronous testing channel

Synopsis
--------

``direct://;[scheme-control=SCHEME];[emulate-control=EMULATE];...``


Description
-----------

.. warning::

  Message callback is called inside ``post`` function and is usable only for testing purposes.

Channel implements synchronous communication between master and slave objects. It is designed for
testing purposes where each post is controled by user and provides ability to verify object reaction
exactly before or after callback is executed.

Messages with any ``msg->type`` are passed to sibling object, not only ``Data`` messages.

Control scheme emulation is implemented to easily mimic other channels like stream or tcp clients
and servers.

Init parameters
~~~~~~~~~~~~~~~

``scheme=SCHEME`` (default empty) - specify data scheme. Slave channel by default inherits scheme
from master.

``scheme-control=SCHEME`` (default empty) - specify control scheme. Slave channel by default
inherits full control scheme from master, with all emulated parts merged into it.

``emulate-control=LIST`` (default empty) - emulate one of standard communcation channels. Additional
control schemes are merged with one given in ``scheme-contorl`` option. ``LIST`` is composed of
following values:

  - ``tcp-client`` - TCP client, ``WriteFull`` and ``WriteReady`` messages (see
    ``tcp-channel-tcp(7)``)
  - ``tcp-server`` - TCP sever, client scheme extended with ``Connect`` and ``Disconnect`` messages
  - ``stream-server`` - Stream server, ``Block`` message (see ``tcp-channel-stream(7)``)
  - ``stream-client`` - Stream client, ``Online`` message

``notify-state=<bool>`` - default ``no``, only for master: generate control messages for slave state
changes.

``manual-open=<bool>`` - default ``no``, only for slave: do not change state to ``Active``, user have to do this
transition manualy, by posting state message to the other side.

State messages
--------------

User can change state of slave channel by posting into master messages with type ``State`` and msgid
of desired state. This can be used to emulate long open (with ``manual-open=yes``) and channel
failure (by posting ``Error``).

Examples
--------

Create pair of direct channels to emulate TCP server in tests, master gets notifications for slave
state changes:

::

    direct://;name=direct-master;notify-state=yes;emulate-control=tcp-server
    direct://;master=direct-master

See also
--------

``tll-channel-common(7)``

..
    vim: sts=4 sw=4 et tw=100
