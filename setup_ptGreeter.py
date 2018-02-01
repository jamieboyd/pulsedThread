# setup.py
from distutils.core import setup, Extension

setup(name='ptGreeter',
      author = 'Jamie Boyd',
      author_email = 'jadobo@gmail.com',
      py_modules=['PTGreeter'],
      ext_modules=[
        Extension('ptGreeter',
                  ['Greeter.cpp', 'pyGreeter.cpp'],
                  include_dirs = ['./',  '/usr/include'],
                  library_dirs = ['./', '/usr/local/lib'],
		  extra_compile_args=["-O3", "-std=gnu++11"],
                  libraries = ['pulsedThread'],
                  )
        ]
)
