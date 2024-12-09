tll-logic-forward
=================

:Manual Section: 7
:Manual Group: TLL
:Subtitle: Message forwarding logic

Synopsis
--------

::

    tll.proto: forward
    tll.channel.input: <input-list>
    tll.channel.output: <output-list>

Defined in module ``tll-logic-forward``


Description
-----------

Logic forwards all data messages from input to output preserving all metadata like sequence number
or address. Size of input and output channel list must be equal. Data is forwarded from n-th element
of input list are sent to n-th element of output list.

Channels
~~~~~~~~

``input`` - list of inputs, can contain several channels.

``output`` - list of outputs, same size as inputs

Init parameters
~~~~~~~~~~~~~~~

Logic has no init parameters.

Examples
--------

Echo back everything client has sent to tcp server:

::

  processor.module:
    - module: tll-logic-forward

  processor.objects:
    forward:
      init: forward://
      channels: {input: tcp, output: tcp}
    tcp:
      init: tcp://*:2222;mode=server;frame=none;dump=frame;af=ipv4
      depends: forward

Duplicate data into several outputs::

  processor.objects:
    forward:
      init: forward://
      channels: {input: "source,source", output: "out-0,out-1"}
      depends: out-0, out-1
    source:
      init: rate+zero://;size=1kb;speed=128kb;dir=r
      depends: forward
    out-0:
      init: null://
    out-1:
      init: null://

See also
--------

``tll-logic-common(7)``

..
    vim: sts=4 sw=4 et tw=100
