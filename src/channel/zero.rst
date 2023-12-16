tll-channel-zero
================

:Manual Section: 7
:Manual Group: TLL
:Subtitle: Zeroed message generator channel

Synopsis
--------

``zero://;size=<size>;msgid=<integer>``

Description
-----------

Channel implements generator that is always ready to provide message, like ``/dev/zero`` special
device. On each ``tll_channel_process`` message callback is invoked. Signalling file descriptor (if
not disabled with ``fd=no``) is always ready.

This channel is mostly useful in benchmarks or transport load tests.  Each time same message is
passed to callback so runtime overhead is minimal - process call takes only several nanoseconds
(depening on machine) so reading from this channel can give hundreds of millions of messages per
second.

Init parameters
~~~~~~~~~~~~~~~

``size=<size>`` (default ``1kb``) - size of generated message.

``msgid=<int>`` (default ``0``) - id of generated message.

``fill=<int8>`` (default ``0``) - fill character used to memset message body.

``pending=<bool>`` (default ``true``) - disable pending dynamic flag. Useful to test performance of
processing loop in different scenarios.

Examples
--------

Generate stream of messages with id 10 of size 256:

::

    zero://;size=256b;msgid=10

See also
--------

``tll-channel-common(7)``

..
    vim: sts=4 sw=4 et tw=100
