File Channel
============

Storage channel that writes sequential stream of messages to append-only file on the disk. File
supports seeks into the middle without iterating from the beginning and can be opened for reading
from arbitrary message.

File Format
-----------

File is a sequence of fixed size blocks. Message can not cross block boundary. Data is appended to
file so there can not be empty block followed by block with data. However it's not an error if file
has some trailing empty blocks. Blocks are used for seeking with ``log(count-of-blocks) +
messages-in-block`` complexity.

Each message is prefixed with 4-byte frame that stores message size and size of frame itself. Zero
frame indicates that there is no data written yet and is considered as end of file. Special ``skip
frame`` with value of ``-1`` is used to indicate that there is no data left in this block. If
message is too large to fit into rest of block new block is started and space left in old is
marked as unused with ``skip frame``.

Each block starts with metadata message. If it's not needed only one frame (4 bytes) of space is
wasted. First metadata message in the file contains block size, compression type, optional message
scheme and list of user defined attributes. Metadata in other blocks is smaller and can be used to
store sequence number of first message in block to allow optimizations for monotonic data without
sequence gaps.

..
    vim: sts=4 sw=4 et tw=100
