tll-channel-blocks
==================

:Manual Section: 7
:Manual Group: TLL
:Subtitle: Store marked points of data stream

Synopsis
--------

``blocks://FILENAME;dir=w``

``blocks://FILENAME;[dir=r;]``

``blocks://;[dir=r;][master=MASTER]``


Description
-----------

Channel stores list of message sequence numbers with tags in a yaml file and provides a way to get
N-th point from the end with specified tag. Lookup is performed either from the file or from the
master channel which holds in-memory version of the same list.

Writer opens yaml file on open and exports maximum sequence number as ``info.seq`` in config (or
``-1`` if no data points are available). Following posts of data messages update this variable.

Reader exports requested data point sequence number as ``info.seq``.

Both have ``info.seq-begin`` always set to ``-1``.

Blocks protocol
~~~~~~~~~~~~~~~

Blocks protocol is used in stream server for aggregated data storage. Channel implementing this
protocol should comply with following requirements:

 - provide ``Block`` control message with ``type`` fixed string field, create new data block when
   this message is posted.
 - export ``seq`` variable in config info with seq number of last available data message in writer.
   Writer does not need to remember not yet processed data messages between close and open calls,
   stream server feeds it with data starting from ``seq`` until last message in linear storage.
 - support ``block`` and ``block-type`` open parameters for reader. When reader is opened it should
   provide data messages and close when they are finished. Export ``seq`` and ``seq-begin``
   variables:

    * ``seq-begin`` as a first seq number of aggregated data block. If block is empty -
      ``-1`` is used to signal that there is no data.
    * ``seq`` - last seq number of aggregated data block.

Init parameters
~~~~~~~~~~~~~~~

``dir={r|w|in|out}``, default ``r`` - read or write mode.

``FILENAME`` - path to the file which holds list of sequence numbers. If channel is opened for
reading (default mode) and with master channel - then filename is not used and can be omitted.

``default-type=<STRING>``, default ``default`` - name of default tag that is used when ``block-type``
parameter is empty

Open parameters
~~~~~~~~~~~~~~~

Writer channel has no open parameters.

``block=<INT>``, mandatory - index of block to lookup, counted from the end, ``block=0`` means last
block.

``block-type=<STRING>``, default ``default`` - block type, only data points with specified tag are
used for lookup.

Control messages
----------------

Control scheme exists only in write mode and is used to create new datapoint:

.. code-block:: yaml

  - name: Block
    id: 100
    fields:
      - {name: type, type: byte64, options.type: string}

When ``Block`` messages is posted into channel new data point is created with last message seq and
``type`` tag. If ``type`` field is empty - ``default-type`` is used. Creating several points without
new data between them is allowed.

File format
-----------

File is stored as yaml list of dicts with two fields, ``seq`` and ``type``, following file has 3
data points:

::

  - { seq: 10, type: default }
  - { seq: 20, type: default }
  - { seq: 20, type: other }

See also
--------

``tll-channel-common(7)``

..
    vim: sts=4 sw=4 et tw=100
