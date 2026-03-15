tll-channel-convert
===================

:Manual Section: 7
:Manual Group: TLL
:Subtitle: Message conversion prefix channel

Synopsis
--------

``convert+CHILD://PARAMS...;scheme=<SCHEME>;[dir=<r|w|...>]``


Description
-----------

Prefix channel that converts messages between two different data schemas: its own and the child's.
Common conversion implementation is used in this channel and others, like ``rotate+`` or
``resolve``.

Conversion is initialized when the child transitions to the ``Active`` state. If conversion is not
possible, the channel's behavior depends on the mode: in strict mode, the channel sets the ``Error``
state immediately; otherwise, it logs warnings for unconvertible messages and fails on the first
bad message.

Init parameters
~~~~~~~~~~~~~~~

``dir={r|w|rw|in|out|inout}`` (default empty) - initialize conversion only for desired direction,
for sending (``post``) or receiving (``process``). By default it respects ``In`` and ``Out`` caps
from the child channel and ignores direction that is not used.

``convert-fail-on={init|data}`` (default ``init``) - fail on activation, if conversion is not
possible, or on first data message, that can not be converted.

Conversion
----------

Fields in the message can be added (new fields are filled with zeroes), removed, reordered.
Following type changes are supported:

 - numeric types - all ``int*``, ``uint*`` and ``double`` conversion can be performed when value
   fits into destination type.
 - simple types can be converted from or into string field (``string`` or ``byte*, options.type:
   string``)
 - arrays (``type[size]``): can be resized or converted into offset pointers (``*type``)
 - offset pointer (``*type``): type can be changed or converted into fixed arrays (``type[size]``) if
   size fits
 - byte fields (``byte*``): can be resized, no other conversions are supported
 - enums (``int*, options.type: enum``): new values can be added, values can be changed for the same
   name. When value is not found in destination enum then conversion fails unless *source* enum has
   ``fallback-value`` option with the name of value in *destination* enum.
 - time types (numeric, ``options.type: {duration|time_point}``): can be converted between base
   types (if the value fits) and between resolutions (microseconds to nanoseconds, for example).
 - fixed point types (integer, ``options.type: fixed*``): can be converted to and from *plain*
   numeric types (``int*``, ``uint*``, ``double``).

See also
--------

``tll-channel-common(7)``
