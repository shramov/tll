tll-channel-udp
===============

:Manual Section: 7
:Manual Group: TLL
:Subtitle: UDP client and server channel

Synopsis
--------

``udp://ADDRESS;mode={server|client};frame={none|std|short|...};af={any|unix|ipv4|ipv6}``

``mudp://ADDRESS;mode={server|client};...``


Description
-----------

Channel implements UDP client and server. If framing is not disabled - messages are prefixed by a
frame with a size, message id and sequence number (however size in received packets is ignored).

Multicast protocol ``mudp://`` is an alias to ``udp://;udp.multicast=yes``.

Init parameters
~~~~~~~~~~~~~~~

``ADDRESS`` - UDP address, either ``HOST:PORT`` pair or Unix socket path. Both IP address or hostname
can be used for HOST. Abstract Unix sockets are supported with common ``@PATH`` notation.

``af={any|unix|ipv4|ipv6}`` (default ``any``) - choose specific address family or use autodetection.
Unix AF is selected when ``/`` character is found in address string. Otherwise first entry of
``getaddrinfo(3)`` result is used.

``mode={server|client}`` (default ``client``) - channel mode, client or server. Server binds to
``ADDRESS`` and listens for incoming packets. Client connects to ``ADDRESS`` and is using ephemeral
port so can get only replies.

``ttl=<unsigned>`` - set time to live for outgoing packets.

``frame={none|std|short|...}`` (default ``std``) - select framing mode (size is filled in outgoing
messages but ignored in incoming):

  - ``none`` - disable framing, each chunk of data is passed to user as it is received, output is sent
    without any modification.
  - ``std`` or ``l4m4s8`` - "standard" frame, 4 bytes of message size, 4 bytes of message id, 8
    bytes of message sequence number. Incoming data is accumulated until full message is available.
    Output is prefixed with the frame.
  - ``short`` or ``l2m2s8`` - short frame, uint16 size, int16 msgid, int64 seq
  - ``seq32`` or ``s4`` but size includes 4 bytes of frame. Output is not prefixed in
    post, input is returned with non stripped frame.

``timestamping=<bool>`` (default ``no``) - enable hardware (if possible) timestamping, for each
incoming message ``msg->time`` is filled with timestamp got from kernel. This time is usually
measured in nanoseconds but has different epoch and can not be compared directly to wall clock time.

``timestamping-tx=<bool>`` (default ``no``) - enable hardware (if possible) transmit timestamping.
This option is checked only if normal timestamping is enabled using ``timestamping=yes``. For each
posted message special ``Time`` control message is generated with same ``msg->seq`` and kernel
reported time of send operation.

``sndbuf=<size>`` (default 0) - set specific send buffer size, for format see
``tll-channel-common(7)``.

``rcvbuf=<size>`` (default 0) - set specific recv buffer size

``size=<size>`` (default ``64kb``) - size of internal buffer used for receiving messages.

Multicast parameters
~~~~~~~~~~~~~~~~~~~~

``multicast=<bool>`` (default ``no``) - join multicast group ``ADDRESS``.

``source=<ipv4>`` - receive messages only from specified source addres, see ``ip(7)`` ``IP_ADD_SOURCE_MEMBERSHIP``

``interface=<str>`` - send multicast data from specified interface, see ``ip(7)``
``IP_MULTICAST_IF`` or corresponding IPv6 socket option.

``loop=<bool>`` (default ``yes``) - enable looping multicast packets back to socket, see ``ip(7)``
``IP_MULTICAST_LOOP`` or ``ipv6(7)`` ``IPV6_MULTICAST_LOOP``.

Control messages
----------------

When transmit timestamping is enabled following control scheme is defined:

.. code-block:: yaml

  - name: Connect
  - name: Time
    id: 10
    fields:
      - {name: time, type: int64, options.type: duration, options.resolution: ns}

Time is defined as ``duration`` so it can not be compared directly to system time directly.

Examples
--------

Create UDP client with unix socket, / symbol is found in host so address family option is not needed

::

    udp:///tmp/tcp.sock;mode=client

Subscribe to multicast group ``224.0.0.1`` limited to source address ``1.2.3.4``:

::

    mudp://224.0.0.1:5555;source=1.2.3.4;mode=server

See also
--------

``tll-channel-common(7)``, ``ip(7)``, ``ipv6(7)``

..
    vim: sts=4 sw=4 et tw=100

