#ifndef DATA_H
#define DATA_H

#include "openbor.h"

typedef struct {
	Script* script;
	char* path;
	char* name;
} s_script_and_path_and_name;

extern const s_script_and_path_and_name script_and_path_and_name[];
extern const int script_and_path_and_name_itemcount;

typedef struct {
	int mp[11];
	int hp[11];
	int ld[11];
} s_color_tables;

extern s_color_tables color_tables;

extern s_barstatus loadingbarstatus;
extern s_barstatus lbarstatus;
extern s_barstatus olbarstatus;
extern s_barstatus mpbarstatus;

typedef struct {
	int* target;
	char* fn;
	char ofsx;
	char ofsy;
} s_special_sprite;

extern const int special_sprites_init_itemcount;
extern const s_special_sprite special_sprites_init[];

// -------dynamic animation indexes-------
typedef struct {
	int *animdowns;
	int *animups;
	int *animbackwalks;
	int *animwalks;
	int *animidles;
	int *animpains;
	int *animdies;
	int *animfalls;
	int *animrises;
	int *animriseattacks;
	int *animblkpains;
	int *animattacks;
	int *animfollows;
	int *animspecials;
} s_dynamic_animation_indexes;

typedef struct {
	const int *animdowns;
	const int *animups;
	const int *animbackwalks;
	const int *animwalks;
	const int *animidles;
	const int *animpains;
	const int *animdies;
	const int *animfalls;
	const int *animrises;
	const int *animriseattacks;
	const int *animblkpains;
	const int *animattacks;
	const int *animfollows;
	const int *animspecials;
} s_const_dynamic_animation_indexes;

typedef struct {
	char animdowns;
	char animups;
	char animbackwalks;
	char animwalks;
	char animidles;
	char animpains;
	char animdies;
	char animfalls;
	char animrises;
	char animriseattacks;
	char animblkpains;
	char animattacks;
	char animfollows;
	char animspecials;
} s_dynamic_animation_indexes_default_sizes;

extern s_dynamic_animation_indexes dyn_anims;

extern const s_dynamic_animation_indexes_default_sizes default_dyn_anims_sizes;
extern const s_const_dynamic_animation_indexes default_dyn_anims;
extern const int dyn_anim_itemcount;

// -----------------------------

typedef struct {
	char obligatory;
	char* path;
} s_font_init;

extern const s_font_init font_init[];
extern const int font_init_itemcount;

typedef enum {
	VTC_VIDEO = 0,
	VTC_SCENES,
	VTC_BACKGROUNDS,
	VTC_LEVELS,
	VTC_MODELS,
	VTC_COLOURDEPTH,
	VTC_FORCEMODE,
	VTC_MAX
} VIDEO_TXT_COMMANDS;

extern const char* video_txt_commands_strings[];
extern char** video_txt_commands_dest[];

extern const s_colors_rgb default_colors;

extern const char *config_button_names[];
extern const char *default_button_names[];

extern const s_savedata savedata_default;
extern const s_videomodes videomodes_init_data[];
extern const s_player_min_max_z_bgheight player_min_max_z_bgheight_init_data[];
extern const s_drawmethod plainmethod;
extern const s_attack emptyattack;

#endif
