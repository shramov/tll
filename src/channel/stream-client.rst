tll-channel-steam-client
========================

:Manual Section: 7
:Manual Group: TLL
:Subtitle: Persistent data stream client

Synopsis
--------

``stream+{ONLINE-CHANNEL}://{ONLINE-HOST};request={REQUEST://HOST}``

Description
-----------

Channel implements persistent data stream client (see ``tll-channel-stream-server(7)``).

Init parameters
~~~~~~~~~~~~~~~

``stream+CHANNEL`` - protocol and host parameters of online channel. It should be client to server
online child, for example ``pub+tcp`` (see ``tll-channel-pub-mem(7)``).

``request=CHANNEL``- init parameters for request channel, which is used to communicate with server
during initialization. It should be client to server ``request`` child, for example ``tcp`` (see
``tll-channel-tcp``). ``mode=client`` parameter is appended if missing and should be omitted. See
``tll-channel-common(7)`` for the way to specify additional parameters in embedded channel.

``mode={server|client}``, default ``client`` - create either server or client channel, see
``tll-channel-stream-server(7)``.

``size=<size>``, default ``128kb`` - size of buffer that is used to store online data while receiving
old data from ``request`` channel.

``peer=<string>``, default empty - client name that is passed to the server where it is included in
logs about data request processing.

``report-block-begin=<bool>``, default ``yes`` - report begin of block with ``BeginOfBlock`` control message,
can be disabled for backward compatibility.

``report-block-end=<bool>``, default ``yes`` - report block end with ``EndOfBlock`` control message,
can be disabled for backward compatibility.

``protocol={old|new}``, default ``new`` - use old or new request message, since TCP channels have
``protocol`` parameter too use ``stream.protocol`` in configuration (or, better, use processor
defaults).

Open parameters
---------------

``mode={online|initial|seq|seq-data|block|last}`` - mandatory parameter, no default value

 - ``online`` - do not request any data from the server, similar to online channel client.

 - ``initial`` - request initial dataset from the server, either last block or linear history,
   depending on server settings. If server has no data - wait for first message and report online
   after it.

 - ``seq`` - additional mandatory ``seq=<unsigned>`` parameter, request linear history starting from
   ``seq``. However if there is gap in the stream user can receive first message with seq greater
   then requested. For example ``open: {mode: seq, seq: 100}`` will request data starting from seq
   100.

 - ``seq-data`` - same as ``seq`` but requested seq is incremented by 1, so last received message
   seqence number can be substituted. For example ``open: {mode: seq-data, seq: 100}`` will request
   data starting from seq 101.

 - ``block`` - additional mandatory ``block=<unsigned>`` and optional ``block-type=<string>`` (default
   ``default``) parameters. Request block number ``block`` from the server and linear history after
   it's end up to last messages, for example ``open: {mode: block, block: 1, block-type: hour}``

 - ``last`` - request last message from the server, same as opening in ``seq`` mode with seq number
   of last available message.

Control messages
----------------

Stream client generates ``Online`` control message when it switches from historical data to online.
Message has empty body and ``seq`` field set to last message sequence number available on the server.
If aggregated (block) data is requested then two additional messages are generated: ``BeginOfBlock``
before first message from ``blocks`` channel on the server and ``EndOfBlock`` after last one.
``BeginOfBlock`` has sequence number same as first data message and ``last_seq`` set to number after
the end of block. If they are same this indicates that aggregated part is empty.
``EndOfBlock`` has same sequence number as ``last_seq`` field.

.. code-block:: yaml

  - name: Online
    id: 10
  - name: EndOfBlock
    id: 11
  - name: BeginOfBlock
    id: 12
    fields:
      - {name: last_seq, type: int64}

.. include::
    stream-resolve.rst

Examples
--------

Basic stream client that is using Unix sockets:

::

    stream+pub+tcp://./online.sock;request=tcp://./request.sock

See also
--------

``tll-channel-common(7)``, ``tll-channel-stream-server(7)``, ``tll-channel-pub-tcp(7)``

..
    vim: sts=4 sw=4 et tw=100
