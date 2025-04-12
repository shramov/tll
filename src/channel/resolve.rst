tll-channel-resolve
===================

:Manual Section: 7
:Manual Group: TLL
:Subtitle: Client with parameters obtained from server channel

Synopsis
--------

``resolve://SERVICE/CHANNEL;[scheme=SCHEME]``


Description
-----------

Request initialization parameters from resolve server and create channel in runtime. Parameters are
not limited to host (which can be done with DNS) or port, channel protocol is obtained from the
server too.

Lookup is performed based on service and channel pair. Service can be either unique name of some
processor instance or tag assigned to several instances. Channel is an endpoint name defined when
objects are exported from that instance. For example (``gateway``, ``inner``) pair will match all
services with name or tag ``gateway`` and channels with export name ``inner`` in them.

Init parameters
~~~~~~~~~~~~~~~

``SERVICE/CHANNEL`` - service and endpoint pair which identify peer, split by first ``/`` symbol.
Short version of ``resolve.service`` and ``resolve.channel`` parameters, is used when both of them
are empty.

``resolve.service=<string>`` - service name or tag that is used as a first part of lookup key.

``resolve.channel=<string>`` - channel export name that is used as a second part of lookup key.

``resolve.mode={once|always}`` (default ``once``) - perform lookup only once, on first successfull
activation, or on each open. If parameters received from the server differs from previous then child
object is destroyed and recreated.

``resolve.request=<url>`` (default ``ipc://;mode=client;master=_tll_resolve_master``) - client
channel parameters used to connect to resolve server and perform lookup. By default tries to reach
resolve node that is running in same processor instance. If processor is configured without it then
tcp channel should be used instead.

``scheme=SCHEME``, default is none - data scheme used for this channel. If resolved scheme is not
equals to this parameter then messages are converted between them on post or in callback.

Examples
--------

Connect to service ``gateway`` and TCP server channel ``tcp`` in it and always request new
parameters::

  resolve://gateway/tcp;resolve.mode=always

Connect to online part of stream channel ``stream`` on same service::

  resolve://gateway/stream/online

See also
--------

``tll-channel-common(7)``

..
    vim: sts=4 sw=4 et tw=100
