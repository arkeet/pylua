#!/usr/bin/env python

import sys
from lua import LuaState

def pydouble(x):
    return 2 * x

def test(L):
    print L

    L.openlibs()

    print '-- basics'
    L.eval('a = 5')
    print L.eval('return a')
    print L.globals().a
    L.globals().a = 12
    print L.eval('return a')

    print '-- table'
    table = L.eval('''
        table = {i = 200, j = 300}
        return table
        ''')
    print table
    print table.j
    table.j = 400
    print table.j
    L.eval('''
        table.j = 500
        mt = {__call = function(t, i) return t[i] end}
        setmetatable(table, mt)
        ''')
    print table('i'), table('j'), table('k')
    print table['i'], table['j'], table['k']

    print '-- double'
    L.eval('function double(x) return 2 * x end')
    print L.eval('return double(2)')
    double = L.eval('return double')
    print double
    print double(2)

    print '-- id'
    L.eval('function id(x) return x end')
    id = L.eval('return id')
    print id
    print id(id)
    print id(6)
    print id(L)

    print '-- pair'
    L.eval('function pair(x, y) return x, y end')
    pair = L.eval('return pair')
    print pair(5)
    print pair(5, 6)
    print pair(5, 6, 8)

    print '-- pydouble'
    print pydouble
    L.globals().pydouble = pydouble
    print L.globals().pydouble
    L.eval('print(pydouble)')
    L.eval('print(pydouble(4))')
    print L.eval('return pydouble(4)')

    class foo(object):
        def pr(self):
            print self
            print self.a
            print self.b
    x = foo()
    x.a = 53

    print '-- python in lua'
    L.globals().x = x
    L.eval('print(x)')
    L.eval('print(x.pr)')
    try:
        L.eval('x.pr()')
    except RuntimeError, e:
        print e
    L.eval('x.a = 58 x.b = 59')
    L.eval('print(x.a) print(x.b)')
    L.eval('return x').pr()
    L.eval('print(x.pr)')
    L.eval('x.pr()')

def main():
    L = LuaState()

    leaktest = False
    #leaktest = True
    if leaktest:
        for i in xrange(1, 1000001):
            if (i % 10000 == 0):
                print '%d loops' % i
            test(L)
    else:
        test(L)

if __name__ == '__main__':
    main()
