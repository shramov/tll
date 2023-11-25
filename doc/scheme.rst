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
 - ``union``: several fields sharing same space;

Logical type is used to elaboration what exactly this field represents. For example you can
declare milliseconds timestamp or nanoseconds duration fields.
It provides a backward compatible way to extend scheme. Old applications that don't know
about new type can process raw wire type.

Supported logical types:

 - ``enum``: set of fixed named values, like C enum type;
 - ``bytestring``: field contains string (maybe without null terminator) with
   length ``strnlen(data, sizeof(data))``, only for ``bytes`` wire type;
 - ``fixed_point``: fixed point real number e.g. ``int64`` with 2 digits after decimal point, only for integral types;
 - ``time_point``: timestamp with resolution e.g. ``int64`` microseconds from epoch, only for integral and real types;
 - ``duration``: duration with resolution e.g. ``double`` seconds interval, only for integral and real types;

Enum
----

An enum is a type with fixed named values, like C enum type. Wire type can be any integer - both
signed and unsigned.

Following scheme declares two identical messages - with normal and inline enum declarations. From
user perspective both messages looks same.

.. code::

  - name: normal_enum
    enums:
      e1: {type: int16, enum: {A: 0, B: 1, C: 2}}
    fields:
      - {name: f1, type: e1}

  - name: inline_enum
    fields:
      - {name: f1, type: int16, options.type: enum, enum: {A: 0, B: 1, C: 2}}

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

Union
-----

A union represents several fields that are stored in the same location with only one present in a
time. It consists of 1 byte tag followed by shared body space with size equals to maximum of fields
sizes. Field indices are started from 0 so zeroed union is correct. New fields can be added to union
without breaking backward compatibility if size is not increased.

.. code::

  - name: Addr
    unions:
      IPAny: {union: [{name: ipv4, type: uint32}, {name: ipv6, type: byte16}]}
    fields:
      - {name: addr, type: IPAny}

.. _capnproto: https://capnproto.org/
.. _sbe: https://github.com/real-logic/simple-binary-encoding

..
    vim: sts=4 sw=4 et tw=100
