#!/usr/bin/env python

from lua import Lua

def pydouble(x):
    return 2 * x

def leaktest():
    global lua
    for i in xrange(10000000):
        #lua.eval('return function(x, y) return x, y end')
        lua.eval('collectgarbage("collect")')
        a = lua.eval('return pair')

def main():
    global lua
    lua = Lua()
    print lua

    lua.openlibs()

    lua.eval('a = 5')
    print lua.eval('return a')
    print lua.getglobal('a')
    lua.setglobal('a', 9)
    print lua.eval('return a')

    print '-- double'
    lua.eval('function double(x) return 2 * x end')
    print lua.eval('return double(2)')
    double = lua.eval('return double')
    print double
    print double.lua
    print double(2)

    print '-- id'
    id = lua.eval('return function(x) return x end')
    print id
    print id(6)
    print id(id)
    print id(lua)

    print '-- pair'
    lua.eval('function pair(x, y) return x, y end')
    pair = lua.eval('return pair')
    print pair(5)
    print pair(5, 6)
    print pair(5, 6, 8)

    print '-- pydouble'
    print pydouble
    lua.setglobal('pydouble', pydouble)
    print lua.getglobal('pydouble')
    lua.eval('print(pydouble)')
    lua.eval('print(pydouble(4))')
    print lua.eval('return pydouble(4)')

if __name__ == '__main__':
    main()
