Stat
====

Lockless (for data source) way to gather statistics inside program. Updates can done in
transactional manner so several updates can be done atomicaly.

Stat data is a list of fields containing value and self-descriptive part:

 - ``type`` : aggregation method, one of sum, min or max
 - ``name``: short 7 bytes name
 - ``unit``: value units, for example nanoseconds for time values or bytes for data amounts

Provider
--------

Main principle of stat subsystem is to provide lockless fast path for data source.
Typical workflow for provider consists of acquiring of stat page, updating and releasing it:

.. code:: c

  // declare stat block somewhere
  auto p = block.acquire();
  if (p != nullptr) { // Check is not needed for single-writer programs
      p.var0.update(1);
      p.var1.update(10);

      block.release(p);
  }

Acquire operation is implemented as single atomic exchange. It can return ``NULL`` only if
page is already busy so if your code has single writer ``NULL`` check
can be omitted.

Release just stores acquired value back into stat block.

Consumer
--------

All waiting is implemented on consumer side. When it wants to read data it has to swap
active and inactive pages. Swapping is not possible when page is locked by provider.

After processing all fields in inactive page are reset to default value.
