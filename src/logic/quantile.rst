tll-logic-quantile
==================

:Manual Section: 7
:Manual Group: TLL
:Subtitle: Quantile calculation logic

Synopsis
--------

::

    tll.proto: quantile
    skip: <int>
    quantiles: <list>
    tll.channel.timer: <timer-channel>
    tll.channel.input: <data-channel>


Description
-----------

Logic implements calculation of quantiles for the stream of input data with result dumped to logs on
each event from timer channel. Two types of quantiles are gathered - "global", covering all data
seen since logic open and "local", for data from last timer event. Input stream can contain
different types of values distinguished by short string tag. When a tag is seen first time new
quantile group is started and included in next dump.

Calculations are performed in constant memory and time using logarithmic aggregation with precision
of ``0.001 * value`` and are not exact. In practice this type of error is tolerable.

Channels
~~~~~~~~

``input`` - stream of input data

``timer`` - timer events

Init parameters
~~~~~~~~~~~~~~~

``quantiles=<list of int>`` (default ``95``) - list of quantiles (in percents), list of integer
values in range (0, 100] separated by comma, for example ``95,90,50``.

``skip=<int>`` (default ``0``) - amount of timer ticks to skip before recording global quantile
values. Useful to ignore inital stabilization time of measured system.

Input scheme
------------

.. code-block:: yaml

  - name: Data
    id: 10
    fields:
      - {name: name, type: byte8, options.type: string}
      - {name: value, type: uint64}

See also
--------

``tll-logic-common(7)``

..
    vim: sts=4 sw=4 et tw=100

