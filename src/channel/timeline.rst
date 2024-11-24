tll-channel-timeline
====================

:Manual Section: 7
:Manual Group: TLL
:Subtitle: Delay messages for period between time fields

Synopsis
--------

``timeline+CHILD://PARAMS...;speed=<float>``


Description
-----------

Prefix channel that delays messages according to their ``time`` fields. Its purpose is to
emulate real world load by replaying recorded data where simple rate limiter is not sufficient.
Delay is calculated as ``(m[n+1].time - m[n].time) / speed`` starting from first message.

Init parameters
~~~~~~~~~~~~~~~

``speed=<float>`` default ``1.0``, speed of data stream compared to ``time`` fields, larger values
means that delay is smaller.

See also
--------

``tll-channel-common(7)``

..
    vim: sts=4 sw=4 et tw=100
