tll-channel-lz4
===============

:Manual Section: 7
:Manual Group: TLL
:Subtitle: Per-message LZ4 compression

Synopsis
--------

``lz4+CHILD://PARAMS...;level=<int>;max-size=<SIZE>``


Description
-----------

Prefix channel that performs per-message LZ4 compression/decompression: messages that are posted
into channel are compressed, incoming messages are decompressed.

Init parameters
~~~~~~~~~~~~~~~

``level=<int>`` (default ``1``) - compression level, higher is better compression and slower speed,
passed to compression library as is.

``max-size=<SIZE>`` (default ``256kb``) maximum size of uncompressed data, larger messages are
discarded with ``EMSGSIZE`` errors.

``inverted=<bool>`` (default ``no``) - invert codec logic: decompress on post, compress incoming messages.

Examples
--------

Send compressed messages into ``pub+tcp`` channel and decompress them in client::

    lz4+pub+tcp://./tmp/pub.sock;mode=server
    lz4+pub+tcp://./tmp/pub.sock;mode=client

Write per-message compressed data into file (not recommended) and compare it with block compression
(which is better, faster and can be opened just with ``file://file-1.dat;dir=r``)::

    lz4+file://file-0.dat;dir=w
    file://file-1.dat;compression=lz4;dir=w

See also
--------

``tll-channel-common(7)``

..
    vim: sts=4 sw=4 et tw=100
