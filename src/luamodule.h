#ifndef _LUAMODULE_H
#define _LUAMODULE_H

#include <Python.h>
#include <lua.h>

/* Debug functions **********************************************************/

void Log(const char *format, ...);
void LogObj(const char *str, void *o);
void LogLuaTop(const char *str, lua_State *L);

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
} LuaObject;

/* Utility functions ********************************************************/

static void lua_pushluaobject(lua_State *L, LuaObject *f);
static PyObject *lua_topyobject(lua_State *L, int index);
static PyObject *Lua_toluaobject(LuaState *lua, int index);
static int lua_obj_gc(lua_State *L);
static int lua_obj_call(lua_State *L);
static int lua_obj_index(lua_State *L);
static int lua_obj_newindex(lua_State *L);
void Lua_settable_cfunction(LuaState *lua, int index, const char *name,
        lua_CFunction fn);
static int Lua_pushpyobject_tuple(LuaState *lua, PyObject *o);
static int Lua_pushpyobject(LuaState *lua, PyObject *o);
static PyObject *Lua_topython(LuaState *lua, int index);
static PyObject *Lua_topython_tuple(LuaState *lua, int n);
static PyObject *Lua_topython_multiple(LuaState *lua, int n);
static int lua_iscallable(lua_State *L, int index);
static int lua_isindexable(lua_State *L, int index);

/* LuaObject type *********************************************************/

static void LuaObject_dealloc(LuaObject *self);
static int LuaObject_init(LuaObject *self, PyObject *args, PyObject
        *kwds);
static PyObject *LuaObject_call(LuaObject *self, PyObject *args, PyObject
        *kwds);
static PyObject *LuaObject_getattro(LuaObject *self, PyObject *name);
static int LuaObject_setattro(LuaObject *self, PyObject *name, PyObject *o);
static PyObject *LuaObject_subscript(LuaObject *self, PyObject *ss);
static int LuaObject_ass_subscript(LuaObject *self, PyObject *ss, PyObject
        *o);

/* LuaState type ************************************************************/

static void LuaState_dealloc(LuaState *self);
static int LuaState_init(LuaState *self, PyObject *args, PyObject *kwds);
static PyObject *LuaState_openlibs(LuaState *self);
static PyObject *LuaState_openlib(LuaState *self, PyObject *args);
static PyObject *LuaState_gettop(LuaState *self);
static PyObject *LuaState_eval(LuaState *self, PyObject *args);
static PyObject *LuaState_globals(LuaState *self, PyObject *args);

#endif
