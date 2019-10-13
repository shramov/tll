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

Life cycle state of the channel can be one of:

 - ``Closed``: initial state, object is closed and inactive, transitions to ``Opening`` or
   ``Destroy``.
 - ``Opening``: object is opening but not yet ready, transitions to ``Active`` or ``Error``;
 - ``Active``: object is fully functional, transitions to ``Closing`` or ``Error``;
 - ``Closing``: channel is closing, but not yet closed, transitions to ``Closed``;
 - ``Error``: object is in failed state, no operations can be performed on it other then ``close``,
   transitions to ``Closing``;
 - ``Destroy``: signalling state, set when object is destroyed, since it's lifetime is very short is
   useful only in notification callbacks.

Capabilities are divided into static ``caps`` and dynamic ``dcaps``. Static capabilities that don't
change during life of the object, for example ``Proxy`` cap on channels like rate limiter with
single child object or ``In`` and ``Out`` caps that reflect mode in which channel was open. In
contrast dynamic capabilities can change during operation of object, for example ``PollIn`` and
``PollOut`` dcaps requesting polling mode on associated file descriptor.

Processing
----------

Channel needs to be processed only if ``Process`` dcap is set.

Suspend
-------

In some cases receiving data from the channel must be temporary suspended. One of the examples is
rate limiting, when channel have to receive data from the child with configured speed. To help
implement such cases there is method to suspend and resume channel and all its child objects.
Suspended object is not processed and thus does not generate incoming messages.

Suspend sets special ``Suspend`` dynamic capability on the object and its whole child tree ignoring
already suspended subtrees. Also ``SuspendPermanent`` dcap is set on the channel which is suspended.

On resume reverse operation is performed - ``Suspend`` and ``SuspendPermanent`` dcap removed from root
object and ``Suspend`` dcap is cleared from child tree. However subtrees of ``SuspendPermament``
objects are skipped. This is needed to correctly handle cases where child object was already
suspended directly.
