Scheme
======

Data description subsystem for structured fixed offset payload. Provides language neutral,
simple and explicit way to describe C-like structures.

Message
-------

Message represents a sequence of fields with fixed offsets. If there are no pointer fields
then message size is also fixed.

Field
-----

Data type is a pair of wire type (e.g. ``int32`` or ``double``) and logical type (e.g. ``duration``).
Since lot of programming languages does not have native support for unsigned integers (for example Lua
and Java), unsigned 64 bit int type is not supported.

Simple wire types:

 - ``int8``: 1 byte signed integer;
 - ``int16``: 2 byte signed integer;
 - ``int32``: 4 byte signed integer;
 - ``int64``: 8 byte signed integer;
 - ``uint8``: 1 byte unsigned integer;
 - ``uint16``: 2 byte unsigned integer;
 - ``uint32``: 4 byte unsigned integer;
 - ``double``: 8 byte floating point value;
 - ``decimal128``: 16 byte IEEE 754 decimal floating point value;
 - ``bytes``: fixed size byte array;

Composite wire types:

 - ``message``: embedded message;
 - ``array``: fixed size array, pair of integer counter and list of elements;
 - ``pointer``: variable size array, that's located somewhere later in the message;

Logical type is used to elaboration what exactly this field represents. For example you can
declare milliseconds timestamp or nanoseconds duration fields.
It provides a backward compatible way to extend scheme. Old applications that don't know
about new type can process raw wire type.

Supported logical types:

 - ``enum``:
 - ``bytestring``: field contains string (maybe without null terminator) with
   length ``strnlen(data, sizeof(data))``, only for ``bytes`` wire type;
 - ``fixed_point``: fixed point real number e.g. ``int64`` with 2 digits after decimal point, only for integral types;
 - ``time_point``: timestamp with resolution e.g. ``int64`` microseconds from epoch, only for integral and real types;
 - ``duration``: duration with resolution e.g. ``double`` seconds interval, only for integral and real types;

Time fields
-----------

For time point and duration fields resolution has to be explicitly specified with ``options.resolution``.
Supported values (self explanatory):

 - ``ns`` or ``nanosecond``;
 - ``us`` or ``microsecond``;
 - ``ms`` or ``millisecond``;
 - ``s`` or ``second``;
 - ``m`` or ``minute``;
 - ``h`` or ``hour``;
 - ``d`` or ``day``;

First variant is canonical form, used when scheme is serialized to string, second is better for reading.

Pointers
--------

.. _capnproto: https://capnproto.org/
.. _sbe: https://github.com/real-logic/simple-binary-encoding
