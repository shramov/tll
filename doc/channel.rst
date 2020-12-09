Channel
=======

Channel is a communication abstraction that provides way to send and receive messages. It can be
used to handle simple IO or hide some complex protocols like AMQP. Providing generalized way to
handle communication gives user ability to switch between transports by reconfiguring some
initialization parameters. Also it allows easier testing by using special mock channels instead of
real protocol implementation.

Messages are received asynchronously via callback function. Choosing this way over synchronous
method allows easier integration in different event loops like ``libev``.

Channels can have arbitrary number of child objects.

State
-----

State of the channel object consists of life cycle state and capabilities.

Life cycle state of the channel is one of:

 - ``Closed``: initial state, object is closed and inactive, transitions to ``Opening`` or
   ``Destroy``.
 - ``Opening``: object is opening but not yet ready, transitions to ``Active`` or ``Error``;
 - ``Active``: object is fully functional, transitions to ``Closing`` or ``Error``;
 - ``Closing``: channel is closing, but not yet closed, transitions to ``Closed`` or ``Error``;
 - ``Error``: object is in failed state, no operations can be performed on it other then ``close``,
   transitions to ``Closing``;
 - ``Destroy``: signalling state, set when object is destroyed, since it's lifetime is very short
   is useful only in notification callbacks.

All states can be divided into inactive - ``Closed`` and ``Error``, when object is frozen and no
input or output operations can be performed, and active - ``Opening``, ``Active`` and ``Closing``.
``Opening`` and ``Closing`` states are specific to channel implementation and can support subset of
normal functionality.

Capabilities are divided into static ``caps`` and dynamic ``dcaps``. Static capabilities that don't
change during life of the object, for example ``Proxy`` cap on channels like rate limiter with
single child object or ``In`` and ``Out`` caps that reflect mode in which channel was open. In
contrast dynamic capabilities can change during operation of object, for example ``PollIn`` and
``PollOut`` dcaps requesting polling mode on associated file descriptor.

Initialization
--------------

Init stage consists of implementation lookup and actual initialization.

Lookup
~~~~~~

Implementation is looked up in context registry based on protocol part of url. If exact
match is not found then first part of protocol separated by ``+`` sign is searched. Only
implementation with ``prefix`` flag are used in second lookup.

However some cases need more complex lookup rules, for example ``tcp`` client and server can be
implemented separately and correct one is chosen according to ``mode=client`` or ``mode=server``
parameters of url.

Another example of complex lookup rule is creating channel written in some scripting language like
Python. ``python://;class=module:Class`` channel can import module ``module`` and initialize object
of type ``Class``. However it's not convenient to remember exact module name and it's better to use
short protocol names just like any other channel do, like ``pytcp://...``.

During initialization channel can return ``EAGAIN`` and replace ``impl`` field of object with
pointer to desired implementation. Bootstrap code checks that ``impl`` has changed and restarts
initialization with original url and new ``impl``. If ``impl`` is unchanged ``EAGAIN`` is treated
just like any other non-zero return code.

Opening
-------

Before any input or output operations can be preformed on the channel it has to be opened.
When channel is opening its state is changed from ``Closed`` to ``Opening`` immediatly and then to
``Active`` when it's fully functional. If something goes wrong state is changed to ``Error``.
So on return from ``open`` call channel can be in one of 3 states:

 - ``Opening``: objects is not yet fully functional.
 - ``Active``: object is ready.
 - ``Error``: error happened during open, object has to be closed before reopening.

Some channels, like ``tcp``, require processing to be able to transition to ``Active`` state.

Processing
----------

Processing performs internal housekeeping tasks of the channel like receiving messages from network,
executing callbacks for pending data or checking asynchronous connect status. It is never called in
inactive states (``Closed`` and ``Error``) and is needed only if ``Process`` dcap is set (calling
``process`` without this dcap is harmless but not recommended). Channels with long open or long
close usually need processing to finish ``Opening - Active`` or ``Closing - Closed`` state
transition.

Closing
-------

Close clears all resources allocated in open, like dynamic memory or file descriptors, and resets
dynamic caps to zero.

Closing, like opening, can be long operation but it's not required. If channel supports long close
it sets special static cap ``LongClose``.

Simpel example of an object with long close capabilities is ``lz4`` compression channel (or any
other prefix channel). It's state depends on child channel state. On close it calls child channel
``close`` function and then asynchronously waits for it completion.

.. note::

    Since handling long close is hard in some cases, every channel must support forced (immediate)
    ``close``.

When force closing channel, ``close`` call has to change state to ``Closed``, it can never fail. Its
needed, for example, when object is destroyed. Forced close can be called in any state of the
channel (even in ``Closed``, when it's silently ignored).

Normal ``close`` changes state to ``Closing`` and leaves channel in this state. Subsequent requests
for non-forced close are ignored. However forced close can be called during this stage.

Suspend
-------

In some cases receiving data from the channel must be temporary suspended. One of the examples is
rate limiting, when channel have to receive data from the child with configured speed. To help
implement such cases there is method to suspend and resume channel and all its child objects.
Suspended object is not processed and thus does not generate incoming messages.

Suspend sets special ``Suspend | SuspendPermanent`` dynamic capabilities on the object and
``Suspend`` on its whole child tree with exception to already suspended subtrees.

On resume reverse operation is performed - ``Suspend`` and ``SuspendPermanent`` dcap removed from
root object and ``Suspend`` dcap is cleared from child tree. Objects with ``SuspendPermament`` dcap
are skipped and their child tree are not resumed. This is needed to correctly handle cases where
child object was already suspended directly.

Pending data
------------

In some cases channel has more then one incoming message for example when receiving data from TCP
socket or reading from file. To allow effective usage of system calls (get all data from socket and
then process it) there is special ``Pending`` dynamic cap. It signals then channel has some pending
data and has to be processed without polling it's file descriptor for incoming events.

..
    vim: sts=4 sw=4 et tw=100
