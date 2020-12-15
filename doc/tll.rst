TLL
###

.. warning::
  This library is a work in progress. API may change, repository may be rebased.

TLL is a framework for building data processing services. It's main goal is to provide
abstractions with minimal overhead for network (or IPC) connections.

Library has pure C ABI and both C and C++ API. C++ API is implemented as header-only wrappers
around C functions for those reasons:

- C API is needed for language bindings
- C API is more stable and does not depend on C++ version used at compile time
- header-only C++ API removes whole class of errors where C and C++ functions do different things


.. include::
        logger.rst

.. include::
        stat.rst

.. include::
        scheme.rst

.. include::
        channel.rst

.. include::
        processor.rst
