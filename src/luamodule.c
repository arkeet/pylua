#include <Python.h>
#include <structmember.h>

#include <lua.h>
#include <lauxlib.h>

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
    PyObject *repr = PyObject_Repr((PyObject *)o);
    Log("  Obj %s %s\n", str, PyString_AsString(repr));
    Py_XDECREF(repr);
}
int LogLuaTop(const char *str, lua_State *L)
{
    Log("  Top %s %d\n", str, lua_gettop(L));
}

/* Type structs *************************************************************/

typedef struct
{
    PyObject_HEAD
    lua_State *L;
} Lua;

typedef struct
{
    PyObject_HEAD
    Lua *lua;
    size_t chunksize;
    void *chunk;
} LuaFunction;

typedef struct
{
    PyObject_HEAD
    Lua *lua;
    size_t chunksize;
    void *chunk;
} LuaTable;

/* LuaFunction type *********************************************************/

static void LuaFunction_dealloc(LuaFunction *self)
{
    LogObj("LuaFunction_dealloc", self);
    if (self->chunk != NULL)
        free(self->chunk);
    Py_XDECREF(self->lua);
}

static int LuaFunction_init(LuaFunction *self, PyObject *args, PyObject *kwds)
{
    PyErr_SetString(PyExc_TypeError, "LuaFunction cannot be instantiated");
    return -1;
}

static void lua_pushpyfunction(lua_State *L, PyObject *o);
static int Lua_pushpython(Lua *lua, PyObject *o);
static PyObject *Lua_topython_multiple(Lua *lua, int n);
static PyObject *LuaFunction_call(LuaFunction *self, PyObject *args, PyObject
        *kwds)
{
    size_t n, i;
    int oldtop;
    lua_State *L;
    PyObject *result;

    if (self->chunk == NULL)
    {
        PyErr_SetString(PyExc_RuntimeError,
                "LuaFunction points to null function");
        return NULL;
    }

    L = self->lua->L;
    n = PyTuple_GET_SIZE(args);

    oldtop = lua_gettop(L);
    lua_pushpyfunction(L, (PyObject *)self);
    for (i = 0; i < n; ++i)
        Lua_pushpython(self->lua, PyTuple_GetItem(args, i));
    lua_pcall(L, n, LUA_MULTRET, 0);
    result = Lua_topython_multiple(self->lua, lua_gettop(L) - oldtop);
}

static PyObject *LuaFunction_get_lua(LuaFunction *self)
{
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

/* utility functions ********************************************************/

static const char *LuaFunction_reader(lua_State *L, void *data, size_t *size)
{
    LuaFunction *f = data;
    *size = f->chunksize;
    return f->chunk;
}

static int LuaFunction_writer(lua_State *L, const void *p, size_t sz, void *ud)
{
    LuaFunction *f = ud;
    f->chunk = realloc(f->chunk, f->chunksize + sz);
    if (f->chunk == NULL)
        return -1;
    memcpy(f->chunk + f->chunksize, p, sz);
    f->chunksize += sz;
    return 0;
}

static void lua_pushpyfunction(lua_State *L, PyObject *o)
{
    lua_load(L, LuaFunction_reader, o, "Python stored Lua chunk");
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

static PyObject *Lua_topyobject(Lua *lua, int index)
{
    PyObject *result;
    lua_State *L = lua->L;

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

static PyObject *Lua_topyfunction(Lua *lua, int index)
{
    LuaFunction *result;

    if (!lua_isfunction(lua->L, index))
        return NULL;

    result = (LuaFunction *)LuaFunctionType.tp_alloc(&LuaFunctionType, 0);
    result->lua = lua;
    result->chunksize = 0;
    result->chunk = NULL;
    Py_INCREF(lua);

    lua_dump(lua->L, LuaFunction_writer, result);

    return (PyObject *)result;
}

static PyObject *Lua_topython_tuple(Lua *lua, int n);
static int lua_fn_pycall(lua_State *L)
{
    PyObject *o, *args, *ret;
    Lua *lua;
    int i, nargs;

    lua = (Lua *)lua_touserdata(L, lua_upvalueindex(1));
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

static int Lua_pushpython(Lua *lua, PyObject *o)
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
        LuaFunction *f = (LuaFunction *)o;
        lua_pushpyfunction(L, o);
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

static PyObject *Lua_topython(Lua *lua, int index)
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
            result = Lua_topyobject(lua, index);
            if (result != NULL)
                return result;
            return Py_BuildValue("s", "lua userdata");
        case LUA_TTHREAD:
            return Py_BuildValue("s", "lua thread");
        case LUA_TLIGHTUSERDATA:
            return Py_BuildValue("s", "lua light userdata");
    }
}

static PyObject *Lua_topython_multiple(Lua *lua, int n)
{
    if (n == 0)
        Py_RETURN_NONE;
    else if (n == 1)
        return Lua_topython(lua, -1);
    else
        return Lua_topython_tuple(lua, n);
}

static PyObject *Lua_topython_tuple(Lua *lua, int n)
{
    PyObject *result;
    int i;

    result = PyTuple_New(n);
    for (i = 0; i < n; ++i)
        PyTuple_SetItem(result, i, Lua_topython(lua, i - n));
    return result;
}

/* LuaState type ************************************************************/

static void Lua_dealloc(Lua *self)
{
    lua_close(self->L);
}

static int Lua_init(Lua *self, PyObject *args, PyObject *kwds)
{
    self->L = luaL_newstate();
    return 0;
}

static PyMemberDef Lua_members[] = {
    {NULL}
};

static PyObject *Lua_openlibs(Lua *self)
{
    luaL_openlibs(self->L);
    Py_RETURN_NONE;
}

static PyObject *Lua_eval(Lua *self, PyObject *args)
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
        lua_insert(self->L, 1);
        lua_concat(self->L, 2);
        PyErr_SetString(PyExc_RuntimeError, lua_tostring(self->L, 1));
        lua_pop(self->L, 1);
        return NULL;
    }

    numresults = lua_gettop(self->L) - oldtop;

    result = Lua_topython_multiple(self, numresults);
    lua_pop(self->L, numresults);
    return result;
}

static PyObject *Lua_getglobal(Lua *self, PyObject *args)
{
    const char *name;
    PyObject *ret;
    if (!PyArg_ParseTuple(args, "s", &name))
        return NULL;
    lua_getglobal(self->L, name);
    return Lua_topython(self, -1);
}

static PyObject *Lua_setglobal(Lua *self, PyObject *args)
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

static PyMethodDef Lua_methods[] = {
    {"openlibs", (PyCFunction)Lua_openlibs, METH_NOARGS,
        "Load the Lua libraries."},
    {"eval", (PyCFunction)Lua_eval, METH_VARARGS,
        "Run a piece of Lua code."},
    {"getglobal", (PyCFunction)Lua_getglobal, METH_VARARGS,
        "Gets a global variable."},
    {"setglobal", (PyCFunction)Lua_setglobal, METH_VARARGS,
        "Sets a global variable."},
    {NULL}
};

static PyTypeObject LuaType = {
    PyObject_HEAD_INIT(NULL)
    0,                          /*ob_size*/
    "lua.Lua",                  /*tp_name*/
    sizeof(Lua),                /*tp_basicsize*/
    0,                          /*tp_itemsize*/
    (destructor)Lua_dealloc,    /*tp_dealloc*/
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
    Lua_methods,                /*tp_methods*/
    Lua_members,                /*tp_members*/
    0,                          /*tp_getset*/
    0,                          /*tp_base*/
    0,                          /*tp_dict*/
    0,                          /*tp_descr_get*/
    0,                          /*tp_descr_set*/
    0,                          /*tp_dictoffset*/
    (initproc)Lua_init,         /*tp_init*/
    0,                          /*tp_alloc*/
    PyType_GenericNew,          /*tp_new*/
};

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

    if (PyType_Ready(&LuaType) < 0)
        return;
    if (PyType_Ready(&LuaFunctionType) < 0)
        return;

    m = Py_InitModule3("lua", lua_methods, "Lua bindings.");

    Py_INCREF(&LuaType);
    Py_INCREF(&LuaFunctionType);
    PyModule_AddObject(m, "Lua", (PyObject *)&LuaType);
    PyModule_AddObject(m, "LuaFunction", (PyObject *)&LuaFunctionType);
}
