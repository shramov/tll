tll-channel-timeit
==================

:Manual Section: 7
:Manual Group: TLL
:Subtitle: Measure post and callback time

Synopsis
--------

``timeit+CHILD://PARAMS...;speed=<FLOAT>``


Description
-----------

Prefix channel that measures time spent in child post method and in callback for Data messages. Time
is stored in ``rxt`` (callback) and ``txt`` (post) stat variables. Collected data can be retrieved
using ``stat://`` logic (see ``tll-logic-stat(7)``).

When statistics is not enabled (see ``stat=<bool>`` in ``tll-channel-common(7)``) then this channel
just forwards data in both directions.

Init parameters
~~~~~~~~~~~~~~~

No specific parameters, but without ``stat=yes`` it does nothing.

Examples
--------

Measure time to send data into the network (effectively ``send`` syscall)::

    timeit+tcp://;mode=client;stat=yes

See also
--------

``tll-channel-common(7)``, ``tll-logic-stat(7)``

..
    vim: sts=4 sw=4 et tw=100
