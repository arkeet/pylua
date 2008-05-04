#!/usr/bin/env python

from lua import LuaState

def pydouble(x):
    return 2 * x

def main():
    lua = LuaState()
    print lua

    lua.openlibs()

    print '-- basics'
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
    lua.eval('function id(x) return x end')
    id = lua.eval('return id')
    print id
    print id(id)
    print id(6)
    #print id(lua)

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

    leaktest = False
    leaktest = True
    if leaktest:
        for i in xrange(1, 1000001):
            if (i % 10000 == 0):
                print '%d loops' % i
            a = lua.eval('return function(x, y) return x, y end')
            lua.eval('collectgarbage("collect")')
            lua.eval('return collectgarbage')

if __name__ == '__main__':
    main()
