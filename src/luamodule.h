#ifndef _LUAMODULE_H
#define _LUAMODULE_H

#include <Python.h>
#include <lua.h>

/* Debug functions **********************************************************/

int Log(const char *format, ...);
int LogObj(const char *str, void *o);
int LogLuaTop(const char *str, lua_State *L);

/* Type structs *************************************************************/

typedef struct
{
    PyObject_HEAD
    lua_State *L;
} LuaState;

typedef struct
{
    PyObject_HEAD
    LuaState *lua;
} LuaFunction;

typedef struct
{
    PyObject_HEAD
    LuaState *lua;
} LuaTable;

/* Utility functions ********************************************************/

static void lua_pushpyfunction(lua_State *L, LuaFunction *f);
static int lua_fn_pygc(lua_State *L);
static PyObject *lua_topyobject(lua_State *L, int index);
static PyObject *Lua_topyfunction(LuaState *lua, int index);
static int lua_fn_pycall(lua_State *L);
static int Lua_pushpython(LuaState *lua, PyObject *o);
static PyObject *Lua_topython(LuaState *lua, int index);
static PyObject *Lua_topython_tuple(LuaState *lua, int n);
static PyObject *Lua_topython_multiple(LuaState *lua, int n);

/* LuaFunction type *********************************************************/

static void LuaFunction_dealloc(LuaFunction *self);
static int LuaFunction_init(LuaFunction *self, PyObject *args, PyObject
        *kwds);
static PyObject *LuaFunction_call(LuaFunction *self, PyObject *args, PyObject
        *kwds);
static PyObject *LuaFunction_get_lua(LuaFunction *self);

/* LuaState type ************************************************************/

static void LuaState_dealloc(LuaState *self);
static int LuaState_init(LuaState *self, PyObject *args, PyObject *kwds);
static PyObject *LuaState_openlibs(LuaState *self);
static PyObject *LuaState_gettop(LuaState *self);
static PyObject *LuaState_eval(LuaState *self, PyObject *args);
static PyObject *LuaState_getglobal(LuaState *self, PyObject *args);
static PyObject *LuaState_setglobal(LuaState *self, PyObject *args);

#endif
