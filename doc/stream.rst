Stream channel
--------------

Stream channel is persistent data stream with ability to request stored messages from server.
It consists of two parts:

 - online subchannel with fresh messages
 - request subchannel with old messages that are read from storage

Message sequence numbers have to be monotonic but may be non-contiguous. For example 10, 20, 30 is
allowed, but 10, 30, 20 is not.

Server writes posted message to storage and then forwards it to child online channel. On client
request old data is fetched from storage and is sent via request channel.

Storage can aggregate messages so client get abbreviated history (if application protocol allows
it). However it's required that sequence numbers are not changed for messages, for example if list
of ``[0, 100]`` messages is aggregated then first aggregated seq must be in same range ``[0, 100]``
and last have to be 100 (last value).

Client lifecycle consists of several states:

 - reading old data from request channel (and storing online stream to internal ring buffer)
 - processing data from ring buffer, when history is overlapping with it; request channel is closed
   in this state
 - forwarding stream data when history is fully processed

..
    vim: sts=4 sw=4 et tw=100
