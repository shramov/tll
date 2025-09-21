tll-channel-steam-server
========================

:Manual Section: 7
:Manual Group: TLL
:Subtitle: Persistent data stream server

Synopsis
--------

``stream+{ONLINE-CHANNEL}://{ONLINE-HOST};request={REQUEST://HOST};storage={STORAGE://DATA};[blocks={BLOCKS://DATA}];mode=server``

Description
-----------

Channel implements persistent data stream backed by sequential storage and optional aggregated
storage. When new data is posted into the channel it is forwarded to storage, blocks and online
childs.

On open server checks if blocks storage contains all data available in sequential one. If it lags
behind then storage is opened starting from ``info.seq`` parameter from blocks channel config and
messages are posted into it.

Client connects using ``request`` channel and requests stored data - either part of linear
history or snapshot created by ``blocks`` channel and linear history after it.

Client channel is described in ``tll-channel-client(7)``.

Init parameters
~~~~~~~~~~~~~~~

``stream+CHANNEL`` - protocol and host parameters of online channel. It should provide
reliable broadcast message delivery, for example ``pub+tcp`` (see ``tll-channel-pub-mem(7)``) or
``pub+mem``.

``request=CHANNEL``- init parameters for request channel, which is used to communicate
with clients during initialization. It should provide reliable unicast message delivery, for example
``tcp`` (see ``tll-channel-tcp``) or ``ipc`` server. ``mode=server`` parameter is appended if
missing and should be omitted. See ``tll-channel-common(7)`` for the way to specify additional
parameters in embedded channel.

``storage=CHANNEL`` - init parameters for sequential storage channel, usually ``file://history.dat`` or
``rotate+file://history``. It should provide persistent storage with ability to read data starting
from requested message sequence number using ``seq`` open parameter. ``scheme`` parameter is
appended and should be omitted.

``blocks=CHANNEL`` - init parameters for aggregated storage channel. It should provide persistent
storage with some form of aggregated data that can be requested using ``block=<unsigned>`` and
``block-type=STRING`` open parameters. For primitive example of aggregating channel see
``tll-channel-blocks(7)``. ``scheme`` parameter is appended and should be omitted.

``mode={server|client}``, default ``client`` - create either server or client channel, see
``tll-channel-stream-client(7)``.

``autoseq=<bool>``, default ``false`` - enable automatic sequence numbers. If old data exists when
channel is opened - continue from last stored sequence number.

``init-message=<string>``, default empty - initialize empty storage with this message filled with
data from ``init-message-data`` subtree and seqence number from ``init-seq`` parameter, can not be
used without scheme.

``init-seq=<unsigned>``, default ``0`` - sequence number for initialization message

``init-message-data=<config>`` - subtree describing field values for initialization message.

``init-block``, default ``default`` - initialize block storage with message from ``init-message``
parameter with block this block type. Applicable only if ``blocks`` channel is defined.

``rotate-on-block=<string>``, default empty - if new block is created with same name as given in
this parameter (and storage channel has ``Rotate`` control message) then ``Rotate`` is posted into
storage. For example ``rotate-on-block=default`` will create new file each time ``default`` block is
created for stream server with ``rotate+file://`` storage.

``max-size=<size>``, default unlimited - check that posted message size does not exceed this
parameter.

Open parameters
~~~~~~~~~~~~~~~

Stream server has no specific open parameters but forwards specific subsections to child channels
with matching names: ``storage``, ``blocks`` and ``request``. Online child is opened with whole
config that is modified by setting ``last-seq`` variable to last known message seq number from the
file (if it is not empty).

Control messages
----------------

Stream server does not have it's own control scheme but exposes one composed from online, request,
storage and block childs. For example if ``blocks`` storage is defined it contains ``Block`` message
used to create new data snapshot. If sequential storage is defined as ``rotate+file://`` then it
contains ``Rotate`` message that is used to start new file.

.. include::
    stream-resolve.rst

Examples
--------

Basic stream server that is using Unix sockets:

::

    stream+pub+tcp://./online.sock;request=tcp://./request.sock;storage=file://file.dat;mode=server

Stream server with scheme, marked points in the history and rotated storage listening on IPv6
localhost addresss ``::1`` and ports 5555 and 5556:

.. code-block:: yaml

  server:
    tll.proto: stream+pub+tcp
    tll.host: ::1:5555
    request: tcp://::1:5556
    mode: server
    storage: rotate+file://storage
    blocks: blocks://blocks.yaml
    scheme: yaml://scheme.yaml

Initialize storage (if it is empty) and create first block with non-zeroed message (it has
``header`` with field ``f0`` and ``f1`` top level field).

.. code-block:: yaml

  server:
    tll.proto: stream+pub+tcp
    tll.host: ::1:5555
    request: tcp://::1:5556
    mode: server
    storage: file://storage
    blocks: some-blocks://params
    scheme: yaml://scheme.yaml
    init-message: Heartbeat
    init-message-data:
      header: {f0: 100}
      f1: 123.456
    init-seq: 100
    init-block: default

See also
--------

``tll-channel-common(7)``, ``tll-channel-stream-client(7)``, ``tll-channel-pub-tcp(7)``,
``tll-channel-file(7)``, ``tll-channel-blocks(7)``

..
    vim: sts=4 sw=4 et tw=100
