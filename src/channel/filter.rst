tll-channel-filter
==================

:Manual Section: 7
:Manual Group: TLL
:Subtitle: Filter messages by id

Synopsis
--------

``filter+CHILD://PARAMS...;messages=<LIST>``


Description
-----------

Prefix channel that filters all sent and received data messages by id. List of messages ids is
obtained from child channel scheme.

Init parameters
~~~~~~~~~~~~~~~

``messages=<LIST>`` - list of message names separated by comma, which is used to build two sets -
excludes, formed from names that starts with ``!``, and includes. If include list is empty then
whole scheme is used. Excluded messages have priority over included ones, ``!A,A`` will drop message
``A``.

Examples
--------

Read file and drop all ``Heartbeat`` messages::

    filter+file://file.dat;messages=!Heartbeat

Read file and pass only ``AddUser`` and ``RemoveUser`` messages::

    filter+file://file.dat;messages=AddUser,RemoveUser

See also
--------

``tll-channel-common(7)``

..
    vim: sts=4 sw=4 et tw=100
