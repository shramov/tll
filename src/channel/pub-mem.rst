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

``FILENAME`` - path to the file that is mapped to the memory. Server creates new file each time it
is opened so old clients can not see new data.

``mode={client | server | pub-client | sub-server}``, default ``client`` - channel mode.

 - ``client`` - subscribe client, opens file in readonly mode and watches for new messages.
 - ``server`` - publish server, creates new file and pushes messages into it.
 - ``pub-client`` - publish client, opens existing file and pushes messages into it
 - ``sub-server`` - subscribe server, creates file for publisher and watches it for messages,
   unlinks it on close.

``size=<SIZE>``, default ``64kb`` - size of ring buffer, for server (``server`` or ``sub-server``)
only. Client reads ring size from file on open.

Control messages
----------------

Control scheme exists only for subscriber server. Subscriber client does not need control messages
since it closes when publisher server is closed.

.. code-block:: yaml

  - name: Connect
    id: 10

  - name: Disconnect
    id: 20

``Connect`` and ``Disconnect`` messages signal when publisher client connects and disconnects.
Messages are bound to data stream so if server connects, pushes some messages and the disconnects
client will receive ``Connect``, data messages and only then ``Disconnect`` control.

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
