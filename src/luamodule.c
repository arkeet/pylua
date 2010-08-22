#include "luamodule.h"
#include <Python.h>
#include <structmember.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#define PYOBJECT "PyObject"

PyThreadState *_save;

/* Debug functions **********************************************************/

//#define LOG
#ifdef LOG
void Log(const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    vfprintf(stderr, format, ap);
    va_end(ap);
}
#else
void Log(const char *format, ...)
{
}
#endif
void LogObj(const char *str, void *o)
{
    PyObject *repr = PyObject_Repr((PyObject *)o);
    Log("  Obj %s %s\n", str, PyString_AsString(repr));
    Py_XDECREF(repr);
}
void LogLuaTop(const char *str, lua_State *L)
{
    Log("  Top %s %d\n", str, lua_gettop(L));
}

/* LuaObject type *********************************************************/

static void LuaObject_dealloc(LuaObject *self)
{
    lua_State *L = self->lua->L;

    lua_pushlightuserdata(L, self);
    lua_pushnil(L);
    lua_rawset(L, LUA_REGISTRYINDEX);

    Py_DECREF(self->lua);
    self->ob_type->tp_free(self);
}

static int LuaObject_init(LuaObject *self, PyObject *args, PyObject *kwds)
{
    PyErr_SetString(PyExc_TypeError, "LuaObject cannot be instantiated");
    return -1;
}

static PyObject *LuaObject_call(LuaObject *self, PyObject *args, PyObject
        *kwds)
{
    size_t n;
    int oldtop;
    PyObject *result;
    lua_State *L = self->lua->L;

    oldtop = lua_gettop(L);

    lua_pushluaobject(L, self);
    if (!lua_iscallable(L, -1))
    {
        PyErr_SetString(PyExc_ValueError, "this LuaObject isn't callable");
        lua_settop(L, oldtop);
        return NULL;
    }

    n = Lua_pushpyobject_tuple(self->lua, args);
    lua_pcall(L, n, LUA_MULTRET, 0);
    result = Lua_topython_multiple(self->lua, lua_gettop(L) - oldtop);
    lua_settop(L, oldtop);
    return result;
}

static PyObject *LuaObject_getattro(LuaObject *self, PyObject *name)
{
    if (PyString_Check(name))
    {
        const char *str;
        str = PyString_AsString(name);
        if (strncmp(str, "__", 2) == 0)
            return PyObject_GenericGetAttr((PyObject *)self, name);
    }

    return LuaObject_subscript(self, name);
}

static int LuaObject_setattro(LuaObject *self, PyObject *name, PyObject *o)
{
    if (PyString_Check(name))
    {
        const char *str;
        str = PyString_AsString(name);
        if (strncmp(str, "__", 2) == 0)
            return PyObject_GenericSetAttr((PyObject *)self, name, o);
    }

    return LuaObject_ass_subscript(self, name, o);
}

static PyObject *LuaObject_subscript(LuaObject *self, PyObject *ss)
{
    PyObject *result;
    lua_State *L = self->lua->L;

    lua_pushluaobject(L, self);
    if (!lua_isindexable(L, -1))
    {
        lua_pop(L, 1);
        PyErr_SetString(PyExc_ValueError, "this LuaObject is not indexable");
        return NULL;
    }
    Lua_pushpyobject(self->lua, ss);
    lua_gettable(L, -2);
    result = Lua_topython(self->lua, -1);
    lua_pop(L, 2);
    return result;
}

static int LuaObject_ass_subscript(LuaObject *self, PyObject *ss, PyObject *o)
{
    lua_State *L = self->lua->L;

    lua_pushluaobject(L, self);
    if (!lua_isindexable(L, -1))
    {
        lua_pop(L, 1);
        PyErr_SetString(PyExc_ValueError, "this LuaObject is not indexable");
        return -1;
    }
    Lua_pushpyobject(self->lua, ss);
    if (o == NULL)
        lua_pushnil(L);
    else
        Lua_pushpyobject(self->lua, o);
    lua_settable(L, -3);
    lua_pop(L, 1);
    return 0;
}

static PyMappingMethods LuaObject_mapping = {
    NULL,                                   /*mp_length*/
    (binaryfunc)LuaObject_subscript,         /*mp_subscript*/
    (objobjargproc)LuaObject_ass_subscript,  /*mp_ass_subscript*/
};

static PyTypeObject LuaObjectType = {
    PyObject_HEAD_INIT(NULL)
    0,                          /*ob_size*/
    "lua.LuaObject",            /*tp_name*/
    sizeof(LuaObject),          /*tp_basicsize*/
    0,                          /*tp_itemsize*/
    (destructor)LuaObject_dealloc, /*tp_dealloc*/
    0,                          /*tp_print*/
    0,                          /*tp_getattr*/
    0,                          /*tp_setattr*/
    0,                          /*tp_compare*/
    0,                          /*tp_repr*/
    0,                          /*tp_as_number*/
    0,                          /*tp_as_sequence*/
    &LuaObject_mapping,         /*tp_as_mapping*/
    0,                          /*tp_hash */
    (ternaryfunc)LuaObject_call, /*tp_call*/
    0,                          /*tp_str*/
    (getattrofunc)LuaObject_getattro, /*tp_getattro*/
    (setattrofunc)LuaObject_setattro, /*tp_setattro*/
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
    0,                          /*tp_getset*/
    0,                          /*tp_base*/
    0,                          /*tp_dict*/
    0,                          /*tp_descr_get*/
    0,                          /*tp_descr_set*/
    0,                          /*tp_dictoffset*/
    (initproc)LuaObject_init,   /*tp_init*/
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

typedef struct luaL_Reg_named {
    const char *pyname;
    const char *name;
    lua_CFunction func;
} luaL_Reg_named;

static const luaL_Reg_named lualibs[] = {
    {"base",          "",              luaopen_base},
    {LUA_LOADLIBNAME, LUA_LOADLIBNAME, luaopen_package},
    {LUA_TABLIBNAME,  LUA_TABLIBNAME,  luaopen_table},
    {LUA_IOLIBNAME,   LUA_IOLIBNAME,   luaopen_io},
    {LUA_OSLIBNAME,   LUA_OSLIBNAME,   luaopen_os},
    {LUA_STRLIBNAME,  LUA_STRLIBNAME,  luaopen_string},
    {LUA_MATHLIBNAME, LUA_MATHLIBNAME, luaopen_math},
    {LUA_DBLIBNAME,   LUA_DBLIBNAME,   luaopen_debug},
    {NULL, NULL, NULL}
};

static PyObject *LuaState_openlib(LuaState *self, PyObject *args)
{
    char *lib;
    lua_State *L;
    const luaL_Reg_named *libs;

    L = self->L;
    libs = lualibs;

    if (!PyArg_ParseTuple(args, "s", &lib))
        return NULL;

    for (; libs->func; libs++)
    {
        if (strcmp(libs->pyname, lib) == 0)
        {
            lua_pushcfunction(L, libs->func);
            lua_pushstring(L, libs->name);
            lua_call(L, 1, 0);
            Py_RETURN_NONE;
        }
    }
    PyErr_SetString(PyExc_ValueError, "library name must be one of:"
            " base"
            " " LUA_LOADLIBNAME
            " " LUA_TABLIBNAME
            " " LUA_IOLIBNAME
            " " LUA_OSLIBNAME
            " " LUA_STRLIBNAME
            " " LUA_MATHLIBNAME
            " " LUA_DBLIBNAME
            );
    return NULL;
}

static PyObject *LuaState_gettop(LuaState *self)
{
    return PyInt_FromLong(lua_gettop(self->L));
}

static PyObject *LuaState_eval(LuaState *self, PyObject *args)
{
    char *code;

    int oldtop, numresults, ret;
    PyObject *result;

    if (!PyArg_ParseTuple(args, "s", &code))
        return NULL;

    oldtop = lua_gettop(self->L);

    if (luaL_loadstring(self->L, code))
    {
        lua_pushstring(self->L, "error loading Lua code: ");
        lua_insert(self->L, -2);
        lua_concat(self->L, 2);
        PyErr_SetString(PyExc_SyntaxError, lua_tostring(self->L, -1));
        lua_pop(self->L, 1);
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

static PyObject *LuaState_globals(LuaState *self, PyObject *args)
{
    PyObject *ret;
    lua_pushvalue(self->L, LUA_GLOBALSINDEX);
    ret = Lua_topython(self, -1);
    lua_pop(self->L, 1);
    return ret;
}

static PyMethodDef LuaState_methods[] = {
    {"openlibs", (PyCFunction)LuaState_openlibs, METH_NOARGS,
        "Load the Lua libraries."},
    {"openlib", (PyCFunction)LuaState_openlib, METH_VARARGS,
        "Load a particular Lua library."},
    {"gettop", (PyCFunction)LuaState_gettop, METH_NOARGS,
        "(debug) Gets the top index of the Lua stack."},
    {"eval", (PyCFunction)LuaState_eval, METH_VARARGS,
        "Run a piece of Lua code."},
    {"globals", (PyCFunction)LuaState_globals, METH_NOARGS,
        "Gets the Lua globals table."},
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

static void lua_pushluaobject(lua_State *L, LuaObject *f)
    // lua stack [-0, +1]
{
    lua_pushlightuserdata(L, f);
    lua_rawget(L, LUA_REGISTRYINDEX);
}

static PyObject *lua_topyobject(lua_State *L, int index)
    // new reference
    // lua stack [-0, +0]
{
    PyObject *result;

    result = *(PyObject **)luaL_checkudata(L, index, PYOBJECT);
    Py_XINCREF(result);

    return result;
}

static PyObject *Lua_toluaobject(LuaState *lua, int index)
    // new reference
    // lua stack [-0, +0]
{
    LuaObject *f;
    lua_State *L = lua->L;

    if (index < 0)
        index = lua_gettop(L) + 1 + index;

    Py_INCREF(lua);
    f = (LuaObject *)LuaObjectType.tp_alloc(&LuaObjectType, 0);
    f->lua = lua;

    lua_pushlightuserdata(L, f);
    lua_pushvalue(L, index);
    lua_rawset(L, LUA_REGISTRYINDEX);

    return (PyObject *)f;
}

static int lua_obj_gc(lua_State *L)
{
    PyObject *o;

    o = *(PyObject **)luaL_checkudata(L, 1, PYOBJECT);
    Py_XDECREF(o);

    return 0;
}

static int lua_obj_call(lua_State *L)
{
    PyObject *o, *args, *ret;
    LuaState *lua;
    int nargs, r;

    lua = (LuaState *)lua_touserdata(L, lua_upvalueindex(1));
    o = *(PyObject **)luaL_checkudata(L, 1, PYOBJECT);
    if (!PyCallable_Check(o))
    {
        luaL_error(L, "Python object is not callable");
        return 0;
    }

    nargs = lua_gettop(L) - 1;
    args = Lua_topython_tuple(lua, nargs);
    ret = PyObject_CallObject(o, args);
    Py_DECREF(args);
    LogObj("returned", ret);
    if (PyErr_Occurred())
    {
        PyErr_Print();
        return luaL_error(L, "error in the function");
    }
    r = Lua_pushpyobject_tuple(lua, ret);
    Py_DECREF(ret);
    return r;
}

static int lua_obj_index(lua_State *L)
{
    PyObject *o, *key, *val;
    LuaState *lua;

    lua = (LuaState *)lua_touserdata(L, lua_upvalueindex(1));
    o = *(PyObject **)luaL_checkudata(L, 1, PYOBJECT);
    if (PyDict_Check(o))
    {
        return luaL_error(L, "todo: implement dict __index");
    }
    else
    {
        key = Lua_topython(lua, 2);
        if (!PyString_Check(key))
        {
            Py_DECREF(key);
            return luaL_error(L, "attribute name isn't a string");
        }
        if (PyObject_HasAttr(o, key))
        {
            val = PyObject_GetAttr(o, key);
            Lua_pushpyobject(lua, val);
            Py_DECREF(val);
        }
        else
        {
            lua_pushnil(L);
        }
        Py_DECREF(key);
        return 1;
    }
}

static int lua_obj_newindex(lua_State *L)
{
    PyObject *o, *key, *val;
    LuaState *lua;
    int ret;

    lua = (LuaState *)lua_touserdata(L, lua_upvalueindex(1));
    o = *(PyObject **)luaL_checkudata(L, 1, PYOBJECT);
    if (PyDict_Check(o))
    {
        return luaL_error(L, "todo: implement dict __index");
    }
    else
    {
        key = Lua_topython(lua, 2);
        if (!PyString_Check(key))
        {
            Py_DECREF(key);
            return luaL_error(L, "attribute name isn't a string");
        }
        val = Lua_topython(lua, 3);
        ret = PyObject_SetAttr(o, key, val);
        Py_DECREF(key);
        Py_DECREF(val);
        if (ret == -1)
            return luaL_error(L, "failed to set attribute");
        return 0;
    }
}

void Lua_settable_cfunction(LuaState *lua, int index, const char *name,
        lua_CFunction fn)
    // lua stack [-0, +0]
{
    lua_pushstring(lua->L, name);
    lua_pushlightuserdata(lua->L, lua);
    lua_pushcclosure(lua->L, fn, 1);
    if (index < 0)
        index -= 2;
    lua_rawset(lua->L, index);
}

static int Lua_pushpyobject_tuple(LuaState *lua, PyObject *o)
    // lua stack [-0, +n]
{
    if (PyTuple_Check(o))
    {
        int size = PyTuple_Size(o), i, n = 0;
        for (i = 0; i < size; ++i)
            n += Lua_pushpyobject(lua, PyTuple_GetItem(o, i));
        return n;
    }
    return Lua_pushpyobject(lua, o);
}

static int Lua_pushpyobject(LuaState *lua, PyObject *o)
    // lua stack [-0, +1]
{
    PyObject **userdata;
    lua_State *L = lua->L;

    if (o == NULL)
    {
        Log("attempted to push null object\n");
        return 0;
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
        ssize_t len;
        PyString_AsStringAndSize(o, &buf, &len);
        lua_pushlstring(L, buf, len);
        return 1;
    }
    if (PyType_IsSubtype(o->ob_type, &LuaObjectType))
    {
        lua_pushluaobject(L, (LuaObject *)o);
        return 1;
    }

    userdata = lua_newuserdata(L, sizeof(PyObject *));
    Py_INCREF(o);
    *userdata = o;
    luaL_newmetatable(L, PYOBJECT);
    Lua_settable_cfunction(lua, -1, "__gc", lua_obj_gc);
    Lua_settable_cfunction(lua, -1, "__call", lua_obj_call);
    Lua_settable_cfunction(lua, -1, "__index", lua_obj_index);
    Lua_settable_cfunction(lua, -1, "__newindex", lua_obj_newindex);
    lua_setmetatable(L, -2);
    return 1;
}

static PyObject *Lua_topython(LuaState *lua, int index)
    // new reference
    // lua stack [-0, +0]
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
        case LUA_TTHREAD:
            return Py_BuildValue("s", "lua thread");
        case LUA_TUSERDATA:
            result = lua_topyobject(L, index);
            if (result != NULL)
                return result;
        case LUA_TLIGHTUSERDATA:
        case LUA_TFUNCTION:
        case LUA_TTABLE:
            return Lua_toluaobject(lua, index);
    }
    return NULL;
}

static PyObject *Lua_topython_tuple(LuaState *lua, int n)
    // new reference
    // lua stack [-0, +0]
{
    PyObject *result;
    int i;

    result = PyTuple_New(n);
    for (i = 0; i < n; ++i)
        PyTuple_SET_ITEM(result, i, Lua_topython(lua, i - n));
    return result;
}

static PyObject *Lua_topython_multiple(LuaState *lua, int n)
    // new reference
    // lua stack [-0, +0]
{
    if (n == 0)
        Py_RETURN_NONE;
    else if (n == 1)
        return Lua_topython(lua, -1);
    else
        return Lua_topython_tuple(lua, n);
}

static int lua_iscallable(lua_State *L, int index)
    // lua stack [-0, +0]
{
    int ret;
    if (lua_isfunction(L, index))
        return 1;
    if (!lua_getmetatable(L, index))
        return 0;
    lua_getfield(L, -1, "__call");
    ret = lua_isfunction(L, -1);
    lua_pop(L, 2);
    return ret;
}

static int lua_isindexable(lua_State *L, int index)
    // lua stack [-0, +0]
{
    int ret;
    if (lua_istable(L, index))
        return 1;
    if (!lua_getmetatable(L, index))
        return 0;
    lua_getfield(L, -1, "__index");
    ret = lua_isindexable(L, -1);
    lua_pop(L, 2);
    return ret;
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
    if (PyType_Ready(&LuaObjectType) < 0)
        return;

    m = Py_InitModule3("lua", lua_methods, "Lua bindings.");

    Py_INCREF(&LuaStateType);
    Py_INCREF(&LuaObjectType);
    PyModule_AddObject(m, "LuaState", (PyObject *)&LuaStateType);
    PyModule_AddObject(m, "LuaObject", (PyObject *)&LuaObjectType);
}
