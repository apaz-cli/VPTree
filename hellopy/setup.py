#!/usr/bin/env python3
# encoding: utf-8

from distutils.core import setup, Extension
import sysconfig

print(sysconfig.get_config_var('CFLAGS').split())

hello_module = Extension('hello', sources=['hello.c'], extra_compile_args=[
                         '-lm', '-lpthread', '-fno-stack-protector', '-D_FORTIFY_SOURCE=0', '-g0', '-Ofast', '-ftree-vectorize', '-msse'])

setup(name='hello',
      version='0.1.0',
      platforms=['POSIX'],
      license='GNU Lesser General Public License v2 or later (LGPLv2+)',
      description='Hello world module written in C',
      ext_modules=[hello_module])
