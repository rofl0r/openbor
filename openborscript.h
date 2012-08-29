/*
 * OpenBOR - http://www.LavaLit.com
 * -----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in OpenBOR root for details.
 *
 * Copyright (c) 2004 - 2011 OpenBOR Team
 */

#ifndef OPENBORSCRIPT_H
#define OPENBORSCRIPT_H

#include "Interpreter.h"
#include "pp_parser.h"

#define MAX_GLOBAL_VAR 2048
#define MAX_KEY_LEN    24

typedef struct {
	ScriptVariant value;
	char key[MAX_KEY_LEN];
	struct Script *owner;
} s_variantnode;

typedef struct Script {
	Interpreter *pinterpreter;
	ScriptVariant *vars;
	int initialized;	//flag
	int interpreterowner;	//flag
} Script;

extern s_variantnode **global_var_list;
extern List theFunctionList;
extern ScriptVariant *indexed_var_list;
extern int max_indexed_vars;
extern int max_entity_vars;
extern int max_script_vars;
extern int max_global_vars;
extern int max_global_var_index;
extern int global_var_count;

//these functions can be used by openbor.c
void Script_Global_Init();
void Script_Global_Clear();
ScriptVariant *Script_Get_Global_Variant(char *theName);
int Script_Set_Global_Variant(char *theName, ScriptVariant * var);
ScriptVariant *Script_Get_Local_Variant(char *theName);
int Script_Set_Local_Variant(char *theName, ScriptVariant * var);
void Script_Init(Script * pscript, char *theName, int first);
Script *alloc_script();
void Script_Copy(Script * pdest, Script * psrc, int localclear);
int Script_IsInitialized(Script * pscript);
void Script_Clear(Script * pscript, int localclear);
int Script_AppendText(Script * pscript, char *text, char *path);
int Script_Compile(Script * pscript);
int Script_Execute(Script * pscript);

void Script_LoadSystemFunctions();

#ifndef COMPILED_SCRIPT
int Script_Call(Script * pscript, char *method, ScriptVariant * pretvar);
#endif

s32 system_isempty(ScriptVariant ** varlist, ScriptVariant ** pretvar, int paramCount);
s32 system_NULL(ScriptVariant ** varlist, ScriptVariant ** pretvar, int paramCount);
s32 system_rand(ScriptVariant ** varlist, ScriptVariant ** pretvar, int paramCount);
s32 system_maxglobalvarindex(ScriptVariant ** varlist, ScriptVariant ** pretvar, int paramCount);
s32 system_getglobalvar(ScriptVariant ** varlist, ScriptVariant ** pretvar, int paramCount);
s32 system_setglobalvar(ScriptVariant ** varlist, ScriptVariant ** pretvar, int paramCount);
s32 system_getlocalvar(ScriptVariant ** varlist, ScriptVariant ** pretvar, int paramCount);
s32 system_setlocalvar(ScriptVariant ** varlist, ScriptVariant ** pretvar, int paramCount);
s32 system_clearlocalvar(ScriptVariant ** varlist, ScriptVariant ** pretvar, int paramCount);
s32 system_clearglobalvar(ScriptVariant ** varlist, ScriptVariant ** pretvar, int paramCount);
s32 system_clearindexedvar(ScriptVariant ** varlist, ScriptVariant ** pretvar, int paramCount);
s32 system_free(ScriptVariant ** varlist, ScriptVariant ** pretvar, int paramCount);
s32 openbor_systemvariant(ScriptVariant ** varlist, ScriptVariant ** pretvar, int paramCount);
s32 openbor_changesystemvariant(ScriptVariant ** varlist, ScriptVariant ** pretvar, int paramCount);
s32 openbor_drawstring(ScriptVariant ** varlist, ScriptVariant ** pretvar, int paramCount);
s32 openbor_drawstringtoscreen(ScriptVariant ** varlist, ScriptVariant ** pretvar, int paramCount);
s32 openbor_drawsprite(ScriptVariant ** varlist, ScriptVariant ** pretvar, int paramCount);
s32 openbor_drawspritetoscreen(ScriptVariant ** varlist, ScriptVariant ** pretvar, int paramCount);
s32 openbor_log(ScriptVariant ** varlist, ScriptVariant ** pretvar, int paramCount);
s32 openbor_drawbox(ScriptVariant ** varlist, ScriptVariant ** pretvar, int paramCount);
s32 openbor_drawline(ScriptVariant ** varlist, ScriptVariant ** pretvar, int paramCount);
s32 openbor_drawdot(ScriptVariant ** varlist, ScriptVariant ** pretvar, int paramCount);
s32 openbor_drawboxtoscreen(ScriptVariant ** varlist, ScriptVariant ** pretvar, int paramCount);
s32 openbor_drawlinetoscreen(ScriptVariant ** varlist, ScriptVariant ** pretvar, int paramCount);
s32 openbor_drawdottoscreen(ScriptVariant ** varlist, ScriptVariant ** pretvar, int paramCount);
s32 openbor_drawscreen(ScriptVariant ** varlist, ScriptVariant ** pretvar, int paramCount);
s32 openbor_changeplayerproperty(ScriptVariant ** varlist, ScriptVariant ** pretvar, int paramCount);
s32 openbor_changeentityproperty(ScriptVariant ** varlist, ScriptVariant ** pretvar, int paramCount);
s32 openbor_getplayerproperty(ScriptVariant ** varlist, ScriptVariant ** pretvar, int paramCount);
s32 openbor_getentityproperty(ScriptVariant ** varlist, ScriptVariant ** pretvar, int paramCount);
s32 openbor_clearspawnentry(ScriptVariant ** varlist, ScriptVariant ** pretvar, int paramCount);
s32 openbor_setspawnentry(ScriptVariant ** varlist, ScriptVariant ** pretvar, int paramCount);
s32 openbor_spawn(ScriptVariant ** varlist, ScriptVariant ** pretvar, int paramCount);
s32 openbor_projectile(ScriptVariant ** varlist, ScriptVariant ** pretvar, int paramCount);
s32 openbor_transconst(ScriptVariant ** varlist, ScriptVariant ** pretvar, int paramCount);
s32 openbor_tossentity(ScriptVariant ** varlist, ScriptVariant ** pretvar, int paramCount);
s32 openbor_playmusic(ScriptVariant ** varlist, ScriptVariant ** pretvar, int paramCount);
s32 openbor_fademusic(ScriptVariant ** varlist, ScriptVariant ** pretvar, int paramCount);
s32 openbor_setmusicvolume(ScriptVariant ** varlist, ScriptVariant ** pretvar, int paramCount);
s32 openbor_setmusictempo(ScriptVariant ** varlist, ScriptVariant ** pretvar, int paramCount);
s32 openbor_pausemusic(ScriptVariant ** varlist, ScriptVariant ** pretvar, int paramCount);
s32 openbor_playsample(ScriptVariant ** varlist, ScriptVariant ** pretvar, int paramCount);
s32 openbor_loadsample(ScriptVariant ** varlist, ScriptVariant ** pretvar, int paramCount);
s32 openbor_unloadsample(ScriptVariant ** varlist, ScriptVariant ** pretvar, int paramCount);
s32 openbor_fadeout(ScriptVariant ** varlist, ScriptVariant ** pretvar, int paramCount);
s32 openbor_playerkeys(ScriptVariant ** varlist, ScriptVariant ** pretvar, int paramCount);
s32 openbor_changepalette(ScriptVariant ** varlist, ScriptVariant ** pretvar, int paramCount);
s32 openbor_damageentity(ScriptVariant ** varlist, ScriptVariant ** pretvar, int paramCount);
s32 openbor_killentity(ScriptVariant ** varlist, ScriptVariant ** pretvar, int paramCount);
s32 openbor_findtarget(ScriptVariant ** varlist, ScriptVariant ** pretvar, int paramCount);
s32 openbor_checkrange(ScriptVariant ** varlist, ScriptVariant ** pretvar, int paramCount);
s32 openbor_gettextobjproperty(ScriptVariant ** varlist, ScriptVariant ** pretvar, int paramCount);
s32 openbor_changetextobjproperty(ScriptVariant ** varlist, ScriptVariant ** pretvar, int paramCount);
s32 openbor_settextobj(ScriptVariant ** varlist, ScriptVariant ** pretvar, int paramCount);
s32 openbor_cleartextobj(ScriptVariant ** varlist, ScriptVariant ** pretvar, int paramCount);
s32 openbor_getbglayerproperty(ScriptVariant ** varlist, ScriptVariant ** pretvar, int paramCount);
s32 openbor_changebglayerproperty(ScriptVariant ** varlist, ScriptVariant ** pretvar, int paramCount);
s32 openbor_getfglayerproperty(ScriptVariant ** varlist, ScriptVariant ** pretvar, int paramCount);
s32 openbor_changefglayerproperty(ScriptVariant ** varlist, ScriptVariant ** pretvar, int paramCount);
s32 openbor_getlevelproperty(ScriptVariant ** varlist, ScriptVariant ** pretvar, int paramCount);
s32 openbor_changelevelproperty(ScriptVariant ** varlist, ScriptVariant ** pretvar, int paramCount);
s32 openbor_checkhole(ScriptVariant ** varlist, ScriptVariant ** pretvar, int paramCount);
s32 openbor_checkwall(ScriptVariant ** varlist, ScriptVariant ** pretvar, int paramCount);
s32 openbor_checkplatformbelow(ScriptVariant ** varlist, ScriptVariant ** pretvar, int paramCount);

s32 openbor_openfilestream(ScriptVariant ** varlist, ScriptVariant ** pretvar, int paramCount);
s32 openbor_getfilestreamline(ScriptVariant ** varlist, ScriptVariant ** pretvar, int paramCount);
s32 openbor_getfilestreamargument(ScriptVariant ** varlist, ScriptVariant ** pretvar, int paramCount);
s32 openbor_filestreamnextline(ScriptVariant ** varlist, ScriptVariant ** pretvar, int paramCount);
s32 openbor_getfilestreamposition(ScriptVariant ** varlist, ScriptVariant ** pretvar, int paramCount);
s32 openbor_setfilestreamposition(ScriptVariant ** varlist, ScriptVariant ** pretvar, int paramCount);

s32 openbor_filestreamappend(ScriptVariant ** varlist, ScriptVariant ** pretvar, int paramCount);
s32 openbor_createfilestream(ScriptVariant ** varlist, ScriptVariant ** pretvar, int paramCount);
s32 openbor_savefilestream(ScriptVariant ** varlist, ScriptVariant ** pretvar, int paramCount);

s32 openbor_getindexedvar(ScriptVariant ** varlist, ScriptVariant ** pretvar, int paramCount);
s32 openbor_setindexedvar(ScriptVariant ** varlist, ScriptVariant ** pretvar, int paramCount);
s32 openbor_getscriptvar(ScriptVariant ** varlist, ScriptVariant ** pretvar, int paramCount);
s32 openbor_setscriptvar(ScriptVariant ** varlist, ScriptVariant ** pretvar, int paramCount);
s32 openbor_getentityvar(ScriptVariant ** varlist, ScriptVariant ** pretvar, int paramCount);
s32 openbor_setentityvar(ScriptVariant ** varlist, ScriptVariant ** pretvar, int paramCount);

s32 openbor_jumptobranch(ScriptVariant ** varlist, ScriptVariant ** pretvar, int paramCount);

s32 openbor_changelight(ScriptVariant ** varlist, ScriptVariant ** pretvar, int paramCount);
s32 openbor_changeshadowcolor(ScriptVariant ** varlist, ScriptVariant ** pretvar, int paramCount);
s32 openbor_bindentity(ScriptVariant ** varlist, ScriptVariant ** pretvar, int paramCount);

s32 openbor_allocscreen(ScriptVariant ** varlist, ScriptVariant ** pretvar, int paramCount);
s32 openbor_clearscreen(ScriptVariant ** varlist, ScriptVariant ** pretvar, int paramCount);
s32 openbor_setdrawmethod(ScriptVariant ** varlist, ScriptVariant ** pretvar, int paramCount);
s32 openbor_updateframe(ScriptVariant ** varlist, ScriptVariant ** pretvar, int paramCount);
s32 openbor_performattack(ScriptVariant ** varlist, ScriptVariant ** pretvar, int paramCount);
s32 openbor_setidle(ScriptVariant ** varlist, ScriptVariant ** pretvar, int paramCount);
s32 openbor_getentity(ScriptVariant ** varlist, ScriptVariant ** pretvar, int paramCount);

s32 openbor_loadmodel(ScriptVariant ** varlist, ScriptVariant ** pretvar, int paramCount);
s32 openbor_loadsprite(ScriptVariant ** varlist, ScriptVariant ** pretvar, int paramCount);
s32 openbor_playgif(ScriptVariant ** varlist, ScriptVariant ** pretvar, int paramCount);

s32 openbor_strinfirst(ScriptVariant ** varlist, ScriptVariant ** pretvar, int paramCount);
s32 openbor_strinlast(ScriptVariant ** varlist, ScriptVariant ** pretvar, int paramCount);
s32 openbor_strleft(ScriptVariant ** varlist, ScriptVariant ** pretvar, int paramCount);
s32 openbor_strlength(ScriptVariant ** varlist, ScriptVariant ** pretvar, int paramCount);
s32 openbor_strright(ScriptVariant ** varlist, ScriptVariant ** pretvar, int paramCount);
s32 openbor_getmodelproperty(ScriptVariant ** varlist, ScriptVariant ** pretvar, int paramCount);
s32 openbor_changemodelproperty(ScriptVariant ** varlist, ScriptVariant ** pretvar, int paramCount);
s32 openbor_rgbcolor(ScriptVariant ** varlist, ScriptVariant ** pretvar, int paramCount);

void mapstrings_systemvariant(ScriptVariant ** varlist, int paramCount);
void mapstrings_changesystemvariant(ScriptVariant ** varlist, int paramCount);
void mapstrings_getentityproperty(ScriptVariant ** varlist, int paramCount);
void mapstrings_changeentityproperty(ScriptVariant ** varlist, int paramCount);
void mapstrings_getplayerproperty(ScriptVariant ** varlist, int paramCount);
void mapstrings_changeplayerproperty(ScriptVariant ** varlist, int paramCount);
void mapstrings_setspawnentry(ScriptVariant ** varlist, int paramCount);
void mapstrings_transconst(ScriptVariant ** varlist, int paramCount);
void mapstrings_playerkeys(ScriptVariant ** varlist, int paramCount);
void mapstrings_gettextobjproperty(ScriptVariant ** varlist, int paramCount);
void mapstrings_changetextobjproperty(ScriptVariant ** varlist, int paramCount);
void mapstrings_getbglayerproperty(ScriptVariant ** varlist, int paramCount);
void mapstrings_changebglayerproperty(ScriptVariant ** varlist, int paramCount);
void mapstrings_getfglayerproperty(ScriptVariant ** varlist, int paramCount);
void mapstrings_changefglayerproperty(ScriptVariant ** varlist, int paramCount);

#endif
