tll-channel-serial
==================

:Manual Section: 7
:Manual Group: TLL
:Subtitle: Serial port IO channel

Synopsis
--------

``serial://DEVICE;speed=SPEED``


Description
-----------

Channel implements read and write IO for serial devices. Most of initialization parameters are
passed to termios functions and should match device settings that are described in documentation.

Init parameters
~~~~~~~~~~~~~~~

``DEVICE`` - path to device node, like ``/dev/ttyS0``;

``speed={9600|19200}`` - default ``9200``: baud rate for device;

``parity={none|even|odd}`` - default ``none``: parity settings
disabled in processor, then all channels are automatically created with ``fd=no`` option.

``stop={1|2}`` - default ``1``: stop bits settings;

``data={8|7}`` - default ``8``: data bits settings;

``flow-control=<bool>`` - default ``false``: enable or disable flow control

Examples
--------

Read data from USB serial device with baud rate 19200, 8 data bits

::

    serial:///dev/ttyUSB0;speed=19200;data=8


See also
--------

``tll-channel-common(7)``, ``termios(3)``

..
    vim: sts=4 sw=4 et tw=100
