tll-channel-file
================

:Manual Section: 7
:Manual Group: TLL
:Subtitle: File storage channel

Synopsis
--------

``file://FILENAME;dir={r|w|in|out};io={posix|mmap}``


Description
-----------

Channel implements file storage for reading and writing. Data is stored in sequential blocks so
search with logarithmic time is supported (however inside block linear search is used). File
contains metadata with scheme, block size and compression type (none or lz4) so when user
opens it for reading he does not need to know exact parameters used to create this file. New data is
appended to the end of the file and there is no way to change old entries.

Init parameters
~~~~~~~~~~~~~~~

``FILENAME`` - filename of the storage, if empty - ``filename`` open parameter is used.

``dir={r|w|in|out}`` (default ``r``) - file mode, read or write. By default channel is opened in
read mode.

``io={posix|mmap}`` (default ``posix``) - IO type used inside: ``posix`` is based on ``read`` and
``write`` syscalls, ``mmap`` uses mapped memory to read and write data.

``scheme={SCHEME}`` - file scheme, ignored in read mode if present in metadata.

``access-mode=<unsigned>`` - file mode, default 0644 (octal, ``rw-r--r--``) - access bits set to
newly created file, can not be greater then 0777.

Write init parameters
^^^^^^^^^^^^^^^^^^^^^

Following init parameters are used only for writing channel (``dir=w``):

``autoseq=<bool>`` (default ``no``) - ignore message seq and fill it with contiguous incrementing
sequence. When file is reopened - use last seq as starting point, otherwise start from 0;

``block={SIZE}`` (default ``1mb``) - block size, used only in write mode.

``extra-space={SIZE}`` (default ``0b``) - keep up to this amount of empty space in the end of the
file. Without this non-zero option file can not be read in ``mmap`` mode. Used only in write mode.

``compression={none|lz4}`` (default ``none``) - compression method:

 - ``none`` - compression is disabled
 - ``lz4`` - lz4 compression in streaming mode, when message is appended message to the block
   its current content is used for compression.

Read init parameters
^^^^^^^^^^^^^^^^^^^^

Following init parameters are used only for reading channel (``dir=r``):

``autoclose=<bool>`` (default ``yes``) - close file when last message is read.

Open parameters
~~~~~~~~~~~~~~~

``filename=FILENAME`` - if init ``FILENAME`` parameter is empty, use this one. This way one channel
can be used to read different files without creating new objects.

``overwrite=<bool>`` (default ``no``) - drop old file and replace it with new one, only in write
mode.

``seq=<UNSIGNED>`` - start reading from specified ``SEQ``, if missing - read from file start. Only
in read mode.

Control messages
----------------

Control scheme is present only in read mode and contains two messages: ``EndOfData`` to signal that
all data is read (if autoclose is disabled) and ``Seek`` to jump to new position in the file that
has seq number greater or equal to ``msg->seq`` of the message.

.. code-block:: yaml

  - name: Seek
    id: 10
  - name: EndOfData
    id: 20

``EndOfData`` is generated only once, even if new data is written and then read by channel.

Config variables
----------------

File channel exposes following variables in its config info subtree:

 - ``seq`` - seq number of last message, -1 for empty files
 - ``seq-begin`` - seq number of first message, -1 for empty files
 - ``block`` - block size with suffix, for example ``1mb``
 - ``compression`` - file compression mode, same as ``compression`` init parameter

Examples
--------

Create file channel with block size of ``512kb`` and 2 blocks of empty space in the end:

::

    file:///tmp/file.dat;block=512kb;extra-space=1mb

Read file using ``mmap`` and without autoclose:

::

    file://tmp/file.dat;io=mmap;autoclose=no

See also
--------

``tll-channel-common(7)``

..
    vim: sts=4 sw=4 et tw=100
