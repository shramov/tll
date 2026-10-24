tll-channel-pub-tcp
===================

:Manual Section: 7
:Manual Group: TLL
:Subtitle: Publish-subscribe over TCP

Synopsis
--------

``pub+tcp://ADDRESS;mode=server;[size=<SIZE>;][tcp-params...]``

``pub+tcp://ADDRESS;[mode=client;][tcp-params...]``


Description
-----------

Channel implements publish-subscribe over TCP. Server stores messages in a ring buffer and sends
them into client TCP sockets as they become ready for writing. Old data is pushed out of the buffer
to make space for new. If removed data is not yet sent to some clients - they are disconnected.

Init parameters
~~~~~~~~~~~~~~~

Parmeters have ``pub`` optional prefix.

``ADDRESS`` - TCP address, either IP host:port pair (ipv4 or ipv6) or Unix socket

``mode={client | server}``, default ``client`` - channel mode. Publisher - ``server`` or subscriber
- ``client``.

``size=<SIZE>``, default ``1mb`` - size of ring buffer, for server only.

``filter.<name>=<LIST>``, default none, only for server - list of message filters, parsed without
``pub.`` prefix, filter name can not contain ``.``. List of message names is comma separated list or
a subtree, used to build two sets - excludes, formed from names that starts with ``!``, and
includes. If include list is empty then whole scheme is used. Excluded messages have priority over
included ones, ``!A,A`` will drop message ``A``. Without scheme names are numeric message ids and
excludes are not supported.

``filter-default=<str>``, default none, server only - if client does not request filter explicitly
then use default filter.

``filter=<str>``, default none, only for client - request filter from the server. If server has no
such filter connection will fail.

Common TCP parameters, like ``sndbuf`` or ``nodelay``, documented in ``tll-channel-tcp(7)`` are also
supported.

Examples
--------

Create pub server with ``1mb`` buffer and attach client to it:

::

  server:
    pub+tcp:///tmp/pub.ring;mode=server;size=1mb
  client:
    pub+tcp:///tmp/pub.ring

See also
--------

``tll-channel-common(7)``, ``tll-channel-tcp(7)``

..
    vim: sts=4 sw=4 et tw=100
