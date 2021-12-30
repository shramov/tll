#!/usr/bin/env python3
# vim: sts=4 sw=4 et

from distutils.core import setup
from distutils.extension import Extension
from Cython.Distutils import build_ext

setup( name = 'tll'
     , packages = ['tll', 'tll.channel', 'tll.processor', 'tll.templates']
     , scripts = ['tll-pyprocessor', 'tll-schemegen']
     , include_dirs = ["../src"]
     , cmdclass = {'build_ext': build_ext}
     , ext_modules =
         [ Extension("tll.s2b", ["tll/s2b.pyx"], libraries=["tll"])
         , Extension("tll.decimal128", ["tll/decimal128.pyx"], libraries=["tll"])
         , Extension("tll.channel.common", ["tll/channel/common.pyx"], libraries=["tll"])
         , Extension("tll.channel.channel", ["tll/channel/channel.pyx"], libraries=["tll"])
         , Extension("tll.channel.context", ["tll/channel/context.pyx"], libraries=["tll"])
         , Extension("tll.channel.base", ["tll/channel/base.pyx"], libraries=["tll"])
         , Extension("tll.config", ["tll/config.pyx"], libraries=["tll"])
         , Extension("tll.logger", ["tll/logger.pyx"], libraries=["tll"])
         , Extension("tll.processor.loop", ["tll/processor/loop.pyx"], libraries=["tll"])
         , Extension("tll.processor.processor", ["tll/processor/processor.pyx"], libraries=["tll"])
         , Extension("tll.scheme", ["tll/scheme.pyx"], libraries=["tll"])
         , Extension("tll.stat", ["tll/stat.pyx"], libraries=["tll"])
         ]
     , package_data = { 'tll': ['templates/*.mako'] }
     , classifiers =
        [ 'Intended Audience :: Developers'
        , 'License :: OSI Approved :: MIT License'
        , 'Operating System :: OS Independent'
        ]
)
