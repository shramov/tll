Keyring
=======

Keyring API is a limited wrapper around Linux keyrings that supports following concepts:

 - reading and writing keys;
 - creating and deleting keyrings;
 - loading keys from the file into the keyring;
 - reading key reference that can point to the key or hold inline secret for test purposes;

Keys and Keyrings
-----------------

Key is an object with name, which is a printable string, and actual content that is stored inside
kernel. Other properties, like access rights or expiration time, are not exposed.

Keyring is a "directory" that contains keys or other keyrings and is linked to one or more parent
keyrings. Number of predefined keyrings exists:

 - User: shared by all processes with same real UID, lives as long as any such processes exists;
 - Session: same as user, but limited by processes of a login session;
 - Process: process-local, dies with it and unaccessable by other processes;
 - Thread: thread-local, same as process but for a thread;

When key is requested it is searched from thread up to a user keyrings. Full documentation is in
`keyrings(7) <https://man7.org/linux/man-pages/man7/keyrings.7.html>`_ manpage.

Loading files
-------------

To limit code that deals with sensitive data there is additional function that loads keys from the
file. Format is lines with ``name: value`` entries where name and where are separated by colon
(``:``) and value is stripped of whitespaces. Lines starting with ``#`` are comments.

Key reference
-------------

Key reference is a URI that describes how key is stored:

 - ``key:name``: key with identifier ``name``
 - ``data:payload``: inline key with body ``payload``, for testing purposes

Parsing can be done either in strict or in compat mode, which is needed for migration from plain
password to key references:

 - strict parsing requires known scheme name, either ``key`` or ``data``
 - compat treats strings without ``:`` separator or with invalid scheme name as prefixed with
   ``data:``

C++ API
-------

``KeyRef`` class implements two operations: parsing of ref string and loading secret from keyring
if needed.

Parsing is done either by directly calling ``KeyRef::parse`` static function or, preferred way, via
``tll::conv::to_any`` integrated with ``getT`` method: ``reader.getT<tll::util::KeyRef>("secret");``
for strict mode and ``tll::util::KeyRefCompat`` for compat mode.

``keyref.read()`` function tries to read key from the keyring (for ``key`` method) or returns stored
data (for ``data``).

Converting old code
~~~~~~~~~~~~~~~~~~~

Typical code that reads password from init parameters and stores it in a ``_password`` member and
uses it in open:

.. code:: c++

    // Declaration
    std::string _password;

    // Init
    auto reader = channel_props_reader(cfg);
    _password = reader.getT<std::string>("password");
    if (!reader)
        return _log.fail(std::nullopt, "Invalid init parameters: {}", reader.error());

    // Open
    external_api(_password.c_str());

New code should use ``tll::util::KeyRef`` to store key reference and load it when secret is needed.
For transition to new configuration format ``tll::util::KeyRefCompat`` is used. When all password
settings are converted to new ``key:ref-name`` format ``KeyRefCompat`` should be replaced with
normal ``KeyRef``.

.. code:: c++

    // Declaration
    tll::util::KeyRef _password;

    // Init
    auto reader = channel_props_reader(cfg);
    _password = reader.getT<tll::util::KeyRefCompat>("password");
    if (!reader)
        return _log.fail(std::nullopt, "Invalid init parameters: {}", reader.error());

    // Open
    if (auto secret = _password.read(); secret)
        external_api(secret->c_str());
    else
        return _log.fail(EINVAL, "Failed to load secret: {}", secret.error());
