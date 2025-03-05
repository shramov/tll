Resolving
---------

Stream server exports itself as a whole ``stream+`` channel and both of its network parts as
a separate ``online`` and ``request`` child exports as they were standalone channels.

For example there is ``upstream`` service that generates data and ``local`` service that receives
data from ``upstream``, both have ``output`` stream channel. Client can be created in different
ways:

 - ``resolve://upstream/output``: both online and historical data are sent from ``upstream``
   service, low online latency;

 - ``resolve://local/output``: both online and historical data are sent from ``local`` service,
   additional hop in online path (forwarding on ``local`` service) but historical data is served
   from ``local`` service and does not affect ``upstream``.

 - ``stream+resolve://upstream/output/online;request=resolve://local/output/request``: online is
   connected to ``upstream`` service, ``request`` - to ``local``. This variant have best properties
   of both mentioned before: low online latency and requests that are offloaded to the local snapshot
   service. Performance is same as with normal resolve - there is only one ``resolve://`` channel
   prefix in both data paths.

..
    vim: sts=4 sw=4 et tw=100
