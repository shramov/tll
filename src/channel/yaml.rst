tll-channel-yaml
================

:Manual Section: 7
:Manual Group: TLL
:Subtitle: YAML reader channel

Synopsis
--------

``yaml://FILENAME;scheme=SCHEME;autoclose=<bool>;autoseq=<bool>;strict=<bool>``

::

  init:
    tll.proto: yaml
    config:
      - { seq: 0, message0...}
      - { seq: 1, message1...}


Description
-----------

Channel reads yaml file or embedded config and generates messages with meta info and body filled
from that configuration. Config consists of the list of message definitions in one of two possible
forms: when scheme is not present that binary body is used, otherwise message is composed from
separate fields.

Init parameters
~~~~~~~~~~~~~~~

``FILENAME`` - file with configuration in YAML format, either filename or ``config`` subtree must be
present.

``config`` - subtree in format described in ``Config Format`` section

``scheme=SCHEME``, default is none - data scheme that is used to fill messages from configuration.
If scheme is not present only binary body can be specified.

``scheme-control=SCHEME``, default is none - control scheme, used for ``type: control`` messages.

``autoclose=<bool>``, default ``yes`` - close channel when last message is read.

``autoseq=<bool>``, default ``no`` - use autoincremented sequence numbers starting from the first
message.

``strict=<bool>``, default ``yes`` - fields that are not present in message scheme are considered
error. Can be disabled when you have to use new data file with old scheme that lacks some new
fields.

Config format
~~~~~~~~~~~~~

Configuration consists of the list (or list-like structure) of messages. Each message has following
parameters:

``seq=<int>``, default 0 - sequence number of the message

``addr=<int>``, default 0 - message address

``type={data | control}``, default ``data`` - type of the message, ``Data`` or ``Control``. Separate
schemes are used to fill them.

``time=<string>``, default empty - time field of the message, value can be following:

 - empty or missing, use previous value;
 - ``now`` - use current time;
 - ``+<duration>`` - if value starts with ``+`` sign, parse rest of the string as duration (with
   resolution suffix like ``1ms``) and add to previous timestamp;
 - otherwise parse as datetime value in form ``YYYY-MM-DDThh:mm:ss[.subsec]``

If scheme is not present for this message type then follwing parameters are used:

``msgid=<int>``, default 0 - message id

``data``, mandatory - message body, passed as is without any decoding

If scheme is present:

``name=<string>``, mandatory - message name that is looked up in the scheme. Parsing is failed if
message is not found

``data`` - subtree with message fields :

  - messages (top level one too) are represented as subtrees: ``sub: {a: 1, b: 2}``, if field is
    missing from subtree then it place in the message is zeroed (techincaly whole message is memset
    to 0 before filling).
  - string values are used as is.
  - integer fields can be in decimal (``10``), hexidecimal (``0xA`` or ``0Xa``) format.
  - floating point fields (``double`` or ``fixedN``) can be in normal (``12.34``) or scientific
    (``1234.E-2``) format.
  - time duration fields are represented as integer or floating point base value (depending on field
    description) and mandatory resolution suffix: ``ns``, ``us``, ``ms``, ``s`` in their usual
    meaning, ``m``, ``h``, ``d`` for minute, hour or day.
  - time point fields are represented as extended ISO 8601 format: ``%Y-%m-%d[T or space]%H:%M:%S``
    followed by optional subsecond part with up to 9 digits.
  - enumeration fields can be either integer values or enumeration names
  - bits are represented with the list of integer values or bit names separated by ``|`` (with
    optional spaces). These values are binary ORed together to provide final value.
  - lists (pointers or arrays) are represented as lists or list-like dicts: ``list: [10, 20, 30]``
    is same as ``list: {'000': 10, '001': 20, '002': 30}``.
  - unions are reprsented as a subtree with one key that has name of the union field: ``union: {
    ufield: ... }``

Examples
--------

Generate 3 data and 1 control messages with sequence numbers starting from 10 without scheme:

::

  init:
    tll.proto: yaml
    autoseq: yes
    config:
      - { seq: 10, msgid: 10, data: aaa }
      - { msgid: 20, data: bbb }
      - { msgid: 30, data: ccc }
      - { type: control, msgid: 40, data: end }

Read messages from the file with simple scheme: ``yaml://file.yaml;scheme=SCHEME`` where ``SCHEME``
is following:

::

  - name: Data
    id: 10
    fields:
      - { name: float, type: double }
      - { name: ts, type: int64, options.type: time_point, options.resolution: us }
      - { name: duration, type: '*uint32', options.type: duration, options.resolution: ns }

file ``file.yaml`` contents:

::

  - name: Data
    seq: 0
    data:
      float: 12.34
      ts: 2000-01-02T03:04:05.123456
      duration: [10ns, 10us, 10ms]

See also
--------

``tll-channel-common(7)``

..
    vim: sts=4 sw=4 et tw=100
