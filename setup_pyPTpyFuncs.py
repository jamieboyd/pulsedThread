# setup.py
from distutils.core import setup, Extension

setup(name='ptPyFuncs',
      author = 'Jamie Boyd',
      author_email = 'jadobo@gmail.com',
      ext_modules=[
        Extension('ptPyFuncs',
                  ['pyPTpyFuncs.cpp'],
                  include_dirs = ['./'],
                  library_dirs = ['./','/usr/local/lib'],
		  extra_compile_args=["-O3", "-std=gnu++11"],
                  libraries = ['pthread', 'pulsedThread'],
                  )
        ]
)
