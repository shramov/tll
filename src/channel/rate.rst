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

Each ``interval`` seconds ``speed`` number of tokens (bytes or number of messages) are added to the
bucket limited by ``max-window`` size. If bucket is not empty (non-negative number of tokens) then
message is passed and either ``msg->size`` (if bytes are counted) or ``1`` (if messages are counted)
tokens are removed from the bucket.

Difference from classical token bucket algorithm is that even if message does not fit into bucket it
is handled. This makes implementation more effective - channel does not need to save message until
it is possible to pass it.

One ore more buckets (depending on initialization parameters) are used to check if it is possible to
send or receive next message. When at least one bucket is fully drained then child channel is
suspended in input mode or ``WriteFull`` control message is generated in output mode. After at least
one token appears in all buckets - ``WriteReady`` control messages is generated or child is resumed.

Implementation is available in header only mode for C++ and can be included from
``tll/channel/rate.h`` file.

Init parameters
~~~~~~~~~~~~~~~

``dir={r|w|rw|in|out|inout}`` (default ``w``) - message stream direction which is rate limited.
Input is affected only in read-only mode (``r`` or ``in``). Read-write or write-only modes work on
output stream. To limit input stream on bidirectional channel ``rate.dir=in`` parameter should be
used.

``master=<name>`` (default none) - if master is rate channel then use its limits and ignore own
bucket settings.

Bucket settings
~~~~~~~~~~~~~~~

Following parameters are used to describe bucket settings:

``speed=<SIZE>`` - mandatory parameter, no default. Add ``speed`` number of bytes into bucket each
``interval`` (see below). With default interval value it is equivalent to ``speed`` number of bytes
per second. Granularity of this parameter is 1 byte, even if ``100mbit`` notation can be used, see
``tll-channel-common(7)``.

``interval=<DURATION>`` - default ``1s``, can be used to set speeds less then 1 byte/second.

``max-window=<SIZE>`` - default ``16kb``, maximum size of tokens in the bucket.

``initial=<SIZE>`` - default ``max-window / 2``. When first message is seen bucket is initialized
with ``initial`` number of tokens.

``watermark=<SIZE>`` - default ``1``, report availability after this amount of tokens are available
in the bucket, by default report as soon as possible.

``unit={byte|message}`` - default ``byte``. Use data size in bytes or number of message as rate
tokens.

Any amount of additional buckets can be defined using ``bucket.*`` subtrees. Each subtree holds
parameters described above: mandatory ``speed`` and optional ``max-window``, ``interval`` and
others. For example ``bucket.a: { speed: 10kb }`` and ``bucket.b: { speed: 100b, unit: message }``
declare 2 additional buckets.

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

Allow sending to UDP 100 messages per second (with burst of 10 messages) with total bandwith limited
to 100kbit (with burst of 64kb), note that ``unit: message`` setting still require ``b`` suffix for
speed/size parameters:

.. code-block:: yaml

  server:
    tll.proto: rate+udp
    tll.host: HOST:PORT
    udp.mode: client
    speed: 100kbit
    max-window: 64kb
    bucket.messages:
      speed: 100b
      max-window: 10b
      unit: message

See also
--------

``tll-channel-common(7)``

..
    vim: sts=4 sw=4 et tw=100
