tll-channel-zero
================

:Manual Section: 7
:Manual Group: TLL
:Subtitle: No-op blackhole channel

Synopsis
--------

``null://``

Description
-----------

Channel implements object that is never ready to provide message but always ready to consume them,
like ``/dev/null`` special device. Post to does nothing and can never fail.

This channel can be used to disable input or output stream - replace normal object with ``null://``
without changing other configuration, output will be silently discarded and no input generated.

Also it can be used in benchmarking - post overhead is several nanoseconds (due to single pointer
call) and does small effect on overall time.

Init parameters
~~~~~~~~~~~~~~~

This channel has no specific parameters other then common ones described in
``tll-channel-common(7)``.

Examples
--------

Create channel that consumes everything and generates nothing:

::

    null://

See also
--------

``tll-channel-common(7)``

..
    vim: sts=4 sw=4 et tw=100
