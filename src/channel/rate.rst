tll-channel-rate
================

:Manual Section: 7
:Manual Group: TLL
:Subtitle: Rate limiting prefix channel

Synopsis
--------

``rate+CHILD://PARAMS...;speed=<SIZE>;interval=<DURATION>;max-window=<SIZE>;dir={r|in|w|out|rw|inout}``


Description
-----------

Prefix channel that limits rate of input or output message stream using token bucket like algorithm.
By default output is rated - ``post`` call returns ``EAGAIN`` if sending message is not possible
now.

Each ``interval`` seconds ``speed`` number of bytes (tokens) are added to the bucket limited to
``max-window`` size. If bucket is not empty (non-negative number of tokens) then message is passed
and ``msg->size`` number of bytes (tokens) are removed from the bucket.

Difference from classical token bucket algorithm is that even if message does not fit into bucket it
is handled. This makes implementation more effective - channel does not need to save message until
it is possible to pass it.

When bucket is fully drained then child channel is suspended in input mode or ``WriteFull`` control
message is generated in output mode. After at least one token appears in the bucket - ``WriteReady``
control messages is generated or child is resumed.

Implementation is available in header only mode for C++ and can be included from
``tll/channel/rate.h`` file.

Init parameters
~~~~~~~~~~~~~~~

``dir={r|w|rw|in|out|inout}`` (default ``w``) - message stream direction which is rate limited.
Input is affected only in read-only mode (``r`` or ``in``). Read-write or write-only modes work on
output stream. To limit input stream on bidirectional channel ``rate.dir=in`` parameter should be
used.

``speed=<SIZE>`` - mandatory parameter, no default. Add ``speed`` number of bytes into bucket each
``interval`` (see below). With default interval value it is equivalent to ``speed`` number of bytes
per second. Granularity of this parameter is 1 byte, even if ``100mbit`` notation can be used, see
``tll-channel-common(7)``.

``interval=<DURATION>`` - default ``1s``, can be used to set speeds less then 1 byte/second.

``max-window=<SIZE>`` - default ``16kb``, maximum size of tokens in the bucket.

``initial=<SIZE>`` - default ``max-window / 2``. When first message is seen bucket is initialized
with ``initial`` number of tokens.

Control messages
----------------

In output mode channel two messages are used to signal bucket state:

  - ``WriteFull``: generated when bucket is empty and next post is not possible.

  - ``WriteReady``: generated after at least one token appears in the bucket and next post is
    possible.

Child channel control scheme is extended with theese messaages.

Examples
--------

Read from the file with average speed of ``16kbit`` and maximum burst size of ``8kbit`` + one
message size:

::

    rate+file://file.dat;speed=16kbit;max-window=8kbit;dir=in

Send message with the speed of 0.1 byte per second (and wait for 10 seconds before first message can
be posted):

::

    rate+null://;speed=1b;interval=10s;initial=0b;max-window=4b


See also
--------

``tll-channel-common(7)``

..
    vim: sts=4 sw=4 et tw=100
