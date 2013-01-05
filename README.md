PyLua is a Python module that lets you run Lua code. It also lets Python code
and Lua code interact seamlessly.

Installation
------------

Currently, PyLua only supports Python 2.x. Building it requires Python and Lua
libraries to be installed.

```sh
python2 setup.py install
```

Example
-------

Using this module is very easy.

```python

from lua import LuaState

# Initialization

L = LuaState()                      # Create a Lua state object
L.openlibs()                        # Load the standard Lua libraries

# Basics

L.eval('a = 5')                     # Run Lua code.
L.eval('print(a)')                  # Prints "5", as one would expect.
print L.eval('return a')            # You can return values too. Prints "5.0"
print L.globals().a                 # This also prints "5.0"

print L.eval('return 4, "x"')       # Returning multiple values: "(4.0, 'x')"

L.globals().a = 8                   # Passing Python values to Lua
L.eval('print(a)')                  # Prints "8"

# Functions

def twice(x): return 2*x
L.globals().twice = twice           # Give Lua a Python function.
L.eval('print(twice(3))')           # Prints "6"

L.eval('function thrice(x) return 3*x end')
thrice = L.globals().thrice         # Get a Lua function into Python
print thrice(3)                     # Prints "9.0"

# Python objects in Lua

class hello(object):
    def __init__(self, name): self.name = name
    def greet(self): print "Hello, my name is", self.name

foo = hello("Foo")
L.globals().foo = foo               # Give Lua a Python object.
L.eval('foo.greet()')               # Prints "Hello, my name is Foo"

L.globals().hello = hello           # Give Lua the class itself
L.eval('bar = hello("Bar")')        # Make a Python object within Lua
L.eval('bar.greet()')               # Prints "Hello, my name is Bar"

bar = L.eval('return bar')          # Hand the object back to Python intact
print type(bar)                     # Prints "<class '__main__.hello'>"

# Lua objects in Python

L.eval('baz = {one = "uno", two = 2}')
baz = L.globals().baz               # Give a Lua table to Python
print baz.one, baz.two              # Prints "uno 2.0"

L.eval('setmetatable(baz, {__call = function(self) return self.one end})')
print baz()                         # It all works. This prints "uno"

```
