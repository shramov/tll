tll-channel-pub-mem
===================

:Manual Section: 7
:Manual Group: TLL
:Subtitle: Publish-subscribe over shared memory

Synopsis
--------

``pub+mem://FILENAME;mode=server;size=<SIZE>``

``pub+mem://FILENAME;[mode=client]``


Description
-----------

Channel implements publish-subscribe using memory mapped file. Server creates file when opened and
deletes it when closed. Communication is performed using ring buffer, old messages are removed (by
server) to make space for new ones. Server pushes messages without taking into account position of
clients. If client has not read message that is already removed then it closes with an error.

Since there is no file descriptor to signal avilability of data it's better to use pub-sub over Unix
socket for non-spin processors.

Init parameters
~~~~~~~~~~~~~~~

``mode={client | server}``, defaut ``client`` - channel mode, publisher - ``server`` or subscriber
- ``client``.

``size=<SIZE>``, defaut ``64kb`` - size of ring buffer, for server only. Client reads ring size from
file on open.

Examples
--------

Create pub server with ``1mb`` buffer and attach client to it:

::

  server:
    pub+mem:///tmp/pub.ring;mode=server;size=1mb
  client:
    pub+mem:///tmp/pub.ring

See also
--------

``tll-channel-common(7)``

..
    vim: sts=4 sw=4 et tw=100
