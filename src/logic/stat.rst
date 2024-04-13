tll-logic-stat
==============

:Manual Section: 7
:Manual Group: TLL
:Subtitle: Statistics reporting logic

Synopsis
--------

::

    tll.proto: stat
    tll.channel.timer: <timer-channel>
    secondary: <bool>
    node: <string>
    page.<name>: { match: <regex>, skip: <bool>, logger: <name> }

Defined in module ``tll-logic-stat``

Description
-----------

Logic logs and resets statistics gathered in current channel context on each timer event.

Stat gathering is enabled by ``stat=yes`` init parameter on channels and is off by default.

Channels
~~~~~~~~

``timer`` - source of timer events, data and message type is not checked.

Init parameters
~~~~~~~~~~~~~~~

``node=<string>`` (default empty) - node name set in generated messages.

``header-level={debug|info}`` (default ``debug``) - log level of the message that is printed before
each page dump.

``secondary=<bool>`` (default ``false``) - skip stat pages that are not explicitly listed in
``page.*`` parameters.

``page.*`` - list of page matching rules:

``match=<regex>`` - regular expression that is matched against stat page name

``skip=<bool>`` (default ``false``) - skip matched pages

``logger=<name>`` (default empty): rule for logging facility name:

 - empty (or missing key), use logic logger
 - string startig with dot, like ``.suffix`` - append it to logic logger
 - otherwise create new logger with specified name

Output scheme
-------------

One each timer tick for each stat page logic generates ``Page`` message defined in scheme
``yaml://tll/logic/stat.yaml``:

::

  - enums:
      Unit: {type: uint8, enum: { Unknown: 0, Bytes: 1, NS: 2}}
      Method: {type: uint8, enum: { Sum: 0, Min: 1, Max: 2, Last: 3}}

  - name: IValue
    fields:
      - {name: method, type: Method}
      - {name: value, type: int64}

  - name: FValue
    fields:
      - {name: method, type: Method}
      - {name: value, type: double}

  - name: IGroup
    fields:
      - {name: count, type: uint64}
      - {name: min, type: int64}
      - {name: max, type: int64}
      - {name: avg, type: double}

  - name: FGroup
    fields:
      - {name: count, type: uint64}
      - {name: min, type: double}
      - {name: max, type: double}
      - {name: avg, type: double}

  - name: Field
    fields:
      - {name: name, type: byte7, options.type: string}
      - {name: unit, type: Unit}
      - name: value
        type: union
        union:
          - {name: ivalue, type: IValue}
          - {name: fvalue, type: FValue}
          - {name: igroup, type: IGroup}
          - {name: fgroup, type: FGroup}

  - name: Page
    id: 10
    fields:
      - {name: node, type: string}
      - {name: name, type: string}
      - {name: fields, type: '*Field'}

Examples
--------

Dump statistics each second and write it to the file:

::

  processor.module:
    - module: tll-logic-stat
    - module: tll-logic-forward

  processor.objects:
    file:
      init: file://stat.dat;dir=w;scheme=yaml://tll/logic/stat.yaml
    forward:
      init: forward://
      channels: {input: stat, output: file}
    stat:
      init: stat://
      channels: {timer: timer}
    timer:
      init: timer://;interval=1s
      depends: stat, file, forward

See also
--------

``tll-logic-common(7)``

..
    vim: sts=2 sw=2 et tw=100
