#include "openbor.h"
#include "data.h"
#include "models.h"
#include "luabindings.h"
#include <lauxlib.h>
#include <string.h>
#include <stdlib.h>

/* Legacy OpenBOR ScriptVariant string storage limit. */
#define MAX_STR_VAR_LEN 64
extern int sprite_map_max_items;
extern int MAX_WALL_HEIGHT;
static s_spawn_entry bor_lua_spawnentry;

/*
 * Accept the OpenBOR-style function wrapper:
 *
 *     function test(int number)
 *     {
 *         ...
 *     }
 *
 * Lua itself receives:
 *
 *     function test(number)
 *         ...
 *     end
 *
 * This is deliberately limited to function wrappers and parameter
 * declarations. Normal Lua table constructors, strings, and comments
 * are left untouched. The source buffer is never modified.
 */
typedef struct bor_lua_source_edit {
    size_t start;
    size_t end;
    const char *replacement;
    size_t replacement_length;
} bor_lua_source_edit;

static int bor_lua_source_edit_compare(const void *left, const void *right)
{
    const bor_lua_source_edit *a = (const bor_lua_source_edit *)left;
    const bor_lua_source_edit *b = (const bor_lua_source_edit *)right;

    if (a->start < b->start) return -1;
    if (a->start > b->start) return 1;
    return 0;
}

static int bor_lua_add_source_edit(bor_lua_source_edit **edits,
                                   size_t *count,
                                   size_t *capacity,
                                   size_t start,
                                   size_t end,
                                   const char *replacement,
                                   size_t replacement_length)
{
    bor_lua_source_edit *grown;

    if (*count == *capacity) {
        size_t new_capacity = *capacity ? *capacity * 2 : 16;
        if (new_capacity < *capacity ||
            new_capacity > ((size_t)-1) / sizeof(bor_lua_source_edit))
            return 0;
        grown = (bor_lua_source_edit *)realloc(
            *edits, new_capacity * sizeof(bor_lua_source_edit));
        if (!grown)
            return 0;
        *edits = grown;
        *capacity = new_capacity;
    }

    (*edits)[*count].start = start;
    (*edits)[*count].end = end;
    (*edits)[*count].replacement = replacement;
    (*edits)[*count].replacement_length = replacement_length;
    (*count)++;
    return 1;
}

static int bor_lua_source_ident_start(char c)
{
    return (c >= 'a' && c <= 'z') ||
           (c >= 'A' && c <= 'Z') || c == '_';
}

static int bor_lua_source_ident_part(char c)
{
    return bor_lua_source_ident_start(c) || (c >= '0' && c <= '9');
}

static int bor_lua_source_space(char c)
{
    return c == ' ' || c == '\t' || c == '\n' ||
           c == '\r' || c == '\v' || c == '\f';
}

/* Return the end of a Lua long-bracket string/comment, or start if absent. */
static size_t bor_lua_skip_long_bracket(const char *source,
                                        size_t size,
                                        size_t start)
{
    size_t p = start + 1;
    size_t equals = 0;
    size_t q;

    if (start >= size || source[start] != '[')
        return start;

    while (p < size && source[p] == '=') {
        equals++;
        p++;
    }
    if (p >= size || source[p] != '[')
        return start;

    p++;
    while (p < size) {
        if (source[p] == ']') {
            q = p + 1;
            while (q < size && source[q] == '=' && q - (p + 1) < equals)
                q++;
            if (q - (p + 1) == equals && q < size && source[q] == ']')
                return q + 1;
        }
        p++;
    }

    return size;
}

/* Skip a quoted string, Lua long string, or Lua comment at position start. */
static size_t bor_lua_skip_lexical_item(const char *source,
                                        size_t size,
                                        size_t start)
{
    size_t p;

    if (start >= size)
        return start;

    if (source[start] == '"' || source[start] == '\'') {
        char quote = source[start];
        p = start + 1;
        while (p < size) {
            if (source[p] == '\\') {
                p += (p + 1 < size) ? 2 : 1;
                continue;
            }
            if (source[p] == quote)
                return p + 1;
            p++;
        }
        return size;
    }

    if (source[start] == '[') {
        p = bor_lua_skip_long_bracket(source, size, start);
        if (p != start)
            return p;
    }

    if (source[start] == '-' && start + 1 < size &&
        source[start + 1] == '-') {
        p = start + 2;
        if (p < size && source[p] == '[') {
            size_t long_end = bor_lua_skip_long_bracket(source, size, p);
            if (long_end != p)
                return long_end;
        }
        while (p < size && source[p] != '\n' && source[p] != '\r')
            p++;
        return p;
    }

    return start;
}

static size_t bor_lua_skip_source_trivia(const char *source,
                                         size_t size,
                                         size_t position)
{
    size_t next;

    while (position < size) {
        if (bor_lua_source_space(source[position])) {
            position++;
            continue;
        }
        next = bor_lua_skip_lexical_item(source, size, position);
        if (next != position) {
            position = next;
            continue;
        }
        break;
    }
    return position;
}

static int bor_lua_find_matching_source_delimiter(const char *source,
                                                  size_t size,
                                                  size_t open_position,
                                                  char open_char,
                                                  char close_char,
                                                  size_t *close_position)
{
    size_t p;
    size_t depth = 1;

    if (open_position >= size || source[open_position] != open_char)
        return 0;

    p = open_position + 1;
    while (p < size) {
        size_t next = bor_lua_skip_lexical_item(source, size, p);
        if (next != p) {
            p = next;
            continue;
        }

        if (source[p] == open_char) {
            depth++;
        } else if (source[p] == close_char) {
            depth--;
            if (depth == 0) {
                *close_position = p;
                return 1;
            }
        }
        p++;
    }

    return 0;
}

/*
 * Remove a C-style type prefix from each simple function parameter.
 * A parameter containing more than one identifier before its comma is
 * treated as a typed declaration only when its tokens are identifiers,
 * pointer/reference markers, and whitespace. Ordinary Lua parameters
 * are a single identifier and are therefore preserved.
 */
static int bor_lua_add_typed_parameter_edits(const char *source,
                                             size_t start,
                                             size_t end,
                                             bor_lua_source_edit **edits,
                                             size_t *edit_count,
                                             size_t *edit_capacity)
{
    size_t segment = start;
    size_t p = start;
    size_t first_identifier = 0;

    while (p <= end) {
        int at_separator = (p == end);
        size_t next;

        if (!at_separator) {
            next = bor_lua_skip_lexical_item(source, end, p);
            if (next != p) {
                p = next;
                continue;
            }
            if (source[p] == '(') {
                size_t close;
                if (bor_lua_find_matching_source_delimiter(
                        source, end, p, '(', ')', &close)) {
                    p = close + 1;
                    continue;
                }
            }
            if (source[p] != ',') {
                p++;
                continue;
            }
        }

        {
            size_t q = segment;
            size_t name_start = 0;
            size_t names = 0;
            int simple_declaration = 1;

            while (q < p) {
                if (bor_lua_source_space(source[q])) {
                    q++;
                } else if (bor_lua_source_ident_start(source[q])) {
                    size_t token_start = q++;
                    while (q < p && bor_lua_source_ident_part(source[q]))
                        q++;
                    if (names == 0)
                        first_identifier = token_start;
                    name_start = token_start;
                    names++;
                } else if (source[q] == '*' || source[q] == '&') {
                    q++;
                } else {
                    simple_declaration = 0;
                    break;
                }
            }

            if (simple_declaration && names >= 2) {
                if (!bor_lua_add_source_edit(edits, edit_count,
                                             edit_capacity,
                                             first_identifier,
                                             name_start, "", 0))
                    return 0;
            } else if (simple_declaration && names == 1) {
                size_t token_end = name_start;
                while (token_end < p &&
                       bor_lua_source_ident_part(source[token_end]))
                    token_end++;
                if (token_end - name_start == 4 &&
                    memcmp(source + name_start, "void", 4) == 0) {
                    if (!bor_lua_add_source_edit(edits, edit_count,
                                                 edit_capacity,
                                                 name_start, token_end, "", 0))
                        return 0;
                }
            }
        }

        if (at_separator)
            break;
        segment = p + 1;
        p++;
    }

    return 1;
}

/*
 * Convert simple C-style local declarations at the start of a statement
 * into Lua locals. Only the type keyword is replaced, so initializers and
 * Lua expressions remain byte-for-byte unchanged.
 *
 * Supported scalar type keywords are intentionally conservative. Arrays,
 * structs, pointers, and declarations embedded in for-loop headers are not
 * rewritten by this pass.
 */
static int bor_lua_add_typed_local_edits(const char *source,
                                         size_t start,
                                         size_t end,
                                         bor_lua_source_edit **edits,
                                         size_t *edit_count,
                                         size_t *edit_capacity)
{
    static const char *const types[] = {
        "int", "float", "double", "char", "long", "short",
        "unsigned", "signed", "bool"
    };
    size_t p = start;
    int line_start = 1;

    while (p < end) {
        size_t next = bor_lua_skip_lexical_item(source, end, p);
        if (next != p) {
            /* A newline inside a long string/comment still ends the line. */
            size_t q;
            for (q = p; q < next; q++) {
                if (source[q] == '\n' || source[q] == '\r')
                    line_start = 1;
            }
            p = next;
            continue;
        }

        if (source[p] == '\n' || source[p] == '\r') {
            line_start = 1;
            p++;
            continue;
        }

        if (line_start && (source[p] == ' ' || source[p] == '\t')) {
            p++;
            continue;
        }

        if (line_start && bor_lua_source_ident_start(source[p])) {
            size_t word_end = p + 1;
            size_t q;
            size_t type_index;
            int is_type = 0;

            while (word_end < end &&
                   bor_lua_source_ident_part(source[word_end]))
                word_end++;

            for (type_index = 0;
                 type_index < sizeof(types) / sizeof(types[0]);
                 type_index++) {
                size_t type_length = strlen(types[type_index]);
                if (word_end - p == type_length &&
                    memcmp(source + p, types[type_index], type_length) == 0) {
                    is_type = 1;
                    break;
                }
            }

            if (is_type) {
                q = word_end;
                if (q < end && bor_lua_source_space(source[q])) {
                    while (q < end && bor_lua_source_space(source[q]) &&
                           source[q] != '\n' && source[q] != '\r')
                        q++;

                    if (q < end && bor_lua_source_ident_start(source[q])) {
                        size_t name_end = q + 1;
                        size_t after_name;

                        while (name_end < end &&
                               bor_lua_source_ident_part(source[name_end]))
                            name_end++;
                        after_name = name_end;

                        while (after_name < end &&
                               (source[after_name] == ' ' ||
                                source[after_name] == '\t'))
                            after_name++;

                        if (after_name < end &&
                            (source[after_name] == '=' ||
                             source[after_name] == ';' ||
                             source[after_name] == ',')) {
                            if (!bor_lua_add_source_edit(
                                    edits, edit_count, edit_capacity,
                                    p, word_end, "local",
                                    sizeof("local") - 1))
                                return 0;
                        }
                    }
                }
            }

            line_start = 0;
            p = word_end;
            continue;
        }

        if (!bor_lua_source_space(source[p]))
            line_start = 0;
        p++;
    }

    return 1;
}

/*
 * Build a temporary Lua-compatible source buffer. Return 1 when a new
 * buffer was produced, 0 when the input is already standard Lua, and -1
 * on allocation failure.
 */
static int bor_lua_preprocess_function_syntax(const char *source,
                                               size_t size,
                                               char **output,
                                               size_t *output_size)
{
    bor_lua_source_edit *edits = NULL;
    size_t edit_count = 0;
    size_t edit_capacity = 0;
    size_t p = 0;
    size_t i;
    size_t cursor;
    size_t capacity;
    char *converted;

    *output = NULL;
    *output_size = size;

    while (p < size) {
        size_t next = bor_lua_skip_lexical_item(source, size, p);
        if (next != p) {
            p = next;
            continue;
        }

        if (bor_lua_source_ident_start(source[p])) {
            size_t word_start = p;
            size_t word_end = p + 1;
            while (word_end < size &&
                   bor_lua_source_ident_part(source[word_end]))
                word_end++;

            if (word_end - word_start == 8 &&
                memcmp(source + word_start, "function", 8) == 0) {
                size_t open_paren;
                size_t close_paren;
                size_t body_open;
                size_t body_close;
                size_t name_pos;

                name_pos = bor_lua_skip_source_trivia(source, size, word_end);
                if (name_pos < size && bor_lua_source_ident_start(source[name_pos])) {
                    /* Consume the complete function name, not just its first character. */
                    while (name_pos < size &&
                           bor_lua_source_ident_part(source[name_pos]))
                        name_pos++;

                    /* Also accept dotted or method-style function names. */
                    while (name_pos < size) {
                        size_t after = bor_lua_skip_source_trivia(source, size, name_pos);
                        if (after < size &&
                            (source[after] == '.' || source[after] == ':')) {
                            after++;
                            after = bor_lua_skip_source_trivia(source, size, after);
                            if (after < size && bor_lua_source_ident_start(source[after])) {
                                name_pos = after + 1;
                                while (name_pos < size &&
                                       bor_lua_source_ident_part(source[name_pos]))
                                    name_pos++;
                                continue;
                            }
                        }
                        name_pos = after;
                        break;
                    }
                }

                open_paren = bor_lua_skip_source_trivia(source, size, name_pos);
                if (open_paren < size && source[open_paren] == '(' &&
                    bor_lua_find_matching_source_delimiter(
                        source, size, open_paren, '(', ')', &close_paren)) {
                    body_open = bor_lua_skip_source_trivia(
                        source, size, close_paren + 1);
                    if (body_open < size && source[body_open] == '{' &&
                        bor_lua_find_matching_source_delimiter(
                            source, size, body_open, '{', '}', &body_close)) {
                        if (!bor_lua_add_typed_parameter_edits(
                                source, open_paren + 1, close_paren,
                                &edits, &edit_count, &edit_capacity) ||
                            !bor_lua_add_typed_local_edits(
                                source, body_open + 1, body_close,
                                &edits, &edit_count, &edit_capacity) ||
                            !bor_lua_add_source_edit(
                                &edits, &edit_count, &edit_capacity,
                                body_open, body_open + 1, "", 0) ||
                            !bor_lua_add_source_edit(
                                &edits, &edit_count, &edit_capacity,
                                body_close, body_close + 1, "end", 3)) {
                            free(edits);
                            return -1;
                        }
                    }
                }
            }

            p = word_end;
        } else {
            p++;
        }
    }

    if (edit_count == 0) {
        free(edits);
        return 0;
    }

    qsort(edits, edit_count, sizeof(bor_lua_source_edit),
          bor_lua_source_edit_compare);

    if (edit_count > (((size_t)-1) - size - 1) / 3) {
        free(edits);
        return -1;
    }
    capacity = size + edit_count * 3 + 1;
    converted = (char *)malloc(capacity);
    if (!converted) {
        free(edits);
        return -1;
    }

    cursor = 0;
    i = 0;
    {
        size_t used = 0;
        for (i = 0; i < edit_count; i++) {
            size_t unchanged;

            if (edits[i].start < cursor || edits[i].end < edits[i].start ||
                edits[i].end > size) {
                free(converted);
                free(edits);
                return -1;
            }

            unchanged = edits[i].start - cursor;
            memcpy(converted + used, source + cursor, unchanged);
            used += unchanged;
            if (edits[i].replacement_length) {
                memcpy(converted + used, edits[i].replacement,
                       edits[i].replacement_length);
                used += edits[i].replacement_length;
            }
            cursor = edits[i].end;
        }

        memcpy(converted + used, source + cursor, size - cursor);
        used += size - cursor;
        converted[used] = '\0';
        *output = converted;
        *output_size = used;
    }

    free(edits);
    return 1;
}


/*
 * Common Lua -> C execution boundary.
 *
 * Lua errors are recoverable here. Native faults inside the C engine are
 * not caught by lua_pcall() and must be handled by the platform crash layer.
 */
static void bor_lua_log_error(lua_State *L, const char *context)
{
    const char *message = lua_tostring(L, -1);

    if (!message)
        message = "(non-string Lua error object)";

    printf("[LUA ERROR] %s: %s\n",
           context ? context : "unknown",
           message);

    /* Include the Lua call stack so the failing script/function is visible. */
    luaL_traceback(L, L, message, 1);
    if (lua_isstring(L, -1))
        printf("[LUA TRACEBACK] %s\n", lua_tostring(L, -1));

    lua_pop(L, 1);
    fflush(stdout);
}

int bor_lua_pcall(lua_State *L, int nargs, int nresults, const char *context)
{
    int status;

    if (!L)
        return LUA_ERRRUN;

    status = lua_pcall(L, nargs, nresults, 0);

    if (status != LUA_OK)
        bor_lua_log_error(L, context);

    return status;
}

int bor_lua_runbuffer(lua_State *L, const char *buffer, size_t size,
                      const char *source)
{
    int status;
    char *converted = NULL;
    size_t converted_size = size;
    const char *lua_source = buffer;
    int preprocess_status;

    if (!L || !buffer)
        return LUA_ERRRUN;

    preprocess_status = bor_lua_preprocess_function_syntax(
        buffer, size, &converted, &converted_size);
    if (preprocess_status < 0) {
        lua_pushliteral(L, "out of memory while preprocessing OpenBOR function syntax");
        bor_lua_log_error(L, source ? source : "Lua preprocess");
        return LUA_ERRMEM;
    }
    if (preprocess_status > 0)
        lua_source = converted;

    status = luaL_loadbufferx(L, lua_source, converted_size,
                             source ? source : "OpenBOR Lua buffer",
                             NULL);
    free(converted);

    if (status != LUA_OK)
    {
        bor_lua_log_error(L, source ? source : "Lua compile");
        return status;
    }

    return bor_lua_pcall(L, 0, 0,
                         source ? source : "Lua chunk");
}



// Bring in the global execution pointer declared in openbor.c
extern lua_State *g_lua_engine_state;
extern s_level *level;
static int bor_lua_stricmp(const char *left, const char *right);
static int bor_lua_check_legacy_integer(lua_State *L, int index, const char *function_name)
{
    if (lua_type(L, index) != LUA_TNUMBER)
        luaL_error(L, "%s expects a numeric argument at position %d", function_name, index);
    return (int)lua_tonumber(L, index);
}

static void bor_lua_push_legacy_string(lua_State *L, int index)
{
    char buffer[256];
    switch (lua_type(L, index)) {
    case LUA_TSTRING:
        lua_pushvalue(L, index);
        return;
    case LUA_TNUMBER:
        if (lua_isinteger(L, index))
            snprintf(buffer, sizeof(buffer), "%d", (int)lua_tointeger(L, index));
        else
            snprintf(buffer, sizeof(buffer), "%lf", (double)lua_tonumber(L, index));
        break;
    case LUA_TLIGHTUSERDATA:
        snprintf(buffer, sizeof(buffer), "#%ld", (long)lua_touserdata(L, index));
        break;
    case LUA_TUSERDATA:
        snprintf(buffer, sizeof(buffer), "#%ld", (long)lua_touserdata(L, index));
        break;
    case LUA_TNIL:
        snprintf(buffer, sizeof(buffer), "<VT_EMPTY>   Unitialized");
        break;
    case LUA_TBOOLEAN:
        snprintf(buffer, sizeof(buffer), "%d", lua_toboolean(L, index) ? 1 : 0);
        break;
    default:
        snprintf(buffer, sizeof(buffer), "<Unprintable VARIANT type.>");
        break;
    }
    lua_pushstring(L, buffer);
}

static int bor_lua_legacy_enum_index(lua_State *L, int index, const char *const *names, size_t count)
{
    size_t i;

    if (lua_isinteger(L, index)) {
        lua_Integer value = lua_tointeger(L, index);
        return value >= 0 && value < (lua_Integer)count ? (int)value : -1;
    }
    if (lua_type(L, index) != LUA_TSTRING)
        return -1;

    for (i = 0; i < count; ++i) {
        if (!bor_lua_stricmp(lua_tostring(L, index), names[i]))
            return (int)i;
    }
    return -1;
}

static const char *const bor_lua_gep_aiflag[] = {
    "animating", "attacking", "autokill", "blink", "blocking", "charging",
    "dead", "drop", "falling", "freezetime", "frozen", "getting", "idling",
    "inpain", "invincible", "jumping", "projectile", "running", "toexplode", "turning"
};
static const char *const bor_lua_gep_defense[] = {
    "blockpower", "blockratio", "blockthreshold", "blocktype", "factor", "knockdown", "pain"
};
static const char *const bor_lua_gep_running[] = {
    "jumpx", "jumpy", "land", "movez", "speed"
};
static const char *const bor_lua_gep_dot[] = {
    "force", "mode", "owner", "rate", "time", "type"
};
static const char *const bor_lua_gep_attack[] = {
    "blast", "blockflash", "blocksound", "coords", "counterattack", "direction", "dol",
    "dot", "dotforce", "dotindex", "dotrate", "dottime", "drop", "dropv", "force",
    "forcemap", "freeze", "freezetime", "grab", "grabdistance", "guardcost", "hitflash",
    "hitsound", "jugglecost", "maptime", "noblock", "noflash", "nopain", "otg",
    "pause", "seal", "sealtime", "staydown", "steal", "type"
};
static const char *const bor_lua_gep_range[] = {
    "amax", "amin", "bmax", "bmin", "xmax", "xmin", "zmax", "zmin"
};
static const char *const bor_lua_gep_edelay[] = {
    "cap_max", "cap_min", "factor", "mode", "range_max", "range_min"
};
static const char *const bor_lua_textobj_properties[] = {"font", "text", "time", "x", "y", "z"};
static const char *const bor_lua_layer_properties[] = {
    "alpha", "amplitude", "bgspeedratio", "enabled", "transparency", "watermode",
    "wavelength", "wavespeed", "xoffset", "xratio", "xrepeat", "xspacing",
    "zoffset", "zratio", "zrepeat", "zspacing"
};
static const char *const bor_lua_getplayer_properties[] = {
    "credits", "ent", "entity", "keys", "lives", "name", "playkeys", "score", "weaponum"
};
static const char *const bor_lua_changeplayer_properties[] = {
    "credits", "lives", "name", "playkeys", "score", "weapon"
};
static const char *const bor_lua_spawnentry_properties[] = {
    "2phealth", "2pitem", "3phealth", "3pitem", "4phealth", "4pitem",
    "aggression", "alias", "alpha", "boss", "coords", "credit", "dying",
    "flip", "health", "item", "itemalias", "itemhealth", "itemmap", "map",
    "mp", "multiple", "name", "nolife", "weapon"
};
static const char *const bor_lua_getentityproperty_properties[] = {
    "a",
    "aggression",
    "aiattack",
    "aiflag",
    "aimove",
    "alpha",
    "animal",
    "animating",
    "animation",
    "animationid",
    "animheight",
    "animhits",
    "animnum",
    "animpos",
    "animvalid",
    "antigravity",
    "attack",
    "attacking",
    "attackid",
    "autokill",
    "base",
    "bbox",
    "blink",
    "blockback",
    "blockpain",
    "bounce",
    "chargerate",
    "colourmap",
    "colourtable",
    "damage_on_landing",
    "dead",
    "defaultmodel",
    "defaultname",
    "defense",
    "detect",
    "direction",
    "dot",
    "dropframe",
    "edelay",
    "energycost",
    "exists",
    "falldie",
    "flash",
    "freezetime",
    "frozen",
    "gfxshadow",
    "grabbing",
    "guardpoints",
    "health",
    "height",
    "hitbyid",
    "hmapl",
    "hmapu",
    "icon",
    "invincible",
    "invinctime",
    "jugglepoints",
    "knockdowncount",
    "komap",
    "landframe",
    "lifespancountdown",
    "link",
    "map",
    "mapcount",
    "mapdefault",
    "maxguardpoints",
    "maxhealth",
    "maxjugglepoints",
    "maxmp",
    "model",
    "mp",
    "mpdroprate",
    "mprate",
    "mpstable",
    "mpstableval",
    "name",
    "nextanim",
    "nextthink",
    "no_adjust_base",
    "noaicontrol",
    "nodieblink",
    "nodrop",
    "nograb",
    "nopain",
    "offense",
    "opponent",
    "owner",
    "parent",
    "path",
    "playerindex",
    "projectile",
    "range",
    "running",
    "rush_count",
    "rush_tally",
    "rush_time",
    "scroll",
    "seal",
    "sealtime",
    "setlayer",
    "spawntype",
    "speed",
    "sprite",
    "stalltime",
    "stats",
    "staydown",
    "staydownatk",
    "stealth",
    "subentity",
    "subject_to_gravity",
    "subject_to_hole",
    "subject_to_maxz",
    "subject_to_minz",
    "subject_to_obstacle",
    "subject_to_platform",
    "subject_to_screen",
    "subject_to_wall",
    "subtype",
    "throwdamage",
    "throwdist",
    "throwframewait",
    "throwheight",
    "tosstime",
    "tossv",
    "type",
    "weapent",
    "x",
    "xdir",
    "z",
    "zdir"
};
static const char *const bor_lua_changeentityproperty_properties[] = {
    "aggression",
    "aiattack",
    "aiflag",
    "aimove",
    "alpha",
    "animation",
    "animhits",
    "animpos",
    "antigrab",
    "antigravity",
    "attacking",
    "attackid",
    "autokill",
    "base",
    "blink",
    "blockback",
    "blockpain",
    "bounce",
    "candamage",
    "combostep",
    "colourmap",
    "damage_on_landing",
    "dead",
    "defaultname",
    "defense",
    "detect",
    "direction",
    "dot",
    "edelay",
    "energycost",
    "falldie",
    "freezetime",
    "frozen",
    "gfxshadow",
    "grabforce",
    "guardpoints",
    "health",
    "hitbyid",
    "hmapl",
    "hmapu",
    "hostile",
    "iconposition",
    "invincible",
    "invinctime",
    "jugglepoints",
    "komap",
    "lifeposition",
    "lifespancountdown",
    "map",
    "maptime",
    "maxguardpoints",
    "maxhealth",
    "maxjugglepoints",
    "maxmp",
    "model",
    "mp",
    "mpset",
    "name",
    "nameposition",
    "nextanim",
    "nextthink",
    "no_adjust_base",
    "noaicontrol",
    "nodieblink",
    "nodrop",
    "nograb",
    "nopain",
    "offense",
    "opponent",
    "owner",
    "parent",
    "position",
    "projectile",
    "projectilehit",
    "running",
    "rush_count",
    "rush_tally",
    "rush_time",
    "scroll",
    "seal",
    "sealtime",
    "setlayer",
    "speed",
    "stalltime",
    "stats",
    "staydown",
    "staydownatk",
    "stealth",
    "subentity",
    "subject_to_gravity",
    "subject_to_hole",
    "subject_to_maxz",
    "subject_to_minz",
    "subject_to_obstacle",
    "subject_to_platform",
    "subject_to_screen",
    "subject_to_wall",
    "takeaction",
    "think",
    "throwdamage",
    "throwdist",
    "throwframewait",
    "throwheight",
    "tosstime",
    "trymove",
    "type",
    "velocity",
    "weapon"
};
static const char *const bor_lua_changeentityproperty_success[] = {
    "model",
    "weapon",
    "maxhealth",
    "health",
    "maxmp",
    "mp",
    "setlayer",
    "speed",
    "throwdamage",
    "throwdist",
    "throwframewait",
    "throwheight",
    "animhits",
    "alpha",
    "guardpoints",
    "maxguardpoints",
    "jugglepoints",
    "komap",
    "maxjugglepoints",
    "antigravity",
    "map",
    "maptime",
    "colourmap",
    "hmapl",
    "hmapu",
    "name",
    "defaultname",
    "position",
    "base",
    "direction",
    "trymove",
    "nextanim",
    "nextthink",
    "tosstime",
    "velocity",
    "defense",
    "offense",
    "animation",
    "invincible",
    "rush_count",
    "rush_tally",
    "blink",
    "subject_to_screen",
    "subject_to_maxz",
    "subject_to_minz",
    "subject_to_wall",
    "subject_to_obstacle",
    "subject_to_platform",
    "subject_to_gravity",
    "subject_to_hole",
    "combostep",
    "attacking",
    "scroll",
    "autokill",
    "staydown",
    "running",
    "dot",
    "edelay",
    "energycost",
    "mpset"
};
static int bor_lua_changeentityproperty_legacy_result(const char *property)
{
    size_t i;
    for (i = 0; i < sizeof(bor_lua_changeentityproperty_success) / sizeof(bor_lua_changeentityproperty_success[0]); ++i) {
        if (!bor_lua_stricmp(property, bor_lua_changeentityproperty_success[i]))
            return 1;
    }
    return 0;
}

static int bor_lua_legacy_truthy(lua_State *L, int index)
{
    switch (lua_type(L, index)) {
    case LUA_TBOOLEAN:
        return lua_toboolean(L, index);
    case LUA_TNUMBER:
        return lua_tonumber(L, index) != 0;
    case LUA_TSTRING: {
        size_t length = 0;
        lua_tolstring(L, index, &length);
        return length != 0;
    }
    case LUA_TLIGHTUSERDATA:
        return lua_touserdata(L, index) != NULL;
    case LUA_TUSERDATA:
        return 1;
    default:
        return 0;
    }
}


extern int noshare;
extern int credits;
extern s_savedata savedata;
extern char musicname[128];
extern float musicfade[2];
extern int musicloop;
extern u32 musicoffset;
extern entity *self;
extern int g_lua_current_player_index;
extern u32 borTime;
extern char branch_name[MAX_NAME_LEN + 1];
extern int current_set;
extern int current_level;
extern int current_stage;
extern int current_palette;
extern float advancex;
extern float advancey;
extern float lasthitx;
extern float lasthitz;
extern float lasthita;
extern int lasthitt;
extern int lasthitc;
extern float blockade;
extern float scrollminz;
extern float scrollmaxz;
extern int light[2];
extern int shadowalpha;
extern unsigned char slowmotion[3];
extern int selectScreen;
extern unsigned int models_loaded;
extern unsigned int models_cached;
extern int ent_count;
extern int ent_max;
extern int gfx_y_offset;
extern int panel_height;
extern int pause;
extern entity *textbox;
extern entity *smartbomber;

/* Entity handle helpers are defined below the variant API. */
void bor_lua_push_entity(lua_State *L, entity *ent);
static entity *bor_lua_get_live_entity(lua_State *L, int index);
static entity *bor_lua_get_live_entity_ptr(entity *candidate);

// openbor.log("message")
static int lua_openbor_log(lua_State *L)
{
    const char *message;
    if (lua_gettop(L) != 1)
        return luaL_error(L, "log expects exactly one value");
    bor_lua_push_legacy_string(L, 1);
    message = lua_tostring(L, -1);
    printf("%s", message);
    fflush(stdout);
    return 0;
}

// openbor.get_time()
static int lua_openbor_get_time(lua_State *L) {
    lua_pushinteger(L, (lua_Integer)borTime);
    return 1;
}


// changeopenborvariant(name, value)
// Implements the numeric legacy system-property setters.
static int lua_changeopenborvariant(lua_State *L) {
    static const char *const properties[] = {
        "blockade", "elapsed_time", "lasthita", "lasthitc", "lasthitt",
        "lasthitx", "lasthitz", "levelpos", "scrollmaxz", "scrollminz",
        "slowmotion", "slowmotion_duration", "smartbomber", "textbox",
        "xpos", "ypos"
    };
    const char *name;
    lua_Integer value;

    if (lua_gettop(L) != 2)
        return luaL_error(L, "changeopenborvariant expects exactly 2 arguments");

    if (lua_isinteger(L, 1)) {
        lua_Integer index = lua_tointeger(L, 1);
        if (index < 0 || index >= (lua_Integer)(sizeof(properties) / sizeof(properties[0])))
            return luaL_error(L, "changeopenborvariant: property index is out of range");
        name = properties[index];
    } else {
        name = luaL_checkstring(L, 1);
    }

    /* The legacy API accepts entity pointers for these two properties. */
    if (!bor_lua_stricmp(name, "smartbomber") || !bor_lua_stricmp(name, "textbox")) {
        entity *ent = NULL;

        if (!lua_isnil(L, 2)) {
            ent = bor_lua_get_live_entity(L, 2);
            if (!ent || !ent->exists)
                return luaL_error(L, "changeopenborvariant: '%s' expects a live entity or nil", name);
        }

        if (!bor_lua_stricmp(name, "smartbomber"))
            smartbomber = ent;
        else
            textbox = ent;

        return 0;
    }

    value = bor_lua_check_legacy_integer(L, 2, "changeopenborvariant");

    if (!bor_lua_stricmp(name, "elapsed_time")) {
        borTime = (u32)value;
    } else if (!bor_lua_stricmp(name, "levelpos")) {
        if (!level)
            return luaL_error(L, "changeopenborvariant: 'levelpos' requires a loaded level");
        level->pos = (int)value;
    } else if (!bor_lua_stricmp(name, "xpos")) {
        advancex = (float)value;
    } else if (!bor_lua_stricmp(name, "ypos")) {
        advancey = (float)value;
    } else if (!bor_lua_stricmp(name, "scrollminz")) {
        scrollminz = (float)value;
    } else if (!bor_lua_stricmp(name, "scrollmaxz")) {
        scrollmaxz = (float)value;
    } else if (!bor_lua_stricmp(name, "blockade")) {
        blockade = (float)value;
    } else if (!bor_lua_stricmp(name, "slowmotion")) {
        slowmotion[0] = (unsigned char)value;
    } else if (!bor_lua_stricmp(name, "slowmotion_duration")) {
        slowmotion[1] = (unsigned char)value;
    } else if (!bor_lua_stricmp(name, "lasthitx")) {
        lasthitx = (float)value;
    } else if (!bor_lua_stricmp(name, "lasthitz")) {
        lasthitz = (float)value;
    } else if (!bor_lua_stricmp(name, "lasthita")) {
        lasthita = (float)value;
    } else if (!bor_lua_stricmp(name, "lasthitc")) {
        lasthitc = (int)value;
    } else if (!bor_lua_stricmp(name, "lasthitt")) {
        lasthitt = (int)value;
    } else {
        return luaL_error(L, "changeopenborvariant: unsupported or unimplemented property '%s'", name);
    }

    return 0;
}

// openborvariant(name)
// First compatibility milestone: read-only legacy system properties.
static int lua_openborvariant(lua_State *L) {
    static const char *const properties[] = {
        "branchname", "count_enemies", "count_entities", "count_npcs",
        "count_players", "current_level", "current_palette", "current_set",
        "current_stage", "elapsed_time", "ent_max", "game_paused",
        "game_speed", "gfx_y_offset", "hResolution", "in_level",
        "in_selectscreen", "lasthita", "lasthitc", "lasthitt", "lasthitx",
        "lasthitz", "levelheight", "levelwidth", "lightx", "lightz",
        "maxentityvars", "maxglobalvars", "maxindexedvars", "maxplayers",
        "maxscriptvars", "models_cached", "models_loaded", "numpalettes",
        "pixelformat", "player", "player1", "player2", "player3", "player4",
        "player_max_z", "player_min_z", "shadowalpha", "shadowcolor",
        "slowmotion", "slowmotion_duration", "totalram", "freeram",
        "usedram", "vResolution", "xpos", "ypos"
    };
    const char *name;
    extern s_player player[4];

    if (lua_gettop(L) != 1)
        return luaL_error(L, "openborvariant expects exactly 1 argument");

    if (lua_isinteger(L, 1)) {
        lua_Integer index = lua_tointeger(L, 1);
        if (index < 0 || index >= (lua_Integer)(sizeof(properties) / sizeof(properties[0])))
            return luaL_error(L, "openborvariant: property index is out of range");
        name = properties[index];
    } else {
        name = luaL_checkstring(L, 1);
    }

    if (!bor_lua_stricmp(name, "branchname")) {
        lua_pushstring(L, branch_name);
    } else if (!bor_lua_stricmp(name, "current_set")) {
        lua_pushinteger(L, current_set);
    } else if (!bor_lua_stricmp(name, "current_level")) {
        lua_pushinteger(L, current_level);
    } else if (!bor_lua_stricmp(name, "current_stage")) {
        lua_pushinteger(L, current_stage);
    } else if (!bor_lua_stricmp(name, "current_palette")) {
        lua_pushinteger(L, current_palette);
    } else if (!bor_lua_stricmp(name, "elapsed_time")) {
        lua_pushinteger(L, (lua_Integer)borTime);
    } else if (!bor_lua_stricmp(name, "ent_max")) {
        lua_pushinteger(L, ent_max);
    } else if (!bor_lua_stricmp(name, "in_level")) {
        /* Preserve the original legacy API's return semantics. */
        lua_pushinteger(L, level == NULL ? 1 : 0);
    } else if (!bor_lua_stricmp(name, "in_selectscreen")) {
        lua_pushinteger(L, selectScreen);
    } else if (!bor_lua_stricmp(name, "xpos")) {
        if (!level) return luaL_error(L, "openborvariant: 'xpos' requires a loaded level");
        lua_pushnumber(L, advancex);
    } else if (!bor_lua_stricmp(name, "ypos")) {
        if (!level) return luaL_error(L, "openborvariant: 'ypos' requires a loaded level");
        lua_pushnumber(L, advancey);
    } else if (!bor_lua_stricmp(name, "lasthitx")) {
        lua_pushnumber(L, lasthitx);
    } else if (!bor_lua_stricmp(name, "lasthitz")) {
        lua_pushnumber(L, lasthitz);
    } else if (!bor_lua_stricmp(name, "lasthita")) {
        lua_pushnumber(L, lasthita);
    } else if (!bor_lua_stricmp(name, "lasthitt")) {
        lua_pushinteger(L, lasthitt);
    } else if (!bor_lua_stricmp(name, "lasthitc")) {
        lua_pushinteger(L, lasthitc);
    } else if (!bor_lua_stricmp(name, "hResolution")) {
        lua_pushinteger(L, videomodes.hRes);
    } else if (!bor_lua_stricmp(name, "vResolution")) {
        lua_pushinteger(L, videomodes.vRes);
    } else if (!bor_lua_stricmp(name, "lightx")) {
        lua_pushinteger(L, light[0]);
    } else if (!bor_lua_stricmp(name, "lightz")) {
        lua_pushinteger(L, light[1]);
    } else if (!bor_lua_stricmp(name, "shadowalpha")) {
        lua_pushinteger(L, shadowalpha);
    } else if (!bor_lua_stricmp(name, "shadowcolor")) {
        lua_pushinteger(L, colors.shadow);
    } else if (!bor_lua_stricmp(name, "pixelformat")) {
        lua_pushinteger(L, pixelformat);
    } else if (!bor_lua_stricmp(name, "totalram")) {
        lua_pushinteger(L, 64 * 1024);
    } else if (!bor_lua_stricmp(name, "freeram")) {
        lua_pushinteger(L, 63 * 1024);
    } else if (!bor_lua_stricmp(name, "usedram")) {
        lua_pushinteger(L, 1024);
    } else if (!bor_lua_stricmp(name, "slowmotion")) {
        lua_pushinteger(L, slowmotion[0]);
    } else if (!bor_lua_stricmp(name, "slowmotion_duration")) {
        lua_pushinteger(L, slowmotion[1]);
    } else if (!bor_lua_stricmp(name, "models_loaded")) {
        lua_pushinteger(L, (lua_Integer)models_loaded);
    } else if (!bor_lua_stricmp(name, "models_cached")) {
        lua_pushinteger(L, (lua_Integer)models_cached);
    } else if (!bor_lua_stricmp(name, "maxentityvars")) {
        lua_pushinteger(L, max_entity_vars);
    } else if (!bor_lua_stricmp(name, "maxglobalvars")) {
        lua_pushinteger(L, 2048);
    } else if (!bor_lua_stricmp(name, "maxindexedvars")) {
        lua_pushinteger(L, max_indexed_vars);
    } else if (!bor_lua_stricmp(name, "maxscriptvars")) {
        lua_pushinteger(L, max_script_vars);
    } else if (!bor_lua_stricmp(name, "maxplayers")) {
        int set_index = current_set >= 0 ? current_set : 0;
        if (set_index >= MAX_DIFFICULTIES)
            return luaL_error(L, "openborvariant: 'maxplayers' current_set is out of range");
        lua_pushinteger(L, maxplayers[set_index]);
    } else if (!bor_lua_stricmp(name, "player") || !bor_lua_stricmp(name, "player1") ||
               !bor_lua_stricmp(name, "player2") || !bor_lua_stricmp(name, "player3") ||
               !bor_lua_stricmp(name, "player4")) {
        int index = !bor_lua_stricmp(name, "player") ? 0 : name[6] - '1';
        entity *player_ent = bor_lua_get_live_entity_ptr(player[index].ent);
        if (player_ent && player_ent->exists)
            bor_lua_push_entity(L, player_ent);
        else
            lua_pushnil(L);
    } else if (!bor_lua_stricmp(name, "count_enemies")) {
        lua_pushinteger(L, count_ents(TYPE_ENEMY));
    } else if (!bor_lua_stricmp(name, "count_npcs")) {
        lua_pushinteger(L, count_ents(TYPE_NPC));
    } else if (!bor_lua_stricmp(name, "count_players")) {
        lua_pushinteger(L, count_ents(TYPE_PLAYER));
    } else if (!bor_lua_stricmp(name, "count_entities")) {
        lua_pushinteger(L, ent_count);
    } else if (!bor_lua_stricmp(name, "game_paused")) {
        lua_pushinteger(L, pause);
    } else if (!bor_lua_stricmp(name, "levelwidth")) {
        if (!level) return luaL_error(L, "openborvariant: 'levelwidth' requires a loaded level");
        lua_pushinteger(L, level->width);
    } else if (!bor_lua_stricmp(name, "levelheight")) {
        if (!level) return luaL_error(L, "openborvariant: 'levelheight' requires a loaded level");
        lua_pushinteger(L, panel_height);
    } else if (!bor_lua_stricmp(name, "numpalettes")) {
        if (!level) return luaL_error(L, "openborvariant: 'numpalettes' requires a loaded level");
        lua_pushinteger(L, level->numpalettes);
    } else if (!bor_lua_stricmp(name, "gfx_y_offset")) {
        if (!level) return luaL_error(L, "openborvariant: 'gfx_y_offset' requires a loaded level");
        lua_pushinteger(L, gfx_y_offset);
    } else if (!bor_lua_stricmp(name, "game_speed")) {
        if (!level) return luaL_error(L, "openborvariant: 'game_speed' requires a loaded level");
        lua_pushinteger(L, GAME_SPEED);
    } else if (!bor_lua_stricmp(name, "player_max_z")) {
        lua_pushinteger(L, PLAYER_MAX_Z);
    } else if (!bor_lua_stricmp(name, "player_min_z")) {
        lua_pushinteger(L, PLAYER_MIN_Z);
    } else {
        return luaL_error(L, "openborvariant: unsupported or unimplemented property '%s'", name);
    }

    return 1;
}

// openbor.get_player_health()
static int lua_openbor_get_player_health(lua_State *L) {
    extern s_player player[4];
    if (player[0].ent && player[0].ent->exists) {
        lua_pushinteger(L, (lua_Integer)player[0].ent->health);
    } else {
        lua_pushinteger(L, 0);
    }
    return 1;
}

// openbor.rand()
static int lua_openbor_rand(lua_State *L) {
    lua_pushinteger(L, (lua_Integer)rand32());
    return 1;
}

// openbor.drawstring(x, y, font, text, z)
static int lua_openbor_drawstring(lua_State *L)
{
    int top = lua_gettop(L);
    int x = bor_lua_check_legacy_integer(L, 1, "drawstring");
    int y = bor_lua_check_legacy_integer(L, 2, "drawstring");
    int font = bor_lua_check_legacy_integer(L, 3, "drawstring");
    int z = top >= 5 ? bor_lua_check_legacy_integer(L, 5, "drawstring") : 0;
    const char *text;

    bor_lua_push_legacy_string(L, 4);
    text = lua_tostring(L, -1);
    font_printf(x, y, font, z, "%s", text);
    return 0;
}


// openbor.drawbox(x, y, width, height, z, color, lut)
static int lua_openbor_drawbox(lua_State *L) {
    int x = (int)bor_lua_check_legacy_integer(L, 1, "drawbox");
    int y = (int)bor_lua_check_legacy_integer(L, 2, "drawbox");
    int w = (int)bor_lua_check_legacy_integer(L, 3, "drawbox");
    int h = (int)bor_lua_check_legacy_integer(L, 4, "drawbox");
    int z = (int)bor_lua_check_legacy_integer(L, 5, "drawbox");
    int color = (int)bor_lua_check_legacy_integer(L, 6, "drawbox");
    int lut_index = (int)(lua_gettop(L) >= 7 ? bor_lua_check_legacy_integer(L, 7, "drawbox") : -1); 

    if (lut_index >= 0) {
        lut_index %= MAX_BLENDINGS + 1;
    }
    spriteq_add_box(x, y, w, h, z, color, lut_index);
    return 0;
}

/* A Lua lightuserdata value can become stale after an entity is destroyed.
 * Never dereference it until it has been found in the live entity table. */
static entity *bor_lua_get_live_entity_ptr(entity *candidate)
{
    extern entity *ent_list[MAX_ENTS];
    int i;

    if (!candidate)
        return NULL;

    for (i = 0; i < MAX_ENTS; ++i)
    {
        if (ent_list[i] == candidate)
            return candidate;
    }

    return NULL;
}

typedef struct bor_lua_entity_handle {
    entity *ptr;
} bor_lua_entity_handle;

#define BOR_LUA_ENTITY_METATABLE "OpenBOR.Entity"
#define BOR_LUA_SCREEN_METATABLE "OpenBOR.Screen"
#define BOR_LUA_SPRITE_METATABLE "OpenBOR.Sprite"
#define BOR_LUA_RESOURCES "OpenBOR.ScriptResources"

static void bor_lua_retain_resource(lua_State *L, int index)
{
    index = lua_absindex(L, index);
    lua_getfield(L, LUA_REGISTRYINDEX, BOR_LUA_RESOURCES);
    lua_pushvalue(L, index);
    lua_pushboolean(L, 1);
    lua_rawset(L, -3);
    lua_pop(L, 1);
}

static void bor_lua_release_resource(lua_State *L, int index)
{
    index = lua_absindex(L, index);
    lua_getfield(L, LUA_REGISTRYINDEX, BOR_LUA_RESOURCES);
    lua_pushvalue(L, index);
    lua_pushnil(L);
    lua_rawset(L, -3);
    lua_pop(L, 1);
}


typedef struct {
    s_sprite *sprite;
} bor_lua_sprite_handle;

static s_sprite *bor_lua_check_sprite(lua_State *L, int index)
{
    bor_lua_sprite_handle *handle =
        (bor_lua_sprite_handle *)luaL_testudata(L, index, BOR_LUA_SPRITE_METATABLE);

    if (handle) {
        if (!handle->sprite)
            luaL_error(L, "sprite handle has already been freed");
        return handle->sprite;
    }

    /* getentityproperty(..., "sprite"/"icon") exposes engine-owned sprites. */
    if (lua_islightuserdata(L, index)) {
        s_sprite *sprite = (s_sprite *)lua_touserdata(L, index);
        if (sprite)
            return sprite;
    }

    luaL_error(L, "expected a sprite handle or engine sprite pointer");
    return NULL;
}

static int bor_lua_sprite_gc(lua_State *L)
{
    bor_lua_sprite_handle *handle = (bor_lua_sprite_handle *)luaL_checkudata(L, 1, BOR_LUA_SPRITE_METATABLE);
    if (handle->sprite) {
        free(handle->sprite);
        handle->sprite = NULL;
    }
    return 0;
}

static s_drawmethod bor_lua_drawmethod;


typedef struct {
    s_screen *screen;
} bor_lua_screen_handle;

static s_screen *bor_lua_check_screen(lua_State *L, int index)
{
    bor_lua_screen_handle *handle = (bor_lua_screen_handle *)luaL_checkudata(L, index, BOR_LUA_SCREEN_METATABLE);
    if (!handle->screen)
        luaL_error(L, "screen handle has already been freed");
    return handle->screen;
}

static int bor_lua_screen_gc(lua_State *L)
{
    bor_lua_screen_handle *handle = (bor_lua_screen_handle *)luaL_checkudata(L, 1, BOR_LUA_SCREEN_METATABLE);
    if (handle->screen)
        freescreen(&handle->screen);
    return 0;
}


void bor_lua_push_entity(lua_State *L, entity *ent)
{
    bor_lua_entity_handle *handle;
    int cache_index;

    if (!ent) {
        lua_pushnil(L);
        return;
    }

    /*
     * Reuse the Lua wrapper while it is still referenced. This preserves
     * Lua identity (including references stored in global variables) when
     * the same live engine entity is returned by different API functions.
     * The registry table uses weak values so it does not keep wrappers alive.
     */
    lua_getfield(L, LUA_REGISTRYINDEX, "OpenBOR.EntityCache");
    cache_index = lua_gettop(L);
    lua_pushlightuserdata(L, ent);
    lua_rawget(L, cache_index);
    if (!lua_isnil(L, -1)) {
        lua_remove(L, cache_index); /* cache table; leave the wrapper */
        return;
    }
    lua_pop(L, 1); /* cache miss */

    handle = (bor_lua_entity_handle *)lua_newuserdata(L, sizeof(*handle));
    handle->ptr = ent;
    luaL_getmetatable(L, BOR_LUA_ENTITY_METATABLE);
    lua_setmetatable(L, -2);

    /* cache[ent] = wrapper */
    lua_pushlightuserdata(L, ent);
    lua_pushvalue(L, -2);
    lua_rawset(L, cache_index);

    lua_remove(L, cache_index); /* leave only the new wrapper on the stack */
}

static entity *bor_lua_get_live_entity(lua_State *L, int index)
{
    entity *candidate = NULL;

    /* Accept raw lightuserdata for compatibility, but expose entity handles
     * as full userdata so Lua can safely use ent.health-style property access. */
    if (lua_islightuserdata(L, index)) {
        candidate = (entity *)lua_touserdata(L, index);
    } else if (lua_isuserdata(L, index)) {
        bor_lua_entity_handle *handle;
        int matches;

        if (!lua_getmetatable(L, index))
            return NULL;
        luaL_getmetatable(L, BOR_LUA_ENTITY_METATABLE);
        matches = lua_rawequal(L, -1, -2);
        lua_pop(L, 2);
        if (!matches)
            return NULL;

        handle = (bor_lua_entity_handle *)lua_touserdata(L, index);
        if (!handle)
            return NULL;
        candidate = handle->ptr;
    } else {
        return NULL;
    }

    return bor_lua_get_live_entity_ptr(candidate);
}

// killentity(entity_pointer)
static int lua_openbor_killentity(lua_State *L) {
    entity *ent;

    if (lua_gettop(L) < 1)
        return luaL_error(L, "killentity expects an entity");
    ent = bor_lua_get_live_entity(L, 1);
    if (ent && ent->exists) {
        kill(ent);
        lua_pushinteger(L, 1);
    } else {
        lua_pushinteger(L, 0);
    }

    return 1;
}

// openbor.getentity(index)
static int lua_openbor_getentity(lua_State *L)
{
    int idx;
    extern entity *ent_list[MAX_ENTS];

    if (lua_gettop(L) != 1 || lua_type(L, 1) != LUA_TNUMBER)
        return luaL_error(L, "getentity expects one numeric entity index");
    idx = (int)lua_tonumber(L, 1);

    if (idx >= 0 && idx < MAX_ENTS && ent_list[idx])
        bor_lua_push_entity(L, ent_list[idx]);
    else
        lua_pushnil(L);
    return 1;
}

static int lua_openbor_damageentity(lua_State *L) {
    entity *ent;
    entity *other;
    entity *temp;
    s_attack attack;
    int force = 1;
    int drop = 0;
    int type = ATK_NORMAL;
    lua_Number value;

    if (lua_gettop(L) < 1)
        return luaL_error(L, "damageentity expects a target entity");
    ent = bor_lua_get_live_entity(L, 1);
    if (!ent || !ent->exists) {
        lua_pushinteger(L, 0);
        return 1;
    }

    other = ent;
    if (lua_gettop(L) >= 2) {
        other = bor_lua_get_live_entity(L, 2);
        if (!other) {
            lua_pushinteger(L, 0);
            return 1;
        }
    }

    if (lua_gettop(L) >= 3) {
        if (lua_type(L, 3) != LUA_TNUMBER) {
            lua_pushinteger(L, 0);
            return 1;
        }
        value = lua_tonumber(L, 3);
        force = (int)value;
    }

    /*
     * The legacy implementation directly adjusts health when the target
     * has no takedamage callback. This path returns before validating the
     * optional drop and attack-type arguments.
     */
    if (!ent->takedamage) {
        ent->health -= force;
        if (ent->health <= 0)
            kill(ent);

        lua_pushinteger(L, 1);
        return 1;
    }

    if (lua_gettop(L) >= 4) {
        if (lua_type(L, 4) != LUA_TNUMBER) {
            lua_pushinteger(L, 0);
            return 1;
        }
        value = lua_tonumber(L, 4);
        drop = (int)value;
    }

    if (lua_gettop(L) >= 5) {
        if (lua_type(L, 5) != LUA_TNUMBER) {
            lua_pushinteger(L, 0);
            return 1;
        }
        value = lua_tonumber(L, 5);
        type = (int)value;
    }

    temp = self;
    self = ent;

    attack = emptyattack;
    attack.attack_force = force;
    attack.attack_drop = drop;
    attack.attack_type = type;

    if (drop) {
        attack.dropv[0] = 3.0f;
        attack.dropv[1] = 1.2f;
        attack.dropv[2] = 0.0f;
    }

    self->takedamage(other, &attack);
    self = temp;

    /* The original scriptlib leaves the return value at zero here. */
    lua_pushinteger(L, 0);
    return 1;
}

// getentityproperty(entity, property, ...)
// Lua implementation of the legacy OpenBOR entity-property query API.
static int lua_openbor_getentityproperty(lua_State *L) {
    entity *ent;
    const char *prop;
    int property_index;

    if (lua_gettop(L) < 2)
        return luaL_error(L, "getentityproperty expects an entity and property name or ID");
    property_index = bor_lua_legacy_enum_index(L, 2, bor_lua_getentityproperty_properties,
        sizeof(bor_lua_getentityproperty_properties) / sizeof(bor_lua_getentityproperty_properties[0]));
    if (!lua_isnil(L, 1) && !lua_islightuserdata(L, 1) &&
        !luaL_testudata(L, 1, BOR_LUA_ENTITY_METATABLE))
        return luaL_error(L, "getentityproperty expects an entity handle or nil");
    ent = bor_lua_get_live_entity(L, 1);
    if (!ent || !ent->exists) {
        lua_pushnil(L);
        return 1;
    }
    if (property_index < 0)
        return luaL_error(L, "getentityproperty: unsupported property");
    prop = bor_lua_getentityproperty_properties[property_index];

    if (!bor_lua_stricmp(prop, "health"))              lua_pushinteger(L, ent->health);
    else if (!bor_lua_stricmp(prop, "mp"))             lua_pushinteger(L, ent->mp);
    else if (!bor_lua_stricmp(prop, "maxhealth"))      lua_pushinteger(L, ent->modeldata.health);
    else if (!bor_lua_stricmp(prop, "maxmp"))          lua_pushinteger(L, ent->modeldata.mp);
    else if (!bor_lua_stricmp(prop, "x"))              lua_pushnumber(L, ent->x);
    else if (!bor_lua_stricmp(prop, "z"))              lua_pushnumber(L, ent->z);
    else if (!bor_lua_stricmp(prop, "a"))              lua_pushnumber(L, ent->a);
    else if (!bor_lua_stricmp(prop, "xdir"))           lua_pushnumber(L, ent->xdir);
    else if (!bor_lua_stricmp(prop, "zdir"))           lua_pushnumber(L, ent->zdir);
    else if (!bor_lua_stricmp(prop, "tossv"))           lua_pushnumber(L, ent->tossv);
    else if (!bor_lua_stricmp(prop, "base"))            lua_pushnumber(L, ent->base);
    else if (!bor_lua_stricmp(prop, "jumpz"))           lua_pushnumber(L, ent->jumpz);
    else if (!bor_lua_stricmp(prop, "jumpx"))           lua_pushnumber(L, ent->jumpx);
    else if (!bor_lua_stricmp(prop, "jumpv"))           lua_pushnumber(L, ent->jumpv);
    else if (!bor_lua_stricmp(prop, "direction"))       lua_pushinteger(L, ent->direction);
    else if (!bor_lua_stricmp(prop, "exists"))          lua_pushinteger(L, ent->exists ? 1 : 0);
    else if (!bor_lua_stricmp(prop, "dead"))            lua_pushinteger(L, ent->dead ? 1 : 0);
    else if (!bor_lua_stricmp(prop, "dying"))           lua_pushinteger(L, ent->dying ? 1 : 0);
    else if (!bor_lua_stricmp(prop, "attacking"))       lua_pushinteger(L, ent->attacking ? 1 : 0);
    else if (!bor_lua_stricmp(prop, "blocking"))        lua_pushinteger(L, ent->blocking ? 1 : 0);
    else if (!bor_lua_stricmp(prop, "falling"))         lua_pushinteger(L, ent->falling ? 1 : 0);
    else if (!bor_lua_stricmp(prop, "frozen"))          lua_pushinteger(L, ent->frozen ? 1 : 0);
    else if (!bor_lua_stricmp(prop, "invincible"))      lua_pushinteger(L, ent->invincible ? 1 : 0);
    else if (!bor_lua_stricmp(prop, "projectile"))      lua_pushinteger(L, ent->projectile ? 1 : 0);
    else if (!bor_lua_stricmp(prop, "autokill"))        lua_pushinteger(L, ent->autokill ? 1 : 0);
    else if (!bor_lua_stricmp(prop, "seal"))            lua_pushinteger(L, ent->seal ? 1 : 0);
    else if (!bor_lua_stricmp(prop, "name"))            lua_pushstring(L, ent->name);
    else if (!bor_lua_stricmp(prop, "model"))           lua_pushstring(L, ent->model ? ent->model->name : "");
    else if (!bor_lua_stricmp(prop, "defaultmodel"))    lua_pushstring(L, ent->defaultmodel ? ent->defaultmodel->name : "");
    else if (!bor_lua_stricmp(prop, "spawntype"))        lua_pushinteger(L, ent->spawntype);
    else if (!bor_lua_stricmp(prop, "playerindex"))     lua_pushinteger(L, ent->playerindex);
    else if (!bor_lua_stricmp(prop, "map")) {
        int i;
        lua_Integer map_index = 0;
        for (i = 0; i < ent->modeldata.maps_loaded; ++i) {
            if (ent->colourmap == ent->modeldata.colourmap[i]) {
                map_index = i + 1;
                break;
            }
        }
        lua_pushinteger(L, map_index);
    }
    else if (!bor_lua_stricmp(prop, "animnum"))         lua_pushinteger(L, ent->animnum);
    else if (!bor_lua_stricmp(prop, "animpos"))         lua_pushinteger(L, ent->animpos);
    else if (!bor_lua_stricmp(prop, "nextanim"))        lua_pushinteger(L, ent->nextanim);
    else if (!bor_lua_stricmp(prop, "nextthink"))       lua_pushinteger(L, ent->nextthink);
    else if (!bor_lua_stricmp(prop, "attackid"))        lua_pushinteger(L, ent->attack_id);
    else if (!bor_lua_stricmp(prop, "hitbyid"))         lua_pushinteger(L, ent->hit_by_attack_id);
    else if (!bor_lua_stricmp(prop, "speed"))           lua_pushnumber(L, ent->modeldata.speed);
    else if (!bor_lua_stricmp(prop, "height"))          lua_pushinteger(L, ent->modeldata.height);
    else if (!bor_lua_stricmp(prop, "alpha"))           lua_pushinteger(L, ent->modeldata.alpha);
    else if (!bor_lua_stricmp(prop, "setlayer"))        lua_pushinteger(L, ent->modeldata.setlayer);
    else if (!bor_lua_stricmp(prop, "mprate"))          lua_pushinteger(L, ent->modeldata.mprate);
    else if (!bor_lua_stricmp(prop, "mpdroprate"))      lua_pushinteger(L, ent->modeldata.mpdroprate);
    else if (!bor_lua_stricmp(prop, "chargerate"))      lua_pushinteger(L, ent->modeldata.chargerate);
    else if (!bor_lua_stricmp(prop, "guardpoints"))     lua_pushinteger(L, ent->modeldata.guardpoints[0]);
    else if (!bor_lua_stricmp(prop, "maxguardpoints"))  lua_pushinteger(L, ent->modeldata.guardpoints[1]);
    else if (!bor_lua_stricmp(prop, "jugglepoints"))    lua_pushinteger(L, ent->modeldata.jugglepoints[0]);
    else if (!bor_lua_stricmp(prop, "maxjugglepoints")) lua_pushinteger(L, ent->modeldata.jugglepoints[1]);
    else if (!bor_lua_stricmp(prop, "player"))           lua_pushinteger(L, ent->playerindex);
    else if (!bor_lua_stricmp(prop, "owner")) {
        if (bor_lua_get_live_entity_ptr(ent->owner) && ent->owner->exists) bor_lua_push_entity(L, ent->owner); else lua_pushnil(L);
    }
    else if (!bor_lua_stricmp(prop, "parent")) {
        if (bor_lua_get_live_entity_ptr(ent->parent) && ent->parent->exists) bor_lua_push_entity(L, ent->parent); else lua_pushnil(L);
    }
    else if (!bor_lua_stricmp(prop, "subentity")) {
        if (bor_lua_get_live_entity_ptr(ent->subentity) && ent->subentity->exists) bor_lua_push_entity(L, ent->subentity); else lua_pushnil(L);
    }
    else if (!bor_lua_stricmp(prop, "opponent")) {
        if (bor_lua_get_live_entity_ptr(ent->opponent) && ent->opponent->exists) bor_lua_push_entity(L, ent->opponent); else lua_pushnil(L);
    }
    else if (!bor_lua_stricmp(prop, "link")) {
        if (bor_lua_get_live_entity_ptr(ent->link) && ent->link->exists) bor_lua_push_entity(L, ent->link); else lua_pushnil(L);
    }
    /* Additional properties supported by the original legacy API. */
    else if (!bor_lua_stricmp(prop, "animal"))            lua_pushinteger(L, ent->modeldata.animal);
    else if (!bor_lua_stricmp(prop, "animhits")) {
        if (ent->animation) lua_pushinteger(L, ent->animation->animhits); else lua_pushnil(L);
    }
    else if (!bor_lua_stricmp(prop, "tosstime"))           lua_pushinteger(L, ent->toss_time);
    else if (!bor_lua_stricmp(prop, "mpstableval"))        lua_pushinteger(L, ent->modeldata.mpstableval);
    else if (!bor_lua_stricmp(prop, "mpstable"))           lua_pushinteger(L, ent->modeldata.mpstable);
    else if (!bor_lua_stricmp(prop, "mapcount"))           lua_pushinteger(L, ent->modeldata.maps_loaded + 1);
    else if (!bor_lua_stricmp(prop, "mapdefault"))         lua_pushinteger(L, ent->map);
    else if (!bor_lua_stricmp(prop, "hmapl"))              lua_pushinteger(L, ent->modeldata.hmap1);
    else if (!bor_lua_stricmp(prop, "hmapu"))              lua_pushinteger(L, ent->modeldata.hmap2);
    else if (!bor_lua_stricmp(prop, "defaultname"))        lua_pushstring(L, ent->defaultmodel ? ent->defaultmodel->name : "");
    else if (!bor_lua_stricmp(prop, "type"))               lua_pushinteger(L, ent->modeldata.type);
    else if (!bor_lua_stricmp(prop, "subtype"))            lua_pushinteger(L, ent->modeldata.subtype);
    else if (!bor_lua_stricmp(prop, "throwdamage"))        lua_pushinteger(L, ent->modeldata.throwdamage);
    else if (!bor_lua_stricmp(prop, "throwdist"))          lua_pushnumber(L, ent->modeldata.throwdist);
    else if (!bor_lua_stricmp(prop, "throwframewait"))     lua_pushinteger(L, ent->modeldata.throwframewait);
    else if (!bor_lua_stricmp(prop, "throwheight"))        lua_pushnumber(L, ent->modeldata.throwheight);
    else if (!bor_lua_stricmp(prop, "animheight")) {
        if (ent->animation) lua_pushinteger(L, ent->animation->height); else lua_pushnil(L);
    }
    else if (!bor_lua_stricmp(prop, "animationid"))        lua_pushinteger(L, ent->animnum);
    else if (!bor_lua_stricmp(prop, "animating"))          lua_pushinteger(L, ent->animating);
    else if (!bor_lua_stricmp(prop, "jumping"))            lua_pushinteger(L, ent->jumping ? 1 : 0);
    else if (!bor_lua_stricmp(prop, "idling"))             lua_pushinteger(L, ent->idling ? 1 : 0);
    else if (!bor_lua_stricmp(prop, "drop"))               lua_pushinteger(L, ent->drop ? 1 : 0);
    else if (!bor_lua_stricmp(prop, "getting"))            lua_pushinteger(L, ent->getting ? 1 : 0);
    else if (!bor_lua_stricmp(prop, "turning"))            lua_pushinteger(L, ent->turning ? 1 : 0);
    else if (!bor_lua_stricmp(prop, "charging"))           lua_pushinteger(L, ent->charging ? 1 : 0);
    else if (!bor_lua_stricmp(prop, "inpain"))             lua_pushinteger(L, ent->inpain ? 1 : 0);
    else if (!bor_lua_stricmp(prop, "toexplode"))          lua_pushinteger(L, ent->toexplode ? 1 : 0);
    else if (!bor_lua_stricmp(prop, "freezetime"))         lua_pushinteger(L, ent->freezetime);
    else if (!bor_lua_stricmp(prop, "invinctime"))         lua_pushinteger(L, ent->invinctime);
    else if (!bor_lua_stricmp(prop, "rush_count"))         lua_pushinteger(L, ent->rush[0]);
    else if (!bor_lua_stricmp(prop, "rush_tally"))         lua_pushinteger(L, ent->rush[1]);
    else if (!bor_lua_stricmp(prop, "rush_time"))          lua_pushinteger(L, ent->rushtime);
    else if (!bor_lua_stricmp(prop, "knockdowncount"))     lua_pushnumber(L, ent->knockdowncount);
    else if (!bor_lua_stricmp(prop, "lifespancountdown"))  lua_pushnumber(L, ent->lifespancountdown);
    else if (!bor_lua_stricmp(prop, "subject_to_screen"))  lua_pushinteger(L, ent->modeldata.subject_to_screen);
    else if (!bor_lua_stricmp(prop, "subject_to_minz"))    lua_pushinteger(L, ent->modeldata.subject_to_minz);
    else if (!bor_lua_stricmp(prop, "subject_to_maxz"))    lua_pushinteger(L, ent->modeldata.subject_to_maxz);
    else if (!bor_lua_stricmp(prop, "subject_to_wall"))    lua_pushinteger(L, ent->modeldata.subject_to_wall);
    else if (!bor_lua_stricmp(prop, "subject_to_hole"))    lua_pushinteger(L, ent->modeldata.subject_to_hole);
    else if (!bor_lua_stricmp(prop, "subject_to_gravity")) lua_pushinteger(L, ent->modeldata.subject_to_gravity);
    else if (!bor_lua_stricmp(prop, "subject_to_obstacle"))lua_pushinteger(L, ent->modeldata.subject_to_obstacle);
    else if (!bor_lua_stricmp(prop, "subject_to_platform"))lua_pushinteger(L, ent->modeldata.subject_to_platform);
    else if (!bor_lua_stricmp(prop, "no_adjust_base"))     lua_pushinteger(L, ent->modeldata.no_adjust_base);
    else if (!bor_lua_stricmp(prop, "noaicontrol"))        lua_pushinteger(L, ent->noaicontrol);
    else if (!bor_lua_stricmp(prop, "nodieblink"))         lua_pushinteger(L, ent->modeldata.nodieblink);
    else if (!bor_lua_stricmp(prop, "bounce"))             lua_pushinteger(L, ent->modeldata.bounce);
    else if (!bor_lua_stricmp(prop, "falldie"))            lua_pushinteger(L, ent->modeldata.falldie);
    else if (!bor_lua_stricmp(prop, "antigravity"))        lua_pushnumber(L, ent->antigravity);
    else if (!bor_lua_stricmp(prop, "path"))               lua_pushstring(L, ent->modeldata.path ? ent->modeldata.path : "");
    else if (!bor_lua_stricmp(prop, "grabbing")) {
        if (bor_lua_get_live_entity_ptr(ent->grabbing) && ent->grabbing->exists) bor_lua_push_entity(L, ent->grabbing); else lua_pushnil(L);
    }
    else if (!bor_lua_stricmp(prop, "aimove"))             lua_pushinteger(L, ent->modeldata.aimove);
    else if (!bor_lua_stricmp(prop, "aggression"))         lua_pushinteger(L, ent->modeldata.aggression);
    else if (!bor_lua_stricmp(prop, "aiattack"))           lua_pushinteger(L, ent->modeldata.aiattack);
    else if (!bor_lua_stricmp(prop, "scroll"))             lua_pushnumber(L, ent->modeldata.scroll);
    else if (!bor_lua_stricmp(prop, "damage_on_landing"))  lua_pushinteger(L, ent->damage_on_landing);
    else if (!bor_lua_stricmp(prop, "detect"))             lua_pushinteger(L, ent->modeldata.stealth[1]);
    else if (!bor_lua_stricmp(prop, "stealth"))            lua_pushinteger(L, ent->modeldata.stealth[0]);
    else if (!bor_lua_stricmp(prop, "stalltime"))          lua_pushinteger(L, ent->stalltime);
    else if (!bor_lua_stricmp(prop, "gfxshadow"))          lua_pushinteger(L, ent->modeldata.gfxshadow);
    else if (!bor_lua_stricmp(prop, "nograb"))             lua_pushinteger(L, ent->nograb);
    else if (!bor_lua_stricmp(prop, "nodrop"))             lua_pushinteger(L, ent->modeldata.nodrop);
    else if (!bor_lua_stricmp(prop, "nopain"))             lua_pushinteger(L, ent->modeldata.nopain);
    else if (!bor_lua_stricmp(prop, "running_speed"))      lua_pushnumber(L, ent->modeldata.runspeed);
    else if (!bor_lua_stricmp(prop, "running_jumpy"))      lua_pushnumber(L, ent->modeldata.runjumpheight);
    else if (!bor_lua_stricmp(prop, "running_jumpx"))      lua_pushnumber(L, ent->modeldata.runjumpdist);
    else if (!bor_lua_stricmp(prop, "running_land"))       lua_pushinteger(L, ent->modeldata.runhold);
    else if (!bor_lua_stricmp(prop, "running_movez"))      lua_pushinteger(L, ent->modeldata.runupdown);
    else if (!bor_lua_stricmp(prop, "sealtime"))           lua_pushinteger(L, ent->sealtime);
    else if (!bor_lua_stricmp(prop, "blockback"))          lua_pushinteger(L, ent->modeldata.blockback);
    else if (!bor_lua_stricmp(prop, "blockpain"))          lua_pushinteger(L, ent->modeldata.blockpain);
    else if (!bor_lua_stricmp(prop, "weapent")) {
        if (bor_lua_get_live_entity_ptr(ent->weapent) && ent->weapent->exists) bor_lua_push_entity(L, ent->weapent); else lua_pushnil(L);
    }
    else if (!bor_lua_stricmp(prop, "blink"))              lua_pushinteger(L, ent->blink ? 1 : 0);
    else if (!bor_lua_stricmp(prop, "animation") || !bor_lua_stricmp(prop, "animation.handle")) {
        if (ent->animation) lua_pushlightuserdata(L, ent->animation); else lua_pushnil(L);
    }
    else if (!bor_lua_stricmp(prop, "colourmap")) {
        if (ent->colourmap) lua_pushlightuserdata(L, ent->colourmap); else lua_pushnil(L);
    }
    else if (!bor_lua_stricmp(prop, "colourtable")) {
        int which = (int)bor_lua_check_legacy_integer(L, 3, "getentityproperty");
        if (which < 0 || which >= ent->modeldata.maps_loaded) lua_pushnil(L);
        else if (ent->modeldata.colourmap[which]) lua_pushlightuserdata(L, ent->modeldata.colourmap[which]);
        else lua_pushnil(L);
    }
    else if (!bor_lua_stricmp(prop, "aiflag")) {
        int flag_index;
        const char *flag;
        if (lua_gettop(L) < 3) { lua_pushnil(L); return 1; }
        flag_index = bor_lua_legacy_enum_index(L, 3, bor_lua_gep_aiflag, sizeof(bor_lua_gep_aiflag) / sizeof(bor_lua_gep_aiflag[0]));
        if (flag_index < 0) {
            if (lua_type(L, 3) == LUA_TSTRING)
                return luaL_error(L, "unknown aiflag property '%s'", lua_tostring(L, 3));
            lua_pushnil(L);
            return 1;
        }
        flag = bor_lua_gep_aiflag[flag_index];
        if (!bor_lua_stricmp(flag, "dead")) lua_pushinteger(L, ent->dead ? 1 : 0);
        else if (!bor_lua_stricmp(flag, "jumping")) lua_pushinteger(L, ent->jumping ? 1 : 0);
        else if (!bor_lua_stricmp(flag, "idling")) lua_pushinteger(L, ent->idling ? 1 : 0);
        else if (!bor_lua_stricmp(flag, "drop")) lua_pushinteger(L, ent->drop ? 1 : 0);
        else if (!bor_lua_stricmp(flag, "attacking")) lua_pushinteger(L, ent->attacking ? 1 : 0);
        else if (!bor_lua_stricmp(flag, "getting")) lua_pushinteger(L, ent->getting ? 1 : 0);
        else if (!bor_lua_stricmp(flag, "turning")) lua_pushinteger(L, ent->turning ? 1 : 0);
        else if (!bor_lua_stricmp(flag, "charging")) lua_pushinteger(L, ent->charging ? 1 : 0);
        else if (!bor_lua_stricmp(flag, "blocking")) lua_pushinteger(L, ent->blocking ? 1 : 0);
        else if (!bor_lua_stricmp(flag, "falling")) lua_pushinteger(L, ent->falling ? 1 : 0);
        else if (!bor_lua_stricmp(flag, "running")) lua_pushinteger(L, ent->running ? 1 : 0);
        else if (!bor_lua_stricmp(flag, "inpain")) lua_pushinteger(L, ent->inpain ? 1 : 0);
        else if (!bor_lua_stricmp(flag, "projectile")) lua_pushinteger(L, ent->projectile ? 1 : 0);
        else if (!bor_lua_stricmp(flag, "frozen")) lua_pushinteger(L, ent->frozen ? 1 : 0);
        else if (!bor_lua_stricmp(flag, "freezetime")) lua_pushinteger(L, ent->freezetime);
        else if (!bor_lua_stricmp(flag, "toexplode")) lua_pushinteger(L, ent->toexplode ? 1 : 0);
        else if (!bor_lua_stricmp(flag, "animating")) lua_pushinteger(L, ent->animating);
        else if (!bor_lua_stricmp(flag, "blink")) lua_pushinteger(L, ent->blink ? 1 : 0);
        else if (!bor_lua_stricmp(flag, "invincible")) lua_pushinteger(L, ent->invincible ? 1 : 0);
        else if (!bor_lua_stricmp(flag, "autokill")) lua_pushinteger(L, ent->autokill ? 1 : 0);
        else return luaL_error(L, "unknown aiflag '%s'", flag);
    }
    else if (!bor_lua_stricmp(prop, "animvalid")) {
        int anim = (int)(lua_gettop(L) >= 3 && !lua_isnil(L, 3) ? bor_lua_check_legacy_integer(L, 3, "getentityproperty") : 0);
        lua_pushinteger(L, validanim(ent, anim) ? 1 : 0);
    }
    else if (!bor_lua_stricmp(prop, "dropframe") || !bor_lua_stricmp(prop, "landframe")) {
        int anim = (int)(lua_gettop(L) >= 3 && !lua_isnil(L, 3) ? bor_lua_check_legacy_integer(L, 3, "getentityproperty") : 0);
        if (!validanim(ent, anim)) lua_pushnil(L);
        else if (!bor_lua_stricmp(prop, "dropframe")) lua_pushinteger(L, ent->modeldata.animation[anim]->dropframe);
        else lua_pushinteger(L, ent->modeldata.animation[anim]->landframe[0]);
    }
    else if (!bor_lua_stricmp(prop, "energycost")) {
        if (lua_gettop(L) != 4)
            return luaL_error(L, "getentityproperty energycost expects exactly 4 arguments");
        int anim = (int)bor_lua_check_legacy_integer(L, 3, "getentityproperty");
        int arg = (int)bor_lua_check_legacy_integer(L, 4, "getentityproperty");
        /* Each animation stores exactly three energy-cost values. */
        if (!validanim(ent, anim) || arg < 0 || arg >= 3)
            return luaL_error(L, "getentityproperty: invalid energycost animation or argument");
        lua_pushinteger(L, ent->modeldata.animation[anim]->energycost[arg]);
    }
    else if (!bor_lua_stricmp(prop, "komap")) {
        if (lua_gettop(L) < 3) { lua_pushnil(L); return 1; }
        int which = (int)bor_lua_check_legacy_integer(L, 3, "getentityproperty");
        if (which < 0 || which > 1) lua_pushnil(L);
        else lua_pushinteger(L, ent->modeldata.komap[which]);
    }
    else if (!bor_lua_stricmp(prop, "offense")) {
        int which = (int)(lua_gettop(L) >= 3 && !lua_isnil(L, 3) ? bor_lua_check_legacy_integer(L, 3, "getentityproperty") : 0);
        if (which < 0 || which >= MAX_ATKS)
            lua_pushnil(L);
        else lua_pushnumber(L, ent->modeldata.offense_factors[which]);
    }
    else if (!bor_lua_stricmp(prop, "defense")) {
        int which = (int)(lua_gettop(L) >= 3 && !lua_isnil(L, 3) ? bor_lua_check_legacy_integer(L, 3, "getentityproperty") : 0);
        int field_index = lua_gettop(L) >= 4 ?
            bor_lua_legacy_enum_index(L, 4, bor_lua_gep_defense, sizeof(bor_lua_gep_defense) / sizeof(bor_lua_gep_defense[0])) : 4;
        const char *field;
        if (field_index < 0)
            return luaL_error(L, "unknown defense property");
        field = bor_lua_gep_defense[field_index];
        if (which < 0 || which >= MAX_ATKS)
            lua_pushnil(L);
        else if (!bor_lua_stricmp(field, "factor")) lua_pushnumber(L, ent->modeldata.defense_factors[which]);
        else if (!bor_lua_stricmp(field, "blockpower")) lua_pushnumber(L, ent->modeldata.defense_blockpower[which]);
        else if (!bor_lua_stricmp(field, "blockratio")) lua_pushnumber(L, ent->modeldata.defense_blockratio[which]);
        else if (!bor_lua_stricmp(field, "blockthreshold")) lua_pushnumber(L, ent->modeldata.defense_blockthreshold[which]);
        else if (!bor_lua_stricmp(field, "blocktype")) lua_pushnumber(L, ent->modeldata.defense_blocktype[which]);
        else if (!bor_lua_stricmp(field, "knockdown")) lua_pushnumber(L, ent->modeldata.defense_knockdown[which]);
        else if (!bor_lua_stricmp(field, "pain")) lua_pushnumber(L, ent->modeldata.defense_pain[which]);
        else return luaL_error(L, "unknown defense property '%s'", field);
    }
    else if (!bor_lua_stricmp(prop, "flash")) {
        int which = (int)bor_lua_check_legacy_integer(L, 3, "getentityproperty");
        if (which == 1) lua_pushinteger(L, ent->modeldata.noatflash);
        else if (which == 2) lua_pushinteger(L, ent->modeldata.bflash);
        else lua_pushinteger(L, ent->modeldata.flash);
    }
    else if (!bor_lua_stricmp(prop, "icon")) {
        if (lua_gettop(L) < 3) {
            lua_pushnil(L);
        } else {
        int which = bor_lua_check_legacy_integer(L, 3, "getentityproperty icon");
        int sprite = ent->modeldata.icon;
        if (which == 1) sprite = ent->modeldata.icondie;
        else if (which == 2) sprite = ent->modeldata.iconget;
        else if (which == 3) sprite = ent->modeldata.iconmp[0];
        else if (which == 4) sprite = ent->modeldata.iconmp[1];
        else if (which == 5) sprite = ent->modeldata.iconmp[2];
        else if (which == 6) sprite = ent->modeldata.iconpain;
        else if (which == 7) sprite = ent->modeldata.iconw;
        else if (which == 8) sprite = ent->modeldata.iconx;
        else if (which == 9) sprite = ent->modeldata.icony;
        if (sprite < 0 || sprite >= sprite_map_max_items || !sprite_map || !sprite_map[sprite].sprite) {
            lua_pushinteger(L, -1);
        } else {
            sprite_map[sprite].sprite->centerx = sprite_map[sprite].centerx;
            sprite_map[sprite].sprite->centery = sprite_map[sprite].centery;
            lua_pushlightuserdata(L, sprite_map[sprite].sprite);
        }
        }
    }
    else if (!bor_lua_stricmp(prop, "sprite")) {
        int sprite;
        if (!ent->animation || ent->animpos < 0 || ent->animpos >= ent->animation->numframes) {
            lua_pushnil(L);
        } else {
            sprite = ent->animation->sprite[ent->animpos];
            if (sprite < 0 || sprite >= sprite_map_max_items || !sprite_map || !sprite_map[sprite].sprite) {
                lua_pushnil(L);
            } else {
                sprite_map[sprite].sprite->centerx = sprite_map[sprite].centerx;
                sprite_map[sprite].sprite->centery = sprite_map[sprite].centery;
                lua_pushlightuserdata(L, sprite_map[sprite].sprite);
            }
        }
    }
    else if (!bor_lua_stricmp(prop, "edelay")) {
        int field_index = bor_lua_legacy_enum_index(L, 3, bor_lua_gep_edelay, sizeof(bor_lua_gep_edelay) / sizeof(bor_lua_gep_edelay[0]));
        const char *field;
        if (field_index < 0)
            return luaL_error(L, "unknown edelay property");
        field = bor_lua_gep_edelay[field_index];
        if (!bor_lua_stricmp(field, "mode")) lua_pushinteger(L, ent->modeldata.edelay.mode);
        else if (!bor_lua_stricmp(field, "factor")) lua_pushnumber(L, ent->modeldata.edelay.factor);
        else if (!bor_lua_stricmp(field, "cap_min")) lua_pushinteger(L, ent->modeldata.edelay.cap_min);
        else if (!bor_lua_stricmp(field, "cap_max")) lua_pushinteger(L, ent->modeldata.edelay.cap_max);
        else if (!bor_lua_stricmp(field, "range_min")) lua_pushinteger(L, ent->modeldata.edelay.range_min);
        else if (!bor_lua_stricmp(field, "range_max")) lua_pushinteger(L, ent->modeldata.edelay.range_max);
        else return luaL_error(L, "unknown edelay property '%s'", field);
    }
    else if (!bor_lua_stricmp(prop, "running")) {
        if (lua_gettop(L) < 3) { lua_pushnil(L); return 1; }
        int field_index = bor_lua_legacy_enum_index(L, 3, bor_lua_gep_running, sizeof(bor_lua_gep_running) / sizeof(bor_lua_gep_running[0]));
        const char *field;
        if (field_index < 0)
            return luaL_error(L, "unknown running property");
        field = bor_lua_gep_running[field_index];
        if (!bor_lua_stricmp(field, "speed")) lua_pushnumber(L, ent->modeldata.runspeed);
        else if (!bor_lua_stricmp(field, "jumpy")) lua_pushnumber(L, ent->modeldata.runjumpheight);
        else if (!bor_lua_stricmp(field, "jumpx")) lua_pushnumber(L, ent->modeldata.runjumpdist);
        else if (!bor_lua_stricmp(field, "land")) lua_pushinteger(L, ent->modeldata.runhold);
        else if (!bor_lua_stricmp(field, "movez")) lua_pushinteger(L, ent->modeldata.runupdown);
        else return luaL_error(L, "unknown running property '%s'", field);
    }
    else if (!bor_lua_stricmp(prop, "bbox")) {
        if (lua_gettop(L) < 6) { lua_pushnil(L); return 1; }
        int anim = (int)bor_lua_check_legacy_integer(L, 4, "getentityproperty");
        int frame = (int)bor_lua_check_legacy_integer(L, 5, "getentityproperty");
        int coord = (int)bor_lua_check_legacy_integer(L, 6, "getentityproperty");
        (void)bor_lua_check_legacy_integer(L, 3, "getentityproperty"); /* legacy bbox index argument */
        if (!validanim(ent, anim) || frame < 0 ||
            frame >= ent->modeldata.animation[anim]->numframes || coord < 0 || coord >= 5 ||
            !ent->modeldata.animation[anim]->bbox_coords)
            lua_pushnil(L);
        else lua_pushinteger(L, ent->modeldata.animation[anim]->bbox_coords[frame][coord]);
    }
    else if (!bor_lua_stricmp(prop, "range")) {
        if (lua_gettop(L) < 4) { lua_pushnil(L); return 1; }
        int field_index = bor_lua_legacy_enum_index(L, 3, bor_lua_gep_range, sizeof(bor_lua_gep_range) / sizeof(bor_lua_gep_range[0]));
        const char *field;
        int anim = (int)bor_lua_check_legacy_integer(L, 4, "getentityproperty");
        int idx = -1;
        if (field_index < 0)
            return luaL_error(L, "unknown range property");
        field = bor_lua_gep_range[field_index];
        if (!validanim(ent, anim)) lua_pushnil(L);
        else {
            if (!bor_lua_stricmp(field, "xmin")) idx = 0;
            else if (!bor_lua_stricmp(field, "xmax")) idx = 1;
            else if (!bor_lua_stricmp(field, "zmin")) idx = 2;
            else if (!bor_lua_stricmp(field, "zmax")) idx = 3;
            else if (!bor_lua_stricmp(field, "amin")) idx = 4;
            else if (!bor_lua_stricmp(field, "amax")) idx = 5;
            else if (!bor_lua_stricmp(field, "bmin")) idx = 6;
            else if (!bor_lua_stricmp(field, "bmax")) idx = 7;
            if (idx < 0) return luaL_error(L, "unknown range property '%s'", field);
            lua_pushinteger(L, ent->modeldata.animation[anim]->range[idx]);
        }
    }
    else if (!bor_lua_stricmp(prop, "stats")) {
        if (lua_gettop(L) < 4) { lua_pushnil(L); return 1; }
        int type = (int)bor_lua_check_legacy_integer(L, 3, "getentityproperty");
        int idx = (int)bor_lua_check_legacy_integer(L, 4, "getentityproperty");
        if (idx < 0 || idx >= 20) lua_pushnil(L);
        else if (type == 1) lua_pushnumber(L, ent->stats[idx]);
        else lua_pushnumber(L, ent->modeldata.stats[idx]);
    }
    else if (!bor_lua_stricmp(prop, "staydown")) {
        if (lua_gettop(L) < 4) { lua_pushnil(L); return 1; }
        int which = 0;
        if (lua_isstring(L, 4)) {
            const char *field = lua_tostring(L, 4);
            if (!bor_lua_stricmp(field, "staydownatk")) which = 1;
            else if (bor_lua_stricmp(field, "staydown") != 0)
                return luaL_error(L, "unknown staydown property '%s'", field);
        } else {
            which = (int)(lua_gettop(L) >= 4 && !lua_isnil(L, 4) ? bor_lua_check_legacy_integer(L, 4, "getentityproperty") : 0);
        }
        if (which < 0 || which > 1) lua_pushnil(L);
        else lua_pushnumber(L, ent->staydown[which]);
    }
    else if (!bor_lua_stricmp(prop, "dot")) {
        if (lua_gettop(L) < 4) { lua_pushnil(L); return 1; }
        int which = (int)bor_lua_check_legacy_integer(L, 3, "getentityproperty");
        int field_index = bor_lua_legacy_enum_index(L, 4, bor_lua_gep_dot, sizeof(bor_lua_gep_dot) / sizeof(bor_lua_gep_dot[0]));
        const char *field;
        if (field_index < 0)
            return luaL_error(L, "unknown dot property");
        field = bor_lua_gep_dot[field_index];
        if (which < 0 || which >= MAX_DOTS) lua_pushnil(L);
        else if (!bor_lua_stricmp(field, "time")) lua_pushinteger(L, ent->dot_time[which]);
        else if (!bor_lua_stricmp(field, "mode")) lua_pushinteger(L, ent->dot[which]);
        else if (!bor_lua_stricmp(field, "force")) lua_pushinteger(L, ent->dot_force[which]);
        else if (!bor_lua_stricmp(field, "rate")) lua_pushinteger(L, ent->dot_rate[which]);
        else if (!bor_lua_stricmp(field, "type")) lua_pushinteger(L, ent->dot_atk[which]);
        else if (!bor_lua_stricmp(field, "owner")) {
            if (bor_lua_get_live_entity_ptr(ent->dot_owner[which]) && ent->dot_owner[which]->exists) bor_lua_push_entity(L, ent->dot_owner[which]);
            else lua_pushnil(L);
        }
        else return luaL_error(L, "unknown dot property '%s'", field);
    }
    else if (!bor_lua_stricmp(prop, "attack")) {
        if (lua_gettop(L) < 6) { lua_pushnil(L); return 1; }
        int field_index = bor_lua_legacy_enum_index(L, 3, bor_lua_gep_attack, sizeof(bor_lua_gep_attack) / sizeof(bor_lua_gep_attack[0]));
        const char *field;
        int anim = (int)bor_lua_check_legacy_integer(L, 5, "getentityproperty");
        if (field_index < 0)
            return luaL_error(L, "unknown attack property");
        field = bor_lua_gep_attack[field_index];
        int frame = (int)bor_lua_check_legacy_integer(L, 6, "getentityproperty");
        int arg = (int)(lua_gettop(L) >= 7 && !lua_isnil(L, 7) ? bor_lua_check_legacy_integer(L, 7, "getentityproperty") : 0);
        s_attack *attack;
        (void)bor_lua_check_legacy_integer(L, 4, "getentityproperty"); /* legacy attack-box index argument */
        if (!validanim(ent, anim) || frame < 0 ||
            frame >= ent->modeldata.animation[anim]->numframes ||
            !ent->modeldata.animation[anim]->attacks ||
            !ent->modeldata.animation[anim]->attacks[frame]) {
            lua_pushnil(L);
        } else {
            attack = ent->modeldata.animation[anim]->attacks[frame];
            if (!bor_lua_stricmp(field, "blockflash")) lua_pushinteger(L, attack->blockflash);
            else if (!bor_lua_stricmp(field, "blocksound")) lua_pushinteger(L, attack->blocksound);
            else if (!bor_lua_stricmp(field, "blast")) lua_pushinteger(L, attack->blast);
            else if (!bor_lua_stricmp(field, "coords")) {
                if (arg < 0 || arg >= 5) lua_pushnil(L); else lua_pushinteger(L, attack->attack_coords[arg]);
            }
            else if (!bor_lua_stricmp(field, "counterattack")) lua_pushinteger(L, attack->counterattack);
            else if (!bor_lua_stricmp(field, "direction")) lua_pushinteger(L, attack->force_direction);
            else if (!bor_lua_stricmp(field, "dol")) lua_pushinteger(L, attack->damage_on_landing);
            else if (!bor_lua_stricmp(field, "dot")) lua_pushinteger(L, attack->dot);
            else if (!bor_lua_stricmp(field, "dotforce")) lua_pushinteger(L, attack->dot_force);
            else if (!bor_lua_stricmp(field, "dotindex")) lua_pushinteger(L, attack->dot_index);
            else if (!bor_lua_stricmp(field, "dotrate")) lua_pushinteger(L, attack->dot_rate);
            else if (!bor_lua_stricmp(field, "dottime")) lua_pushinteger(L, attack->dot_time);
            else if (!bor_lua_stricmp(field, "drop")) lua_pushinteger(L, attack->attack_drop);
            else if (!bor_lua_stricmp(field, "dropv")) {
                if (arg < 0 || arg >= 3) lua_pushnil(L); else lua_pushnumber(L, attack->dropv[arg]);
            }
            else if (!bor_lua_stricmp(field, "force")) lua_pushinteger(L, attack->attack_force);
            else if (!bor_lua_stricmp(field, "forcemap")) lua_pushinteger(L, attack->forcemap);
            else if (!bor_lua_stricmp(field, "freeze")) lua_pushinteger(L, attack->freeze);
            else if (!bor_lua_stricmp(field, "freezetime")) lua_pushinteger(L, attack->freezetime);
            else if (!bor_lua_stricmp(field, "grab")) lua_pushinteger(L, attack->grab);
            else if (!bor_lua_stricmp(field, "grabdistance")) lua_pushinteger(L, attack->grab_distance);
            else if (!bor_lua_stricmp(field, "guardcost")) lua_pushinteger(L, attack->guardcost);
            else if (!bor_lua_stricmp(field, "hitflash")) lua_pushinteger(L, attack->hitflash);
            else if (!bor_lua_stricmp(field, "hitsound")) lua_pushinteger(L, attack->hitsound);
            else if (!bor_lua_stricmp(field, "jugglecost")) lua_pushinteger(L, attack->jugglecost);
            else if (!bor_lua_stricmp(field, "maptime")) lua_pushinteger(L, attack->maptime);
            else if (!bor_lua_stricmp(field, "noblock")) lua_pushinteger(L, attack->no_block);
            else if (!bor_lua_stricmp(field, "noflash")) lua_pushinteger(L, attack->no_flash);
            else if (!bor_lua_stricmp(field, "nopain")) lua_pushinteger(L, attack->no_pain);
            else if (!bor_lua_stricmp(field, "otg")) lua_pushinteger(L, attack->otg);
            else if (!bor_lua_stricmp(field, "pause")) lua_pushinteger(L, attack->pause_add);
            else if (!bor_lua_stricmp(field, "seal")) lua_pushinteger(L, attack->seal);
            else if (!bor_lua_stricmp(field, "sealtime")) lua_pushinteger(L, attack->sealtime);
            else if (!bor_lua_stricmp(field, "staydown")) lua_pushlightuserdata(L, attack->staydown);
            else if (!bor_lua_stricmp(field, "steal")) lua_pushinteger(L, attack->steal);
            else if (!bor_lua_stricmp(field, "type")) lua_pushinteger(L, attack->attack_type);
            else return luaL_error(L, "unknown attack property '%s'", field);
        }
    }
    else {
        return luaL_error(L, "unsupported legacy entity property '%s'", prop);
    }

    return 1;
}

// openbor.setEntityProperty(entity, property, value)
static int lua_legacy_entity_type_mask(lua_State *L, int index, int *is_ground)
{
    static const int entity_types[] = {0, TYPE_ENEMY, TYPE_NPC, TYPE_OBSTACLE, TYPE_PLAYER, TYPE_SHOT};

    *is_ground = 0;
    if (lua_isinteger(L, index)) {
        lua_Integer value = lua_tointeger(L, index);
        if (value < 0 || value >= (lua_Integer)(sizeof(entity_types) / sizeof(entity_types[0])))
            return luaL_error(L, "changeentityproperty: entity type index out of range");
        if (value == 0)
            *is_ground = 1;
        return entity_types[value];
    }

    if (lua_type(L, index) != LUA_TSTRING)
        return luaL_error(L, "changeentityproperty: entity type must be a name or integer constant");

    {
        const char *name = lua_tostring(L, index);
        if (!bor_lua_stricmp(name, "ground")) {
            *is_ground = 1;
            return 0;
        }
        /* These type_* names are the exact legacy mapstrings values. */
        if (!bor_lua_stricmp(name, "type_enemy") || !bor_lua_stricmp(name, "enemy")) return TYPE_ENEMY;
        if (!bor_lua_stricmp(name, "type_npc") || !bor_lua_stricmp(name, "npc")) return TYPE_NPC;
        if (!bor_lua_stricmp(name, "type_obstacle") || !bor_lua_stricmp(name, "obstacle")) return TYPE_OBSTACLE;
        if (!bor_lua_stricmp(name, "type_player") || !bor_lua_stricmp(name, "player")) return TYPE_PLAYER;
        if (!bor_lua_stricmp(name, "type_shot") || !bor_lua_stricmp(name, "shot")) return TYPE_SHOT;
        return luaL_error(L, "changeentityproperty: unknown entity type '%s'", name);
    }
}

static int lua_openbor_changeentityproperty(lua_State *L) {
    entity *ent;
    const char *prop;
    int property_index;
    lua_Integer ivalue;
    static const char *action_names[] = {"bomb_explode", "common_attack_proc", "common_block", "common_drop", "common_fall", "common_get", "common_grab", "common_grabattack", "common_grabbed", "common_jump", "common_land", "common_lie", "common_pain", "common_prejump", "common_rise", "common_spawn", "common_turn", "common_vault", "normal_prepare", "npc_warp", "player_blink", "suicide"};
    static const void *actions[] = {bomb_explode, common_attack_proc, common_block, common_drop, common_fall, common_get, common_grab, common_grabattack, common_grabbed, common_jump, common_land, common_lie, common_pain, common_prejump, common_rise, common_spawn, common_turn, common_vault, normal_prepare, npc_warp, player_blink, suicide};
    static const char *think_names[] = {"common_think", "player_think", "steam_think", "steamer_think", "text_think", "trap_think"};
    static const char *aiflag_names[] = {"animating", "attacking", "autokill", "blink", "blocking", "charging", "dead", "drop", "falling", "frozen", "getting", "idling", "inpain", "invincible", "jumping", "projectile", "running", "toexplode", "turning"};
    static const void *think_functions[] = {common_think, player_think, steam_think, steamer_think, text_think, trap_think};

    if (lua_gettop(L) < 3)
        return luaL_error(L, "changeentityproperty expects an entity, property, and value");
    property_index = bor_lua_legacy_enum_index(L, 2, bor_lua_changeentityproperty_properties,
        sizeof(bor_lua_changeentityproperty_properties) / sizeof(bor_lua_changeentityproperty_properties[0]));
    if (!lua_isnil(L, 1) && !lua_islightuserdata(L, 1) &&
        !luaL_testudata(L, 1, BOR_LUA_ENTITY_METATABLE))
        return luaL_error(L, "changeentityproperty expects an entity handle or nil");
    ent = bor_lua_get_live_entity(L, 1);
    if (!ent || !ent->exists) {
        lua_pushinteger(L, 0);
        return 1;
    }
    if (property_index < 0)
        return luaL_error(L, "changeentityproperty: unsupported property");
    prop = bor_lua_changeentityproperty_properties[property_index];

    if (!bor_lua_stricmp(prop, "type")) {
        if (!lua_isinteger(L, 3))
            return luaL_error(L, "changeentityproperty type expects an integer constant");
        ent->modeldata.type = (int)lua_tointeger(L, 3);
    } else if (!bor_lua_stricmp(prop, "candamage") || !bor_lua_stricmp(prop, "hostile") ||
               !bor_lua_stricmp(prop, "projectilehit")) {
        int i;
        int mask = 0;
        int ground = 0;
        for (i = 3; i <= lua_gettop(L); i++) {
            int is_ground = 0;
            mask |= lua_legacy_entity_type_mask(L, i, &is_ground);
            if (is_ground) ground = 1;
        }
        if (!bor_lua_stricmp(prop, "candamage")) {
            ent->modeldata.candamage = (short)mask;
            if (ground) ent->modeldata.ground = 1;
        } else if (!bor_lua_stricmp(prop, "hostile")) {
            ent->modeldata.hostile = (short)mask;
        } else {
            ent->modeldata.projectilehit = (short)mask;
        }
    } else if (!bor_lua_stricmp(prop, "health")) {
        ivalue = bor_lua_check_legacy_integer(L, 3, "changeentityproperty");
        if (ivalue > ent->modeldata.health) ivalue = ent->modeldata.health;
        if (ivalue < 0) ivalue = 0;
        ent->health = (int)ivalue;
    } else if (!bor_lua_stricmp(prop, "mp")) {
        ivalue = bor_lua_check_legacy_integer(L, 3, "changeentityproperty");
        if (ivalue > ent->modeldata.mp) ivalue = ent->modeldata.mp;
        if (ivalue < 0) ivalue = 0;
        ent->mp = (int)ivalue;
    } else if (!bor_lua_stricmp(prop, "x"))          ent->x = (float)luaL_checknumber(L, 3);
    else if (!bor_lua_stricmp(prop, "z"))             ent->z = (float)luaL_checknumber(L, 3);
    else if (!bor_lua_stricmp(prop, "a"))             ent->a = (float)luaL_checknumber(L, 3);
    else if (!bor_lua_stricmp(prop, "xdir"))          ent->xdir = (float)luaL_checknumber(L, 3);
    else if (!bor_lua_stricmp(prop, "zdir"))          ent->zdir = (float)luaL_checknumber(L, 3);
    else if (!bor_lua_stricmp(prop, "tossv"))         ent->tossv = (float)luaL_checknumber(L, 3);
    else if (!bor_lua_stricmp(prop, "base"))          ent->base = (float)luaL_checknumber(L, 3);
    else if (!bor_lua_stricmp(prop, "direction"))     ent->direction = (char)bor_lua_check_legacy_integer(L, 3, "changeentityproperty");
    else if (!bor_lua_stricmp(prop, "map"))           ent_set_colourmap(ent, (int)bor_lua_check_legacy_integer(L, 3, "changeentityproperty"));
    else if (!bor_lua_stricmp(prop, "nextanim"))      ent->nextanim = (unsigned int)bor_lua_check_legacy_integer(L, 3, "changeentityproperty");
    else if (!bor_lua_stricmp(prop, "nextthink"))     ent->nextthink = (unsigned int)bor_lua_check_legacy_integer(L, 3, "changeentityproperty");
    else if (!bor_lua_stricmp(prop, "autokill"))      ent->autokill = (char)bor_lua_check_legacy_integer(L, 3, "changeentityproperty");
    else if (!bor_lua_stricmp(prop, "invincible"))   ent->invincible = (char)bor_lua_check_legacy_integer(L, 3, "changeentityproperty");
    else if (!bor_lua_stricmp(prop, "frozen"))        ent->frozen = (char)bor_lua_check_legacy_integer(L, 3, "changeentityproperty");
    else if (!bor_lua_stricmp(prop, "seal"))          ent->seal = (char)bor_lua_check_legacy_integer(L, 3, "changeentityproperty");
    else if (!bor_lua_stricmp(prop, "position")) {
        ent->x = (float)luaL_checknumber(L, 3);
        if (!lua_isnoneornil(L, 4)) ent->z = (float)luaL_checknumber(L, 4);
        if (!lua_isnoneornil(L, 5)) ent->a = (float)luaL_checknumber(L, 5);
    } else if (!bor_lua_stricmp(prop, "velocity")) {
        ent->xdir = (float)luaL_checknumber(L, 3);
        if (!lua_isnoneornil(L, 4)) ent->zdir = (float)luaL_checknumber(L, 4);
        if (!lua_isnoneornil(L, 5)) ent->tossv = (float)luaL_checknumber(L, 5);
    } else if (!bor_lua_stricmp(prop, "animation")) {
        int anim = (int)bor_lua_check_legacy_integer(L, 3, "changeentityproperty");
        int reset = lua_isnoneornil(L, 4) ? 1 : (int)bor_lua_check_legacy_integer(L, 4, "changeentityproperty");
        ent_set_anim(ent, anim, reset);
    } else if (!bor_lua_stricmp(prop, "animpos")) {
        ent->animpos = (int)bor_lua_check_legacy_integer(L, 3, "changeentityproperty");
    } else if (!bor_lua_stricmp(prop, "maxhealth")) {
        ivalue = bor_lua_check_legacy_integer(L, 3, "changeentityproperty");
        ent->modeldata.health = (int)(ivalue < 0 ? 0 : ivalue);
        if (ent->health > ent->modeldata.health) ent->health = ent->modeldata.health;
    } else if (!bor_lua_stricmp(prop, "maxmp")) {
        ivalue = bor_lua_check_legacy_integer(L, 3, "changeentityproperty");
        ent->modeldata.mp = (int)(ivalue < 0 ? 0 : ivalue);
        if (ent->mp > ent->modeldata.mp) ent->mp = ent->modeldata.mp;
    } else if (!bor_lua_stricmp(prop, "speed")) {
        ent->modeldata.speed = (float)luaL_checknumber(L, 3);
    } else if (!bor_lua_stricmp(prop, "weapon")) {
        int weapon = (int)bor_lua_check_legacy_integer(L, 3, "changeentityproperty");
        int anim_flag = lua_isnoneornil(L, 4) ? 0 : (int)bor_lua_check_legacy_integer(L, 4, "changeentityproperty");
        set_weapon(ent, weapon, anim_flag);
    } else if (!bor_lua_stricmp(prop, "trymove")) {
        int mode = (int)bor_lua_check_legacy_integer(L, 3, "changeentityproperty");
        if (mode == 1) ent->trymove = common_trymove;
        else if (mode == 2) ent->trymove = player_trymove;
        else ent->trymove = NULL;
    } else if (!bor_lua_stricmp(prop, "combostep")) {
        int index = (int)bor_lua_check_legacy_integer(L, 3, "changeentityproperty");
        if (index < 0 || index >= MAX_SPECIAL_INPUTS) return luaL_error(L, "combostep index out of range");
        ent->combostep[index] = (unsigned char)bor_lua_check_legacy_integer(L, 4, "changeentityproperty");
    } else if (!bor_lua_stricmp(prop, "mpset")) {
        ent->modeldata.mp = (int)luaL_checknumber(L, 3);
        if (!lua_isnoneornil(L, 4)) ent->modeldata.mpstable = (int)luaL_checknumber(L, 4);
        if (!lua_isnoneornil(L, 5)) ent->modeldata.mpstableval = (int)luaL_checknumber(L, 5);
        if (!lua_isnoneornil(L, 6)) ent->modeldata.mprate = (int)luaL_checknumber(L, 6);
        if (!lua_isnoneornil(L, 7)) ent->modeldata.mpdroprate = (int)luaL_checknumber(L, 7);
        if (!lua_isnoneornil(L, 8)) ent->modeldata.chargerate = (int)luaL_checknumber(L, 8);
    } else if (!bor_lua_stricmp(prop, "grabforce")) {
        ent->model->grabforce = (int)bor_lua_check_legacy_integer(L, 3, "changeentityproperty");
    } else if (!bor_lua_stricmp(prop, "iconposition")) {
        ent->modeldata.iconx = (short)bor_lua_check_legacy_integer(L, 3, "changeentityproperty");
        if (!lua_isnoneornil(L, 4)) ent->modeldata.icony = (short)bor_lua_check_legacy_integer(L, 4, "changeentityproperty");
    } else if (!bor_lua_stricmp(prop, "lifeposition")) {
        ent->modeldata.hpx = (short)bor_lua_check_legacy_integer(L, 3, "changeentityproperty");
        if (!lua_isnoneornil(L, 4)) ent->modeldata.hpy = (short)bor_lua_check_legacy_integer(L, 4, "changeentityproperty");
    } else if (!bor_lua_stricmp(prop, "nameposition")) {
        ent->modeldata.namex = (short)bor_lua_check_legacy_integer(L, 3, "changeentityproperty");
        if (!lua_isnoneornil(L, 4)) ent->modeldata.namey = (short)bor_lua_check_legacy_integer(L, 4, "changeentityproperty");
    } else if (!bor_lua_stricmp(prop, "setlayer")) {
        ent->modeldata.setlayer = (int)bor_lua_check_legacy_integer(L, 3, "changeentityproperty");
    } else if (!bor_lua_stricmp(prop, "throwdamage")) {
        ent->modeldata.throwdamage = (int)bor_lua_check_legacy_integer(L, 3, "changeentityproperty");
    } else if (!bor_lua_stricmp(prop, "throwdist")) {
        ent->modeldata.throwdist = (float)luaL_checknumber(L, 3);
    } else if (!bor_lua_stricmp(prop, "throwframewait")) {
        ent->modeldata.throwframewait = (int)bor_lua_check_legacy_integer(L, 3, "changeentityproperty");
    } else if (!bor_lua_stricmp(prop, "throwheight")) {
        ent->modeldata.throwheight = (float)luaL_checknumber(L, 3);
    } else if (!bor_lua_stricmp(prop, "alpha")) {
        ent->modeldata.alpha = (int)bor_lua_check_legacy_integer(L, 3, "changeentityproperty");
    } else if (!bor_lua_stricmp(prop, "guardpoints")) {
        ent->modeldata.guardpoints[0] = (int)bor_lua_check_legacy_integer(L, 3, "changeentityproperty");
    } else if (!bor_lua_stricmp(prop, "maxguardpoints")) {
        ent->modeldata.guardpoints[1] = (int)bor_lua_check_legacy_integer(L, 3, "changeentityproperty");
    } else if (!bor_lua_stricmp(prop, "jugglepoints")) {
        ent->modeldata.jugglepoints[0] = (int)bor_lua_check_legacy_integer(L, 3, "changeentityproperty");
    } else if (!bor_lua_stricmp(prop, "maxjugglepoints")) {
        ent->modeldata.jugglepoints[1] = (int)bor_lua_check_legacy_integer(L, 3, "changeentityproperty");
    } else if (!bor_lua_stricmp(prop, "komap")) {
        ent->modeldata.komap[0] = (char)bor_lua_check_legacy_integer(L, 3, "changeentityproperty");
        if (!lua_isnoneornil(L, 4)) ent->modeldata.komap[1] = (char)bor_lua_check_legacy_integer(L, 4, "changeentityproperty");
    } else if (!bor_lua_stricmp(prop, "rush_count")) {
        ent->rush[0] = (int)bor_lua_check_legacy_integer(L, 3, "changeentityproperty");
    } else if (!bor_lua_stricmp(prop, "rush_tally")) {
        ent->rush[1] = (int)bor_lua_check_legacy_integer(L, 3, "changeentityproperty");
    } else if (!bor_lua_stricmp(prop, "rush_time")) {
        ent->rushtime = (int)bor_lua_check_legacy_integer(L, 3, "changeentityproperty");
    } else if (!bor_lua_stricmp(prop, "offense")) {
        int which = (int)bor_lua_check_legacy_integer(L, 3, "changeentityproperty");
        if (which < 0 || which >= MAX_ATKS) return luaL_error(L, "offense index out of range");
        ent->modeldata.offense_factors[which] = (float)luaL_checknumber(L, 4);
    } else if (!bor_lua_stricmp(prop, "defense")) {
        int which = (int)bor_lua_check_legacy_integer(L, 3, "changeentityproperty");
        if (which < 0 || which >= MAX_ATKS) return luaL_error(L, "defense index out of range");
        ent->modeldata.defense_factors[which] = (float)luaL_checknumber(L, 4);
        if (!lua_isnoneornil(L, 5)) ent->modeldata.defense_pain[which] = (float)luaL_checknumber(L, 5);
        if (!lua_isnoneornil(L, 6)) ent->modeldata.defense_knockdown[which] = (float)luaL_checknumber(L, 6);
        if (!lua_isnoneornil(L, 7)) ent->modeldata.defense_blockpower[which] = (float)luaL_checknumber(L, 7);
        if (!lua_isnoneornil(L, 8)) ent->modeldata.defense_blockthreshold[which] = (float)luaL_checknumber(L, 8);
        if (!lua_isnoneornil(L, 9)) ent->modeldata.defense_blockratio[which] = (float)luaL_checknumber(L, 9);
        if (!lua_isnoneornil(L, 10)) ent->modeldata.defense_blocktype[which] = (float)luaL_checknumber(L, 10);
    } else if (!bor_lua_stricmp(prop, "stats")) {
        int which;
        int index = (int)bor_lua_check_legacy_integer(L, 4, "changeentityproperty");
        if (!lua_isinteger(L, 3))
            return luaL_error(L, "changeentityproperty stats expects an integer stat table selector");
        which = (int)lua_tointeger(L, 3);
        if (index < 0 || index >= 20) return luaL_error(L, "stats index out of range");
        if (which == 1) ent->stats[index] = (float)luaL_checknumber(L, 5);
        else ent->modeldata.stats[index] = (float)luaL_checknumber(L, 5);
    } else if (!bor_lua_stricmp(prop, "antigravity")) {
        float value = (float)luaL_checknumber(L, 3);
        ent->modeldata.antigravity = value;
        ent->antigravity = value;
    } else if (!bor_lua_stricmp(prop, "maptime")) {
        ent->maptime = (int)bor_lua_check_legacy_integer(L, 3, "changeentityproperty");
    } else if (!bor_lua_stricmp(prop, "tosstime")) {
        ent->toss_time = (int)bor_lua_check_legacy_integer(L, 3, "changeentityproperty");
    } else if (!bor_lua_stricmp(prop, "invinctime")) {
        ent->invinctime = (int)bor_lua_check_legacy_integer(L, 3, "changeentityproperty");
    } else if (!bor_lua_stricmp(prop, "lifespancountdown")) {
        ent->lifespancountdown = (float)luaL_checknumber(L, 3);
    } else if (!bor_lua_stricmp(prop, "freezetime")) {
        ent->freezetime = (int)bor_lua_check_legacy_integer(L, 3, "changeentityproperty");
    } else if (!bor_lua_stricmp(prop, "bounce")) {
        ent->modeldata.bounce = (int)bor_lua_check_legacy_integer(L, 3, "changeentityproperty");
    } else if (!bor_lua_stricmp(prop, "antigrab")) {
        ent->model->antigrab = (int)bor_lua_check_legacy_integer(L, 3, "changeentityproperty");
    } else if (!bor_lua_stricmp(prop, "nodrop")) {
        ent->modeldata.nodrop = (int)bor_lua_check_legacy_integer(L, 3, "changeentityproperty");
    } else if (!bor_lua_stricmp(prop, "nopain")) {
        ent->modeldata.nopain = (int)bor_lua_check_legacy_integer(L, 3, "changeentityproperty");
    } else if (!bor_lua_stricmp(prop, "no_adjust_base")) {
        ent->modeldata.no_adjust_base = (int)bor_lua_check_legacy_integer(L, 3, "changeentityproperty");
    } else if (!bor_lua_stricmp(prop, "noaicontrol")) {
        ent->noaicontrol = (int)bor_lua_check_legacy_integer(L, 3, "changeentityproperty");
    } else if (!bor_lua_stricmp(prop, "nodieblink")) {
        ent->modeldata.nodieblink = (int)bor_lua_check_legacy_integer(L, 3, "changeentityproperty");
    } else if (!bor_lua_stricmp(prop, "falldie")) {
        ent->modeldata.falldie = (int)bor_lua_check_legacy_integer(L, 3, "changeentityproperty");
    } else if (!bor_lua_stricmp(prop, "aiflag")) {
        const char *flag = NULL;
        int value;
        size_t n;
        if (lua_type(L, 3) == LUA_TSTRING) {
            flag = lua_tostring(L, 3);
            for (n = 0; n < sizeof(aiflag_names) / sizeof(aiflag_names[0]); ++n) {
                if (!bor_lua_stricmp(flag, aiflag_names[n]))
                    break;
            }
            if (n == sizeof(aiflag_names) / sizeof(aiflag_names[0]))
                return luaL_error(L, "changeentityproperty: unknown aiflag '%s'", flag);
        } else if (lua_isinteger(L, 3)) {
            lua_Integer flag_index = lua_tointeger(L, 3);
            if (flag_index >= 0 && flag_index < (lua_Integer)(sizeof(aiflag_names) / sizeof(aiflag_names[0])))
                flag = aiflag_names[flag_index];
        } else {
            return luaL_error(L, "changeentityproperty aiflag expects a flag name or integer constant");
        }

        if (flag && lua_gettop(L) >= 4 && lua_type(L, 4) == LUA_TNUMBER) {
            value = (int)lua_tonumber(L, 4);
            if (!bor_lua_stricmp(flag, "dead")) ent->dead = value;
            else if (!bor_lua_stricmp(flag, "jumping")) ent->jumping = value;
            else if (!bor_lua_stricmp(flag, "idling")) ent->idling = value;
            else if (!bor_lua_stricmp(flag, "drop")) ent->drop = value;
            else if (!bor_lua_stricmp(flag, "attacking")) ent->attacking = value;
            else if (!bor_lua_stricmp(flag, "getting")) ent->getting = value;
            else if (!bor_lua_stricmp(flag, "turning")) ent->turning = value;
            else if (!bor_lua_stricmp(flag, "charging")) ent->charging = value;
            else if (!bor_lua_stricmp(flag, "blocking")) ent->blocking = value;
            else if (!bor_lua_stricmp(flag, "falling")) ent->falling = value;
            else if (!bor_lua_stricmp(flag, "running")) ent->running = value;
            else if (!bor_lua_stricmp(flag, "inpain")) ent->inpain = value;
            else if (!bor_lua_stricmp(flag, "projectile")) ent->projectile = value;
            else if (!bor_lua_stricmp(flag, "frozen")) ent->frozen = value;
            else if (!bor_lua_stricmp(flag, "toexplode")) ent->toexplode = value;
            else if (!bor_lua_stricmp(flag, "animating")) ent->animating = value;
            else if (!bor_lua_stricmp(flag, "blink")) ent->blink = value;
            else if (!bor_lua_stricmp(flag, "invincible")) ent->invincible = value;
            else if (!bor_lua_stricmp(flag, "autokill")) ent->autokill = value;
        }
    } else if (!bor_lua_stricmp(prop, "dead"))            ent->dead = (int)bor_lua_check_legacy_integer(L, 3, "changeentityproperty");
    else if (!bor_lua_stricmp(prop, "blink"))             ent->blink = (int)bor_lua_check_legacy_integer(L, 3, "changeentityproperty");
    else if (!bor_lua_stricmp(prop, "attacking"))         ent->attacking = (int)bor_lua_check_legacy_integer(L, 3, "changeentityproperty");
    else if (!bor_lua_stricmp(prop, "projectile"))        ent->projectile = (int)bor_lua_check_legacy_integer(L, 3, "changeentityproperty");
    else if (!bor_lua_stricmp(prop, "damage_on_landing")) ent->damage_on_landing = (int)bor_lua_check_legacy_integer(L, 3, "changeentityproperty");
    else if (!bor_lua_stricmp(prop, "stalltime"))         ent->stalltime = (int)bor_lua_check_legacy_integer(L, 3, "changeentityproperty");
    else if (!bor_lua_stricmp(prop, "staydown")) {
        ent->staydown[0] = (int)bor_lua_check_legacy_integer(L, 3, "changeentityproperty");
        if (!lua_isnoneornil(L, 4)) ent->staydown[1] = (int)bor_lua_check_legacy_integer(L, 4, "changeentityproperty");
    } else if (!bor_lua_stricmp(prop, "stealth")) {
        ent->modeldata.stealth[0] = (short)bor_lua_check_legacy_integer(L, 3, "changeentityproperty");
        if (!lua_isnoneornil(L, 4)) ent->modeldata.stealth[1] = (short)bor_lua_check_legacy_integer(L, 4, "changeentityproperty");
    }
    else if (!bor_lua_stricmp(prop, "aggression")) ent->modeldata.aggression = (int)bor_lua_check_legacy_integer(L, 3, "changeentityproperty");
    else if (!bor_lua_stricmp(prop, "aiattack")) ent->modeldata.aiattack = (int)bor_lua_check_legacy_integer(L, 3, "changeentityproperty");
    else if (!bor_lua_stricmp(prop, "aimove")) ent->modeldata.aimove = (int)bor_lua_check_legacy_integer(L, 3, "changeentityproperty");
    else if (!bor_lua_stricmp(prop, "animhits")) ent->animation->animhits = (int)bor_lua_check_legacy_integer(L, 3, "changeentityproperty");
    else if (!bor_lua_stricmp(prop, "attackid")) ent->attack_id = (int)bor_lua_check_legacy_integer(L, 3, "changeentityproperty");
    else if (!bor_lua_stricmp(prop, "blockback")) ent->modeldata.blockback = (int)bor_lua_check_legacy_integer(L, 3, "changeentityproperty");
    else if (!bor_lua_stricmp(prop, "blockpain")) ent->modeldata.blockpain = (int)bor_lua_check_legacy_integer(L, 3, "changeentityproperty");
    else if (!bor_lua_stricmp(prop, "detect")) ent->modeldata.stealth[1] = (int)bor_lua_check_legacy_integer(L, 3, "changeentityproperty");
    else if (!bor_lua_stricmp(prop, "hitbyid")) ent->hit_by_attack_id = (int)bor_lua_check_legacy_integer(L, 3, "changeentityproperty");
    else if (!bor_lua_stricmp(prop, "hmapl")) ent->modeldata.hmap1 = (int)bor_lua_check_legacy_integer(L, 3, "changeentityproperty");
    else if (!bor_lua_stricmp(prop, "hmapu")) ent->modeldata.hmap2 = (int)bor_lua_check_legacy_integer(L, 3, "changeentityproperty");
    else if (!bor_lua_stricmp(prop, "nograb")) ent->nograb = (int)bor_lua_check_legacy_integer(L, 3, "changeentityproperty");
    else if (!bor_lua_stricmp(prop, "scroll")) ent->modeldata.scroll = (float)luaL_checknumber(L, 3);
    else if (!bor_lua_stricmp(prop, "sealtime")) ent->sealtime = (int)bor_lua_check_legacy_integer(L, 3, "changeentityproperty");
    else if (!bor_lua_stricmp(prop, "subject_to_screen")) ent->modeldata.subject_to_screen = (int)bor_lua_check_legacy_integer(L, 3, "changeentityproperty");
    else if (!bor_lua_stricmp(prop, "subject_to_maxz")) ent->modeldata.subject_to_maxz = (int)bor_lua_check_legacy_integer(L, 3, "changeentityproperty");
    else if (!bor_lua_stricmp(prop, "subject_to_minz")) ent->modeldata.subject_to_minz = (int)bor_lua_check_legacy_integer(L, 3, "changeentityproperty");
    else if (!bor_lua_stricmp(prop, "subject_to_wall")) ent->modeldata.subject_to_wall = (int)bor_lua_check_legacy_integer(L, 3, "changeentityproperty");
    else if (!bor_lua_stricmp(prop, "subject_to_obstacle")) ent->modeldata.subject_to_obstacle = (int)bor_lua_check_legacy_integer(L, 3, "changeentityproperty");
    else if (!bor_lua_stricmp(prop, "subject_to_platform")) ent->modeldata.subject_to_platform = (int)bor_lua_check_legacy_integer(L, 3, "changeentityproperty");
    else if (!bor_lua_stricmp(prop, "subject_to_gravity")) ent->modeldata.subject_to_gravity = (int)bor_lua_check_legacy_integer(L, 3, "changeentityproperty");
    else if (!bor_lua_stricmp(prop, "subject_to_hole")) ent->modeldata.subject_to_hole = (int)bor_lua_check_legacy_integer(L, 3, "changeentityproperty");
    else if (!bor_lua_stricmp(prop, "owner") || !bor_lua_stricmp(prop, "parent") || !bor_lua_stricmp(prop, "opponent")) {
        entity *other = lua_isnil(L, 3) ? NULL : bor_lua_get_live_entity(L, 3);
        if (!lua_isnil(L, 3) && (!other || !other->exists))
            return luaL_error(L, "changeentityproperty '%s' expects a live entity or nil", prop);
        if (!bor_lua_stricmp(prop, "owner")) ent->owner = other;
        else if (!bor_lua_stricmp(prop, "parent")) ent->parent = other;
        else ent->opponent = other;
    } else if (!bor_lua_stricmp(prop, "subentity")) {
        entity *other = lua_isnil(L, 3) ? NULL : bor_lua_get_live_entity(L, 3);
        if (!lua_isnil(L, 3) && (!other || !other->exists))
            return luaL_error(L, "changeentityproperty 'subentity' expects a live entity or nil");
        if (ent->subentity) ent->subentity->parent = NULL;
        ent->subentity = other;
        if (other) other->parent = ent;
    } else if (!bor_lua_stricmp(prop, "name")) {
        const char *name = luaL_checkstring(L, 3);
        strncpy(ent->name, name, sizeof(ent->name) - 1);
        ent->name[sizeof(ent->name) - 1] = '\0';
    } else if (!bor_lua_stricmp(prop, "defaultname")) {
        s_model *model = findmodel((char *)luaL_checkstring(L, 3));
        if (!model) return luaL_error(L, "changeentityproperty defaultname requires an existing model");
        ent->defaultmodel = model;
    } else if (!bor_lua_stricmp(prop, "model")) {
        const char *model_name = luaL_checkstring(L, 3);
        int unload = lua_isnoneornil(L, 4) ? 0 : bor_lua_check_legacy_integer(L, 4, "changeentityproperty model");
        set_model_ex(ent, (char *)model_name, -1, NULL, unload);
        if (!ent->weapent) ent->weapent = ent;
    } else if (!bor_lua_stricmp(prop, "colourmap")) {
        int map_index = bor_lua_check_legacy_integer(L, 3, "changeentityproperty colourmap");
        if (map_index < 1 || map_index > ent->modeldata.maps_loaded)
            return luaL_error(L, "changeentityproperty colourmap index out of range");
        ent->colourmap = ent->modeldata.colourmap[map_index - 1];
    } else if (!bor_lua_stricmp(prop, "dot")) {
        int index = bor_lua_check_legacy_integer(L, 3, "changeentityproperty dot");
        if (index < 0 || index >= MAX_DOTS) return luaL_error(L, "dot index out of range");
        ent->dot_time[index] = (int)luaL_checknumber(L, 4);
        if (!lua_isnoneornil(L, 5)) ent->dot[index] = (int)luaL_checknumber(L, 5);
        if (!lua_isnoneornil(L, 6)) ent->dot_force[index] = (int)luaL_checknumber(L, 6);
        if (!lua_isnoneornil(L, 7)) ent->dot_rate[index] = (int)luaL_checknumber(L, 7);
        if (!lua_isnoneornil(L, 8)) ent->dot_atk[index] = (int)luaL_checknumber(L, 8);
        if (!lua_isnoneornil(L, 9)) {
            entity *owner = lua_isnil(L, 9) ? NULL : bor_lua_get_live_entity(L, 9);
            if (!lua_isnil(L, 9) && (!owner || !owner->exists))
                return luaL_error(L, "dot owner must be a live entity or nil");
            ent->dot_owner[index] = owner;
        }
    } else if (!bor_lua_stricmp(prop, "edelay")) {
        ent->modeldata.edelay.mode = bor_lua_check_legacy_integer(L, 3, "changeentityproperty edelay");
        if (!lua_isnoneornil(L, 4)) ent->modeldata.edelay.factor = (float)luaL_checknumber(L, 4);
        if (!lua_isnoneornil(L, 5)) ent->modeldata.edelay.cap_min = bor_lua_check_legacy_integer(L, 5, "changeentityproperty edelay");
        if (!lua_isnoneornil(L, 6)) ent->modeldata.edelay.cap_max = bor_lua_check_legacy_integer(L, 6, "changeentityproperty edelay");
        if (!lua_isnoneornil(L, 7)) ent->modeldata.edelay.range_min = bor_lua_check_legacy_integer(L, 7, "changeentityproperty edelay");
        if (!lua_isnoneornil(L, 8)) ent->modeldata.edelay.range_max = bor_lua_check_legacy_integer(L, 8, "changeentityproperty edelay");
    } else if (!bor_lua_stricmp(prop, "energycost")) {
        int animation = bor_lua_check_legacy_integer(L, 3, "changeentityproperty energycost");
        if (lua_gettop(L) != 6 || !validanim(ent, animation))
            return luaL_error(L, "energycost expects entity, property, animation, cost, mponly, and disable");
        ent->modeldata.animation[animation]->energycost[0] = bor_lua_check_legacy_integer(L, 4, "changeentityproperty energycost");
        ent->modeldata.animation[animation]->energycost[1] = bor_lua_check_legacy_integer(L, 5, "changeentityproperty energycost");
        ent->modeldata.animation[animation]->energycost[2] = bor_lua_check_legacy_integer(L, 6, "changeentityproperty energycost");
    } else if (!bor_lua_stricmp(prop, "running")) {
        ent->modeldata.runspeed = (float)luaL_checknumber(L, 3);
        if (!lua_isnoneornil(L, 4)) ent->modeldata.runjumpheight = (float)luaL_checknumber(L, 4);
        if (!lua_isnoneornil(L, 5)) ent->modeldata.runjumpdist = (float)luaL_checknumber(L, 5);
        if (!lua_isnoneornil(L, 6)) ent->modeldata.runhold = (int)luaL_checknumber(L, 6);
        if (!lua_isnoneornil(L, 7)) ent->modeldata.runupdown = (int)luaL_checknumber(L, 7);
    } else if (!bor_lua_stricmp(prop, "takeaction")) {
        size_t n;
        if (lua_type(L, 3) == LUA_TSTRING) {
            const char *action = lua_tostring(L, 3);
            ent->takeaction = NULL;
            for (n = 0; n < sizeof(action_names) / sizeof(action_names[0]); ++n) {
                if (!bor_lua_stricmp(action, action_names[n])) {
                    ent->takeaction = (void *)actions[n];
                    break;
                }
            }
        } else if (lua_isinteger(L, 3)) {
            lua_Integer action = lua_tointeger(L, 3);
            if (action >= 0 && action < (lua_Integer)(sizeof(actions) / sizeof(actions[0])))
                ent->takeaction = (void *)actions[action];
        } else {
            return luaL_error(L, "changeentityproperty takeaction expects an action name or integer constant");
        }
    } else if (!bor_lua_stricmp(prop, "think")) {
        size_t n;
        if (lua_type(L, 3) == LUA_TSTRING) {
            const char *think_name = lua_tostring(L, 3);
            for (n = 0; n < sizeof(think_names) / sizeof(think_names[0]); ++n) {
                if (!bor_lua_stricmp(think_name, think_names[n])) {
                    ent->think = (void *)think_functions[n];
                    break;
                }
            }
        } else if (lua_isinteger(L, 3)) {
            lua_Integer think = lua_tointeger(L, 3);
            if (think >= 0 && think < (lua_Integer)(sizeof(think_functions) / sizeof(think_functions[0])))
                ent->think = (void *)think_functions[think];
        } else {
            return luaL_error(L, "changeentityproperty think expects a think name or integer constant");
        }
    } else if (!bor_lua_stricmp(prop, "gfxshadow"))         ent->modeldata.gfxshadow = (int)bor_lua_check_legacy_integer(L, 3, "changeentityproperty");
    else {
        return luaL_error(L, "changeentityproperty: property '%s' is not implemented yet", prop);
    }

    lua_pushinteger(L, bor_lua_changeentityproperty_legacy_result(prop));
    return 1;
}

/* Entity userdata metatable: property reads reuse the legacy getter, and
 * writes reuse the supported setter so both interfaces stay consistent. */
static int lua_openbor_entity_index(lua_State *L)
{
    if (lua_type(L, 2) != LUA_TSTRING) {
        lua_pushnil(L);
        return 1;
    }
    return lua_openbor_getentityproperty(L);
}

static int lua_openbor_entity_newindex(lua_State *L)
{
    if (lua_type(L, 2) != LUA_TSTRING)
        return luaL_error(L, "entity property name must be a string");

    lua_openbor_changeentityproperty(L);
    lua_pop(L, 1);
    return 0;
}

// openbor.isEntityValid(entity)
// Lightweight validity check for the light-userdata entity handle.
static int lua_openbor_isentityvalid(lua_State *L) {
    entity *ent = bor_lua_get_live_entity(L, 1);
    lua_pushboolean(L, ent && ent->exists);
    return 1;
}

// openbor.isEntityAlive(entity)
static int lua_openbor_isentityalive(lua_State *L) {
    entity *ent = bor_lua_get_live_entity(L, 1);
    lua_pushboolean(L, ent && ent->exists && !ent->dead);
    return 1;
}

// openbor.getEntityHealth(entity)
static int lua_openbor_getentityhealth(lua_State *L) {
    entity *ent = bor_lua_get_live_entity(L, 1);
    if (!ent || !ent->exists) {
        lua_pushnil(L);
        return 1;
    }
    lua_pushinteger(L, (lua_Integer)ent->health);
    return 1;
}

// openbor.getEntityTarget(entity)
// OpenBOR's built-in target relationship is represented by entity->opponent.
static int lua_openbor_getentitytarget(lua_State *L) {
    entity *ent = bor_lua_get_live_entity(L, 1);
    if (!ent || !ent->exists || !bor_lua_get_live_entity_ptr(ent->opponent) || !ent->opponent->exists) {
        lua_pushnil(L);
        return 1;
    }
    bor_lua_push_entity(L, ent->opponent);
    return 1;
}

// openbor.setEntityTarget(entity, target)
static int lua_openbor_setentitytarget(lua_State *L) {
    entity *ent = bor_lua_get_live_entity(L, 1);
    entity *target = NULL;

    if (!ent || !ent->exists) {
        lua_pushboolean(L, 0);
        return 1;
    }

    if (!lua_isnoneornil(L, 2)) {
        target = bor_lua_get_live_entity(L, 2);
        if (!target || !target->exists) {
            return luaL_error(L, "invalid target entity");
        }
    }

    ent->opponent = target;
    lua_pushboolean(L, 1);
    return 1;
}

// openbor.getEntityPosition(entity) -> x, z, a
// OpenBOR uses x/z for ground position and a for altitude.
static int lua_openbor_getentityposition(lua_State *L) {
    entity *ent = bor_lua_get_live_entity(L, 1);
    if (!ent || !ent->exists) {
        return luaL_error(L, "invalid entity");
    }

    lua_pushnumber(L, (lua_Number)ent->x);
    lua_pushnumber(L, (lua_Number)ent->z);
    lua_pushnumber(L, (lua_Number)ent->a);
    return 3;
}

// openbor.setEntityPosition(entity, x, z, a)
static int lua_openbor_setentityposition(lua_State *L) {
    entity *ent = bor_lua_get_live_entity(L, 1);
    if (!ent || !ent->exists) {
        lua_pushboolean(L, 0);
        return 1;
    }

    ent->x = (float)luaL_checknumber(L, 2);
    ent->z = (float)luaL_checknumber(L, 3);
    ent->a = (float)luaL_checknumber(L, 4);

    lua_pushboolean(L, 1);
    return 1;
}

// openbor.changeAnimation(entity, animation, resetable)
static int lua_openbor_changeanimation(lua_State *L) {
    entity *ent = bor_lua_get_live_entity(L, 1);
    int animation = (int)luaL_checkinteger(L, 2);
    int resetable = (int)luaL_optinteger(L, 3, 1);

    if (!ent || !ent->exists) {
        lua_pushboolean(L, 0);
        return 1;
    }

    if (animation < 0 || animation >= dyn_anim_custom_maxvalues.max_animations ||
        !validanim(ent, animation)) {
        return luaL_error(L, "invalid animation %d for entity", animation);
    }

    ent_set_anim(ent, animation, resetable);
    lua_pushboolean(L, 1);
    return 1;
}

// openbor.spawn(model, x, z, a, direction)
// The public Lua API uses a model name; x/z/a match OpenBOR's native spawn().
static int lua_openbor_spawn(lua_State *L)
{
    entity *ent;

    /*
     * Legacy spawn() takes no arguments. It creates an entity from the
     * current spawnentry configured by setspawnentry().
     */
    (void)L;
    if (!level)
        return luaL_error(L, "spawn called without an active level");

    ent = smartspawn(&bor_lua_spawnentry);
    bor_lua_push_entity(L, ent);
    return 1;
}

// openbor.get_self()
static int lua_openbor_get_self(lua_State *L) {
    if (self && self->exists) {
        bor_lua_push_entity(L, self);
    } else {
        lua_pushnil(L);
    }
    return 1;
}

/*
 * Callback-scoped legacy local variables.
 * These values are installed by the engine immediately around a callback.
 */
#define BOR_LUA_LOCAL_VARS "OpenBOR.LocalVars"
#define BOR_LUA_SCRIPT_LOCAL_VARS "OpenBOR.ScriptLocalVars"
static int bor_lua_push_script_scope_table(lua_State *L, const char *registry_key);

void bor_lua_clear_localvars(lua_State *L)
{
    if (!L) return;
    lua_pushnil(L);
    lua_setfield(L, LUA_REGISTRYINDEX, BOR_LUA_LOCAL_VARS);
}

void bor_lua_set_localvar(lua_State *L, const char *name, int index)
{
    int abs_index;

    if (!L || !name) return;
    abs_index = (index > 0 || index <= LUA_REGISTRYINDEX)
        ? index : lua_gettop(L) + index + 1;

    lua_getfield(L, LUA_REGISTRYINDEX, BOR_LUA_LOCAL_VARS);
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_setfield(L, LUA_REGISTRYINDEX, BOR_LUA_LOCAL_VARS);
    }

    lua_pushvalue(L, abs_index);
    lua_setfield(L, -2, name);
    lua_pop(L, 1);
}

/*
 * Legacy getlocalvar() compatibility. Callback values are checked first,
 * followed by persistent locals owned by the current script instance.
 */
static int lua_openbor_getlocalvar(lua_State *L) {
    const char *name;
    lua_Debug frame;
    if (lua_gettop(L) != 1 || lua_type(L, 1) != LUA_TSTRING) return luaL_error(L, "getlocalvar expects exactly one string name");
    name = lua_tostring(L, 1);

    lua_getfield(L, LUA_REGISTRYINDEX, BOR_LUA_LOCAL_VARS);
    if (lua_istable(L, -1)) {
        lua_getfield(L, -1, name);
        if (!lua_isnil(L, -1)) {
            lua_remove(L, -2);
            return 1;
        }
        lua_pop(L, 1);
    }
    lua_pop(L, 1);

    /* Persistent local variables belong to the current script instance. */
    if (lua_getstack(L, 1, &frame)) {
        bor_lua_push_script_scope_table(L, BOR_LUA_SCRIPT_LOCAL_VARS);
        lua_pushstring(L, name);
        lua_rawget(L, -2);
        lua_remove(L, -2);
        if (!lua_isnil(L, -1))
            return 1;
        lua_pop(L, 1);
    }

    lua_pushnil(L);
    return 1;
}

#define BOR_LUA_GLOBAL_VARS "OpenBOR.GlobalVars"
#define BOR_LUA_INDEXED_VARS "OpenBOR.IndexedVars"
#define BOR_LUA_MAX_GLOBAL_VARS 2048

static int bor_lua_count_global_and_local_vars(lua_State *L)
{
    int count = 0;
    int table_index;

    lua_getfield(L, LUA_REGISTRYINDEX, BOR_LUA_GLOBAL_VARS);
    if (lua_istable(L, -1)) {
        table_index = lua_gettop(L);
        lua_pushnil(L);
        while (lua_next(L, table_index) != 0) {
            ++count;
            lua_pop(L, 1);
        }
    }
    lua_pop(L, 1);

    /*
     * The legacy global-variable index is a shared pool of global names and
     * all current local names. This includes the engine-installed callback
     * values (self, damage, player, etc.) while a script callback is running.
     */
    lua_getfield(L, LUA_REGISTRYINDEX, BOR_LUA_LOCAL_VARS);
    if (lua_istable(L, -1)) {
        table_index = lua_gettop(L);
        lua_pushnil(L);
        while (lua_next(L, table_index) != 0) {
            ++count;
            lua_pop(L, 1);
        }
    }
    lua_pop(L, 1);

    lua_getfield(L, LUA_REGISTRYINDEX, BOR_LUA_SCRIPT_LOCAL_VARS);
    if (lua_istable(L, -1)) {
        int entity_index = lua_gettop(L);
        lua_pushnil(L);
        while (lua_next(L, entity_index) != 0) {
            if (lua_istable(L, -1)) {
                int source_index = lua_gettop(L);
                lua_pushnil(L);
                while (lua_next(L, source_index) != 0) {
                    if (lua_istable(L, -1)) {
                        int vars_index = lua_gettop(L);
                        lua_pushnil(L);
                        while (lua_next(L, vars_index) != 0) {
                            ++count;
                            lua_pop(L, 1);
                        }
                    }
                    lua_pop(L, 1);
                }
            }
            lua_pop(L, 1);
        }
    }
    lua_pop(L, 1);
    return count;
}

static int lua_openbor_getindexedvar(lua_State *L)
{
    lua_Integer index;

    if (lua_gettop(L) < 1 || max_indexed_vars <= 0)
        return luaL_error(L, "getindexedvar requires an index and configured indexed-variable storage");

    index = bor_lua_check_legacy_integer(L, 1, "getindexedvar");
    if (index < 0 || index >= max_indexed_vars) {
        lua_pushnil(L);
        return 1;
    }

    lua_getfield(L, LUA_REGISTRYINDEX, BOR_LUA_INDEXED_VARS);
    lua_pushinteger(L, index);
    lua_rawget(L, -2);
    lua_remove(L, -2);
    return 1;
}

static int lua_openbor_setindexedvar(lua_State *L)
{
    lua_Integer index;

    if (lua_gettop(L) < 2 || max_indexed_vars <= 0)
        return luaL_error(L, "setindexedvar requires an index, value, and configured indexed-variable storage");

    index = bor_lua_check_legacy_integer(L, 1, "setindexedvar");
    if (index < 0 || index >= max_indexed_vars) {
        lua_pushinteger(L, 0);
        return 1;
    }

    lua_getfield(L, LUA_REGISTRYINDEX, BOR_LUA_INDEXED_VARS);
    lua_pushinteger(L, index);
    lua_pushvalue(L, 2);
    lua_rawset(L, -3);
    lua_pop(L, 1);

    lua_pushinteger(L, 1);
    return 1;
}

static int lua_openbor_clearindexedvar(lua_State *L)
{
    lua_newtable(L);
    lua_setfield(L, LUA_REGISTRYINDEX, BOR_LUA_INDEXED_VARS);
    return 0;
}

#define BOR_LUA_ENTITY_VARS "OpenBOR.EntityVars"
#define BOR_LUA_SCRIPT_VARS "OpenBOR.ScriptVars"

static int bor_lua_push_script_scope_table(lua_State *L, const char *registry_key)
{
    lua_Debug frame;

    if (!lua_getstack(L, 1, &frame) || !lua_getinfo(L, "S", &frame) || !frame.source)
        return luaL_error(L, "script variable access requires a Lua script caller");

    lua_getfield(L, LUA_REGISTRYINDEX, registry_key);
    if (!lua_istable(L, -1))
        return luaL_error(L, "script variable storage is not initialized");

    /*
     * Legacy Script state belongs to the executing script instance. In Lua,
     * the entity plus calling source identifies that instance: two entities
     * running one source keep independent local and indexed script variables.
     */
    lua_pushlightuserdata(L, self);
    lua_rawget(L, -2);
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushlightuserdata(L, self);
        lua_pushvalue(L, -2);
        lua_rawset(L, -4);
    }

    lua_getfield(L, -1, frame.source);
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_setfield(L, -3, frame.source);
    }

    lua_remove(L, -2);
    lua_remove(L, -2);
    return 1;
}

static int bor_lua_push_script_var_table(lua_State *L)
{
    return bor_lua_push_script_scope_table(L, BOR_LUA_SCRIPT_VARS);
}

static int lua_openbor_getscriptvar(lua_State *L)
{
    lua_Integer index;

    if (lua_gettop(L) < 1 || max_script_vars <= 0)
        return luaL_error(L, "getscriptvar requires an index and configured script-variable storage");

    index = bor_lua_check_legacy_integer(L, 1, "getscriptvar");
    if (index < 0 || index >= max_script_vars) {
        lua_pushnil(L);
        return 1;
    }

    bor_lua_push_script_var_table(L);
    lua_pushinteger(L, index);
    lua_rawget(L, -2);
    lua_remove(L, -2);
    return 1;
}

static int lua_openbor_setscriptvar(lua_State *L)
{
    lua_Integer index;

    if (lua_gettop(L) < 2 || max_script_vars <= 0)
        return luaL_error(L, "setscriptvar requires an index, value, and configured script-variable storage");

    index = bor_lua_check_legacy_integer(L, 1, "setscriptvar");
    if (index < 0 || index >= max_script_vars) {
        lua_pushinteger(L, 0);
        return 1;
    }

    bor_lua_push_script_var_table(L);
    lua_pushinteger(L, index);
    lua_pushvalue(L, 2);
    lua_rawset(L, -3);
    lua_pop(L, 1);
    lua_pushinteger(L, 1);
    return 1;
}

static int lua_openbor_getentityvar(lua_State *L)
{
    entity *ent;
    lua_Integer index;

    if (lua_gettop(L) < 2)
        return luaL_error(L, "getentityvar expects an entity and index");
    if (max_entity_vars <= 0)
        return luaL_error(L, "getentityvar requires configured entity-variable storage");
    ent = bor_lua_get_live_entity(L, 1);
    if (!lua_isnil(L, 1) && !ent)
        return luaL_error(L, "getentityvar expects a valid entity handle or nil");

    if (!ent) {
        lua_pushnil(L);
        return 1;
    }

    lua_getfield(L, LUA_REGISTRYINDEX, BOR_LUA_ENTITY_VARS);
    lua_pushlightuserdata(L, ent);
    lua_rawget(L, -2);
    if (!lua_istable(L, -1)) {
        lua_pop(L, 2);
        lua_pushnil(L);
        return 1;
    }

    /* Legacy returns an empty value for entities without an allocated var list
       before validating the requested index. */
    index = bor_lua_check_legacy_integer(L, 2, "getentityvar");
    if (index < 0 || index >= max_entity_vars) {
        lua_pop(L, 2);
        lua_pushnil(L);
        return 1;
    }

    lua_pushinteger(L, index);
    lua_rawget(L, -2);
    lua_remove(L, -2);
    lua_remove(L, -2);
    return 1;
}

static int lua_openbor_setentityvar(lua_State *L)
{
    entity *ent;
    lua_Integer index;

    if (lua_gettop(L) < 3)
        return luaL_error(L, "setentityvar expects an entity, index, and value");
    if (max_entity_vars <= 0)
        return luaL_error(L, "setentityvar requires configured entity-variable storage");
    ent = bor_lua_get_live_entity(L, 1);
    if (!lua_isnil(L, 1) && !ent)
        return luaL_error(L, "setentityvar expects a valid entity handle or nil");
    if (!ent) {
        lua_pushinteger(L, 0);
        return 1;
    }

    index = bor_lua_check_legacy_integer(L, 2, "setentityvar");
    if (index < 0 || index >= max_entity_vars) {
        lua_pushinteger(L, 0);
        return 1;
    }

    lua_getfield(L, LUA_REGISTRYINDEX, BOR_LUA_ENTITY_VARS);
    lua_pushlightuserdata(L, ent);
    lua_rawget(L, -2);
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushlightuserdata(L, ent);
        lua_pushvalue(L, -2);
        lua_rawset(L, -4);
    }

    lua_pushinteger(L, index);
    lua_pushvalue(L, 3);
    lua_rawset(L, -3);
    lua_pop(L, 2);
    lua_pushinteger(L, 1);
    return 1;
}

/*
 * Release per-entity Lua storage when an engine entity is destroyed or its
 * slot is recycled. Script vars and entity vars are separate legacy stores,
 * but both are owned by the entity lifetime.
 */
void bor_lua_clear_entity_vars(struct entity *ent)
{
    lua_State *L = g_lua_engine_state;

    if (!L || !ent)
        return;

    lua_getfield(L, LUA_REGISTRYINDEX, BOR_LUA_ENTITY_VARS);
    if (lua_istable(L, -1)) {
        lua_pushlightuserdata(L, ent);
        lua_pushnil(L);
        lua_rawset(L, -3);
    }
    lua_pop(L, 1);

    lua_getfield(L, LUA_REGISTRYINDEX, BOR_LUA_SCRIPT_VARS);
    if (lua_istable(L, -1)) {
        lua_pushlightuserdata(L, ent);
        lua_pushnil(L);
        lua_rawset(L, -3);
    }
    lua_pop(L, 1);

    lua_getfield(L, LUA_REGISTRYINDEX, BOR_LUA_SCRIPT_LOCAL_VARS);
    if (lua_istable(L, -1)) {
        lua_pushlightuserdata(L, ent);
        lua_pushnil(L);
        lua_rawset(L, -3);
    }
    lua_pop(L, 1);
}

void bor_lua_clear_all_entity_vars(void)
{
    lua_State *L = g_lua_engine_state;

    if (!L)
        return;

    lua_newtable(L);
    lua_setfield(L, LUA_REGISTRYINDEX, BOR_LUA_ENTITY_VARS);

    lua_newtable(L);
    lua_setfield(L, LUA_REGISTRYINDEX, BOR_LUA_SCRIPT_VARS);

    lua_newtable(L);
    lua_setfield(L, LUA_REGISTRYINDEX, BOR_LUA_SCRIPT_LOCAL_VARS);
}

/* Legacy utility and variable functions retained from the original script API. */
static int lua_openbor_isempty(lua_State *L)
{
    if (lua_gettop(L) != 1)
        return luaL_error(L, "isempty expects exactly 1 argument");

    lua_pushinteger(L, lua_isnil(L, 1) ? 1 : 0);
    return 1;
}

static int lua_openbor_null(lua_State *L)
{
    (void)L;
    lua_pushnil(L);
    return 1;
}

static int lua_openbor_maxglobalvarindex(lua_State *L)
{
    lua_pushinteger(L, bor_lua_count_global_and_local_vars(L) - 1);
    return 1;
}

static int lua_openbor_setlocalvar(lua_State *L)
{
    const char *name;

    if (lua_gettop(L) < 2 || lua_type(L, 1) != LUA_TSTRING)
        return luaL_error(L, "setlocalvar expects a string name and value");
    name = lua_tostring(L, 1);

    /*
     * Engine callback arguments are installed as current-script locals.
     * Updating one of those names must replace the active callback value,
     * rather than create a second persistent variable with the same name.
     */
    lua_getfield(L, LUA_REGISTRYINDEX, BOR_LUA_LOCAL_VARS);
    if (lua_istable(L, -1)) {
        lua_pushstring(L, name);
        lua_rawget(L, -2);
        if (!lua_isnil(L, -1)) {
            lua_pop(L, 1);
            lua_pushstring(L, name);
            lua_pushvalue(L, 2);
            lua_rawset(L, -3);
            lua_pop(L, 1);
            lua_pushinteger(L, 1);
            return 1;
        }
        lua_pop(L, 1);
    }
    lua_pop(L, 1);

    if (!lua_isnil(L, 2)) {
        bor_lua_push_script_scope_table(L, BOR_LUA_SCRIPT_LOCAL_VARS);
        lua_pushstring(L, name);
        lua_rawget(L, -2);
        if (lua_isnil(L, -1) &&
            bor_lua_count_global_and_local_vars(L) >= BOR_LUA_MAX_GLOBAL_VARS) {
            lua_pop(L, 2);
            lua_pushinteger(L, 0);
            return 1;
        }
        lua_pop(L, 1);
        lua_pushstring(L, name);
        lua_pushvalue(L, 2);
        lua_rawset(L, -3);
        lua_pop(L, 1);
    } else {
        bor_lua_push_script_scope_table(L, BOR_LUA_SCRIPT_LOCAL_VARS);
        lua_pushstring(L, name);
        lua_pushnil(L);
        lua_rawset(L, -3);
        lua_pop(L, 1);
    }

    lua_pushinteger(L, 1);
    return 1;
}

static void bor_lua_clear_table(lua_State *L, int index)
{
    index = lua_absindex(L, index);
    lua_pushnil(L);
    while (lua_next(L, index) != 0) {
        lua_pop(L, 1);
        lua_pushvalue(L, -1);
        lua_pushnil(L);
        lua_rawset(L, index);
    }
}

static int lua_openbor_clearlocalvar(lua_State *L)
{
    /* Clear named variables owned by this script instance. */
    bor_lua_push_script_scope_table(L, BOR_LUA_SCRIPT_LOCAL_VARS);
    bor_lua_clear_table(L, -1);
    lua_pop(L, 1);

    /* Script_Local_Clear() also clears this Script instance's indexed slots. */
    bor_lua_push_script_var_table(L);
    bor_lua_clear_table(L, -1);
    lua_pop(L, 1);

    /* Callback context is the Lua bridge's representation of built-in locals. */
    bor_lua_clear_localvars(L);
    return 0;
}

static int lua_openbor_clearglobalvar(lua_State *L)
{
    lua_newtable(L);
    lua_setfield(L, LUA_REGISTRYINDEX, BOR_LUA_GLOBAL_VARS);
    /* Legacy resets the shared global/local name pool, not scriptvar slots. */
    lua_newtable(L);
    lua_setfield(L, LUA_REGISTRYINDEX, BOR_LUA_SCRIPT_LOCAL_VARS);
    bor_lua_clear_localvars(L);
    return 0;
}

/*
 * Legacy global-variable API.
 * Values are kept in a Lua registry table so entity userdata and other Lua
 * values retain their identity when stored and retrieved across callbacks.
 */

static int lua_openbor_globalvar(lua_State *L)
{
    const char *name;
    if (lua_gettop(L) != 1 || lua_type(L, 1) != LUA_TSTRING)
        return luaL_error(L, "globalvar expects exactly one string name");
    name = lua_tostring(L, 1);
    lua_getfield(L, LUA_REGISTRYINDEX, BOR_LUA_GLOBAL_VARS);
    lua_pushstring(L, name);
    lua_rawget(L, -2);
    if (lua_isnil(L, -1)) {
        if (bor_lua_count_global_and_local_vars(L) >= BOR_LUA_MAX_GLOBAL_VARS) {
            lua_pop(L, 2);
            return 0;
        }
        lua_pop(L, 1);
        lua_pushstring(L, name);
        lua_pushinteger(L, 0);
        lua_rawset(L, -3);
    } else {
        lua_pop(L, 1);
    }
    lua_pop(L, 1);
    return 0;
}

static int lua_openbor_getglobalvar(lua_State *L)
{
    const char *name;
    if (lua_gettop(L) != 1 || lua_type(L, 1) != LUA_TSTRING) return luaL_error(L, "getglobalvar expects exactly one string name");
    name = lua_tostring(L, 1);
    lua_getfield(L, LUA_REGISTRYINDEX, BOR_LUA_GLOBAL_VARS);
    lua_pushstring(L, name);
    lua_rawget(L, -2);
    lua_remove(L, -2);
    return 1;
}

static int lua_openbor_setglobalvar(lua_State *L)
{
    const char *name;
    if (lua_gettop(L) != 2 || lua_type(L, 1) != LUA_TSTRING) return luaL_error(L, "setglobalvar expects a string name and value");
    name = lua_tostring(L, 1);
    luaL_checkany(L, 2);
    lua_getfield(L, LUA_REGISTRYINDEX, BOR_LUA_GLOBAL_VARS);
    lua_pushstring(L, name);
    lua_rawget(L, -2);
    if (lua_isnil(L, -1) && !lua_isnil(L, 2) &&
        bor_lua_count_global_and_local_vars(L) >= BOR_LUA_MAX_GLOBAL_VARS) {
        lua_pop(L, 2);
        lua_pushinteger(L, 0);
        return 1;
    }
    lua_pop(L, 1);
    lua_pushstring(L, name);
    lua_pushvalue(L, 2);
    lua_rawset(L, -3);
    lua_pop(L, 1);
    lua_pushinteger(L, 1);
    return 1;
}

/* File-stream helpers retain the original level-scoped stream handles. */
static s_filestream *bor_lua_get_filestream(lua_State *L, int arg)
{
    lua_Integer index;

    if (lua_type(L, arg) != LUA_TNUMBER)
        luaL_error(L, "file stream index must be numeric");
    index = (lua_Integer)lua_tonumber(L, arg);

    if (!level)
        luaL_error(L, "file stream operation requires a loaded level");
    if (index < 0 || index >= level->numfilestreams ||
        index >= LEVEL_MAX_FILESTREAMS || !level->filestreams[index].buf)
        luaL_error(L, "invalid file stream index");

    return &level->filestreams[index];
}

static int lua_openbor_createfilestream(lua_State *L)
{
    s_filestream *stream;
    char *buffer;

    if (!level)
        return luaL_error(L, "createfilestream requires a loaded level");
    if (level->numfilestreams >= LEVEL_MAX_FILESTREAMS)
        return luaL_error(L, "maximum file streams exceeded");

    buffer = (char *)malloc(1);
    if (!buffer)
        return luaL_error(L, "createfilestream ran out of memory");
    buffer[0] = '\0';

    stream = &level->filestreams[level->numfilestreams];
    stream->buf = buffer;
    stream->size = 0;
    stream->pos = 0;

    lua_pushinteger(L, level->numfilestreams);
    ++level->numfilestreams;
    return 1;
}

static int lua_openbor_savefilestream(lua_State *L)
{
    s_filestream *stream;
    const char *filename;
    char path[512] = {0};
    char modname[256] = {0};
    const char *base;
    size_t pack_length;
    size_t modname_length;
    size_t i;
    FILE *handle;

    if (lua_gettop(L) < 2 || lua_type(L, 2) != LUA_TSTRING)
        return luaL_error(L, "savefilestream expects a stream index and filename");
    stream = bor_lua_get_filestream(L, 1);
    filename = lua_tostring(L, 2);

    getBasePath(path, "Saves/", 0);

    pack_length = strlen(packfile);
    if (pack_length > 4)
        pack_length -= 4;
    else
        pack_length = 0;

    base = packfile;
    for (i = 0; i < pack_length; ++i) {
        if (packfile[i] == '/' || packfile[i] == '\\')
            base = packfile + i + 1;
    }
    if (base >= packfile + pack_length)
        base = packfile;
    modname_length = (size_t)((packfile + pack_length) - base);
    if (modname_length >= sizeof(modname))
        modname_length = sizeof(modname) - 1;

    if (strlen(path) + modname_length + strlen(filename) + 3 >= sizeof(path))
        return luaL_error(L, "savefilestream path is too long");
    memcpy(modname, base, modname_length);
    modname[modname_length] = '\0';
    strncat(path, modname, sizeof(path) - strlen(path) - 1);
#ifndef DC
    dirExists(path, 1);
#endif
    strncat(path, "/", sizeof(path) - strlen(path) - 1);
    strncat(path, filename, sizeof(path) - strlen(path) - 1);

    handle = fopen(path, "wb");
    if (!handle)
        return luaL_error(L, "savefilestream could not open '%s'", path);
    if (stream->size && fwrite(stream->buf, 1, stream->size, handle) != stream->size) {
        fclose(handle);
        return luaL_error(L, "savefilestream could not write '%s'", path);
    }
    if (fwrite("\r\n", 1, 2, handle) != 2) {
        fclose(handle);
        return luaL_error(L, "savefilestream could not finish '%s'", path);
    }
    fclose(handle);
    return 0;
}

static int lua_openbor_openfilestream(lua_State *L)
{
    const char *filename;
    int location = 0;
    s_filestream *stream;
    char path[512] = {0};
    char *buffer = NULL;
    size_t size = 0;

    if (lua_gettop(L) < 1 || lua_type(L, 1) != LUA_TSTRING)
        return luaL_error(L, "openfilestream expects a filename string");
    filename = lua_tostring(L, 1);
    if (lua_gettop(L) >= 2) {
        if (lua_type(L, 2) != LUA_TNUMBER)
            return luaL_error(L, "openfilestream location must be numeric");
        location = (int)lua_tonumber(L, 2);
    }

    if (!level)
        return luaL_error(L, "openfilestream requires a loaded level");
    if (level->numfilestreams >= LEVEL_MAX_FILESTREAMS)
        return luaL_error(L, "maximum file streams exceeded");

    if (location) {
        FILE *handle;
        long file_size;

        getBasePath(path, "Saves", 0);
        if (strlen(path) + strlen(filename) + 2 >= sizeof(path))
            return luaL_error(L, "file stream path is too long");
        strcat(path, "/");
        strcat(path, filename);

        handle = fopen(path, "rb");
        if (!handle)
            return luaL_error(L, "openfilestream could not open '%s'", path);
        if (fseek(handle, 0, SEEK_END) != 0 ||
            (file_size = ftell(handle)) < 0 ||
            fseek(handle, 0, SEEK_SET) != 0) {
            fclose(handle);
            return luaL_error(L, "openfilestream could not read '%s'", path);
        }

        size = (size_t)file_size;
        buffer = (char *)malloc(size + 1);
        if (!buffer) {
            fclose(handle);
            return luaL_error(L, "openfilestream ran out of memory");
        }
        if (size && fread(buffer, 1, size, handle) != size) {
            free(buffer);
            fclose(handle);
            return luaL_error(L, "openfilestream could not read '%s'", path);
        }
        fclose(handle);
        buffer[size] = '\0';
    } else {
        if (buffer_pakfile((char *)filename, &buffer, &size) != 1 || !buffer)
            return luaL_error(L, "invalid filename for openfilestream: '%s'", filename);
        if (size == 0)
            size = strlen(buffer);
    }

    stream = &level->filestreams[level->numfilestreams];
    stream->buf = buffer;
    stream->size = size;
    stream->pos = 0;
    lua_pushinteger(L, level->numfilestreams);
    ++level->numfilestreams;
    return 1;
}

static int lua_openbor_getfilestreamline(lua_State *L)
{
    s_filestream *stream = bor_lua_get_filestream(L, 1);
    size_t end;

    if (stream->pos < 0 || (size_t)stream->pos > stream->size)
        return luaL_error(L, "file stream position is out of range");
    end = (size_t)stream->pos;

    while (end < stream->size && stream->buf[end] != '\0' &&
           stream->buf[end] != '\n' && stream->buf[end] != '\r')
        ++end;

    {
        size_t length = end - (size_t)stream->pos;
        if (length > MAX_STR_VAR_LEN)
            length = MAX_STR_VAR_LEN;
        lua_pushlstring(L, stream->buf + stream->pos, length);
    }
    return 1;
}

static int lua_openbor_getfilestreamargument(lua_State *L)
{
    s_filestream *stream = bor_lua_get_filestream(L, 1);
    lua_Integer argument;
    const char *type;
    size_t end = (size_t)stream->pos;
    char *line;
    char *value;

    if (lua_gettop(L) < 3 || lua_type(L, 2) != LUA_TNUMBER ||
        lua_type(L, 3) != LUA_TSTRING)
        return luaL_error(L, "getfilestreamargument expects stream, numeric argument index, and type string");
    argument = (lua_Integer)lua_tonumber(L, 2);
    type = lua_tostring(L, 3);

    if (stream->pos < 0 || (size_t)stream->pos > stream->size)
        return luaL_error(L, "file stream position is out of range");
    if (argument < 0)
        return luaL_error(L, "file stream argument index cannot be negative");

    while (end < stream->size && stream->buf[end] != '\0' &&
           stream->buf[end] != '\n' && stream->buf[end] != '\r')
        ++end;
    line = (char *)malloc(end - (size_t)stream->pos + 1);
    if (!line)
        return luaL_error(L, "getfilestreamargument ran out of memory");
    memcpy(line, stream->buf + stream->pos, end - (size_t)stream->pos);
    line[end - (size_t)stream->pos] = '\0';

    value = findarg(line, (int)argument);
    if (bor_lua_stricmp(type, "string") == 0) {
        size_t length = value ? strlen(value) : 0;
        if (length > MAX_STR_VAR_LEN)
            length = MAX_STR_VAR_LEN;
        lua_pushlstring(L, value ? value : "", length);
    }
    else if (bor_lua_stricmp(type, "int") == 0)
        lua_pushinteger(L, value ? (lua_Integer)atoi(value) : 0);
    else if (bor_lua_stricmp(type, "float") == 0)
        lua_pushnumber(L, value ? (lua_Number)atof(value) : 0);
    else {
        free(line);
        return luaL_error(L, "getfilestreamargument type must be string, int, or float");
    }

    free(line);
    return 1;
}

static int lua_openbor_filestreamnextline(lua_State *L)
{
    s_filestream *stream = bor_lua_get_filestream(L, 1);
    size_t pos;

    if (stream->pos < 0 || (size_t)stream->pos > stream->size)
        return luaL_error(L, "file stream position is out of range");

    pos = (size_t)stream->pos;
    while (pos < stream->size && stream->buf[pos] != '\0' &&
           stream->buf[pos] != '\n' && stream->buf[pos] != '\r')
        ++pos;
    while (pos < stream->size && (stream->buf[pos] == '\n' ||
           stream->buf[pos] == '\r'))
        ++pos;

    stream->pos = (int)pos;
    return 0;
}

static int lua_openbor_getfilestreamposition(lua_State *L)
{
    s_filestream *stream = bor_lua_get_filestream(L, 1);
    lua_pushinteger(L, stream->pos);
    return 1;
}

static int lua_openbor_setfilestreamposition(lua_State *L)
{
    s_filestream *stream = bor_lua_get_filestream(L, 1);
    lua_Integer position;

    if (lua_type(L, 2) != LUA_TNUMBER)
        return luaL_error(L, "file stream position must be numeric");
    position = (lua_Integer)lua_tonumber(L, 2);
    if (position < 0 || (lua_Unsigned)position > stream->size)
        return luaL_error(L, "file stream position is out of range");
    stream->pos = (int)position;
    return 0;
}

static int lua_openbor_filestreamappend(lua_State *L)
{
    s_filestream *stream = bor_lua_get_filestream(L, 1);
    char append[MAX_STR_VAR_LEN];
    size_t append_length;
    size_t extra;
    char *new_buffer;
    int append_type;

    if (lua_gettop(L) < 3)
        return luaL_error(L, "filestreamappend expects stream, value, and append type");

    if (lua_type(L, 2) == LUA_TSTRING) {
        const char *value = lua_tostring(L, 2);
        strncpy(append, value, sizeof(append) - 1);
        append[sizeof(append) - 1] = '\0';
    } else if (lua_type(L, 2) == LUA_TNUMBER) {
        if (lua_isinteger(L, 2))
            snprintf(append, sizeof(append), "%d", (int)lua_tointeger(L, 2));
        else
            snprintf(append, sizeof(append), "%f", (double)lua_tonumber(L, 2));
    } else {
        return luaL_error(L, "filestreamappend value must be a string or number");
    }

    if (lua_type(L, 3) != LUA_TNUMBER)
        return luaL_error(L, "filestreamappend type must be numeric");
    append_type = (int)lua_tonumber(L, 3);
    /* Legacy treats unknown append modes as a successful no-op. */
    if (append_type != 0 && append_type != 1)
        return 0;

    append_length = strlen(append);
    extra = append_length + (append_type == 0 ? 3 : 1);
    new_buffer = (char *)realloc(stream->buf, stream->size + extra + 1);
    if (!new_buffer)
        return luaL_error(L, "filestreamappend ran out of memory");
    stream->buf = new_buffer;

    if (append_type == 0) {
        stream->buf[stream->size++] = '\r';
        stream->buf[stream->size++] = '\n';
    }
    memcpy(stream->buf + stream->size, append, append_length);
    stream->size += append_length;
    stream->buf[stream->size++] = ' ';
    stream->buf[stream->size] = '\0';
    return 0;
}

static int lua_openbor_loadsample(lua_State *L)
{
    const char *filename;
    int log_errors = 0;
    int sample;
    if (lua_gettop(L) < 1 || lua_type(L, 1) != LUA_TSTRING)
        return luaL_error(L, "loadsample expects a filename string");
    filename = lua_tostring(L, 1);
    if (lua_gettop(L) >= 2) {
        if (!lua_isinteger(L, 2))
            return luaL_error(L, "loadsample log flag must be an integer");
        log_errors = (int)lua_tointeger(L, 2);
    }
    sample = sound_load_sample((char *)filename, packfile, log_errors);
    lua_pushinteger(L, sample);
    return 1;
}

static int lua_openbor_free(lua_State *L)
{
    bor_lua_screen_handle *screen;
    bor_lua_sprite_handle *sprite;

    if (lua_gettop(L) < 1)
        return luaL_error(L, "free expects exactly 1 allocated screen or sprite handle");

    screen = (bor_lua_screen_handle *)luaL_testudata(L, 1, BOR_LUA_SCREEN_METATABLE);
    if (screen && screen->screen) {
        bor_lua_release_resource(L, 1);
        freescreen(&screen->screen);
        return 0;
    }

    sprite = (bor_lua_sprite_handle *)luaL_testudata(L, 1, BOR_LUA_SPRITE_METATABLE);
    if (sprite && sprite->sprite) {
        bor_lua_release_resource(L, 1);
        free(sprite->sprite);
        sprite->sprite = NULL;
        return 0;
    }

    return luaL_error(L, "free expects a live screen or sprite handle");
}

static int lua_openbor_fadeout(lua_State *L)
{
    int type;
    int speed;

    if (lua_gettop(L) < 2 || lua_type(L, 1) != LUA_TNUMBER ||
        lua_type(L, 2) != LUA_TNUMBER)
        return luaL_error(L, "fadeout expects 2 numeric arguments");

    type = (int)lua_tonumber(L, 1);
    speed = (int)lua_tonumber(L, 2);
    fade_out(type, speed);
    return 0;
}

static int lua_openbor_playmusic(lua_State *L)
{
    const char *filename;
    int loop = 0;
    s32 offset = 0;
    if (lua_gettop(L) == 0) { sound_close_music(); return 0; }
    if (lua_type(L, 1) != LUA_TSTRING)
        return luaL_error(L, "playmusic expects a filename string");
    filename = lua_tostring(L, 1);
    if (lua_gettop(L) >= 2) loop = bor_lua_legacy_truthy(L, 2);
    if (lua_gettop(L) >= 3) {
        if (lua_type(L, 3) != LUA_TNUMBER)
            return luaL_error(L, "playmusic offset must be numeric");
        offset = (s32)lua_tonumber(L, 3);
    }
    music((char *)filename, loop, offset);
    return 0;
}

static int lua_openbor_fademusic(lua_State *L)
{
    if (lua_gettop(L) < 1 || lua_type(L, 1) != LUA_TNUMBER)
        return luaL_error(L, "fademusic expects a numeric fade value");
    musicfade[0] = (float)lua_tonumber(L, 1);
    musicfade[1] = (float)savedata.musicvol;
    if (lua_gettop(L) == 4) {
        if (lua_type(L, 2) != LUA_TSTRING || lua_type(L, 3) != LUA_TNUMBER ||
            lua_type(L, 4) != LUA_TNUMBER)
            return luaL_error(L, "fademusic trigger arguments have invalid types");
        strncpy(musicname, lua_tostring(L, 2), sizeof(musicname) - 1);
        musicname[sizeof(musicname) - 1] = '\0';
        musicloop = (int)lua_tonumber(L, 3);
        musicoffset = (u32)lua_tonumber(L, 4);
    }
    return 0;
}

static int lua_openbor_setmusicvolume(lua_State *L)
{
    int left, right;
    if (lua_gettop(L) == 0) return 0;
    if (lua_type(L, 1) != LUA_TNUMBER)
        return luaL_error(L, "setmusicvolume values must be numeric");
    left = (int)lua_tonumber(L, 1);
    right = left;
    if (lua_gettop(L) >= 2) {
        if (lua_type(L, 2) != LUA_TNUMBER)
            return luaL_error(L, "setmusicvolume values must be numeric");
        right = (int)lua_tonumber(L, 2);
    }
    sound_volume_music(left, right);
    return 0;
}

static int lua_openbor_setmusictempo(lua_State *L)
{
    if (lua_gettop(L) == 0) return 0;
    if (lua_type(L, 1) != LUA_TNUMBER)
        return luaL_error(L, "setmusictempo value must be numeric");
    sound_music_tempo((int)lua_tonumber(L, 1));
    return 0;
}

static int lua_openbor_pausemusic(lua_State *L)
{
    if (lua_gettop(L) > 0)
        sound_pause_music(bor_lua_legacy_truthy(L, 1));
    return 0;
}

static int lua_openbor_playsample(lua_State *L)
{
    int values[6], i;
    if (lua_gettop(L) != 6)
        return luaL_error(L, "playsample expects 6 integer arguments");
    for (i = 0; i < 6; ++i) {
        if (lua_type(L, i + 1) != LUA_TNUMBER)
            return luaL_error(L, "playsample arguments must be numeric");
        values[i] = (int)lua_tonumber(L, i + 1);
    }
    if (values[0] < 0)
        return luaL_error(L, "playsample sample ID cannot be negative");
    if (values[5])
        sound_loop_sample(values[0], (unsigned int)values[1], values[2], values[3], (unsigned int)values[4]);
    else
        sound_play_sample(values[0], (unsigned int)values[1], values[2], values[3], (unsigned int)values[4]);
    return 0;
}

static int lua_openbor_unloadsample(lua_State *L)
{
    if (lua_gettop(L) != 1 || lua_type(L, 1) != LUA_TNUMBER)
        return luaL_error(L, "unloadsample expects exactly 1 numeric argument");
    sound_unload_sample((int)lua_tonumber(L, 1));
    return 0;
}

static int lua_openbor_clearspawnentry(lua_State *L)
{
    (void)L;
    memset(&bor_lua_spawnentry, 0, sizeof(bor_lua_spawnentry));
    bor_lua_spawnentry.index = -1;
    bor_lua_spawnentry.itemindex = -1;
    bor_lua_spawnentry.weaponindex = -1;
    return 0;
}

static int lua_openbor_setspawnentry(lua_State *L)
{
    const char *property;
    const char *name;
    s_model *model;
    int value;
    int property_index;

    if (lua_gettop(L) < 2)
        return luaL_error(L, "setspawnentry expects a property and value");
    if (lua_type(L, 1) != LUA_TSTRING && !lua_isinteger(L, 1))
        return luaL_error(L, "setspawnentry property must be a string or integer ID");

    property_index = bor_lua_legacy_enum_index(L, 1, bor_lua_spawnentry_properties,
                                                sizeof(bor_lua_spawnentry_properties) / sizeof(bor_lua_spawnentry_properties[0]));
    if (property_index < 0)
        return luaL_error(L, "setspawnentry: unsupported property");
    property = bor_lua_spawnentry_properties[property_index];

    if (bor_lua_stricmp(property, "name") == 0) {
        if (lua_type(L, 2) != LUA_TSTRING)
            return luaL_error(L, "setspawnentry string property requires a string value");
        name = lua_tostring(L, 2);
        /* Legacy stores the lookup result, including NULL for an unknown model. */
        bor_lua_spawnentry.model = findmodel((char *)name);
    } else if (bor_lua_stricmp(property, "alias") == 0) {
        if (lua_type(L, 2) != LUA_TSTRING)
            return luaL_error(L, "setspawnentry string property requires a string value");
        name = lua_tostring(L, 2);
        strncpy(bor_lua_spawnentry.alias, name, sizeof(bor_lua_spawnentry.alias) - 1);
        bor_lua_spawnentry.alias[sizeof(bor_lua_spawnentry.alias) - 1] = '\0';
    } else if (bor_lua_stricmp(property, "item") == 0) {
        if (lua_type(L, 2) != LUA_TSTRING)
            return luaL_error(L, "setspawnentry string property requires a string value");
        name = lua_tostring(L, 2);
        model = findmodel((char *)name);
        if (!model) {
            lua_pushinteger(L, 0);
            return 1;
        }
        bor_lua_spawnentry.itemmodel = model;
        bor_lua_spawnentry.item = model->name;
        bor_lua_spawnentry.itemindex = get_cached_model_index(bor_lua_spawnentry.item);
        bor_lua_spawnentry.itemplayer_count = 0;
    } else if (bor_lua_stricmp(property, "2pitem") == 0) {
        if (lua_type(L, 2) != LUA_TSTRING)
            return luaL_error(L, "setspawnentry string property requires a string value");
        name = lua_tostring(L, 2);
        model = findmodel((char *)name);
        bor_lua_spawnentry.item = model ? model->name : NULL;
        bor_lua_spawnentry.itemplayer_count = 1;
    } else if (bor_lua_stricmp(property, "3pitem") == 0 ||
               bor_lua_stricmp(property, "4pitem") == 0) {
        if (lua_type(L, 2) != LUA_TSTRING)
            return luaL_error(L, "setspawnentry string property requires a string value");
        name = lua_tostring(L, 2);
        bor_lua_spawnentry.itemmodel = findmodel((char *)name);
        bor_lua_spawnentry.itemplayer_count =
            bor_lua_stricmp(property, "3pitem") == 0 ? 2 : 3;
    } else if (bor_lua_stricmp(property, "itemalias") == 0) {
        if (lua_type(L, 2) != LUA_TSTRING)
            return luaL_error(L, "setspawnentry string property requires a string value");
        name = lua_tostring(L, 2);
        strncpy(bor_lua_spawnentry.itemalias, name, sizeof(bor_lua_spawnentry.itemalias) - 1);
        bor_lua_spawnentry.itemalias[sizeof(bor_lua_spawnentry.itemalias) - 1] = '\0';
    } else if (bor_lua_stricmp(property, "weapon") == 0) {
        if (lua_type(L, 2) != LUA_TSTRING)
            return luaL_error(L, "setspawnentry string property requires a string value");
        name = lua_tostring(L, 2);
        bor_lua_spawnentry.weaponmodel = findmodel((char *)name);
    } else if (bor_lua_stricmp(property, "coords") == 0) {
        if (lua_type(L, 2) != LUA_TNUMBER) goto numeric_error;
        bor_lua_spawnentry.x = (float)lua_tonumber(L, 2);
        if (lua_gettop(L) >= 3) {
            if (lua_type(L, 3) != LUA_TNUMBER) goto numeric_error;
            bor_lua_spawnentry.z = (float)lua_tonumber(L, 3);
        }
        if (lua_gettop(L) >= 4) {
            if (lua_type(L, 4) != LUA_TNUMBER) goto numeric_error;
            bor_lua_spawnentry.a = (float)lua_tonumber(L, 4);
        }
    } else if (bor_lua_stricmp(property, "dying") == 0) {
        if (lua_type(L, 2) != LUA_TNUMBER) goto numeric_error;
        bor_lua_spawnentry.dying = (char)lua_tonumber(L, 2);
        if (lua_gettop(L) >= 3) {
            if (lua_type(L, 3) != LUA_TNUMBER) goto numeric_error;
            bor_lua_spawnentry.per1 = (char)lua_tonumber(L, 3);
        }
        if (lua_gettop(L) >= 4) {
            if (lua_type(L, 4) != LUA_TNUMBER) goto numeric_error;
            bor_lua_spawnentry.per2 = (char)lua_tonumber(L, 4);
        }
    } else {
        if (bor_lua_stricmp(property, "health") != 0 &&
            bor_lua_stricmp(property, "2phealth") != 0 &&
            bor_lua_stricmp(property, "3phealth") != 0 &&
            bor_lua_stricmp(property, "4phealth") != 0 &&
            bor_lua_stricmp(property, "itemhealth") != 0 &&
            bor_lua_stricmp(property, "itemmap") != 0 &&
            bor_lua_stricmp(property, "map") != 0 &&
            bor_lua_stricmp(property, "mp") != 0 &&
            bor_lua_stricmp(property, "alpha") != 0 &&
            bor_lua_stricmp(property, "multiple") != 0 &&
            bor_lua_stricmp(property, "nolife") != 0 &&
            bor_lua_stricmp(property, "boss") != 0 &&
            bor_lua_stricmp(property, "flip") != 0 &&
            bor_lua_stricmp(property, "credit") != 0 &&
            bor_lua_stricmp(property, "aggression") != 0)
            return luaL_error(L, "setspawnentry: unsupported property '%s'", property);

        if (lua_type(L, 2) != LUA_TNUMBER) goto numeric_error;
        value = (int)lua_tonumber(L, 2);
        if (bor_lua_stricmp(property, "health") == 0)
            bor_lua_spawnentry.health[0] = value;
        else if (bor_lua_stricmp(property, "2phealth") == 0)
            bor_lua_spawnentry.health[1] = value;
        else if (bor_lua_stricmp(property, "3phealth") == 0)
            bor_lua_spawnentry.health[2] = value;
        else if (bor_lua_stricmp(property, "4phealth") == 0)
            bor_lua_spawnentry.health[3] = value;
        else if (bor_lua_stricmp(property, "itemhealth") == 0)
            bor_lua_spawnentry.itemhealth = value;
        else if (bor_lua_stricmp(property, "itemmap") == 0)
            bor_lua_spawnentry.itemmap = value;
        else if (bor_lua_stricmp(property, "map") == 0)
            bor_lua_spawnentry.colourmap = value;
        else if (bor_lua_stricmp(property, "mp") == 0)
            bor_lua_spawnentry.mp = value;
        else if (bor_lua_stricmp(property, "alpha") == 0)
            bor_lua_spawnentry.alpha = value;
        else if (bor_lua_stricmp(property, "multiple") == 0)
            bor_lua_spawnentry.multiple = value;
        else if (bor_lua_stricmp(property, "nolife") == 0)
            bor_lua_spawnentry.nolife = value;
        else if (bor_lua_stricmp(property, "boss") == 0)
            bor_lua_spawnentry.boss = value;
        else if (bor_lua_stricmp(property, "flip") == 0)
            bor_lua_spawnentry.flip = value;
        else if (bor_lua_stricmp(property, "credit") == 0)
            bor_lua_spawnentry.credit = value;
        else if (bor_lua_stricmp(property, "aggression") == 0)
            bor_lua_spawnentry.aggression = value;
    }

    lua_pushinteger(L, 1);
    return 1;

numeric_error:
    lua_pushinteger(L, 0);
    return 1;
}

static int lua_openbor_projectile(lua_State *L)
{
    const char *name;
    int value[7];
    int i;
    entity *ent;

    if (lua_gettop(L) != 8)
        return luaL_error(L, "projectile expects 8 arguments");
    if (lua_type(L, 1) != LUA_TSTRING)
        return luaL_error(L, "projectile expects a string model name");
    name = lua_tostring(L, 1);

    /*
     * The original ScriptVariant_IntegerValue accepted integer and decimal
     * variants, truncating decimals before the coordinates were converted
     * back to float for knife_spawn()/bomb_spawn().
     */
    for (i = 0; i < 7; ++i) {
        if (lua_type(L, i + 2) != LUA_TNUMBER)
            return luaL_error(L, "projectile argument %d must be numeric", i + 2);
        value[i] = (int)lua_tonumber(L, i + 2);
    }

    if (value[5] == 1)
        ent = bomb_spawn((char *)name, -1, (float)value[0], (float)value[1],
                         (float)value[2], value[3], value[6]);
    else
        ent = knife_spawn((char *)name, -1, (float)value[0], (float)value[1],
                          (float)value[2], value[3], value[4], value[6]);

    bor_lua_push_entity(L, ent);
    return 1;
}

/* Legacy screen allocation and drawing API. */
static int lua_openbor_allocscreen(lua_State *L)
{
    int width;
    int height;
    s_screen *screen;
    bor_lua_screen_handle *handle;

    if (lua_gettop(L) < 2)
        return luaL_error(L, "allocscreen expects width and height");
    width = (int)bor_lua_check_legacy_integer(L, 1, "allocscreen");
    height = (int)bor_lua_check_legacy_integer(L, 2, "allocscreen");
    if (width <= 0 || height <= 0)
        return luaL_error(L, "allocscreen dimensions must be positive");

    screen = allocscreen(width, height, screenformat);
    if (!screen)
        return luaL_error(L, "allocscreen could not allocate %d x %d screen", width, height);
    clearscreen(screen);

    handle = (bor_lua_screen_handle *)lua_newuserdatauv(L, sizeof(*handle), 0);
    handle->screen = screen;
    luaL_getmetatable(L, BOR_LUA_SCREEN_METATABLE);
    lua_setmetatable(L, -2);
    bor_lua_retain_resource(L, -1);
    return 1;
}

static int lua_openbor_clearscreen(lua_State *L)
{
    if (lua_gettop(L) != 1)
        return luaL_error(L, "clearscreen expects exactly one screen handle");
    clearscreen(bor_lua_check_screen(L, 1));
    return 0;
}

static int lua_openbor_drawstringtoscreen(lua_State *L)
{
    s_screen *screen;
    int x, y, font;
    const char *text;

    if (lua_gettop(L) != 5)
        return luaL_error(L, "drawstringtoscreen expects screen, x, y, font, and text");

    screen = bor_lua_check_screen(L, 1);
    x = bor_lua_check_legacy_integer(L, 2, "drawstringtoscreen");
    y = bor_lua_check_legacy_integer(L, 3, "drawstringtoscreen");
    font = bor_lua_check_legacy_integer(L, 4, "drawstringtoscreen");
    bor_lua_push_legacy_string(L, 5);
    text = lua_tostring(L, -1);

    screen_printf(screen, x, y, font, "%s", text);
    return 0;
}

static int lua_openbor_drawboxtoscreen(lua_State *L)
{
    s_screen *screen = bor_lua_check_screen(L, 1);
    int x = (int)bor_lua_check_legacy_integer(L, 2, "drawboxtoscreen");
    int y = (int)bor_lua_check_legacy_integer(L, 3, "drawboxtoscreen");
    int width = (int)bor_lua_check_legacy_integer(L, 4, "drawboxtoscreen");
    int height = (int)bor_lua_check_legacy_integer(L, 5, "drawboxtoscreen");
    int colour = (int)bor_lua_check_legacy_integer(L, 6, "drawboxtoscreen");
    int blend_index = (int)(lua_gettop(L) >= 7 ? bor_lua_check_legacy_integer(L, 7, "drawboxtoscreen") : -1);

    if (lua_gettop(L) < 6)
        return luaL_error(L, "drawboxtoscreen expects screen, x, y, width, height, colour, and optional LUT");
    if (blend_index >= 0) blend_index %= MAX_BLENDINGS + 1;

    switch (screen->pixelformat) {
    case PIXEL_8: drawbox(x, y, width, height, colour, screen, blend_index); break;
    case PIXEL_16: drawbox16(x, y, width, height, (unsigned short)colour, screen, blend_index); break;
    case PIXEL_32: drawbox32(x, y, width, height, (unsigned int)colour, screen, blend_index); break;
    }
    return 0;
}

static int lua_openbor_drawlinetoscreen(lua_State *L)
{
    s_screen *screen = bor_lua_check_screen(L, 1);
    int x1 = (int)bor_lua_check_legacy_integer(L, 2, "drawlinetoscreen");
    int y1 = (int)bor_lua_check_legacy_integer(L, 3, "drawlinetoscreen");
    int x2 = (int)bor_lua_check_legacy_integer(L, 4, "drawlinetoscreen");
    int y2 = (int)bor_lua_check_legacy_integer(L, 5, "drawlinetoscreen");
    int colour = (int)bor_lua_check_legacy_integer(L, 6, "drawlinetoscreen");
    int blend_index = (int)(lua_gettop(L) >= 7 ? bor_lua_check_legacy_integer(L, 7, "drawlinetoscreen") : -1);

    if (lua_gettop(L) < 6)
        return luaL_error(L, "drawlinetoscreen expects screen, 5 integers, and optional LUT");
    if (blend_index >= 0) blend_index %= MAX_BLENDINGS + 1;

    switch (screen->pixelformat) {
    case PIXEL_8: line(x1, y1, x2, y2, colour, screen, blend_index); break;
    case PIXEL_16: line16(x1, y1, x2, y2, colour, screen, blend_index); break;
    case PIXEL_32: line32(x1, y1, x2, y2, colour, screen, blend_index); break;
    }
    return 0;
}

static int lua_openbor_drawdottoscreen(lua_State *L)
{
    s_screen *screen = bor_lua_check_screen(L, 1);
    int x = (int)bor_lua_check_legacy_integer(L, 2, "drawdottoscreen");
    int y = (int)bor_lua_check_legacy_integer(L, 3, "drawdottoscreen");
    int colour = (int)bor_lua_check_legacy_integer(L, 4, "drawdottoscreen");
    int blend_index = (int)(lua_gettop(L) >= 5 ? bor_lua_check_legacy_integer(L, 5, "drawdottoscreen") : -1);

    if (lua_gettop(L) < 4)
        return luaL_error(L, "drawdottoscreen expects screen, x, y, colour, and optional LUT");
    if (blend_index >= 0) blend_index %= MAX_BLENDINGS + 1;

    switch (screen->pixelformat) {
    case PIXEL_8: putpixel(x, y, colour, screen, blend_index); break;
    case PIXEL_16: putpixel16(x, y, colour, screen, blend_index); break;
    case PIXEL_32: putpixel32(x, y, colour, screen, blend_index); break;
    }
    return 0;
}

static int lua_openbor_drawscreen(lua_State *L)
{
    s_screen *screen = bor_lua_check_screen(L, 1);
    int x = (int)bor_lua_check_legacy_integer(L, 2, "drawscreen");
    int y = (int)bor_lua_check_legacy_integer(L, 3, "drawscreen");
    int z = (int)bor_lua_check_legacy_integer(L, 4, "drawscreen");
    s_drawmethod method;

    if (lua_gettop(L) < 4)
        return luaL_error(L, "drawscreen expects screen, x, y, z, and optional LUT");
    if (lua_gettop(L) == 4) {
        method = bor_lua_drawmethod;
    } else {
        int blend_index = (int)bor_lua_check_legacy_integer(L, 5, "drawscreen");
        method = plainmethod;
        method.alpha = blend_index >= 0 ? blend_index % (MAX_BLENDINGS + 1) : blend_index;
        method.transbg = 1;
    }
    spriteq_add_screen(x, y, z, screen, &method, 0);
    return 0;
}

static int lua_openbor_drawsprite(lua_State *L)
{
    s_sprite *sprite = bor_lua_check_sprite(L, 1);
    int x = (int)bor_lua_check_legacy_integer(L, 2, "drawsprite");
    int y = (int)bor_lua_check_legacy_integer(L, 3, "drawsprite");
    int z = (int)bor_lua_check_legacy_integer(L, 4, "drawsprite");
    int sortid = 0;

    if (lua_gettop(L) < 4 || !sprite)
        return luaL_error(L, "drawsprite expects a sprite handle, x, y, z, and optional sort ID");
    if (lua_gettop(L) >= 5)
        sortid = (int)bor_lua_check_legacy_integer(L, 5, "drawsprite");
    spriteq_add_frame(x, y, z, sprite, &bor_lua_drawmethod, sortid);
    return 0;
}

static int lua_openbor_drawspritetoscreen(lua_State *L)
{
    s_sprite *sprite = bor_lua_check_sprite(L, 1);
    s_screen *screen = bor_lua_check_screen(L, 2);
    int x = (int)bor_lua_check_legacy_integer(L, 3, "drawspritetoscreen");
    int y = (int)bor_lua_check_legacy_integer(L, 4, "drawspritetoscreen");

    if (lua_gettop(L) < 4 || !sprite)
        return luaL_error(L, "drawspritetoscreen expects sprite, screen, x, and y");
    putsprite(x, y, sprite, screen, &bor_lua_drawmethod);
    return 0;
}

static int lua_openbor_setdrawmethod(lua_State *L)
{
    entity *ent = NULL;
    s_drawmethod *method;
    int values[12] = {0};
    int i;

    if (lua_gettop(L) < 2)
        return luaL_error(L, "setdrawmethod expects an entity or nil, draw flags, and optional colourmap");
    if (!lua_isnil(L, 1)) {
        ent = bor_lua_get_live_entity(L, 1);
        if (!ent)
            return luaL_error(L, "setdrawmethod expects an entity handle or nil");
    }
    method = ent ? &ent->drawmethod : &bor_lua_drawmethod;

    for (i = 2; i <= 13 && i <= lua_gettop(L); ++i)
        values[i - 2] = bor_lua_check_legacy_integer(L, i, "setdrawmethod");

    method->flag = values[0];
    method->scalex = values[1];
    method->scaley = values[2];
    method->flipx = values[3];
    method->flipy = values[4];
    method->shiftx = values[5];
    method->alpha = values[6];
    method->remap = values[7];
    method->fillcolor = values[8];
    method->rotate = values[9] % 360;
    method->fliprotate = values[10];
    method->transbg = values[11];
    if (lua_gettop(L) >= 14) {
        if (!lua_isnil(L, 14) && !lua_islightuserdata(L, 14))
            return luaL_error(L, "setdrawmethod colourmap must be a lightuserdata pointer or nil");
        method->table = (unsigned char *)lua_touserdata(L, 14);
    }
    if (method->rotate < 0)
        method->rotate += 360;
    return 0;
}

/* Legacy model, sprite, and GIF helpers. */
static int lua_openbor_loadmodel(lua_State *L)
{
    const char *name;
    char unload = 0;
    s_model *model;

    if (lua_gettop(L) < 1 || lua_type(L, 1) != LUA_TSTRING)
        return luaL_error(L, "loadmodel expects a model name");
    name = lua_tostring(L, 1);
    if (lua_gettop(L) == 2) {
        if (lua_type(L, 2) != LUA_TNUMBER)
            return luaL_error(L, "loadmodel unload flag must be numeric");
        unload = (char)lua_tonumber(L, 2);
    }

    model = load_cached_model((char *)name, "openbor_loadmodel", unload);
    if (model)
        lua_pushlightuserdata(L, model);
    else
        lua_pushnil(L);
    return 1;
}

static int lua_openbor_loadsprite(lua_State *L)
{
    const char *filename;
    s_sprite *sprite;
    bor_lua_sprite_handle *handle;

    if (lua_gettop(L) != 1 || lua_type(L, 1) != LUA_TSTRING)
        return luaL_error(L, "loadsprite expects exactly 1 filename string");
    filename = lua_tostring(L, 1);
    sprite = loadsprite2((char *)filename, NULL, NULL);
    if (!sprite) {
        lua_pushnil(L);
        return 1;
    }
    handle = (bor_lua_sprite_handle *)lua_newuserdatauv(L, sizeof(*handle), 0);
    handle->sprite = sprite;
    luaL_getmetatable(L, BOR_LUA_SPRITE_METATABLE);
    lua_setmetatable(L, -2);
    bor_lua_retain_resource(L, -1);
    return 1;
}

static int lua_openbor_playgif(lua_State *L)
{
    extern unsigned char pal[1024];
    const char *filename;
    int values[3] = {0, 0, 0};
    int i;
    int result;

    if (lua_gettop(L) < 1 || lua_type(L, 1) != LUA_TSTRING)
        return luaL_error(L, "playgif expects a filename");
    filename = lua_tostring(L, 1);

    for (i = 0; i < 3 && i + 2 <= lua_gettop(L); ++i) {
        if (lua_type(L, i + 2) != LUA_TNUMBER)
            return luaL_error(L, "playgif optional arguments must be numeric");
        values[i] = (int)lua_tonumber(L, i + 2);
    }

    result = playgif((char *)filename, values[0], values[1], values[2]);
    palette_set_corrected(pal, savedata.gamma, savedata.gamma, savedata.gamma,
                          savedata.brightness, savedata.brightness, savedata.brightness);
    lua_pushinteger(L, result);
    return 1;
}

static int lua_openbor_getmodelproperty(lua_State *L)
{
    int index;
    int property;

    if (!lua_isinteger(L, 1) || !lua_isinteger(L, 2))
        return luaL_error(L, "getmodelproperty expects integer model and property indices");
    index = (int)lua_tointeger(L, 1);
    property = (int)lua_tointeger(L, 2);
    if (index < 0 || (unsigned int)index >= models_cached || !model_cache) {
        lua_pushnil(L);
        return 1;
    }

    switch (property) {
    case 0:
        lua_pushinteger(L, model_cache[index].loadflag);
        break;
    case 1:
        /* The legacy switch falls through to property 2 and returns the name. */
        lua_pushstring(L, model_cache[index].name ? model_cache[index].name : "");
        break;
    case 2:
        lua_pushstring(L, model_cache[index].name ? model_cache[index].name : "");
        break;
    case 3:
        lua_pushstring(L, model_cache[index].path ? model_cache[index].path : "");
        break;
    case 4:
        lua_pushinteger(L, model_cache[index].selectable);
        break;
    default:
        lua_pushnil(L);
        break;
    }
    return 1;
}

static int lua_openbor_changemodelproperty(lua_State *L)
{
    int index;
    int property;

    if (lua_gettop(L) < 2)
        return luaL_error(L, "changemodelproperty expects model index and property index");

    if (!lua_isinteger(L, 1) || !lua_isinteger(L, 2))
        return luaL_error(L, "changemodelproperty expects integer model and property indices");
    index = (int)lua_tointeger(L, 1);
    property = (int)lua_tointeger(L, 2);

    if (index < 0 || (unsigned int)index >= models_cached || !model_cache)
        return 0;

    /*
     * The legacy switch intentionally/accidentally falls through from
     * properties 0-3 into case 4. Preserve that observable behaviour:
     * every property ID from 0 through 4 writes the selectable flag.
     */
    if (property >= 0 && property <= 4) {
        int value;
        if (lua_gettop(L) < 3 || lua_type(L, 3) != LUA_TNUMBER) {
            lua_pushinteger(L, 0);
            return 1;
        }
        value = bor_lua_check_legacy_integer(L, 3, "changemodelproperty");
        model_cache[index].selectable = (char)value;
    }

    return 0;
}

/* Legacy engine state and entity action helpers. */
static int lua_openbor_changelight(lua_State *L)
{
    if (lua_gettop(L) < 2)
        return luaL_error(L, "changelight expects x and z values");

    if (!lua_isnil(L, 1))
        light[0] = (int)bor_lua_check_legacy_integer(L, 1, "changelight");
    if (!lua_isnil(L, 2))
        light[1] = (int)bor_lua_check_legacy_integer(L, 2, "changelight");
    return 0;
}

static int lua_openbor_changeshadowcolor(lua_State *L)
{
    if (lua_gettop(L) < 1)
        return luaL_error(L, "changeshadowcolor expects a color and optional alpha");

    colors.shadow = (int)bor_lua_check_legacy_integer(L, 1, "changeshadowcolor");
    if (lua_gettop(L) >= 2)
        shadowalpha = (int)bor_lua_check_legacy_integer(L, 2, "changeshadowcolor");
    return 0;
}

static int lua_openbor_changepalette(lua_State *L)
{
    int index = (int)bor_lua_check_legacy_integer(L, 1, "changepalette");
    change_system_palette(index);
    return 0;
}

static int lua_openbor_jumptobranch(lua_State *L)
{
    extern int endgame;
    const char *name;

    if (lua_gettop(L) < 1)
        return luaL_error(L, "jumptobranch expects a branch name and optional immediate flag");
    if (lua_type(L, 1) != LUA_TSTRING)
        return luaL_error(L, "jumptobranch branch name must be a string");
    name = lua_tostring(L, 1);

    strncpy(branch_name, name, MAX_NAME_LEN);
    branch_name[MAX_NAME_LEN] = '\0';
    if (lua_gettop(L) >= 2)
        endgame = (int)bor_lua_check_legacy_integer(L, 2, "jumptobranch");
    return 0;
}

static int lua_openbor_bindentity(lua_State *L)
{
    entity *ent = bor_lua_get_live_entity(L, 1);
    entity *other = bor_lua_get_live_entity(L, 2);
    int values[5] = {0, 0, 0, 0, 0};
    int i;
    extern void adjust_bind(entity *e);

    if (lua_gettop(L) < 2)
        return luaL_error(L, "bindentity expects an entity and target");
    if (!ent || !ent->exists)
        return 0;
    if (!other || !other->exists) {
        ent->bound = NULL;
        return 0;
    }

    ent->bound = other;
    for (i = 3; i <= 7 && i <= lua_gettop(L); ++i) {
        if (!lua_isnil(L, i)) {
            if (lua_type(L, i) != LUA_TNUMBER)
                return luaL_error(L, "bindentity offsets must be numeric or nil");
            values[i - 3] = (int)lua_tonumber(L, i);
        }
    }
    if (lua_gettop(L) >= 3 && !lua_isnil(L, 3)) ent->bindoffset[0] = values[0];
    if (lua_gettop(L) >= 4 && !lua_isnil(L, 4)) ent->bindoffset[1] = values[1];
    if (lua_gettop(L) >= 5 && !lua_isnil(L, 5)) ent->bindoffset[2] = values[2];
    if (lua_gettop(L) >= 6 && !lua_isnil(L, 6)) ent->bindoffset[3] = values[3];
    if (lua_gettop(L) >= 7 && !lua_isnil(L, 7)) ent->bindanim = values[4];

    adjust_bind(ent);
    return 0;
}

static int lua_openbor_performattack(lua_State *L)
{
    entity *ent = bor_lua_get_live_entity(L, 1);
    int animation;
    int resetable = 0;

    if (lua_gettop(L) < 1)
        return luaL_error(L, "performattack expects an entity and optional integers");
    if (!ent)
        return luaL_error(L, "performattack expects an entity handle");

    ent->takeaction = common_attack_proc;
    ent->attacking = 1;
    ent->idling = 0;
    ent->drop = 0;
    ent->falling = 0;
    ent->inpain = 0;
    ent->blocking = 0;

    if (lua_gettop(L) > 1) {
        if (lua_type(L, 2) != LUA_TNUMBER)
            return luaL_error(L, "performattack animation must be numeric");
        animation = (int)lua_tonumber(L, 2);
        if (lua_gettop(L) > 2) {
            if (lua_type(L, 3) != LUA_TNUMBER)
                return luaL_error(L, "performattack resetable must be numeric");
            resetable = (int)lua_tonumber(L, 3);
        }
        ent_set_anim(ent, animation, resetable);
    }
    return 0;
}

static int lua_openbor_setidle(lua_State *L)
{
    entity *ent = bor_lua_get_live_entity(L, 1);
    int animation = 0;
    int resetable = 0;
    int stalladd = 0;

    if (lua_gettop(L) < 1)
        return luaL_error(L, "setidle expects an entity and optional integers");
    if (!ent)
        return luaL_error(L, "setidle expects an entity handle");

    ent->takeaction = NULL;
    ent->attacking = 0;
    ent->idling = 1;
    ent->drop = 0;
    ent->falling = 0;
    ent->inpain = 0;
    ent->blocking = 0;
    ent->nograb = 0;

    if (lua_gettop(L) > 1) {
        if (lua_type(L, 2) != LUA_TNUMBER)
            return luaL_error(L, "setidle animation must be numeric");
        animation = (int)lua_tonumber(L, 2);
        if (lua_gettop(L) > 2) {
            if (lua_type(L, 3) != LUA_TNUMBER)
                return luaL_error(L, "setidle resetable must be numeric");
            resetable = (int)lua_tonumber(L, 3);
        }
        if (lua_gettop(L) > 3) {
            if (lua_type(L, 4) != LUA_TNUMBER)
                return luaL_error(L, "setidle stalladd must be numeric");
            stalladd = (int)lua_tonumber(L, 4);
        }
        ent_set_anim(ent, animation, resetable);
    }
    if (stalladd > 0)
        ent->stalltime = borTime + (u32)stalladd;
    return 0;
}

static int lua_openbor_updateframe(lua_State *L)
{
    entity *ent = bor_lua_get_live_entity(L, 1);
    int frame;
    void update_frame(entity *ent, int frame);

    if (lua_gettop(L) < 2)
        return luaL_error(L, "updateframe expects an entity and frame number");
    if (!ent)
        return luaL_error(L, "updateframe expects an entity handle");
    if (lua_type(L, 2) != LUA_TNUMBER)
        return luaL_error(L, "updateframe frame number must be numeric");

    frame = (int)lua_tonumber(L, 2);
    update_frame(ent, frame);
    return 0;
}

static int lua_openbor_gettextobjproperty(lua_State *L)
{
    int index = (int)bor_lua_check_legacy_integer(L, 1, "gettextobjproperty");
    int property;
    s_textobj *obj;
    if (!level) return luaL_error(L, "gettextobjproperty requires a loaded level");
    if (index < 0 || index >= LEVEL_MAX_TEXTOBJS) { lua_pushinteger(L, 0); return 1; }
    property = bor_lua_legacy_enum_index(L, 2, bor_lua_textobj_properties, 6);
    obj = &level->textobjs[index];
    switch (property) {
    case 0: lua_pushinteger(L, obj->font); break;
    case 1: lua_pushstring(L, obj->text ? obj->text : ""); break;
    case 2: lua_pushinteger(L, obj->t); break;
    case 3: lua_pushinteger(L, obj->x); break;
    case 4: lua_pushinteger(L, obj->y); break;
    case 5: lua_pushinteger(L, obj->z); break;
    default: return luaL_error(L, "gettextobjproperty: unsupported property");
    }
    return 1;
}

static int lua_openbor_changetextobjproperty(lua_State *L)
{
    int index = (int)bor_lua_check_legacy_integer(L, 1, "changetextobjproperty");
    int property;
    s_textobj *obj;
    int value;
    if (lua_gettop(L) < 3) return luaL_error(L, "changetextobjproperty expects index, property, and value");
    if (!level) return luaL_error(L, "changetextobjproperty requires a loaded level");
    if (index < 0 || index >= LEVEL_MAX_TEXTOBJS) { lua_pushinteger(L, 0); return 1; }
    property = bor_lua_legacy_enum_index(L, 2, bor_lua_textobj_properties, 6);
    if (property < 0)
        return luaL_error(L, "changetextobjproperty: unsupported property");
    obj = &level->textobjs[index];
    if (property == 1) {
        const char *value_text;
        char *copy;
        if (lua_type(L, 3) != LUA_TSTRING) return luaL_error(L, "changetextobjproperty text value must be a string");
        value_text = lua_tostring(L, 3);
        copy = (char *)malloc(MAX_STR_VAR_LEN);
        if (!copy) return luaL_error(L, "out of memory changing text object");
        strncpy(copy, value_text, MAX_STR_VAR_LEN - 1);
        copy[MAX_STR_VAR_LEN - 1] = '\0';
        free(obj->text);
        obj->text = copy;
    } else {
        if (lua_type(L, 3) != LUA_TNUMBER) { lua_pushinteger(L, 0); return 1; }
        value = (int)lua_tonumber(L, 3);
        switch (property) {
        case 0: obj->font = value; break;
        case 2: obj->t = value; break;
        case 3: obj->x = value; break;
        case 4: obj->y = value; break;
        case 5: obj->z = value; break;
        default: return luaL_error(L, "changetextobjproperty: unsupported property");
        }
    }
    lua_pushinteger(L, 1);
    return 1;
}

static int lua_openbor_settextobj(lua_State *L)
{
    int index = (int)bor_lua_check_legacy_integer(L, 1, "settextobj");
    const char *text;
    s_textobj *obj;
    char *copy;
    if (lua_gettop(L) < 6) return luaL_error(L, "settextobj expects index, x, y, font, z, text, and optional time");
    if (!level) return luaL_error(L, "settextobj requires a loaded level");
    if (index < 0 || index >= LEVEL_MAX_TEXTOBJS) { lua_pushinteger(L, 0); return 1; }
    if (lua_type(L, 6) != LUA_TSTRING) return luaL_error(L, "settextobj text must be a string");
    text = lua_tostring(L, 6);
    copy = (char *)malloc(MAX_STR_VAR_LEN);
    if (!copy) return luaL_error(L, "out of memory setting text object");
    strncpy(copy, text, MAX_STR_VAR_LEN - 1);
    copy[MAX_STR_VAR_LEN - 1] = '\0';
    obj = &level->textobjs[index];
    if (lua_type(L, 2) == LUA_TNUMBER) obj->x = (int)lua_tonumber(L, 2);
    if (lua_type(L, 3) == LUA_TNUMBER) obj->y = (int)lua_tonumber(L, 3);
    if (lua_type(L, 4) == LUA_TNUMBER) obj->font = (int)lua_tonumber(L, 4);
    if (lua_type(L, 5) == LUA_TNUMBER) obj->z = (int)lua_tonumber(L, 5);
    if (lua_gettop(L) >= 7 && lua_type(L, 7) == LUA_TNUMBER) obj->t = (int)lua_tonumber(L, 7);
    free(obj->text);
    obj->text = copy;
    lua_pushinteger(L, 1);
    return 1;
}

static int lua_openbor_cleartextobj(lua_State *L)
{
    int index = (int)bor_lua_check_legacy_integer(L, 1, "cleartextobj");
    s_textobj *obj;

    if (!level)
        return luaL_error(L, "cleartextobj requires a loaded level");
    if (index < 0 || index >= LEVEL_MAX_TEXTOBJS) {
        lua_pushinteger(L, 0);
        return 1;
    }

    obj = &level->textobjs[index];
    obj->t = obj->x = obj->y = obj->font = obj->z = 0;
    free(obj->text);
    obj->text = NULL;
    lua_pushinteger(L, 0);
    return 1;
}

static int bor_lua_layer_property(lua_State *L, int foreground, int change, int offset)
{
    int index, property, count, value;
    int index_arg = 1 + offset, property_arg = 2 + offset, value_arg = 3 + offset;
    float decimal;
    if (lua_gettop(L) < property_arg || lua_type(L, index_arg) != LUA_TNUMBER)
        return luaL_error(L, "layer property access expects a numeric index and property name or ID");
    index = (int)lua_tonumber(L, index_arg);
    if (!level) return luaL_error(L, "layer property access requires a loaded level");
    if (change && lua_gettop(L) < value_arg) return luaL_error(L, "layer property change expects a value");
    count = foreground ? level->numfglayers : level->numbglayers;
    if (index < 0 || index >= count) { lua_pushinteger(L, 0); return 1; }
    property = bor_lua_legacy_enum_index(L, property_arg, bor_lua_layer_properties, 16);
    if (property < 0) return luaL_error(L, "unsupported layer property");
#define BOR_LAYER_INT(field, id) do { \
    if (property == id) { \
        if (change) { \
            if (lua_type(L, value_arg) != LUA_TNUMBER) { lua_pushinteger(L, 0); return 1; } \
            value = (int)lua_tonumber(L, value_arg); \
            if (foreground) level->fglayers[index].field = value; else level->bglayers[index].field = value; \
            lua_pushinteger(L, 1); \
        } else lua_pushinteger(L, foreground ? level->fglayers[index].field : level->bglayers[index].field); \
        return 1; \
    } \
} while (0)
#define BOR_LAYER_FLOAT(field, id) do { \
    if (property == id) { \
        if (change) { \
            if (lua_type(L, value_arg) != LUA_TNUMBER) { lua_pushinteger(L, 0); return 1; } \
            decimal = (float)lua_tonumber(L, value_arg); \
            if (foreground) level->fglayers[index].field = decimal; else level->bglayers[index].field = decimal; \
            lua_pushinteger(L, 1); \
        } else lua_pushnumber(L, foreground ? level->fglayers[index].field : level->bglayers[index].field); \
        return 1; \
    } \
} while (0)
    BOR_LAYER_INT(alpha, 0);
    BOR_LAYER_INT(amplitude, 1);
    BOR_LAYER_FLOAT(bgspeedratio, 2);
    BOR_LAYER_INT(enabled, 3);
    BOR_LAYER_INT(transparency, 4);
    BOR_LAYER_INT(watermode, 5);
    BOR_LAYER_INT(wavelength, 6);
    BOR_LAYER_FLOAT(wavespeed, 7);
    BOR_LAYER_INT(xoffset, 8);
    BOR_LAYER_FLOAT(xratio, 9);
    BOR_LAYER_INT(xrepeat, 10);
    BOR_LAYER_INT(xspacing, 11);
    BOR_LAYER_INT(zoffset, 12);
    BOR_LAYER_FLOAT(zratio, 13);
    BOR_LAYER_INT(zrepeat, 14);
    BOR_LAYER_INT(zspacing, 15);
#undef BOR_LAYER_INT
#undef BOR_LAYER_FLOAT
    return luaL_error(L, "unsupported layer property");
}

static int lua_openbor_getbglayerproperty(lua_State *L)
{
    return bor_lua_layer_property(L, 0, 0, 0);
}

static int lua_openbor_changebglayerproperty(lua_State *L)
{
    return bor_lua_layer_property(L, 0, 1, 0);
}

static int lua_openbor_getfglayerproperty(lua_State *L)
{
    return bor_lua_layer_property(L, 1, 0, 0);
}

static int lua_openbor_changefglayerproperty(lua_State *L)
{
    return bor_lua_layer_property(L, 1, 1, 0);
}

/* Generic legacy-style layer setter. This engine currently exposes background
 * and foreground layer arrays, so only those two layer types are supported. */
static int lua_openbor_changelayerproperty(lua_State *L)
{
    const char *type;
    int foreground;
    if (lua_gettop(L) < 4)
        return luaL_error(L, "changelayerproperty expects type, index, property, and value");
    if (lua_type(L, 1) != LUA_TSTRING)
        return luaL_error(L, "changelayerproperty expects a layer type string");
    type = lua_tostring(L, 1);
    if (!bor_lua_stricmp(type, "bglayer")) foreground = 0;
    else if (!bor_lua_stricmp(type, "fglayer")) foreground = 1;
    else return luaL_error(L, "changelayerproperty supports only 'bglayer' and 'fglayer' in this engine");
    return bor_lua_layer_property(L, foreground, 1, 1);
}

/* Legacy player property accessors. */
static int lua_openbor_getplayerproperty(lua_State *L)
{
    extern s_player player[4];
    int index = 0, property;
    entity *ent;
    if (lua_gettop(L) < 2) return luaL_error(L, "getplayerproperty expects a player index and property name or ID");
    if (lua_type(L, 1) == LUA_TNUMBER) index = (int)lua_tonumber(L, 1);
    property = bor_lua_legacy_enum_index(L, 2, bor_lua_getplayer_properties, 9);
    if (index < 0 || index >= 4) { lua_pushnil(L); return 1; }
    ent = player[index].ent;
    if (!ent) { lua_pushnil(L); return 1; }
    if (property < 0) {
        if (lua_isinteger(L, 2)) {
            lua_pushnil(L);
            return 1;
        }
        return luaL_error(L, "getplayerproperty: unsupported property");
    }
    switch (property) {
    case 0: lua_pushinteger(L, noshare ? player[index].credits : credits); break;
    case 1: case 2: bor_lua_push_entity(L, ent); break;
    case 3: lua_pushinteger(L, (lua_Integer)player[index].keys); break;
    case 4: lua_pushinteger(L, (lua_Integer)player[index].lives); break;
    case 5: lua_pushstring(L, player[index].name); break;
    case 6: lua_pushinteger(L, (lua_Integer)player[index].playkeys); break;
    case 7: lua_pushinteger(L, (lua_Integer)player[index].score); break;
    case 8: lua_pushinteger(L, (lua_Integer)player[index].weapnum); break;
    default: lua_pushnil(L); break;
    }
    return 1;
}

static int lua_openbor_changeplayerproperty(lua_State *L)
{
    extern s_player player[4];
    int index = 0, property, value;
    entity *ent;
    if (lua_gettop(L) < 3) return luaL_error(L, "changeplayerproperty expects player, property, and value");
    if (lua_type(L, 1) == LUA_TNUMBER) index = (int)lua_tonumber(L, 1);
    property = bor_lua_legacy_enum_index(L, 2, bor_lua_changeplayer_properties, 6);
    if (index < 0 || index >= 4) { lua_pushinteger(L, 0); return 1; }
    ent = player[index].ent;
    if (!ent) { lua_pushinteger(L, 1); return 1; }
    if (property < 0 || property >= 6) {
        if (lua_isinteger(L, 2)) {
            lua_pushinteger(L, 1);
            return 1;
        }
        return luaL_error(L, "changeplayerproperty: unsupported property");
    }
    if (property == 5) {
        int weapon, animation = 0;
        if (lua_type(L, 3) != LUA_TNUMBER) goto invalid_value;
        weapon = (int)lua_tonumber(L, 3);
        if (lua_gettop(L) > 3) {
            if (lua_type(L, 4) != LUA_TNUMBER) goto invalid_value;
            animation = (int)lua_tonumber(L, 4);
        }
        set_weapon(ent, weapon, animation);
    } else if (property == 2) {
        const char *name;
        if (lua_type(L, 3) != LUA_TSTRING) return luaL_error(L, "changeplayerproperty name value must be a string");
        name = lua_tostring(L, 3);
        strncpy(player[index].name, name, sizeof(player[index].name) - 1);
        player[index].name[sizeof(player[index].name) - 1] = '\0';
    } else {
        if (lua_type(L, 3) != LUA_TNUMBER) goto invalid_value;
        value = (int)lua_tonumber(L, 3);
        if (property == 4) {
            if (value < 0) value = 0;
            else if (value > 999999999) value = 999999999;
            player[index].score = (unsigned int)value;
        } else if (property == 1) player[index].lives = value;
        else if (property == 3) player[index].playkeys = value;
        else if (property == 0) {
            if (noshare) player[index].credits = value;
            else credits = value;
        }
    }
    lua_pushinteger(L, 1);
    return 1;
invalid_value:
    lua_pushinteger(L, 0);
    return 1;
}

/* Legacy entity targeting, collision queries, and toss helper. */
static int lua_openbor_tossentity(lua_State *L)
{
    entity *ent = bor_lua_get_live_entity(L, 1);
    float height = 0;
    float speedx = 0;
    float speedz = 0;
    lua_Number value;

    if (lua_gettop(L) < 1)
        return luaL_error(L, "tossentity expects an entity");

    if (!ent || !ent->exists) {
        lua_pushinteger(L, 0);
        return 1;
    }

    if (lua_gettop(L) >= 2) {
        if (lua_type(L, 2) != LUA_TNUMBER) {
            lua_pushinteger(L, 0);
            return 1;
        }
        value = lua_tonumber(L, 2);
        height = (float)value;
    }

    if (lua_gettop(L) >= 3) {
        if (lua_type(L, 3) != LUA_TNUMBER) {
            lua_pushinteger(L, 0);
            return 1;
        }
        value = lua_tonumber(L, 3);
        speedx = (float)value;
    }

    if (lua_gettop(L) >= 4) {
        if (lua_type(L, 4) != LUA_TNUMBER) {
            lua_pushinteger(L, 0);
            return 1;
        }
        value = lua_tonumber(L, 4);
        speedz = (float)value;
    }

    ent->xdir = speedx;
    ent->zdir = speedz;
    toss(ent, height);
    lua_pushinteger(L, 1);
    return 1;
}

static int lua_openbor_findtarget(lua_State *L)
{
    entity *ent = bor_lua_get_live_entity(L, 1);
    entity *previous_self;
    entity *target;
    int animation = -1;

    if (lua_gettop(L) < 1)
        return luaL_error(L, "findtarget expects an entity and an optional animation");

    if (!ent || !ent->exists) {
        lua_pushnil(L);
        return 1;
    }

    if (lua_gettop(L) >= 2) {
        lua_Number value;

        if (lua_type(L, 2) != LUA_TNUMBER)
            return luaL_error(L, "findtarget animation must be a number");

        value = lua_tonumber(L, 2);
        animation = (int)value;
    }

    previous_self = self;
    self = ent;
    target = normal_find_target(animation);
    self = previous_self;

    bor_lua_push_entity(L, target);
    return 1;
}

static int lua_openbor_checkrange(lua_State *L)
{
    entity *ent;
    entity *target;
    int animation;

    if (lua_gettop(L) < 2)
        return luaL_error(L, "checkrange expects 2 entities and an optional animation");

    ent = bor_lua_get_live_entity(L, 1);
    target = bor_lua_get_live_entity(L, 2);
    if (!ent || !target || !ent->exists || !target->exists)
        return luaL_error(L, "checkrange expects two valid entity handles");

    if (lua_gettop(L) >= 3) {
        if (lua_type(L, 3) != LUA_TNUMBER)
            return luaL_error(L, "checkrange animation must be numeric");
        animation = (int)lua_tonumber(L, 3);
    } else {
        animation = ent->animnum;
    }

    if (animation < 0 || animation >= dyn_anim_custom_maxvalues.max_animations)
        return luaL_error(L, "checkrange animation index is out of range");
    lua_pushinteger(L, check_range(ent, target, animation));
    return 1;
}

static int lua_openbor_checkhole(lua_State *L)
{
    float x, z;

    if (lua_gettop(L) < 2)
        return luaL_error(L, "checkhole expects x and z coordinates");
    if (lua_type(L, 1) != LUA_TNUMBER || lua_type(L, 2) != LUA_TNUMBER || !level) {
        lua_pushinteger(L, 0);
        return 1;
    }
    x = (float)lua_tonumber(L, 1);
    z = (float)lua_tonumber(L, 2);
    lua_pushinteger(L, checkhole(x, z) && checkwall(x, z) < 0 ? 1 : 0);
    return 1;
}

static int lua_openbor_checkwall(lua_State *L)
{
    float x, z;
    int wall;

    if (lua_gettop(L) < 2)
        return luaL_error(L, "checkwall expects x and z coordinates");
    if (lua_type(L, 1) != LUA_TNUMBER || lua_type(L, 2) != LUA_TNUMBER || !level) {
        lua_pushnumber(L, 0);
        return 1;
    }
    x = (float)lua_tonumber(L, 1);
    z = (float)lua_tonumber(L, 2);
    wall = checkwall_below(x, z, 100000);
    lua_pushnumber(L, wall >= 0 ? level->walls[wall].alt : 0);
    return 1;
}

static int lua_openbor_checkplatformbelow(lua_State *L)
{
    float x, z, a;
    entity *platform;

    if (lua_gettop(L) < 3)
        return luaL_error(L, "checkplatformbelow expects x, z, and altitude");
    if (lua_type(L, 1) != LUA_TNUMBER || lua_type(L, 2) != LUA_TNUMBER ||
        lua_type(L, 3) != LUA_TNUMBER || !level) {
        lua_pushnil(L);
        return 1;
    }
    x = (float)lua_tonumber(L, 1);
    z = (float)lua_tonumber(L, 2);
    a = (float)lua_tonumber(L, 3);
    platform = check_platform_below(x, z, a);
    bor_lua_push_entity(L, platform);
    return 1;
}

/* Legacy queued drawing primitives. */
static int lua_openbor_drawline(lua_State *L)
{
    int x1 = (int)bor_lua_check_legacy_integer(L, 1, "drawline");
    int y1 = (int)bor_lua_check_legacy_integer(L, 2, "drawline");
    int x2 = (int)bor_lua_check_legacy_integer(L, 3, "drawline");
    int y2 = (int)bor_lua_check_legacy_integer(L, 4, "drawline");
    int z = (int)bor_lua_check_legacy_integer(L, 5, "drawline");
    int color = (int)bor_lua_check_legacy_integer(L, 6, "drawline");
    int blend_index = -1;

    if (lua_gettop(L) < 6)
        return luaL_error(L, "drawline expects 6 required arguments and 1 optional LUT");
    if (lua_gettop(L) >= 7)
        blend_index = (int)bor_lua_check_legacy_integer(L, 7, "drawline");

    if (blend_index >= 0)
        blend_index %= MAX_BLENDINGS + 1;

    spriteq_add_line(x1, y1, x2, y2, z, color, blend_index);
    return 0;
}

static int lua_openbor_drawdot(lua_State *L)
{
    int x = (int)bor_lua_check_legacy_integer(L, 1, "drawdot");
    int y = (int)bor_lua_check_legacy_integer(L, 2, "drawdot");
    int z = (int)bor_lua_check_legacy_integer(L, 3, "drawdot");
    int color = (int)bor_lua_check_legacy_integer(L, 4, "drawdot");
    int blend_index = -1;

    if (lua_gettop(L) < 4)
        return luaL_error(L, "drawdot expects 4 required arguments and 1 optional LUT");
    if (lua_gettop(L) >= 5)
        blend_index = (int)bor_lua_check_legacy_integer(L, 5, "drawdot");

    if (blend_index >= 0)
        blend_index %= MAX_BLENDINGS + 1;

    spriteq_add_dot(x, y, z, color, blend_index);
    return 0;
}

/* Legacy string and colour helpers. */
static int lua_openbor_strinfirst(lua_State *L)
{
    const char *source = luaL_checkstring(L, 1);
    const char *search = luaL_checkstring(L, 2);
    const char *match = strstr(source, search);

    if (match)
        lua_pushstring(L, match);
    else
        lua_pushinteger(L, -1);
    return 1;
}

static int lua_openbor_strinlast(lua_State *L)
{
    const char *source = luaL_checkstring(L, 1);
    const char *search = luaL_checkstring(L, 2);
    const char *match = NULL;
    const char *cursor;

    /* Legacy strinlast is character-based, using the first character of arg 2. */
    if (*search) {
        cursor = source;
        while ((cursor = strchr(cursor, *search)) != NULL) {
            match = cursor++;
        }
    }

    if (match)
        lua_pushstring(L, match);
    else
        lua_pushinteger(L, -1);
    return 1;
}

static int lua_openbor_strleft(lua_State *L)
{
    const char *source = luaL_checkstring(L, 1);
    lua_Integer count;
    size_t length = strlen(source);

    if (!lua_isinteger(L, 2))
        return luaL_error(L, "strleft requires an integer character count");
    count = lua_tointeger(L, 2);
    if (count < 0)
        return luaL_error(L, "strleft character count cannot be negative");
    if ((lua_Unsigned)count > length)
        count = (lua_Integer)length;
    /* The legacy implementation used a 66-byte temporary buffer. */
    if (count > 65)
        count = 65;

    lua_pushlstring(L, source, (size_t)count);
    return 1;
}

static int lua_openbor_strlength(lua_State *L)
{
    const char *source = luaL_checkstring(L, 1);
    lua_pushinteger(L, (lua_Integer)strlen(source));
    return 1;
}

static int lua_openbor_strright(lua_State *L)
{
    const char *source = luaL_checkstring(L, 1);
    lua_Integer start;
    size_t length = strlen(source);

    if (!lua_isinteger(L, 2))
        return luaL_error(L, "strright requires an integer start position");
    start = lua_tointeger(L, 2);
    if (start < 0)
        return luaL_error(L, "strright start position cannot be negative");
    if ((lua_Unsigned)start > length)
        start = (lua_Integer)length;

    lua_pushstring(L, source + (size_t)start);
    return 1;
}

static int lua_openbor_rgbcolor(lua_State *L)
{
    int red = (int)bor_lua_check_legacy_integer(L, 1, "rgbcolor");
    int green = (int)bor_lua_check_legacy_integer(L, 2, "rgbcolor");
    int blue = (int)bor_lua_check_legacy_integer(L, 3, "rgbcolor");

    if (lua_gettop(L) != 3)
        return luaL_error(L, "rgbcolor expects exactly 3 arguments");

    lua_pushinteger(L, (lua_Integer)_makecolour(red, green, blue));
    return 1;
}

/*
 * Resolve legacy OpenBOR constants by name for Lua scripts.
 * This first pass covers the static constants defined by this engine version;
 * dynamically allocated animation names are handled separately.
 */
/* ASCII case-insensitive comparisons, independent of platform stricmp APIs. */
static int bor_lua_ascii_lower(int ch)
{
    if (ch >= 'A' && ch <= 'Z')
        return ch - 'A' + 'a';
    return ch;
}

static int bor_lua_strnicmp(const char *left, const char *right, size_t count)
{
    size_t i;

    for (i = 0; i < count; ++i) {
        int a = bor_lua_ascii_lower((unsigned char)left[i]);
        int b = bor_lua_ascii_lower((unsigned char)right[i]);

        if (a != b)
            return a - b;
        if (a == '\0')
            return 0;
    }

    return 0;
}

static int bor_lua_stricmp(const char *left, const char *right)
{
    size_t length = strlen(left);
    int result = bor_lua_strnicmp(left, right, length);

    if (result != 0)
        return result;
    return left[length] == '\0' && right[length] == '\0' ? 0 :
           (left[length] == '\0' ? -1 : 1);
}

static int lua_openborconstant(lua_State *L)
{
    const char *name;

    if (lua_gettop(L) < 1)
        return luaL_error(L, "openborconstant expects a constant name");

    /* The legacy function passes integer arguments through unchanged. */
    if (lua_type(L, 1) == LUA_TNUMBER) {
        if (!lua_isinteger(L, 1))
            return luaL_error(L, "openborconstant expects a string or integer");
        lua_pushinteger(L, lua_tointeger(L, 1));
        return 1;
    }

    name = luaL_checkstring(L, 1);

    /* The legacy engine exposed this default as a global integer. */
    if (bor_lua_stricmp(name, "MAX_WALL_HEIGHT") == 0) {
        lua_pushinteger(L, MAX_WALL_HEIGHT);
        return 1;
    }

    /* Built-in sample IDs are fields in the engine's sample table. */
#define BOR_LUA_SAMPLE_CONSTANT(symbol, field) \
    if (bor_lua_stricmp(name, #symbol) == 0) { lua_pushinteger(L, samples.field); return 1; }
    BOR_LUA_SAMPLE_CONSTANT(SAMPLE_GO, go);
    BOR_LUA_SAMPLE_CONSTANT(SAMPLE_BEAT, beat);
    BOR_LUA_SAMPLE_CONSTANT(SAMPLE_BLOCK, block);
    BOR_LUA_SAMPLE_CONSTANT(SAMPLE_INDIRECT, indirect);
    BOR_LUA_SAMPLE_CONSTANT(SAMPLE_GET, get);
    BOR_LUA_SAMPLE_CONSTANT(SAMPLE_GET2, get2);
    BOR_LUA_SAMPLE_CONSTANT(SAMPLE_FALL, fall);
    BOR_LUA_SAMPLE_CONSTANT(SAMPLE_JUMP, jump);
    BOR_LUA_SAMPLE_CONSTANT(SAMPLE_PUNCH, punch);
    BOR_LUA_SAMPLE_CONSTANT(SAMPLE_1UP, oneup);
    BOR_LUA_SAMPLE_CONSTANT(SAMPLE_TIMEOVER, timeover);
    BOR_LUA_SAMPLE_CONSTANT(SAMPLE_BEEP, beep);
    BOR_LUA_SAMPLE_CONSTANT(SAMPLE_BEEP2, beep2);
    BOR_LUA_SAMPLE_CONSTANT(SAMPLE_BIKE, bike);
#undef BOR_LUA_SAMPLE_CONSTANT

    /* Keep this list aligned with the legacy constant map and engine headers. */
#define BOR_LUA_CONSTANT(symbol) \
    if (bor_lua_stricmp(name, #symbol) == 0) { lua_pushinteger(L, (lua_Integer)(symbol)); return 1; }
    BOR_LUA_CONSTANT(PIXEL_8);
    BOR_LUA_CONSTANT(PIXEL_x8);
    BOR_LUA_CONSTANT(PIXEL_16);
    BOR_LUA_CONSTANT(PIXEL_32);
    BOR_LUA_CONSTANT(COMPATIBLEVERSION);
    BOR_LUA_CONSTANT(CV_SAVED_GAME);
    BOR_LUA_CONSTANT(CV_HIGH_SCORE);
    BOR_LUA_CONSTANT(THINK_SPEED);
    BOR_LUA_CONSTANT(COUNTER_SPEED);
    BOR_LUA_CONSTANT(MAX_ENTS);
    BOR_LUA_CONSTANT(MAX_PANELS);
    BOR_LUA_CONSTANT(MAX_WEAPONS);
    BOR_LUA_CONSTANT(MAX_COLOUR_MAPS);
    BOR_LUA_CONSTANT(MAX_NAME_LEN);
    BOR_LUA_CONSTANT(LEVEL_MAX_SPAWNS);
    BOR_LUA_CONSTANT(LEVEL_MAX_PANELS);
    BOR_LUA_CONSTANT(LEVEL_MAX_HOLES);
    BOR_LUA_CONSTANT(LEVEL_MAX_WALLS);
    BOR_LUA_CONSTANT(MAX_LEVELS);
    BOR_LUA_CONSTANT(MAX_DIFFICULTIES);
    BOR_LUA_CONSTANT(MAX_SPECIALS);
    BOR_LUA_CONSTANT(MAX_ATCHAIN);
    BOR_LUA_CONSTANT(MAX_ATTACKS);
    BOR_LUA_CONSTANT(MAX_FOLLOWS);
    BOR_LUA_CONSTANT(MAX_PLAYERS);
    BOR_LUA_CONSTANT(MAX_ARG_LEN);
    BOR_LUA_CONSTANT(MAX_ANIS);
    BOR_LUA_CONSTANT(FLAG_ESC);
    BOR_LUA_CONSTANT(FLAG_START);
    BOR_LUA_CONSTANT(FLAG_MOVELEFT);
    BOR_LUA_CONSTANT(FLAG_MOVERIGHT);
    BOR_LUA_CONSTANT(FLAG_MOVEUP);
    BOR_LUA_CONSTANT(FLAG_MOVEDOWN);
    BOR_LUA_CONSTANT(FLAG_ATTACK);
    BOR_LUA_CONSTANT(FLAG_ATTACK2);
    BOR_LUA_CONSTANT(FLAG_ATTACK3);
    BOR_LUA_CONSTANT(FLAG_ATTACK4);
    BOR_LUA_CONSTANT(FLAG_JUMP);
    BOR_LUA_CONSTANT(FLAG_SPECIAL);
    BOR_LUA_CONSTANT(FLAG_SCREENSHOT);
    BOR_LUA_CONSTANT(FLAG_ANYBUTTON);
    BOR_LUA_CONSTANT(FLAG_FORWARD);
    BOR_LUA_CONSTANT(FLAG_BACKWARD);
    BOR_LUA_CONSTANT(SDID_MOVEUP);
    BOR_LUA_CONSTANT(SDID_MOVEDOWN);
    BOR_LUA_CONSTANT(SDID_MOVELEFT);
    BOR_LUA_CONSTANT(SDID_MOVERIGHT);
    BOR_LUA_CONSTANT(SDID_SPECIAL);
    BOR_LUA_CONSTANT(SDID_ATTACK);
    BOR_LUA_CONSTANT(SDID_ATTACK2);
    BOR_LUA_CONSTANT(SDID_ATTACK3);
    BOR_LUA_CONSTANT(SDID_ATTACK4);
    BOR_LUA_CONSTANT(SDID_JUMP);
    BOR_LUA_CONSTANT(SDID_START);
    BOR_LUA_CONSTANT(SDID_SCREENSHOT);
    BOR_LUA_CONSTANT(TYPE_NONE);
    BOR_LUA_CONSTANT(TYPE_PLAYER);
    BOR_LUA_CONSTANT(TYPE_ENEMY);
    BOR_LUA_CONSTANT(TYPE_ITEM);
    BOR_LUA_CONSTANT(TYPE_OBSTACLE);
    BOR_LUA_CONSTANT(TYPE_STEAMER);
    BOR_LUA_CONSTANT(TYPE_SHOT);
    BOR_LUA_CONSTANT(TYPE_TRAP);
    BOR_LUA_CONSTANT(TYPE_TEXTBOX);
    BOR_LUA_CONSTANT(TYPE_ENDLEVEL);
    BOR_LUA_CONSTANT(TYPE_NPC);
    BOR_LUA_CONSTANT(TYPE_PANEL);
    BOR_LUA_CONSTANT(SUBTYPE_NONE);
    BOR_LUA_CONSTANT(SUBTYPE_BIKER);
    BOR_LUA_CONSTANT(SUBTYPE_NOTGRAB);
    BOR_LUA_CONSTANT(SUBTYPE_ARROW);
    BOR_LUA_CONSTANT(SUBTYPE_TOUCH);
    BOR_LUA_CONSTANT(SUBTYPE_WEAPON);
    BOR_LUA_CONSTANT(SUBTYPE_NOSKIP);
    BOR_LUA_CONSTANT(SUBTYPE_FLYDIE);
    BOR_LUA_CONSTANT(SUBTYPE_BOTH);
    BOR_LUA_CONSTANT(SUBTYPE_PROJECTILE);
    BOR_LUA_CONSTANT(SUBTYPE_FOLLOW);
    BOR_LUA_CONSTANT(SUBTYPE_CHASE);
    BOR_LUA_CONSTANT(AIMOVE1_NORMAL);
    BOR_LUA_CONSTANT(AIMOVE1_CHASE);
    BOR_LUA_CONSTANT(AIMOVE1_CHASEZ);
    BOR_LUA_CONSTANT(AIMOVE1_CHASEX);
    BOR_LUA_CONSTANT(AIMOVE1_AVOID);
    BOR_LUA_CONSTANT(AIMOVE1_AVOIDZ);
    BOR_LUA_CONSTANT(AIMOVE1_AVOIDX);
    BOR_LUA_CONSTANT(AIMOVE1_WANDER);
    BOR_LUA_CONSTANT(AIMOVE1_NOMOVE);
    BOR_LUA_CONSTANT(AIMOVE1_BIKER);
    BOR_LUA_CONSTANT(AIMOVE1_STAR);
    BOR_LUA_CONSTANT(AIMOVE1_ARROW);
    BOR_LUA_CONSTANT(AIMOVE1_BOMB);
    BOR_LUA_CONSTANT(AIMOVE2_NORMAL);
    BOR_LUA_CONSTANT(AIMOVE2_IGNOREHOLES);
    BOR_LUA_CONSTANT(AIATTACK1_NORMAL);
    BOR_LUA_CONSTANT(AIATTACK1_LONG);
    BOR_LUA_CONSTANT(AIATTACK1_MELEE);
    BOR_LUA_CONSTANT(AIATTACK1_NOATTACK);
    BOR_LUA_CONSTANT(AIATTACK2_NORMAL);
    BOR_LUA_CONSTANT(AIATTACK2_DODGE);
    BOR_LUA_CONSTANT(AIATTACK2_DODGEMOVE);
    BOR_LUA_CONSTANT(FRONTPANEL_Z);
    BOR_LUA_CONSTANT(HOLE_Z);
    BOR_LUA_CONSTANT(NEONPANEL_Z);
    BOR_LUA_CONSTANT(SHADOW_Z);
    BOR_LUA_CONSTANT(SCREENPANEL_Z);
    BOR_LUA_CONSTANT(PANEL_Z);
    BOR_LUA_CONSTANT(MIRROR_Z);
    BOR_LUA_CONSTANT(PIT_DEPTH);
    BOR_LUA_CONSTANT(P2_STATS_DIST);
    BOR_LUA_CONSTANT(CONTACT_DIST_H);
    BOR_LUA_CONSTANT(CONTACT_DIST_V);
    BOR_LUA_CONSTANT(GRAB_DIST);
    BOR_LUA_CONSTANT(GRAB_STALL);
    BOR_LUA_CONSTANT(ATK_NORMAL);
    BOR_LUA_CONSTANT(ATK_NORMAL2);
    BOR_LUA_CONSTANT(ATK_NORMAL3);
    BOR_LUA_CONSTANT(ATK_NORMAL4);
    BOR_LUA_CONSTANT(ATK_BLAST);
    BOR_LUA_CONSTANT(ATK_BURN);
    BOR_LUA_CONSTANT(ATK_FREEZE);
    BOR_LUA_CONSTANT(ATK_SHOCK);
    BOR_LUA_CONSTANT(ATK_STEAL);
    BOR_LUA_CONSTANT(ATK_NORMAL5);
    BOR_LUA_CONSTANT(ATK_NORMAL6);
    BOR_LUA_CONSTANT(ATK_NORMAL7);
    BOR_LUA_CONSTANT(ATK_NORMAL8);
    BOR_LUA_CONSTANT(ATK_NORMAL9);
    BOR_LUA_CONSTANT(ATK_NORMAL10);
    BOR_LUA_CONSTANT(ATK_ITEM);
    BOR_LUA_CONSTANT(SCROLL_RIGHT);
    BOR_LUA_CONSTANT(SCROLL_DOWN);
    BOR_LUA_CONSTANT(SCROLL_LEFT);
    BOR_LUA_CONSTANT(SCROLL_UP);
    BOR_LUA_CONSTANT(SCROLL_BOTH);
    BOR_LUA_CONSTANT(SCROLL_LEFTRIGHT);
    BOR_LUA_CONSTANT(SCROLL_RIGHTLEFT);
    BOR_LUA_CONSTANT(SCROLL_INWARD);
    BOR_LUA_CONSTANT(SCROLL_OUTWARD);
    BOR_LUA_CONSTANT(SCROLL_INOUT);
    BOR_LUA_CONSTANT(SCROLL_OUTIN);
    BOR_LUA_CONSTANT(SCROLL_UPWARD);
    BOR_LUA_CONSTANT(SCROLL_DOWNWARD);
    BOR_LUA_CONSTANT(ANI_IDLE);
    BOR_LUA_CONSTANT(ANI_WALK);
    BOR_LUA_CONSTANT(ANI_JUMP);
    BOR_LUA_CONSTANT(ANI_LAND);
    BOR_LUA_CONSTANT(ANI_PAIN);
    BOR_LUA_CONSTANT(ANI_FALL);
    BOR_LUA_CONSTANT(ANI_RISE);
    BOR_LUA_CONSTANT(ANI_UPPER);
    BOR_LUA_CONSTANT(ANI_BLOCK);
    BOR_LUA_CONSTANT(ANI_JUMPATTACK);
    BOR_LUA_CONSTANT(ANI_JUMPATTACK2);
    BOR_LUA_CONSTANT(ANI_GET);
    BOR_LUA_CONSTANT(ANI_GRAB);
    BOR_LUA_CONSTANT(ANI_GRABATTACK);
    BOR_LUA_CONSTANT(ANI_GRABATTACK2);
    BOR_LUA_CONSTANT(ANI_THROW);
    BOR_LUA_CONSTANT(ANI_SPECIAL);
    BOR_LUA_CONSTANT(ANI_SPAWN);
    BOR_LUA_CONSTANT(ANI_DIE);
    BOR_LUA_CONSTANT(ANI_PICK);
    BOR_LUA_CONSTANT(ANI_JUMPATTACK3);
    BOR_LUA_CONSTANT(ANI_UP);
    BOR_LUA_CONSTANT(ANI_DOWN);
    BOR_LUA_CONSTANT(ANI_SHOCK);
    BOR_LUA_CONSTANT(ANI_BURN);
    BOR_LUA_CONSTANT(ANI_SHOCKPAIN);
    BOR_LUA_CONSTANT(ANI_BURNPAIN);
    BOR_LUA_CONSTANT(ANI_GRABBED);
    BOR_LUA_CONSTANT(ANI_SPECIAL2);
    BOR_LUA_CONSTANT(ANI_RUN);
    BOR_LUA_CONSTANT(ANI_RUNATTACK);
    BOR_LUA_CONSTANT(ANI_RUNJUMPATTACK);
    BOR_LUA_CONSTANT(ANI_ATTACKUP);
    BOR_LUA_CONSTANT(ANI_ATTACKDOWN);
    BOR_LUA_CONSTANT(ANI_ATTACKFORWARD);
    BOR_LUA_CONSTANT(ANI_ATTACKBACKWARD);
    BOR_LUA_CONSTANT(ANI_RISEATTACK);
    BOR_LUA_CONSTANT(ANI_DODGE);
    BOR_LUA_CONSTANT(ANI_ATTACKBOTH);
    BOR_LUA_CONSTANT(ANI_GRABFORWARD);
    BOR_LUA_CONSTANT(ANI_GRABFORWARD2);
    BOR_LUA_CONSTANT(ANI_JUMPFORWARD);
    BOR_LUA_CONSTANT(ANI_GRABDOWN);
    BOR_LUA_CONSTANT(ANI_GRABDOWN2);
    BOR_LUA_CONSTANT(ANI_GRABUP);
    BOR_LUA_CONSTANT(ANI_GRABUP2);
    BOR_LUA_CONSTANT(ANI_SELECT);
    BOR_LUA_CONSTANT(ANI_DUCK);
    BOR_LUA_CONSTANT(ANI_FAINT);
    BOR_LUA_CONSTANT(ANI_CANT);
    BOR_LUA_CONSTANT(ANI_THROWATTACK);
    BOR_LUA_CONSTANT(ANI_CHARGEATTACK);
    BOR_LUA_CONSTANT(ANI_VAULT);
    BOR_LUA_CONSTANT(ANI_JUMPCANT);
    BOR_LUA_CONSTANT(ANI_JUMPSPECIAL);
    BOR_LUA_CONSTANT(ANI_BURNDIE);
    BOR_LUA_CONSTANT(ANI_SHOCKDIE);
    BOR_LUA_CONSTANT(ANI_PAIN2);
    BOR_LUA_CONSTANT(ANI_PAIN3);
    BOR_LUA_CONSTANT(ANI_PAIN4);
    BOR_LUA_CONSTANT(ANI_FALL2);
    BOR_LUA_CONSTANT(ANI_FALL3);
    BOR_LUA_CONSTANT(ANI_FALL4);
    BOR_LUA_CONSTANT(ANI_DIE2);
    BOR_LUA_CONSTANT(ANI_DIE3);
    BOR_LUA_CONSTANT(ANI_DIE4);
    BOR_LUA_CONSTANT(ANI_CHARGE);
    BOR_LUA_CONSTANT(ANI_BACKWALK);
    BOR_LUA_CONSTANT(ANI_SLEEP);
    BOR_LUA_CONSTANT(ANI_PAIN5);
    BOR_LUA_CONSTANT(ANI_PAIN6);
    BOR_LUA_CONSTANT(ANI_PAIN7);
    BOR_LUA_CONSTANT(ANI_PAIN8);
    BOR_LUA_CONSTANT(ANI_PAIN9);
    BOR_LUA_CONSTANT(ANI_PAIN10);
    BOR_LUA_CONSTANT(ANI_FALL5);
    BOR_LUA_CONSTANT(ANI_FALL6);
    BOR_LUA_CONSTANT(ANI_FALL7);
    BOR_LUA_CONSTANT(ANI_FALL8);
    BOR_LUA_CONSTANT(ANI_FALL9);
    BOR_LUA_CONSTANT(ANI_FALL10);
    BOR_LUA_CONSTANT(ANI_DIE5);
    BOR_LUA_CONSTANT(ANI_DIE6);
    BOR_LUA_CONSTANT(ANI_DIE7);
    BOR_LUA_CONSTANT(ANI_DIE8);
    BOR_LUA_CONSTANT(ANI_DIE9);
    BOR_LUA_CONSTANT(ANI_DIE10);
    BOR_LUA_CONSTANT(ANI_TURN);
    BOR_LUA_CONSTANT(ANI_RESPAWN);
    BOR_LUA_CONSTANT(ANI_FORWARDJUMP);
    BOR_LUA_CONSTANT(ANI_RUNJUMP);
    BOR_LUA_CONSTANT(ANI_JUMPLAND);
    BOR_LUA_CONSTANT(ANI_JUMPDELAY);
    BOR_LUA_CONSTANT(ANI_HITWALL);
    BOR_LUA_CONSTANT(ANI_GRABBACKWARD);
    BOR_LUA_CONSTANT(ANI_GRABBACKWARD2);
    BOR_LUA_CONSTANT(ANI_GRABWALK);
    BOR_LUA_CONSTANT(ANI_GRABBEDWALK);
    BOR_LUA_CONSTANT(ANI_GRABWALKUP);
    BOR_LUA_CONSTANT(ANI_GRABBEDWALKUP);
    BOR_LUA_CONSTANT(ANI_GRABWALKDOWN);
    BOR_LUA_CONSTANT(ANI_GRABBEDWALKDOWN);
    BOR_LUA_CONSTANT(ANI_GRABTURN);
    BOR_LUA_CONSTANT(ANI_GRABBEDTURN);
    BOR_LUA_CONSTANT(ANI_GRABBACKWALK);
    BOR_LUA_CONSTANT(ANI_GRABBEDBACKWALK);
    BOR_LUA_CONSTANT(ANI_SLIDE);
    BOR_LUA_CONSTANT(ANI_RUNSLIDE);
    BOR_LUA_CONSTANT(ANI_BLOCKPAIN);
    BOR_LUA_CONSTANT(ANI_DUCKATTACK);
    BOR_LUA_CONSTANT(PLAYER_MIN_Z);
    BOR_LUA_CONSTANT(PLAYER_MAX_Z);
    BOR_LUA_CONSTANT(BGHEIGHT);
    BOR_LUA_CONSTANT(ANI_RISE2);
    BOR_LUA_CONSTANT(ANI_RISE3);
    BOR_LUA_CONSTANT(ANI_RISE4);
    BOR_LUA_CONSTANT(ANI_RISE5);
    BOR_LUA_CONSTANT(ANI_RISE6);
    BOR_LUA_CONSTANT(ANI_RISE7);
    BOR_LUA_CONSTANT(ANI_RISE8);
    BOR_LUA_CONSTANT(ANI_RISE9);
    BOR_LUA_CONSTANT(ANI_RISE10);
    BOR_LUA_CONSTANT(ANI_RISEB);
    BOR_LUA_CONSTANT(ANI_RISES);
    BOR_LUA_CONSTANT(ANI_BLOCKPAIN2);
    BOR_LUA_CONSTANT(ANI_BLOCKPAIN3);
    BOR_LUA_CONSTANT(ANI_BLOCKPAIN4);
    BOR_LUA_CONSTANT(ANI_BLOCKPAIN5);
    BOR_LUA_CONSTANT(ANI_BLOCKPAIN6);
    BOR_LUA_CONSTANT(ANI_BLOCKPAIN7);
    BOR_LUA_CONSTANT(ANI_BLOCKPAIN8);
    BOR_LUA_CONSTANT(ANI_BLOCKPAIN9);
    BOR_LUA_CONSTANT(ANI_BLOCKPAIN10);
    BOR_LUA_CONSTANT(ANI_BLOCKPAINB);
    BOR_LUA_CONSTANT(ANI_BLOCKPAINS);
    BOR_LUA_CONSTANT(ANI_CHIPDEATH);
    BOR_LUA_CONSTANT(ANI_GUARDBREAK);
    BOR_LUA_CONSTANT(ANI_RISEATTACK2);
    BOR_LUA_CONSTANT(ANI_RISEATTACK3);
    BOR_LUA_CONSTANT(ANI_RISEATTACK4);
    BOR_LUA_CONSTANT(ANI_RISEATTACK5);
    BOR_LUA_CONSTANT(ANI_RISEATTACK6);
    BOR_LUA_CONSTANT(ANI_RISEATTACK7);
    BOR_LUA_CONSTANT(ANI_RISEATTACK8);
    BOR_LUA_CONSTANT(ANI_RISEATTACK9);
    BOR_LUA_CONSTANT(ANI_RISEATTACK10);
    BOR_LUA_CONSTANT(ANI_RISEATTACKB);
    BOR_LUA_CONSTANT(ANI_RISEATTACKS);
#undef BOR_LUA_CONSTANT

    /*
     * Legacy scripts also use numbered animation constants such as
     * ANI_ATTACK1 and ANI_FREESPECIAL2. These map through the engine's
     * dynamic animation-index tables rather than directly to enum values.
     */
    {
        struct bor_lua_dynamic_constant {
            const char *prefix;
            const int *values;
            int max_count;
        } dynamic_constants[] = {
            {"ANI_DOWN", dyn_anims.animdowns ? dyn_anims.animdowns : default_dyn_anims.animdowns, MAX_DOWNS},
            {"ANI_UP", dyn_anims.animups ? dyn_anims.animups : default_dyn_anims.animups, MAX_UPS},
            {"ANI_BACKWALK", dyn_anims.animbackwalks ? dyn_anims.animbackwalks : default_dyn_anims.animbackwalks, MAX_BACKWALKS},
            {"ANI_WALK", dyn_anims.animwalks ? dyn_anims.animwalks : default_dyn_anims.animwalks, MAX_WALKS},
            {"ANI_IDLE", dyn_anims.animidles ? dyn_anims.animidles : default_dyn_anims.animidles, MAX_IDLES},
            {"ANI_FALL", dyn_anims.animfalls ? dyn_anims.animfalls : default_dyn_anims.animfalls, MAX_ATKS},
            {"ANI_RISE", dyn_anims.animrises ? dyn_anims.animrises : default_dyn_anims.animrises, MAX_ATKS},
            {"ANI_RISEATTACK", dyn_anims.animriseattacks ? dyn_anims.animriseattacks : default_dyn_anims.animriseattacks, MAX_ATKS},
            {"ANI_PAIN", dyn_anims.animpains ? dyn_anims.animpains : default_dyn_anims.animpains, MAX_ATKS},
            {"ANI_DIE", dyn_anims.animdies ? dyn_anims.animdies : default_dyn_anims.animdies, MAX_ATKS},
            {"ANI_ATTACK", dyn_anims.animattacks ? dyn_anims.animattacks : default_dyn_anims.animattacks, dyn_anims.animattacks ? MAX_ATTACKS : 4},
            {"ANI_FOLLOW", dyn_anims.animfollows ? dyn_anims.animfollows : default_dyn_anims.animfollows, dyn_anims.animfollows ? MAX_FOLLOWS : 4},
            {"ANI_FREESPECIAL", dyn_anims.animspecials ? dyn_anims.animspecials : default_dyn_anims.animspecials, dyn_anims.animspecials ? MAX_SPECIALS : 8}
        };
        size_t i;

        for (i = 0; i < sizeof(dynamic_constants) / sizeof(dynamic_constants[0]); ++i) {
            size_t prefix_length = strlen(dynamic_constants[i].prefix);
            char *end = NULL;
            long animation_index;

            if (bor_lua_strnicmp(name, dynamic_constants[i].prefix, prefix_length) != 0)
                continue;

            /*
             * The legacy lookup requires a numeric suffix for dynamic
             * animation names, including ANI_ATTACK1 and ANI_FREESPECIAL1.
             */
            if (name[prefix_length] < '0' || name[prefix_length] > '9')
                continue;

            animation_index = strtol(name + prefix_length, &end, 10);
            if (end == name + prefix_length || *end != '\0')
                continue;
            if (animation_index < 1 ||
                animation_index > dynamic_constants[i].max_count) {
                continue;
            }

            /*
             * The legacy implementation offsets numbered RISE, RISEATTACK,
             * PAIN, and DIE constants into the upper attack-animation slots.
             */
            if (i >= 6 && i <= 9) {
                long first_slot = MAX_ATKS - STA_ATKS + 1;
                if (animation_index < first_slot)
                    animation_index = first_slot;
            }

            if (!dynamic_constants[i].values) {
                lua_pushnil(L);
                return 1;
            }

            if (animation_index > dynamic_constants[i].max_count) {
                lua_pushnil(L);
                return 1;
            }

            lua_pushinteger(L, (lua_Integer)dynamic_constants[i].values[animation_index - 1]);
            return 1;
        }
    }

    /* The legacy implementation clears the result when no constant matches. */
    lua_pushnil(L);
    return 1;
}

// getlevelproperty(name)
// Preserve the legacy getter's case-insensitive property lookup and integer results.
static int lua_openbor_getlevelproperty(lua_State *L)
{
    const char *name;

    if (lua_type(L, 1) != LUA_TSTRING)
        return luaL_error(L, "getlevelproperty expects a property name string");
    name = lua_tostring(L, 1);
    if (!level)
        return luaL_error(L, "getlevelproperty requires a loaded level");

    if (!stricmp(name, "bgspeed"))
        lua_pushinteger(L, (lua_Integer)level->bgspeed);
    else if (!stricmp(name, "cameraxoffset"))
        lua_pushinteger(L, level->cameraxoffset);
    else if (!stricmp(name, "camerazoffset"))
        lua_pushinteger(L, level->camerazoffset);
    else
        return luaL_error(L, "getlevelproperty: unsupported property '%s'", name);

    return 1;
}

// changelevelproperty(name, value)
// Preserve the legacy setter's case-sensitive names and return behavior.
static int lua_openbor_changelevelproperty(lua_State *L)
{
    const char *name;
    int value;

    if (lua_gettop(L) < 2 || lua_type(L, 1) != LUA_TSTRING)
        return luaL_error(L, "changelevelproperty expects a property name and value");
    name = lua_tostring(L, 1);
    if (!level) {
        lua_pushinteger(L, 0);
        return 1;
    }

    /* Legacy reports success for unknown names; only known names mutate state. */
    if (!strcmp(name, "rock") || !strcmp(name, "bgspeed") ||
        !strcmp(name, "cameraxoffset") || !strcmp(name, "camerazoffset")) {
        if (lua_type(L, 2) != LUA_TNUMBER) {
            lua_pushinteger(L, 0);
            return 1;
        }
        value = (int)lua_tonumber(L, 2);
        if (!strcmp(name, "rock"))
            level->rocking = value;
        else if (!strcmp(name, "bgspeed"))
            level->bgspeed = (float)value;
        else if (!strcmp(name, "cameraxoffset"))
            level->cameraxoffset = value;
        else
            level->camerazoffset = value;
    }

    lua_pushinteger(L, 1);
    return 1;
}

// playerkeys(playerindex, newkey?, key1, key2, ...)
// Preserve legacy button names, key-state selection, and bitmask return values.
static int lua_openbor_playerkeys(lua_State *L) {
    static const struct {
        const char *name;
        u32 flag;
    } buttons[] = {
        {"anybutton", FLAG_ANYBUTTON},
        {"attack", FLAG_ATTACK},
        {"attack2", FLAG_ATTACK2},
        {"attack3", FLAG_ATTACK3},
        {"attack4", FLAG_ATTACK4},
        {"esc", FLAG_ESC},
        {"jump", FLAG_JUMP},
        {"movedown", FLAG_MOVEDOWN},
        {"moveleft", FLAG_MOVELEFT},
        {"moveright", FLAG_MOVERIGHT},
        {"moveup", FLAG_MOVEUP},
        {"screenshot", FLAG_SCREENSHOT},
        {"special", FLAG_SPECIAL},
        {"start", FLAG_START}
    };
    int top = lua_gettop(L);
    int index = 0;
    int newkey = 0;
    int i;
    u32 keys;

    if (top < 3)
        return luaL_error(L, "playerkeys expects a player index, key state, and at least one button");

    if (lua_type(L, 1) == LUA_TNUMBER)
        index = (int)lua_tonumber(L, 1);
    if (lua_type(L, 2) == LUA_TNUMBER)
        newkey = (int)lua_tonumber(L, 2);

    /* The legacy code did not validate the player index; guard it in Lua. */
    if (index < 0 || index >= MAX_PLAYERS) {
        lua_pushinteger(L, 0);
        return 1;
    }

    if (newkey == 1)
        keys = player[index].newkeys;
    else if (newkey == 2)
        keys = player[index].releasekeys;
    else
        keys = player[index].keys;

    for (i = 3; i <= top; i++) {
        lua_Integer button = -1;
        u32 flag = 0;
        int j;

        if (lua_isinteger(L, i)) {
            button = lua_tointeger(L, i);
        } else if (lua_type(L, i) == LUA_TSTRING) {
            const char *name = lua_tostring(L, i);
            for (j = 0; j < (int)(sizeof(buttons) / sizeof(buttons[0])); j++) {
                if (!stricmp(name, buttons[j].name)) {
                    flag = buttons[j].flag;
                    button = j;
                    break;
                }
            }
        }

        if (button >= 0 && button < (lua_Integer)(sizeof(buttons) / sizeof(buttons[0]))) {
            if (!flag)
                flag = buttons[button].flag;
            lua_pushinteger(L, (lua_Integer)(keys & flag));
        } else {
            lua_pushinteger(L, 0);
        }

        if (lua_tointeger(L, -1) == 0)
            return 1;
        /* Keep the last checked button's bitmask, as the legacy API does. */
        if (i < top)
            lua_remove(L, -1);
    }

    return 1;
}

// Global table registration index called during startup()
void openbor_register_lua_api(void) {
    if (!g_lua_engine_state) return;

    /* Register before any entity handles are created. */
    bor_lua_drawmethod = plainmethod;

    luaL_newmetatable(g_lua_engine_state, BOR_LUA_SPRITE_METATABLE);
    lua_pushcfunction(g_lua_engine_state, bor_lua_sprite_gc);
    lua_setfield(g_lua_engine_state, -2, "__gc");
    lua_pop(g_lua_engine_state, 1);

    luaL_newmetatable(g_lua_engine_state, BOR_LUA_SCREEN_METATABLE);
    lua_pushcfunction(g_lua_engine_state, bor_lua_screen_gc);
    lua_setfield(g_lua_engine_state, -2, "__gc");
    lua_pop(g_lua_engine_state, 1);


    luaL_newmetatable(g_lua_engine_state, BOR_LUA_ENTITY_METATABLE);
    lua_pushcfunction(g_lua_engine_state, lua_openbor_entity_index);
    lua_setfield(g_lua_engine_state, -2, "__index");
    lua_pushcfunction(g_lua_engine_state, lua_openbor_entity_newindex);
    lua_setfield(g_lua_engine_state, -2, "__newindex");
    lua_pop(g_lua_engine_state, 1);

    /* Weak-value cache: stable wrapper identity without retaining dead wrappers. */
    lua_newtable(g_lua_engine_state);
    lua_newtable(g_lua_engine_state);
    lua_pushstring(g_lua_engine_state, "v");
    lua_setfield(g_lua_engine_state, -2, "__mode");
    lua_setmetatable(g_lua_engine_state, -2);
    lua_setfield(g_lua_engine_state, LUA_REGISTRYINDEX, "OpenBOR.EntityCache");

    lua_newtable(g_lua_engine_state);
    lua_setfield(g_lua_engine_state, LUA_REGISTRYINDEX, BOR_LUA_RESOURCES);

    lua_newtable(g_lua_engine_state);
    lua_setfield(g_lua_engine_state, LUA_REGISTRYINDEX, BOR_LUA_ENTITY_VARS);

    /* Script variables are isolated by entity and calling Lua source file. */
    lua_newtable(g_lua_engine_state);
    lua_setfield(g_lua_engine_state, LUA_REGISTRYINDEX, BOR_LUA_SCRIPT_VARS);

    lua_newtable(g_lua_engine_state);
    lua_setfield(g_lua_engine_state, LUA_REGISTRYINDEX, BOR_LUA_SCRIPT_LOCAL_VARS);

    /* Indexed variables use a Lua table with legacy zero-based indices. */
    lua_newtable(g_lua_engine_state);
    lua_setfield(g_lua_engine_state, LUA_REGISTRYINDEX, BOR_LUA_INDEXED_VARS);

    /* Strong-value storage for legacy global variables. */
    lua_newtable(g_lua_engine_state);
    lua_setfield(g_lua_engine_state, LUA_REGISTRYINDEX, BOR_LUA_GLOBAL_VARS);

    /*
     * Register the API as global Lua functions. Scripts should call
     * spawn(...), get_self(), log(), etc., without an openbor. prefix.
     */
    lua_pushcfunction(g_lua_engine_state, lua_openbor_isempty);
    lua_setglobal(g_lua_engine_state, "isempty");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_null);
    lua_setglobal(g_lua_engine_state, "NULL");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_maxglobalvarindex);
    lua_setglobal(g_lua_engine_state, "maxglobalvarindex");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_setlocalvar);
    lua_setglobal(g_lua_engine_state, "setlocalvar");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_clearlocalvar);
    lua_setglobal(g_lua_engine_state, "clearlocalvar");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_clearglobalvar);
    lua_setglobal(g_lua_engine_state, "clearglobalvar");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_drawline);
    lua_setglobal(g_lua_engine_state, "drawline");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_drawdot);
    lua_setglobal(g_lua_engine_state, "drawdot");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_strinfirst);
    lua_setglobal(g_lua_engine_state, "strinfirst");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_strinlast);
    lua_setglobal(g_lua_engine_state, "strinlast");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_strleft);
    lua_setglobal(g_lua_engine_state, "strleft");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_strlength);
    lua_setglobal(g_lua_engine_state, "strlength");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_strright);
    lua_setglobal(g_lua_engine_state, "strright");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_rgbcolor);
    lua_setglobal(g_lua_engine_state, "rgbcolor");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_free);
    lua_setglobal(g_lua_engine_state, "free");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_fadeout);
    lua_setglobal(g_lua_engine_state, "fadeout");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_playmusic);
    lua_setglobal(g_lua_engine_state, "playmusic");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_fademusic);
    lua_setglobal(g_lua_engine_state, "fademusic");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_setmusicvolume);
    lua_setglobal(g_lua_engine_state, "setmusicvolume");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_setmusictempo);
    lua_setglobal(g_lua_engine_state, "setmusictempo");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_pausemusic);
    lua_setglobal(g_lua_engine_state, "pausemusic");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_loadsample);
    lua_setglobal(g_lua_engine_state, "loadsample");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_playsample);
    lua_setglobal(g_lua_engine_state, "playsample");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_unloadsample);
    lua_setglobal(g_lua_engine_state, "unloadsample");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_changelight);
    lua_setglobal(g_lua_engine_state, "changelight");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_changeshadowcolor);
    lua_setglobal(g_lua_engine_state, "changeshadowcolor");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_changepalette);
    lua_setglobal(g_lua_engine_state, "changepalette");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_jumptobranch);
    lua_setglobal(g_lua_engine_state, "jumptobranch");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_bindentity);
    lua_setglobal(g_lua_engine_state, "bindentity");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_performattack);
    lua_setglobal(g_lua_engine_state, "performattack");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_setidle);
    lua_setglobal(g_lua_engine_state, "setidle");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_updateframe);
    lua_setglobal(g_lua_engine_state, "updateframe");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_gettextobjproperty);
    lua_setglobal(g_lua_engine_state, "gettextobjproperty");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_changetextobjproperty);
    lua_setglobal(g_lua_engine_state, "changetextobjproperty");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_settextobj);
    lua_setglobal(g_lua_engine_state, "settextobj");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_cleartextobj);
    lua_setglobal(g_lua_engine_state, "cleartextobj");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_getbglayerproperty);
    lua_setglobal(g_lua_engine_state, "getbglayerproperty");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_changebglayerproperty);
    lua_setglobal(g_lua_engine_state, "changebglayerproperty");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_getfglayerproperty);
    lua_setglobal(g_lua_engine_state, "getfglayerproperty");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_changefglayerproperty);
    lua_setglobal(g_lua_engine_state, "changefglayerproperty");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_changelayerproperty);
    lua_setglobal(g_lua_engine_state, "changelayerproperty");

    memset(&bor_lua_spawnentry, 0, sizeof(bor_lua_spawnentry));
    bor_lua_spawnentry.index = -1;
    bor_lua_spawnentry.itemindex = -1;
    bor_lua_spawnentry.weaponindex = -1;

    lua_pushcfunction(g_lua_engine_state, lua_openbor_clearspawnentry);
    lua_setglobal(g_lua_engine_state, "clearspawnentry");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_setspawnentry);
    lua_setglobal(g_lua_engine_state, "setspawnentry");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_projectile);
    lua_setglobal(g_lua_engine_state, "projectile");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_allocscreen);
    lua_setglobal(g_lua_engine_state, "allocscreen");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_clearscreen);
    lua_setglobal(g_lua_engine_state, "clearscreen");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_drawstringtoscreen);
    lua_setglobal(g_lua_engine_state, "drawstringtoscreen");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_drawboxtoscreen);
    lua_setglobal(g_lua_engine_state, "drawboxtoscreen");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_drawlinetoscreen);
    lua_setglobal(g_lua_engine_state, "drawlinetoscreen");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_drawdottoscreen);
    lua_setglobal(g_lua_engine_state, "drawdottoscreen");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_drawscreen);
    lua_setglobal(g_lua_engine_state, "drawscreen");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_drawsprite);
    lua_setglobal(g_lua_engine_state, "drawsprite");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_drawspritetoscreen);
    lua_setglobal(g_lua_engine_state, "drawspritetoscreen");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_setdrawmethod);
    lua_setglobal(g_lua_engine_state, "setdrawmethod");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_loadmodel);
    lua_setglobal(g_lua_engine_state, "loadmodel");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_loadsprite);
    lua_setglobal(g_lua_engine_state, "loadsprite");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_playgif);
    lua_setglobal(g_lua_engine_state, "playgif");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_getmodelproperty);
    lua_setglobal(g_lua_engine_state, "getmodelproperty");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_changemodelproperty);
    lua_setglobal(g_lua_engine_state, "changemodelproperty");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_getplayerproperty);
    lua_setglobal(g_lua_engine_state, "getplayerproperty");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_changeplayerproperty);
    lua_setglobal(g_lua_engine_state, "changeplayerproperty");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_tossentity);
    lua_setglobal(g_lua_engine_state, "tossentity");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_findtarget);
    lua_setglobal(g_lua_engine_state, "findtarget");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_checkrange);
    lua_setglobal(g_lua_engine_state, "checkrange");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_checkhole);
    lua_setglobal(g_lua_engine_state, "checkhole");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_checkwall);
    lua_setglobal(g_lua_engine_state, "checkwall");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_checkplatformbelow);
    lua_setglobal(g_lua_engine_state, "checkplatformbelow");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_openfilestream);
    lua_setglobal(g_lua_engine_state, "openfilestream");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_createfilestream);
    lua_setglobal(g_lua_engine_state, "createfilestream");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_getfilestreamline);
    lua_setglobal(g_lua_engine_state, "getfilestreamline");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_getfilestreamargument);
    lua_setglobal(g_lua_engine_state, "getfilestreamargument");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_filestreamnextline);
    lua_setglobal(g_lua_engine_state, "filestreamnextline");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_getfilestreamposition);
    lua_setglobal(g_lua_engine_state, "getfilestreamposition");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_setfilestreamposition);
    lua_setglobal(g_lua_engine_state, "setfilestreamposition");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_savefilestream);
    lua_setglobal(g_lua_engine_state, "savefilestream");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_filestreamappend);
    lua_setglobal(g_lua_engine_state, "filestreamappend");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_getentityvar);
    lua_setglobal(g_lua_engine_state, "getentityvar");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_setentityvar);
    lua_setglobal(g_lua_engine_state, "setentityvar");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_getscriptvar);
    lua_setglobal(g_lua_engine_state, "getscriptvar");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_setscriptvar);
    lua_setglobal(g_lua_engine_state, "setscriptvar");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_getindexedvar);
    lua_setglobal(g_lua_engine_state, "getindexedvar");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_setindexedvar);
    lua_setglobal(g_lua_engine_state, "setindexedvar");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_clearindexedvar);
    lua_setglobal(g_lua_engine_state, "clearindexedvar");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_log);
    lua_setglobal(g_lua_engine_state, "log");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_rand);
    lua_setglobal(g_lua_engine_state, "rand");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_drawstring);
    lua_setglobal(g_lua_engine_state, "drawstring");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_drawbox);
    lua_setglobal(g_lua_engine_state, "drawbox");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_killentity);
    lua_setglobal(g_lua_engine_state, "killentity");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_get_time);
    lua_setglobal(g_lua_engine_state, "get_time");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_get_player_health);
    lua_setglobal(g_lua_engine_state, "get_player_health");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_getentity);
    lua_setglobal(g_lua_engine_state, "getentity");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_damageentity);
    lua_setglobal(g_lua_engine_state, "damageentity");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_get_self);
    lua_setglobal(g_lua_engine_state, "get_self");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_getlocalvar);
    lua_setglobal(g_lua_engine_state, "getlocalvar");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_globalvar);
    lua_setglobal(g_lua_engine_state, "globalvar");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_getglobalvar);
    lua_setglobal(g_lua_engine_state, "getglobalvar");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_setglobalvar);
    lua_setglobal(g_lua_engine_state, "setglobalvar");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_changeentityproperty);
    lua_setglobal(g_lua_engine_state, "changeentityproperty");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_isentityvalid);
    lua_setglobal(g_lua_engine_state, "isEntityValid");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_isentityalive);
    lua_setglobal(g_lua_engine_state, "isEntityAlive");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_getentityhealth);
    lua_setglobal(g_lua_engine_state, "getEntityHealth");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_getentitytarget);
    lua_setglobal(g_lua_engine_state, "getEntityTarget");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_setentitytarget);
    lua_setglobal(g_lua_engine_state, "setEntityTarget");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_getentityposition);
    lua_setglobal(g_lua_engine_state, "getEntityPosition");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_setentityposition);
    lua_setglobal(g_lua_engine_state, "setEntityPosition");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_changeanimation);
    lua_setglobal(g_lua_engine_state, "changeAnimation");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_spawn);
    lua_setglobal(g_lua_engine_state, "spawn");

    /* Legacy property access is also available as a lowercase global. */
    lua_pushcfunction(g_lua_engine_state, lua_openborvariant);
    lua_setglobal(g_lua_engine_state, "openborvariant");

    lua_pushcfunction(g_lua_engine_state, lua_changeopenborvariant);
    lua_setglobal(g_lua_engine_state, "changeopenborvariant");

    lua_pushcfunction(g_lua_engine_state, lua_openborconstant);
    lua_setglobal(g_lua_engine_state, "openborconstant");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_getentityproperty);
    lua_setglobal(g_lua_engine_state, "getentityproperty");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_getlevelproperty);
    lua_setglobal(g_lua_engine_state, "getlevelproperty");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_changelevelproperty);
    lua_setglobal(g_lua_engine_state, "changelevelproperty");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_playerkeys);
    lua_setglobal(g_lua_engine_state, "playerkeys");
}
