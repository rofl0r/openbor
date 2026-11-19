#include <lauxlib.h>
#include <lualib.h>

/*
 * Open the Lua standard libraries retained for the engine's embedded
 * scripting environment. Package/require, coroutine, UTF-8 helpers, host
 * filesystem I/O, OS, and debug libraries are omitted for this experiment.
 * The engine-specific API is registered separately.
 */

/*
 * transconst is part of the legacy ScriptLib.  The old implementation
 * resolves a constant by name and returns its translated integer value.
 * The Lua API already owns the complete constant map through
 * openborconstant(), so keep one canonical map and expose the legacy
 * transconst name as a forwarding wrapper.
 *
 * The lookup is performed when the function is called rather than when the
 * libraries are opened, because the engine API is registered immediately
 * after the Lua standard libraries are initialized.
 */
static int bor_lua_transconst(lua_State *L)
{
    int nargs = lua_gettop(L);

    if (nargs < 1)
        return luaL_error(L, "transconst expects a constant name");

    lua_getglobal(L, "openborconstant");
    if (!lua_isfunction(L, -1))
        return luaL_error(L, "transconst: openborconstant is not registered");

    lua_insert(L, 1);
    if (lua_pcall(L, nargs, 1, 0) != LUA_OK)
        return lua_error(L);

    return 1;
}

void bor_lua_openlibs(lua_State *L)
{
    luaL_requiref(L, LUA_GNAME, luaopen_base, 1);
    lua_pop(L, 1);

    luaL_requiref(L, LUA_TABLIBNAME, luaopen_table, 1);
    lua_pop(L, 1);

    luaL_requiref(L, LUA_STRLIBNAME, luaopen_string, 1);
    lua_pop(L, 1);

    luaL_requiref(L, LUA_MATHLIBNAME, luaopen_math, 1);
    lua_pop(L, 1);

    /* Legacy ScriptLib compatibility name; resolved lazily after API setup. */
    lua_pushcfunction(L, bor_lua_transconst);
    lua_setglobal(L, "transconst");
}
