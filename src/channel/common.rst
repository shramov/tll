tll-channel-common
==================

:Manual Section: 7
:Manual Group: TLL
:Subtitle: Common TLL channel parameters

Synopsis
--------

``protocol://[host];dump={no|yes|frame|...};scheme=PATH;stat={yes|no};...``

.. code-block:: yaml

  config:
    url: protocol://host
    dump: {yes|no}
    ...


Description
-----------

TLL channel is an object that implements simple state machine and asyncronous IO API. It is
constructed from the list of parameters that in simple cases can be represented as a string that
looks like URL or with config subtree if string is not suitable. Some parameters are common, like
statistics and dump switches, others are implementation specific.

Special class of *prefix* channels have one configurable child that is created inside parent object.
Such channels can be used for filtering, data modification or testing purposes. Prefix protocol ends
with ``+`` sign, for example ``lua+``. Full protocol is a concatenation of prefixes and normal one,
like ``timeit+lua+null://`` to measure time of posts through lua filter.

Parameter names
---------------

Each channel type has parameter prefix that is in most cases is equal to protocol string (``tcp``
for ``tcp://`` channels). Parameter lookup is performed in the following order:

  - ``prefix.name`` and ``name`` from initialization dictionary
  - ``prefix.name`` from context-wide defaults

Prefixed form can be used to pass common parameter to specific part of channel chain, for example
for ``lua+file://`` filter channel, where ``lua+`` has ``file`` child, ``file.dump=yes`` enables
dump only for inner file channel.

Value types
-----------

Most of parameters are parsed in a same way depending on their type:

``str`` string value, used without modifications

``bool`` boolean flag, possible values are ``{yes|true|1|no|false|0}``

``integer`` signed integer value, decimal or hexidecimal with ``0x`` or ``0X`` prefix

``unsigned`` unsigned integer value, same as ``integer`` but negative values are not allowed

``float`` fractional value, either in normal ``0.123`` or scientific ``123E-3`` notations

``size`` size value, integer (or in some cases float) value followed by unit suffix: ``b`` for
bytes, ``kb`` for kilobytes (1024 bytes), ``mb`` for megabytes (1024 * 1024 bytes) and ``gb`` for
gigabytes

``duration`` integral (or float if specified in documentation) value followed by time suffix:
``ns``, ``us``, ``ms``, ``s`` in their usual meaning, ``m``, ``h``, ``d`` for minute, hour or day.

``scheme`` scheme url that is loaded through channel context:

  - normal scheme url, like ``yaml://file.yaml``, ``yamls://{inline-yaml}`` or
    ``yamls+gz://COMPRESSED..``
  - scheme hash of already loaded scheme: ``sha256://HASH``
  - reference ot other channel: ``channel://name``

``url`` set of initialization parameters, mostly used for child channels. Can be specified in three
different ways:

  * scalar url string value: ``param=null://``, since ``;`` is not escaped it is not possible to
    use this variant to embed list of parameters inside other string url. Yaml configuration is not
    converted to intermediate string so it's ok to pass longer values: ``param: null://;stat=yes``
    is possible to pass long values, since 
  * broken down parameters url: ``param.tll.proto=null;param.stat=yes`` or config equivalent
    ``param: {tll.proto: null, stat: yes}``
  * mixed variant: ``param: {url: null://, stat: yes}`` or in string variant
    ``param.url=null://;param.stat=yes``

Common parameters
-----------------

``name=<str>`` (default ``noname``) - channel name, log messages are emited using
``tll.channel.{name}``, child objects are created with ``{name}/`` prefix.

``stat=<bool>`` (default ``no``) - enable gathering of runtime statistics for the channel. Basic
stat counts number of messages and sum of their body sizes both posted and received. Channel
implementation can extend statistics with other values, for example sequence number of last message
or average time used for post.

``dump={no|yes|frame|auto|scheme|text+hex}`` log every sent and received message

 - ``no`` disable logging
 - ``yes`` or ``auto`` log unpacked message body if scheme is available, hex otherwise
 - ``frame`` log only meta information - size, msgid, address and body size
 - ``scheme`` (deprecated) - always try to log unpacked message body
 - ``text+hex`` log body hex side by side printable part like normal hexdump tools

If full message logging is enabled (with or without scheme) body is written into logs which can leak
sensitive fields like passwords. However formatting function respects ``tll.secret: yes`` field
option and replaces value with ``*`` for strings or zero value for numbers.

``scheme=<scheme>`` (default is none) - specify data scheme for the channel, format is described
above

``scheme-cache=<bool>`` (default is true) - do not use scheme cache in channel context. By default
loading same scheme second time will return result from the cache.

``dir={r|w|rw|in|out|inout}`` (default depends on the channel) - create channel in read, write or
readwrite mode. This parameter is not universal and is meaningful only for subset of channels.

``fd=<bool>`` (default true) - disable usage of signalling file descriptor for this channel and all
its child objects. This flag is used by processor when worker is configured in spin mode so in most
cases it should not be specified manualy.

Examples
--------

Simple parameter string:

``null://;name=null``

Complex channel with mixed syntax and parameter prefixes (``lua.dump`` and ``yaml.dump``) to enable
dump both for parent and child channels, ``code`` parameter is using yaml literal syntax to preserve
line breaks:

.. code-block:: yaml

  lua:
    url: lua+yaml://
    yaml.dump: yes
    lua.dump: yes
    scheme: yaml://scheme.yaml
    config:
      - {name: Message, seq: 10, fields: {field0: 10}}
    code: |
      function tll_on_data(seq, name, data)
        if seq % 2 == 0 then
          tll_callback(seq, name, data)
        end
      end

..
    vim: sts=4 sw=4 et tw=100
