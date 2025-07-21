tll-channel-ipc
===============

:Manual Section: 7
:Manual Group: TLL
:Subtitle: In-process communication channel

Synopsis
--------

``ipc://``

``ipc://;master=MASTER``


Description
-----------

Channel implements TCP-like client-server in-process communication. System IO is used only for
polling based on ``eventfd`` and can be disabled with ``fd=no`` parameter. Current implementation
does not use ring buffer for data messages, each one is allocated with ``malloc`` call.

Client address can be obtained from ``addr`` fields of ``Connect`` control message or from data
messages. That address is not tied to client channel object and is changed when it is closed and
then opened again.

Init parameters
~~~~~~~~~~~~~~~

Client and server modes are distinguished by ``master`` parameter: without master server is created,
otherwise client for specified master. Client has no configurable parameters (other then common like
``dump`` or ``scheme``).

``broadcast=<bool>``, default ``false``: treat zero address as broadcast and send message to all
connected clients

``size=<size>``, default ``64kb`` - size of marker queue, each message needs 8 bytes of data.

Control messages
----------------

IPC server generates control messages when client connects or disconnects with following
scheme:

.. code-block:: yaml

  - name: Connect
    id: 10

  - name: Disconnect
    id: 20

See also
--------

``tll-channel-common(7)``

..
    vim: sts=4 sw=4 et tw=100
