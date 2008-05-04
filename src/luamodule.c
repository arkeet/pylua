#include "luamodule.h"
#include <Python.h>
#include <structmember.h>
#include <lua.h>
#include <lauxlib.h>

/* Debug functions **********************************************************/

#define LOG
#ifdef LOG
int Log(const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    vfprintf(stderr, format, ap);
    va_end(ap);
}
#else
int Log(const char *format, ...)
{
    return 0;
}
#endif
int LogObj(const char *str, void *o)
{
    int ret;
    PyObject *repr = PyObject_Repr((PyObject *)o);
    ret = Log("  Obj %s %s\n", str, PyString_AsString(repr));
    Py_XDECREF(repr);
    return ret;
}
int LogLuaTop(const char *str, lua_State *L)
{
    return Log("  Top %s %d\n", str, lua_gettop(L));
}

/* LuaFunction type *********************************************************/

static void LuaFunction_dealloc(LuaFunction *self)
{
    lua_State *L = self->lua->L;

    lua_pushlightuserdata(L, self);
    lua_pushnil(L);
    lua_rawset(L, LUA_REGISTRYINDEX);

    Py_DECREF(self->lua);
    self->ob_type->tp_free(self);
}

static int LuaFunction_init(LuaFunction *self, PyObject *args, PyObject *kwds)
{
    PyErr_SetString(PyExc_TypeError, "LuaFunction cannot be instantiated");
    return -1;
}

static PyObject *LuaFunction_call(LuaFunction *self, PyObject *args, PyObject
        *kwds)
{
    size_t n, i;
    int oldtop;
    PyObject *result;
    lua_State *L = self->lua->L;

    oldtop = lua_gettop(L);

    lua_pushpyfunction(L, self);
    if (lua_isnil(L, -1))
    {
        PyErr_SetString(PyExc_RuntimeError,
                "LuaFunction doesn't point to a function");
        lua_pop(L, 1);
        return NULL;
    }

    n = Lua_pushpython(self->lua, args);
    lua_pcall(L, n, LUA_MULTRET, 0);
    result = Lua_topython_multiple(self->lua, lua_gettop(L) - oldtop);
    lua_settop(L, oldtop);
    return result;
}

static PyObject *LuaFunction_get_lua(LuaFunction *self)
{
    Py_INCREF(self->lua);
    return (PyObject *)self->lua;
}

static PyGetSetDef LuaFunction_getset[] = {
    {"lua", (getter)LuaFunction_get_lua, NULL,
        "The Lua state object associated with the function.", NULL},
    {NULL}
};

static PyTypeObject LuaFunctionType = {
    PyObject_HEAD_INIT(NULL)
    0,                          /*ob_size*/
    "lua.LuaFunction",          /*tp_name*/
    sizeof(LuaFunction),        /*tp_basicsize*/
    0,                          /*tp_itemsize*/
    (destructor)LuaFunction_dealloc, /*tp_dealloc*/
    0,                          /*tp_print*/
    0,                          /*tp_getattr*/
    0,                          /*tp_setattr*/
    0,                          /*tp_compare*/
    0,                          /*tp_repr*/
    0,                          /*tp_as_number*/
    0,                          /*tp_as_sequence*/
    0,                          /*tp_as_mapping*/
    0,                          /*tp_hash */
    (ternaryfunc)LuaFunction_call, /*tp_call*/
    0,                          /*tp_str*/
    0,                          /*tp_getattro*/
    0,                          /*tp_setattro*/
    0,                          /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT,         /*tp_flags*/
    "Lua function objects",     /*tp_doc*/
    0,                          /*tp_traverse*/
    0,                          /*tp_clear*/
    0,                          /*tp_richcompare*/
    0,                          /*tp_weaklistoffset*/
    0,                          /*tp_iter*/
    0,                          /*tp_iternext*/
    0,                          /*tp_methods*/
    0,                          /*tp_members*/
    LuaFunction_getset,         /*tp_getset*/
    0,                          /*tp_base*/
    0,                          /*tp_dict*/
    0,                          /*tp_descr_get*/
    0,                          /*tp_descr_set*/
    0,                          /*tp_dictoffset*/
    (initproc)LuaFunction_init, /*tp_init*/
    0,                          /*tp_alloc*/
    PyType_GenericNew,          /*tp_new*/
};

/* LuaState type ************************************************************/

static void LuaState_dealloc(LuaState *self)
{
    lua_close(self->L);
    self->ob_type->tp_free(self);
}

static int LuaState_init(LuaState *self, PyObject *args, PyObject *kwds)
{
    self->L = luaL_newstate();
    return 0;
}

static PyMemberDef LuaState_members[] = {
    {NULL}
};

static PyObject *LuaState_openlibs(LuaState *self)
{
    luaL_openlibs(self->L);
    Py_RETURN_NONE;
}

static PyObject *LuaState_gettop(LuaState *self)
{
    return PyInt_FromSsize_t(lua_gettop(self->L));
}

static PyObject *LuaState_eval(LuaState *self, PyObject *args)
{
    char *code;

    int status;
    int oldtop, numresults;
    PyObject *result;

    if (!PyArg_ParseTuple(args, "s", &code))
        return NULL;

    oldtop = lua_gettop(self->L);

    if (luaL_loadstring(self->L, code))
    {
        PyErr_SetString(PyExc_SyntaxError, "error loading Lua code");
        return NULL;
    }

    if (lua_pcall(self->L, 0, LUA_MULTRET, 0))
    {
        lua_pushstring(self->L, "lua error: ");
        lua_insert(self->L, -2);
        lua_concat(self->L, 2);
        PyErr_SetString(PyExc_RuntimeError, lua_tostring(self->L, -1));
        lua_pop(self->L, 1);
        return NULL;
    }

    numresults = lua_gettop(self->L) - oldtop;

    result = Lua_topython_multiple(self, numresults);
    lua_pop(self->L, numresults);
    return result;
}

static PyObject *LuaState_getglobal(LuaState *self, PyObject *args)
{
    const char *name;
    PyObject *ret;
    if (!PyArg_ParseTuple(args, "s", &name))
        return NULL;
    lua_getglobal(self->L, name);
    ret = Lua_topython(self, -1);
    lua_pop(self->L, 1);
    return ret;
}

static PyObject *LuaState_setglobal(LuaState *self, PyObject *args)
{
    const char *name;
    PyObject *set;
    int num;

    if (!PyArg_ParseTuple(args, "sO", &name, &set))
        return NULL;
    num = Lua_pushpython(self, set);
    lua_pop(self->L, num - 1);
    lua_setglobal(self->L, name);
    Py_RETURN_NONE;
}

static PyMethodDef LuaState_methods[] = {
    {"openlibs", (PyCFunction)LuaState_openlibs, METH_NOARGS,
        "Load the Lua libraries."},
    {"gettop", (PyCFunction)LuaState_gettop, METH_NOARGS,
        "(debug) Gets the top index of the Lua stack."},
    {"eval", (PyCFunction)LuaState_eval, METH_VARARGS,
        "Run a piece of Lua code."},
    {"getglobal", (PyCFunction)LuaState_getglobal, METH_VARARGS,
        "Gets a global variable."},
    {"setglobal", (PyCFunction)LuaState_setglobal, METH_VARARGS,
        "Sets a global variable."},
    {NULL}
};

static PyTypeObject LuaStateType = {
    PyObject_HEAD_INIT(NULL)
    0,                          /*ob_size*/
    "lua.LuaState",             /*tp_name*/
    sizeof(LuaState),           /*tp_basicsize*/
    0,                          /*tp_itemsize*/
    (destructor)LuaState_dealloc, /*tp_dealloc*/
    0,                          /*tp_print*/
    0,                          /*tp_getattr*/
    0,                          /*tp_setattr*/
    0,                          /*tp_compare*/
    0,                          /*tp_repr*/
    0,                          /*tp_as_number*/
    0,                          /*tp_as_sequence*/
    0,                          /*tp_as_mapping*/
    0,                          /*tp_hash */
    0,                          /*tp_call*/
    0,                          /*tp_str*/
    0,                          /*tp_getattro*/
    0,                          /*tp_setattro*/
    0,                          /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT,         /*tp_flags*/
    "Lua state objects",        /*tp_doc*/
    0,                          /*tp_traverse*/
    0,                          /*tp_clear*/
    0,                          /*tp_richcompare*/
    0,                          /*tp_weaklistoffset*/
    0,                          /*tp_iter*/
    0,                          /*tp_iternext*/
    LuaState_methods,           /*tp_methods*/
    LuaState_members,           /*tp_members*/
    0,                          /*tp_getset*/
    0,                          /*tp_base*/
    0,                          /*tp_dict*/
    0,                          /*tp_descr_get*/
    0,                          /*tp_descr_set*/
    0,                          /*tp_dictoffset*/
    (initproc)LuaState_init,    /*tp_init*/
    0,                          /*tp_alloc*/
    PyType_GenericNew,          /*tp_new*/
};

/* Utility functions ********************************************************/

static void lua_pushpyfunction(lua_State *L, LuaFunction *f)
{
    lua_pushlightuserdata(L, f);
    lua_rawget(L, LUA_REGISTRYINDEX);
}

static int lua_fn_pygc(lua_State *L)
{
    PyObject *userdata;

    if (lua_gettop(L) != 1 || lua_isuserdata(L, 1))
    {
        lua_pushstring(L, "incorrect argument");
        lua_error(L);
    }
    else
    {
        userdata = lua_touserdata(L, 1);
        Py_DECREF(userdata);
    }
    return 0;
}

static PyObject *lua_topyobject(lua_State *L, int index)
{
    PyObject *result;

    if (!lua_isuserdata(L, index) || !lua_getmetatable(L, index))
    {
        return NULL;
    }
    lua_pushstring(L, "__gc");
    lua_gettable(L, -2);
    if (!lua_iscfunction(L, -1) || lua_tocfunction(L, -1) != lua_fn_pygc)
    {
        lua_pop(L, 2);
        return NULL;
    }
    lua_pop(L, 2);
    result = *(PyObject **)lua_touserdata(L, index);
    Py_XINCREF(result);
    return result;
}

static PyObject *Lua_topyfunction(LuaState *lua, int index)
{
    LuaFunction *f;
    lua_State *L = lua->L;

    if (index < 0)
        index = lua_gettop(L) + 1 + index;

    if (!lua_isfunction(L, index))
    {
        PyErr_SetString(PyExc_RuntimeError,
                "tried to get a function out of a value that wasn't a Lua function");
        return NULL;
    }

    Py_INCREF(lua);
    f = (LuaFunction *)LuaFunctionType.tp_alloc(&LuaFunctionType, 0);
    f->lua = lua;

    lua_pushlightuserdata(L, f);
    lua_pushvalue(L, index);
    lua_rawset(L, LUA_REGISTRYINDEX);

    return (PyObject *)f;
}

static int lua_fn_pycall(lua_State *L)
{
    PyObject *o, *args, *ret;
    LuaState *lua;
    int i, nargs;

    lua = (LuaState *)lua_touserdata(L, lua_upvalueindex(1));
    o = *(PyObject **)lua_touserdata(L, 1);
    if (!PyCallable_Check(o))
    {
        lua_pushstring(L, "Python object is not callable");
        lua_error(L);
        return 0;
    }
    nargs = lua_gettop(L) - 1;
    args = Lua_topython_tuple(lua, nargs);
    ret = PyObject_CallObject(o, args);
    Py_XINCREF(ret);
    return Lua_pushpython(lua, ret);
}

static int Lua_pushpython(LuaState *lua, PyObject *o)
{
    PyObject **userdata;
    lua_State *L = lua->L;

    if (o == NULL)
    {
        return 0;
    }

    if (PyTuple_Check(o))
    {
        int size = PyTuple_Size(o), i, n = 0;
        for (i = 0; i < size; ++i)
            n += Lua_pushpython(lua, PyTuple_GetItem(o, i));
        return n;
    }

    if (o == Py_None)
    {
        lua_pushnil(L);
        return 1;
    }
    if (PyBool_Check(o))
    {
        lua_pushboolean(L, o == Py_True);
        return 1;
    }
    if (PyInt_Check(o))
    {
        lua_pushinteger(L, PyInt_AsLong(o));
        return 1;
    }
    if (PyLong_Check(o))
    {
        lua_pushinteger(L, PyLong_AsLong(o));
        return 1;
    }
    if (PyFloat_Check(o))
    {
        lua_pushnumber(L, PyFloat_AsDouble(o));
        return 1;
    }
    if (PyString_Check(o))
    {
        char *buf;
        size_t len;
        PyString_AsStringAndSize(o, &buf, &len);
        lua_pushlstring(L, buf, len);
        return 1;
    }
    if (PyType_IsSubtype(o->ob_type, &LuaFunctionType))
    {
        lua_pushpyfunction(L, (LuaFunction *)o);
        return 1;
    }

    userdata = lua_newuserdata(L, sizeof(PyObject *));
    Py_INCREF(o);
    *userdata = o;
    lua_newtable(L);

    lua_pushstring(L, "__gc");
    lua_pushcfunction(L, lua_fn_pygc);
    lua_rawset(L, -3);

    lua_pushstring(L, "__call");
    lua_pushlightuserdata(L, lua);
    lua_pushcclosure(L, lua_fn_pycall, 1);
    lua_rawset(L, -3);

    lua_setmetatable(L, -2);
    return 1;
}

static PyObject *Lua_topython(LuaState *lua, int index)
{
    size_t len;
    const char *str;
    lua_State *L = lua->L;
    PyObject *result;

    switch (lua_type(L, index))
    {
        case LUA_TNONE:
            return NULL;
        case LUA_TNIL:
            Py_RETURN_NONE;
        case LUA_TNUMBER:
            return Py_BuildValue("d", lua_tonumber(L, index));
        case LUA_TBOOLEAN:
            if (lua_toboolean(L, index))
                Py_RETURN_TRUE;
            else
                Py_RETURN_FALSE;
        case LUA_TSTRING:
            str = lua_tolstring(L, index, &len);
            return Py_BuildValue("s#", str, len);
        case LUA_TTABLE:
            return Py_BuildValue("s", "lua table");
        case LUA_TFUNCTION:
            return Lua_topyfunction(lua, index);
        case LUA_TUSERDATA:
            result = lua_topyobject(L, index);
            if (result != NULL)
                return result;
            return Py_BuildValue("s", "lua userdata");
        case LUA_TTHREAD:
            return Py_BuildValue("s", "lua thread");
        case LUA_TLIGHTUSERDATA:
            return Py_BuildValue("s", "lua light userdata");
    }
}

static PyObject *Lua_topython_tuple(LuaState *lua, int n)
{
    PyObject *result;
    int i;

    result = PyTuple_New(n);
    for (i = 0; i < n; ++i)
        PyTuple_SetItem(result, i, Lua_topython(lua, i - n));
    return result;
}

static PyObject *Lua_topython_multiple(LuaState *lua, int n)
{
    if (n == 0)
        Py_RETURN_NONE;
    else if (n == 1)
        return Lua_topython(lua, -1);
    else
        return Lua_topython_tuple(lua, n);
}

/* lua module ***************************************************************/

static PyMethodDef lua_methods[] = {
    {NULL}
};

#ifndef PyMODINIT_FUNC
#define PyMODINIT_FUNC void
#endif

PyMODINIT_FUNC initlua()
{
    PyObject *m;

    if (PyType_Ready(&LuaStateType) < 0)
        return;
    if (PyType_Ready(&LuaFunctionType) < 0)
        return;

    m = Py_InitModule3("lua", lua_methods, "Lua bindings.");

    Py_INCREF(&LuaStateType);
    Py_INCREF(&LuaFunctionType);
    PyModule_AddObject(m, "LuaState", (PyObject *)&LuaStateType);
    PyModule_AddObject(m, "LuaFunction", (PyObject *)&LuaFunctionType);
}
