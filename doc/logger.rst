Logger
======

Non-macro logging API similar to Python logging or other hierarcical interfaces.
Provides way to set your own logging method so code written in any language can use
common log.

C++ API uses fmt_ for formatting

.. code:: c

  tll::Logger log("a.b.c");
  log.info("Float: {.2f}", 1.1);

.. _fmt: http://fmtlib.net/
