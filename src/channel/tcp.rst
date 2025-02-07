tll-channel-tcp
===============

:Manual Section: 7
:Manual Group: TLL
:Subtitle: TCP client and server channel

Synopsis
--------

``tcp://ADDRESS;mode={server|client};frame={none|std|short|...};af={any|unix|ipv4|ipv6}``


Description
-----------

Channel implements TCP client and server. If framing is not disabled - messages are prefixed by a
frame with a size, message id and sequence number. Server assigns unique 8 byte address to each
client that is filled in every message received by user, both for data messages and
connect/disconnect notifications. Same address is used to specify connection for outgoing messages.

Server creates child channel for each client so it's not best choice for usecases with thousands of
short living connections.

Init parameters
~~~~~~~~~~~~~~~

All parameters except ``frame`` are used in channels that are based on common tcp implementations,
for example ``pub+tcp``.


``ADDRESS`` - TCP address, either ``HOST:PORT`` pair or Unix socket path. Both IP address or hostname
can be used for HOST. Abstract Unix sockets are supported with common ``@PATH`` notation. If
``ADDRESS`` is empty then it can be passed in open parameters as ``hostname`` parameter.

``af={any|unix|ipv4|ipv6}`` (default ``any``) - choose specific address family or use autodetection.
Unix AF is selected when ``/`` character is found in address string. Otherwise result of
``getaddrinfo(3)`` is used.

``mode={server|client}`` (default ``client``) - channel mode, client or server. Client connect stage
is performed in non-blocking mode so user should wait until channel states becomes ``Active`` before
sending data. Server binds to all addresses returned by ``getaddrinfo(3)`` and handles multipl
client connections.

``bind=<address>`` (default is ``*:0``, only in client mode) - bind local side of connection to
specified address. Can be used for load balancing when several connections to one endpoint are
placed on different network interfaces.

``frame={none|std|short|...}`` (default ``std``) - select framing mode:

  - ``none`` - disable framing, each chunk of data is passed to user as it is received, output is sent
    without any modification.
  - ``std`` or ``l4m4s8`` - "standard" frame, 4 bytes of message size, 4 bytes of message id, 8
    bytes of message sequence number. Incoming data is accumulated until full message is available.
    Output is prefixed with the frame.
  - ``short`` or ``l2m2s8`` - short frame, uint16 size, int16 msgid, int64 seq
  - ``tiny`` or ``l2m2s4`` - short frame, uint16 size, int16 msgid, int32 seq
  - ``size32`` or ``l4`` - only size is transfered, uint32 size, no msgid or seq
  - ``bson`` - same as ``size32`` but size includes 4 bytes of frame. Output is not prefixed in
    post, input is returned with non stripped frame.

``timestamping=<bool>`` (default ``no``) - enable hardware (if possible) timestamping, for each
received message ``msg->time`` field is filled with time of last recv operation obtained from
kernel. If message body was gathered from several recv calls then time of last is used. This
feature is supporeted only on Linux.

``keepalive=<bool>`` (default ``yes``) - enable or disable TCP keepalive.

``nodelay=<bool>`` (default ``yes``) - enable or disable TCP Nagle algorithm (see ``tcp(7)``,
``TCP_NODELAY``).

``protocol={tcp|mptcp|sctp}`` (default ``tcp``) - choose specific IP protocol:

  - ``tcp`` - default TCP mode
  - ``mptcp`` - enable Multipath TCP if available. Fallback to TCP occures for Unix sockets or when
    ``IPPROTO_MPTCP`` constant is not available for target platform. On Linux it is always defined
    (even if not available from included headers).
  - ``sctp`` - use SCTP (see ``sctp(7)``) in TCP-like mode, not available for UNIX-sockets

``sndbuf=<size>`` (default ``0b``) - if not zero set kernel send buffer size (``SO_SNDBUF``, see
``socket(7)``), for format see ``tll-channel-common(7)``.

``rcvbuf=<size>`` (default ``0b``) - if not zero set kernel recv buffer size (``SO_RCVBUF``)

``buffer-size=<size>`` (default ``64kb``) - size of internal buffers used for receiving messages
before passing them to user and storing data that is not possible to sent to user.

``recv-buffer-size=<size>`` (default ``buffer-size``) - size of userspace receiving buffer,
overrides ``buffer-side``.

``send-buffer-size=<size>`` (default ``buffer-size``) - size of userspace sending buffer, overrides
``buffer-side``.

``send-buffer-hwm=<size>`` (default ``0``) - high watermark for send buffer. If connection is
blocked - store up to this value amount of bytes in send buffer and only after it report
``WriteFull`` control message and start to return ``EAGAIN`` error on post. Can not be larger
then 80% of send buffer size.

Open parameters
~~~~~~~~~~~~~~~

``hostname=ADDRESS`` - ``ADDRESS`` in init parameters was empty it can be passed to open

``af=AF`` - same as init ``af`` parameter, only if address is specified in open.

Control messages
----------------

TCP server generates control messages when client connects or disconnects with following
scheme:

.. code-block:: yaml

  - name: Connect
    id: 10
    unions:
      IPAny: {union: [{name: ipv4, type: uint32}, {name: ipv6, type: byte16}, {name: unix, type: uint8}]}
    fields:
      - {name: host, type: IPAny }
      - {name: port, type: uint16 }

  - name: Disconnect
    id: 20

If ``Disconnect`` message is posted then connection with specified address is closed.

Both server and client use two additonal messages are used to signal buffer state:

  - ``WriteFull``: generated when write buffer is full (next post will buffer or return ``EAGAIN``
    if no buffer space is available).

  - ``WriteReady``: generated after buffer is flushed and new data can be posted.

Examples
--------

Create TCP client with unix socket, / symbol is found in address so address family option is not needed

::

    tcp:///tmp/tcp.sock;mode=client

See also
--------

``tll-channel-common(7)``, ``socket(7)``

..
    vim: sts=4 sw=4 et tw=100
