#!/usr/bin/env python

from distutils.core import setup, Extension

setup(
        name = 'pylua',
        version = '0.1',
        description = 'Lua binding',
        author = 'Adrian Keet',
        author_email = 'demonthing@gmail.com',
        license = 'MIT',
        url = 'https://code.google.com/p/py-lua/',
        ext_modules=[Extension(
            'lua',
            ['src/luamodule.c'],
            libraries = ["lua"],
            )],
        )
