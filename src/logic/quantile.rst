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

``node=<string>`` (default empty) - node name field in generated ``Page`` message

Input scheme
------------

.. code-block:: yaml

  - name: Data
    id: 10
    fields:
      - {name: name, type: byte8, options.type: string}
      - {name: value, type: uint64}

Output scheme
-------------

On each timer tick for each unique tag logic generates local and global ``Page`` messages same as in
``tll-logic-stat(7)``. Global one can be ommited if ``skip`` parameter is used and there is not
enought data. Page name field has format ``{node}/{local|global}/{tag}`` where ``node`` is taken
from init parameters. Fields list match ``quantile`` parameter. Example dump looks like this::

  node: "quantile"
  name: "test/local/tcp"
  time: 2026-04-03T10:20:30.123456789
  fields:
    - name: "95"
      unit: Unknown
      value:
        ivalue:
          method: Max
          value: 1000
    - name: "75"
      unit: Unknown
      value:
        ivalue:
          method: Max
          value: 900
    - name: "50"
      unit: Unknown
      value:
        ivalue:
          method: Max
          value: 600

Examples
--------

Calculate quantiles in separate worker each second and write them into the file::

  processor.module:
    - module: tll-logic-stat
    - module: tll-logic-forward

  processor.objects:
    file:
      init: file://stat.dat;dir=w;scheme=yaml://tll/logic/stat.yaml
      worker: stat
    forward:
      init: forward://
      channels: {input: quantile, output: file}
      worker: stat
    quantile:
      init: quantile://;quantiles=95,90,50
      channels: {timer: timer, input: data-master}
      worker: stat
      depends: file
    timer:
      init: timer://;interval=1s
      depends: quantile
      worker: stat
    data-master:
      init: mem://;size=1mb;dir=r;fd=no
      worker: stat
      depends: quantile

    data:
      init: mem://;master=data-master
      depends: data-master

See also
--------

``tll-logic-common(7)``

..
    vim: sts=4 sw=4 et tw=100

