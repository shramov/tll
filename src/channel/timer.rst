tll-channel-timer
=================

:Manual Section: 7
:Manual Group: TLL
:Subtitle: Timer channel

Synopsis
--------

``timer://;interval=<time>>;oneshot=<bool>;clock=<monotonic | realtime>``


Description
-----------

Channel generates messages with fixed time interval between them. Alternative mode is to post
message with next event time (relative or absolute).

Init parameters
~~~~~~~~~~~~~~~

``interval=<time>`` (default empty) - time between generated messages. If not set - wait for user
message with next event time. Format is ``<integer><suffix>`` where suffix is one of ``h``, ``m,``,
``s``, ``ms``, ``us``, ``ns`` with their usual meaning and ``d`` for days.

``oneshot=<bool>`` (default ``false``) - if enabled - generate only one message after open and then
disable timer.

``clock={realtime|monotonic}`` (default ``monotonic``) - clock to use for time. ``CLOCK_REALTIME``
or ``CLOCK_MONOTONIC`` are used internaly. See ``clock_gettime(2)`` for more details.

``skip-old=<bool>`` (default ``false``) - if channel is not processed for a while and more then one
message should be generated - skip all them and produce only one message.

Data scheme
-----------

.. code-block:: yaml

  - name: relative
    id: 1
    fields: [{name: ts, type: int64, options.type: duration, options.resolution: ns}]
  - name: absolute
    id: 2
    fields: [{name: ts, type: int64, options.type: time_point, options.resolution: ns}]

Channel generates ``absolute`` messages, but for ``monotonic`` clock it's body is zeroed.

When posted - set time when next message will be generated (only one). For ``monotonic`` clock only
relative time can be requested.

Examples
--------

Generate message every second

::

  timer://;interval=1s


See also
--------

``tll-channel-common(7)``, ``clock_gettime(2)``

..
    vim: sts=2 sw=2 et tw=100
