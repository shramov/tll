tll-channel-random
==================

:Manual Section: 7
:Manual Group: TLL
:Subtitle: Generate random data payload

Synopsis
--------

``random+CHILD://PARAMS...;min=<SIZE>;max=<SIZE>;data-mode={seq|pattern|random}``


Description
-----------

Prefix channel that replaces on each message from the child channel produces new one with random
size and/or data.

Init parameters
~~~~~~~~~~~~~~~

``min=<SIZE>`` (default ``100b``) - minimum size of data payload. Each message will have it's size
from uniform integer generator in range ``[min, max]`` (including both ``min`` and ``max``).

``max=<SIZE>`` (default ``500b``) - maximum size of data payload.

``data-mode={seq|random|pattern}`` (default ``seq``) - how to fill payload:

 - ``seq`` - sequential data, N-th byte will be ``N % 256``;
 - ``pattern`` - fill data with repeated 64 bit pattern (by default ``0``);
 - ``random`` - fill data with random data.

``pattern=<uint64>`` (default ``0``) - 64 bit pattern used to fill payload in ``pattern`` data mode.

``validate=<bool>`` (default ``no``) - validate posted data, not available if ``data-mode`` is
``random``. Otherwise check that message is filled correctly (with sequential data or pattern).
However size is not checked.

Examples
--------

Generate message with random body and fixed size each second: ::

    random+timer://;interval=1s;min=1kb;max=1kb;data-mode=random

Generate 100kbit data stream with message size from 200 to 500 bytes filled with pattern (parameters
in processor format): ::

  init:
    tll.proto: rate+random+zero
    rate:
      speed: 100kbit
      dir: in
    random:
      min: 200b
      max: 500b
      data-mode: pattern
      pattern: 0xdeadbeefdeadbeef

See also
--------

``tll-channel-common(7)``

..
    vim: sts=4 sw=4 et tw=100
