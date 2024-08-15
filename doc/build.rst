Building
========

.. note::

   Project contains debian build rules and control files that contains all following steps. If in
   doubt about dependencies or compile steps - consult ``debian/control``, ``debian/rules`` or CI
   rules for GitHub in ``.github/workflows/ci.yml``.

Following packages are required to build TLL:

 * meson_ build system
 * fmtlib_
 * libyaml_ for parsing configuration files
 * rapidjson_ for JSON parsing/composing
 * ``zlib`` compression library

Some packages are optional:

 * rhash_ enables hashed scheme lookup
 * spdlog_ for better logging backend
 * ``lz4`` compression library
 * ``gtest`` test framework
 * Python ``docutils`` to build HTML documentation
 * ``rst2pdf`` to build PDF documentation

Building core part:

.. code::

   meson build
   ninja -vC build

Python bindings require some additional packages to build and to run tests (all of them can be
install using PIP):

 * ``cython`` extension compiler
 * ``setuptools``
 * ``decorator`` package for asynctll module
 * ``mako`` to generate source code from data scheme
 * ``pytest``, ``pyyaml`` and ``lz4`` to run tests

To build python bindings and to run tests:

.. code::

   cd python
   python3 ./setup.py build_ext --inplace --library-dirs=../build/src --rpath=`pwd`/../build/src
   python3 -m pytest --log-level=DEBUG -v test/

.. _fmtlib: http://fmtlib.net/
.. _libyaml: https://pyyaml.org/wiki/LibYAML
.. _meson: http://mesonbuild.com/
.. _rapidjson: https://rapidjson.org/
.. _rhash: https://github.com/rhash/RHash
.. _spdlog: https://github.com/gabime/spdlog

..
    vim: sts=4 sw=4 et tw=100
