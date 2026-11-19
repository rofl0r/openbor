#ifndef LUABINDINGS_H
#define LUABINDINGS_H

#include <stddef.h>
#include <lua.h>

// Forward declaration signature
void openbor_register_lua_api(void);

/* Execute Lua code through the common protected boundary. */
int bor_lua_pcall(lua_State *L, int nargs, int nresults, const char *context);
struct entity;

void bor_lua_clear_localvars(lua_State *L);
void bor_lua_set_localvar(lua_State *L, const char *name, int index);
void bor_lua_push_entity(lua_State *L, struct entity *ent);
int bor_lua_runbuffer(lua_State *L, const char *buffer, size_t size, const char *source);

/*
 * The embedded engine uses a deliberately small Lua standard-library set.
 * Keep existing luaL_openlibs() call sites unchanged while routing them to
 * our reduced loader instead of Lua's full host-oriented library set.
 */
void bor_lua_openlibs(lua_State *L);
#define luaL_openlibs bor_lua_openlibs

void bor_lua_clear_entity_vars(struct entity *ent);
void bor_lua_clear_all_entity_vars(void);

#endif
