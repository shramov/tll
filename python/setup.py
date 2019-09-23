#!/usr/bin/env python3
# vim: sts=4 sw=4 et

from distutils.core import setup
from distutils.extension import Extension
from Cython.Distutils import build_ext

setup( name = 'tll'
     , packages = ['tll']
     , include_dirs = ["../src"]
     , cmdclass = {'build_ext': build_ext}
     , ext_modules =
         [ Extension("tll.s2b", ["tll/s2b.pyx"], libraries=["tll"])
         , Extension("tll.config", ["tll/config.pyx"], libraries=["tll"])
         , Extension("tll.logger", ["tll/logger.pyx"], libraries=["tll"])
         , Extension("tll.scheme", ["tll/scheme.pyx"], libraries=["tll"])
         , Extension("tll.stat", ["tll/stat.pyx"], libraries=["tll"])
         ]
     , classifiers =
        [ 'Intended Audience :: Developers'
        , 'License :: OSI Approved :: MIT License'
        , 'Operating System :: OS Independent'
        ]
)
