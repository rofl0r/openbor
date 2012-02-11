#include "openbor.h"
#include "data.h"

s_color_tables color_tables = {
	.mp = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	.hp =  { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	.ld = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

s_barstatus loadingbarstatus = {
	.offsetx = 0,
	.offsety = 0,
	.sizex = 0,
	.sizey = 10,
	.type = percentagebar,
	.orientation = horizontalbar,
	.noborder = 0,
	.direction = 0,
	.barlayer = 0,
	.backlayer = 0,
	.borderlayer = 0,
	.shadowlayer = 0,
	.colourtable = &color_tables.ld,
};

s_barstatus lbarstatus =	// Used for customizable lifebar size
{
	.offsetx = 0,
	.offsety = 0,
	.sizex = 0,
	.sizey = 0,
	.type = valuebar,
	.orientation = horizontalbar,
	.noborder = 0,
	.direction = 0,
	.barlayer = 0,
	.backlayer = 0,
	.borderlayer = 0,
	.shadowlayer = 0,
	.colourtable = &color_tables.hp,
};

s_barstatus olbarstatus =	// Used for customizable opponent lifebar size
{
	.offsetx = 0,
	.offsety = 0,
	.sizex = 0,
	.sizey = 0,
	.type = valuebar,
	.orientation = horizontalbar,
	.noborder = 0,
	.direction = 0,
	.barlayer = 0,
	.backlayer = 0,
	.borderlayer = 0,
	.shadowlayer = 0,
	.colourtable = &color_tables.hp,
};

s_barstatus mpbarstatus =	// Used for customizable lifebar size
{
	.offsetx = 0,
	.offsety = 0,
	.sizex = 0,
	.sizey = 0,
	.type = valuebar,
	.orientation = horizontalbar,
	.noborder = 0,
	.direction = 0,
	.barlayer = 0,
	.backlayer = 0,
	.borderlayer = 0,
	.shadowlayer = 0,
	.colourtable = &color_tables.mp,
};

// -------dynamic animation indexes-------
const s_dynamic_animation_indexes_default_sizes default_dyn_anims_sizes = {
	.animdowns = MAX_DOWNS,
	.animups = MAX_UPS,
	.animbackwalks = MAX_BACKWALKS,
	.animwalks = MAX_WALKS,
	.animidles = MAX_IDLES,
	.animpains = MAX_ATKS,
	.animdies = MAX_ATKS,
	.animfalls = MAX_ATKS,
	.animrises = MAX_ATKS,
	.animriseattacks = MAX_ATKS,
	.animblkpains = MAX_ATKS,
	.animattacks = MAX_ATTACKS,
	.animfollows = MAX_FOLLOWS,
	.animspecials = MAX_SPECIALS,
};

s_dynamic_animation_indexes dyn_anims = {0};
const int dyn_anim_itemcount = sizeof(s_dynamic_animation_indexes) / sizeof(int*);


const s_const_dynamic_animation_indexes default_dyn_anims = {
	.animdowns = (const int[]) { ANI_DOWN },
	.animups = (const int[]) { ANI_UP },
	.animbackwalks = (const int[]) { ANI_BACKWALK },
	.animwalks = (const int[]) { ANI_WALK },
	.animidles = (const int[]) { ANI_IDLE },
	.animpains = (const int[]) {
		ANI_PAIN, ANI_PAIN2, ANI_PAIN3, ANI_PAIN4,
		ANI_PAIN, ANI_BURNPAIN, ANI_PAIN, ANI_SHOCKPAIN,
		ANI_PAIN, ANI_PAIN5, ANI_PAIN6, ANI_PAIN7,
		ANI_PAIN8, ANI_PAIN9, ANI_PAIN10
	},
	.animdies = (const int[]) {
		ANI_DIE, ANI_DIE2, ANI_DIE3, ANI_DIE4,
		ANI_DIE, ANI_BURNDIE, ANI_DIE, ANI_SHOCKDIE,
		ANI_DIE, ANI_DIE5, ANI_DIE6, ANI_DIE7,
		ANI_DIE8, ANI_DIE9, ANI_DIE10
	},	
	.animfalls = (const int[]) {
		ANI_FALL, ANI_FALL2, ANI_FALL3, ANI_FALL4,
		ANI_FALL, ANI_BURN, ANI_FALL, ANI_SHOCK,
		ANI_FALL, ANI_FALL5, ANI_FALL6, ANI_FALL7,
		ANI_FALL8, ANI_FALL9, ANI_FALL10
	},
	.animrises = (const int[]) {
		ANI_RISE, ANI_RISE2, ANI_RISE3, ANI_RISE4,
		ANI_RISE, ANI_RISEB, ANI_RISE, ANI_RISES,
		ANI_RISE, ANI_RISE5, ANI_RISE6, ANI_RISE7,
		ANI_RISE8, ANI_RISE9, ANI_RISE10
	},
	.animriseattacks = (const int[]) {
		ANI_RISEATTACK, ANI_RISEATTACK2, ANI_RISEATTACK3, ANI_RISEATTACK4,
		ANI_RISEATTACK, ANI_RISEATTACKB, ANI_RISEATTACK, ANI_RISEATTACKS,
		ANI_RISEATTACK, ANI_RISEATTACK5, ANI_RISEATTACK6, ANI_RISEATTACK7,
		ANI_RISEATTACK8, ANI_RISEATTACK9, ANI_RISEATTACK10
	},
	.animblkpains = (const int[]) {
		ANI_BLOCKPAIN, ANI_BLOCKPAIN2, ANI_BLOCKPAIN3, ANI_BLOCKPAIN4,
		ANI_BLOCKPAIN, ANI_BLOCKPAINB, ANI_BLOCKPAIN, ANI_BLOCKPAINS,
		ANI_BLOCKPAIN, ANI_BLOCKPAIN5, ANI_BLOCKPAIN6, ANI_BLOCKPAIN7,
		ANI_BLOCKPAIN8, ANI_BLOCKPAIN9, ANI_BLOCKPAIN10
	},
	.animattacks = (const int[]) {
		ANI_ATTACK1, ANI_ATTACK2, ANI_ATTACK3, ANI_ATTACK4
	},
	.animfollows = (const int[]) {
		ANI_FOLLOW1, ANI_FOLLOW2, ANI_FOLLOW3, ANI_FOLLOW4
	},	
	.animspecials = (const int[]) {
		ANI_FREESPECIAL, ANI_FREESPECIAL2, ANI_FREESPECIAL3,
		ANI_FREESPECIAL4, ANI_FREESPECIAL5, ANI_FREESPECIAL6,
		ANI_FREESPECIAL7, ANI_FREESPECIAL8
	},
};

const s_dyn_anim_custom_maxvalues dyn_anim_default_custom_maxvalues = {
	.max_downs = MAX_DOWNS,
	.max_ups = MAX_UPS,
	.max_backwalks = MAX_BACKWALKS,
	.max_walks = MAX_WALKS,
	.max_idles = MAX_IDLES,
	.max_attack_types = MAX_ATKS,
	.max_freespecials = MAX_SPECIALS,
	.max_follows = MAX_FOLLOWS,
	.max_attacks = MAX_ATTACKS,
	.max_animations = MAX_ANIS,
};

s_dyn_anim_custom_maxvalues dyn_anim_custom_maxvalues = {
	.max_downs = MAX_DOWNS,
	.max_ups = MAX_UPS,
	.max_backwalks = MAX_BACKWALKS,
	.max_walks = MAX_WALKS,
	.max_idles = MAX_IDLES,
	.max_attack_types = MAX_ATKS,
	.max_freespecials = MAX_SPECIALS,
	.max_follows = MAX_FOLLOWS,
	.max_attacks = MAX_ATTACKS,
	.max_animations = MAX_ANIS,
};

//this table is used to find the custom max value for a
//s_dynamic_animation_indexes member.
const s_const_dynamic_animation_indexes dyn_anim_custom_max_ptr = {
	.animdowns = &dyn_anim_custom_maxvalues.max_downs,
	.animups = &dyn_anim_custom_maxvalues.max_ups,
	.animbackwalks = &dyn_anim_custom_maxvalues.max_backwalks,
	.animwalks = &dyn_anim_custom_maxvalues.max_walks,
	.animidles = &dyn_anim_custom_maxvalues.max_idles,
	.animpains = &dyn_anim_custom_maxvalues.max_attack_types,
	.animdies = &dyn_anim_custom_maxvalues.max_attack_types,
	.animfalls = &dyn_anim_custom_maxvalues.max_attack_types,
	.animrises = &dyn_anim_custom_maxvalues.max_attack_types,
	.animriseattacks = &dyn_anim_custom_maxvalues.max_attack_types,
	.animblkpains = &dyn_anim_custom_maxvalues.max_attack_types,
	.animattacks = &dyn_anim_custom_maxvalues.max_attacks,
	.animfollows = &dyn_anim_custom_maxvalues.max_follows,
	.animspecials = &dyn_anim_custom_maxvalues.max_freespecials,
};
//----------------------------------

s_button_names buttonnames = {
	.moveup = "Move Up",
	.movedown = "Move Down",
	.moveleft = "Move Left",
	.moveright = "Move Right",
	.attack = "Attack 1",
	.attack2 = "Attack 2",
	.attack3 = "Attack 3",
	.attack4 = "Attack 4",
	.jump = "Jump",
	.special = "Special",
	.start = "Start",
	.screenshot = "Screenshot",
};

const s_button_names_const config_button_names = {
	.moveup = "moveup",
	.movedown = "movedown",
	.moveleft = "moveleft",
	.moveright = "moveright",
	.attack = "attack",
	.attack2 = "attack2",
	.attack3 = "attack3",
	.attack4 = "attack4",
	.jump = "jump",
	.special = "special",
	.start = "start",
	.screenshot = "screenshot",
};

const s_special_sprite special_sprites_init[] = {
	{&shadowsprites[0], "data/sprites/shadow1.gif", 9, 3},
	{&shadowsprites[1], "data/sprites/shadow2.gif", 14, 5},
	{&shadowsprites[2], "data/sprites/shadow3.gif", 19, 6},
	{&shadowsprites[3], "data/sprites/shadow4.gif", 24, 8},
	{&shadowsprites[4], "data/sprites/shadow5.gif", 29, 9},
	{&shadowsprites[5], "data/sprites/shadow6.gif", 34, 11},
	{&gosprite, "data/sprites/arrow.gif", 35, 23},
	{&golsprite, "data/sprites/arrowl.gif", 35, 23},
};

const int special_sprites_init_itemcount = ARRAY_SIZE(special_sprites_init);

const s_font_init font_init[] = {
	{1, "data/sprites/font.gif"},
	{1, "data/sprites/font2.gif"},
	{1, "data/sprites/font3.gif"},
	{1, "data/sprites/font4.gif"},
	{0, "data/sprites/font5.gif"},
	{0, "data/sprites/font6.gif"},
	{0, "data/sprites/font7.gif"},
	{0, "data/sprites/font8.gif"},
};

const int font_init_itemcount = ARRAY_SIZE(font_init);

const char* video_txt_commands_strings[] = {
	[VTC_VIDEO] = "video",
	[VTC_SCENES] = "scenes",
	[VTC_BACKGROUNDS] = "backgrounds",
	[VTC_LEVELS] = "levels",
	[VTC_MODELS] = "models",
	[VTC_COLOURDEPTH] = "colourdepth",
	[VTC_FORCEMODE] = "forcemode",
};

char** video_txt_commands_dest[] = {
	[VTC_VIDEO] = NULL,
	[VTC_SCENES] = NULL,
	[VTC_BACKGROUNDS] = &custBkgrds,
	[VTC_LEVELS] = &custLevels,
	[VTC_MODELS] = &custModels,
	[VTC_COLOURDEPTH] = NULL,
	[VTC_FORCEMODE] = NULL,
};

const s_colors_rgb default_colors = {
	.black = {0, 0, 0},
	.red = {255, 0, 0},	// 1% - 25% Full Health
	.orange = {255, 150, 0},	// 200-300HP
	.yellow = {0xF8, 0xB8, 0x40},	// 26%-50% Full health
	.white = {255, 255, 255},	// white boxes 600+ HP
	.blue = {0, 0, 255},	// 100-200 HP
	.green = {0, 255, 0},	// 51% - 100% full health
	.pink = {255, 0, 255},	// 300-400HP
	.purple = {128, 48, 208},	// transbox 400-500HP
	.magic = {98, 180, 255},	// 1st magic bar color by tails
	.magic2 = {24, 48, 143},	// 2sec magic bar color by tails
	.shadow = {64, 64, 64},
};

const s_script_and_path_and_name script_and_path_and_name[] = {
	{&game_scripts.update_script, "data/scripts/update.c", "update"},
	{&game_scripts.updated_script, "data/scripts/updated.c", "updated"},
	{&game_scripts.level_script, "data/scripts/level.c", "level"},
	{&game_scripts.endlevel_script, "data/scripts/endlevel.c", "endlevel"},
	{&game_scripts.key_script_all, "data/scripts/keyall.c", "keyall"},
	{&game_scripts.timetick_script, "data/scripts/timetick.c", "timetick"},
	{&game_scripts.score_script[0], "data/scripts/score1.c", "score"},
	{&game_scripts.score_script[1], "data/scripts/score2.c", "score"},
	{&game_scripts.score_script[2], "data/scripts/score3.c", "score"},
	{&game_scripts.score_script[3], "data/scripts/score4.c", "score"},
	{&game_scripts.key_script[0], "data/scripts/key1.c", "key"},
	{&game_scripts.key_script[1], "data/scripts/key2.c", "key"},
	{&game_scripts.key_script[2], "data/scripts/key3.c", "key"},
	{&game_scripts.key_script[3], "data/scripts/key4.c", "key"},
	{&game_scripts.join_script[0], "data/scripts/join1.c", "join"},
	{&game_scripts.join_script[1], "data/scripts/join2.c", "join"},
	{&game_scripts.join_script[2], "data/scripts/join3.c", "join"},
	{&game_scripts.join_script[3], "data/scripts/join4.c", "join"},
	{&game_scripts.respawn_script[0], "data/scripts/respawn1.c", "respawn"},
	{&game_scripts.respawn_script[1], "data/scripts/respawn2.c", "respawn"},
	{&game_scripts.respawn_script[2], "data/scripts/respawn3.c", "respawn"},
	{&game_scripts.respawn_script[3], "data/scripts/respawn4.c", "respawn"},
	{&game_scripts.pdie_script[0], "data/scripts/die1.c", "die"},
	{&game_scripts.pdie_script[1], "data/scripts/die2.c", "die"},
	{&game_scripts.pdie_script[2], "data/scripts/die3.c", "die"},
	{&game_scripts.pdie_script[3], "data/scripts/die4.c", "die"},
};

const int script_and_path_and_name_itemcount = ARRAY_SIZE(script_and_path_and_name);

const s_videomodes videomodes_init_data[] = {
	[VTM_320_240] = {320, 240, 0, 0, 231, {0, 0, 0, 0}, 0, 0, 0, 1.f, 1.f},
	[VTM_480_272] = {480, 272, 80, 20, 163, {0, 0, 0, 0,}, 0, 0, 0, 1.5f, 1.13f},
	[VTM_640_480] = {640, 480, 160, 35, 464, {0, 0, 0, 0}, 0, 0, 0, 2.f, 2.f},
	[VTM_720_480] = {720, 480, 200, 35, 464, {0, 0, 0, 0}, 0, 0, 0, 2.25f, 2.f},
	[VTM_800_480] = {800, 480, 240, 35, 464, {0, 0, 0, 0}, 0, 0, 0, 2.5f, 2.f},
	[VTM_800_600] = {800, 600, 240, 44, 580, {0, 0, 0, 0}, 0, 0, 0, 2.5f, 2.5f},
	[VTM_960_560] = {960, 540, 320, 40, 522, {0, 0, 0, 0}, 0, 0, 0, 3.f, 2.25f}
};

const s_player_min_max_z_bgheight player_min_max_z_bgheight_init_data[] = {
	[VTM_320_240] = {160, 232, 160},
	[VTM_480_272] = {182, 264, 182},
	[VTM_640_480] = {321, 465, 321},
	[VTM_720_480] = {321, 465, 321},
	[VTM_800_480] = {321, 465, 321},
	[VTM_800_600] = {401, 582, 401},
	[VTM_960_560] = {362, 524, 362}
};

const s_drawmethod plainmethod = {
	.table = NULL,
	.fp = NULL,
	.fillcolor = 0,
	.scalex = 256,
	.scaley = 256,
	.shiftx = 0,
	.centerx = 0, //currently used only by gfxshadow, do not touch it
	.centery = 0,
	.alpha = -1,
	.remap = -1,
	.rotate = 0,
	.flag = 1,
	.flipx = 0,
	.flipy = 0,
	.transbg = 0,
	.fliprotate = 0,
};

const s_attack emptyattack = {
	.attack_force = 0,
	.hitsound = -1,
	.hitflash = -1,
	.blockflash = -1,
	.blocksound = -1,
	.counterattack = 0,
	.attack_coords = {0, 0, 0, 0, 0},
	.no_pain = 0,
	.no_flash = 0,
	.no_block = 0,
	.grab = 0,
	.force_direction = 0,
	.blast = 0,
	.freeze = 0,
	.steal = 0,
	.forcemap = 0,
	.seal = 0,
	.freezetime = 0,
	.maptime = 0,
	.sealtime = 0,
	.dot = 0,
	.dot_index = 0,
	.dot_time = 0,
	.dot_force = 0,
	.dot_rate = 0,
	.dropv = {0.f, 0.f, 0.f},
	.otg = 0,
	.jugglecost = 0,
	.guardcost = 0,
	.attack_drop = 0,
	.attack_type = 0,
	.damage_on_landing = 0,
	.grab_distance = 0,
	.pause_add = 0,
	.staydown = {0, 0, 0},
};


const s_savedata savedata_default = {
	.compatibleversion = COMPATIBLEVERSION,
	.gamma = 0,
	.brightness = 0,
	.usesound = 1,
	.soundrate = 44100,
	.soundbits = 16,
	.soundvol = 15,
	.usemusic = 1,
	.musicvol = 100,
	.effectvol = 120,
	.usejoy = 1,
	.mode = 0,
	.showtitles = 0,
	.windowpos = 0,
	.logo = 0,
	.uselog = 1,
	.debuginfo = 0,
	.fullscreen = 0,
	.stretch = 0,
	.usegl = 0,
	.glscale = 1.0,
	.glfilter = 0,
	.screen[0][0] = 0,
	.screen[0][1] = 0,
	.screen[1][0] = 0,
	.screen[1][1] = 0,
	.screen[2][0] = 0,
	.screen[2][1] = 0,
	.screen[3][0] = 0,
	.screen[3][1] = 0,
	.screen[4][0] = 0,
	.screen[4][1] = 0,
	.screen[5][0] = 0,
	.screen[5][1] = 0,
	.screen[6][0] = 0,
	.screen[6][1] = 0,
	.keys[0][SDID_MOVEUP] = CONTROL_DEFAULT1_UP,
	.keys[0][SDID_MOVEDOWN] = CONTROL_DEFAULT1_DOWN,
	.keys[0][SDID_MOVELEFT] = CONTROL_DEFAULT1_LEFT,
	.keys[0][SDID_MOVERIGHT] = CONTROL_DEFAULT1_RIGHT,
	.keys[0][SDID_ATTACK] = CONTROL_DEFAULT1_FIRE1,
	.keys[0][SDID_ATTACK2] = CONTROL_DEFAULT1_FIRE2,
	.keys[0][SDID_ATTACK3] = CONTROL_DEFAULT1_FIRE3,
	.keys[0][SDID_ATTACK4] = CONTROL_DEFAULT1_FIRE4,
	.keys[0][SDID_JUMP] = CONTROL_DEFAULT1_FIRE5,
	.keys[0][SDID_SPECIAL] = CONTROL_DEFAULT1_FIRE6,
	.keys[0][SDID_START] = CONTROL_DEFAULT1_START,
	.keys[0][SDID_SCREENSHOT] = CONTROL_DEFAULT1_SCREENSHOT,
	.keys[1][SDID_MOVEUP] = CONTROL_DEFAULT2_UP,
	.keys[1][SDID_MOVEDOWN] = CONTROL_DEFAULT2_DOWN,
	.keys[1][SDID_MOVELEFT] = CONTROL_DEFAULT2_LEFT,
	.keys[1][SDID_MOVERIGHT] = CONTROL_DEFAULT2_RIGHT,
	.keys[1][SDID_ATTACK] = CONTROL_DEFAULT2_FIRE1,
	.keys[1][SDID_ATTACK2] = CONTROL_DEFAULT2_FIRE2,
	.keys[1][SDID_ATTACK3] = CONTROL_DEFAULT2_FIRE3,
	.keys[1][SDID_ATTACK4] = CONTROL_DEFAULT2_FIRE4,
	.keys[1][SDID_JUMP] = CONTROL_DEFAULT2_FIRE5,
	.keys[1][SDID_SPECIAL] = CONTROL_DEFAULT2_FIRE6,
	.keys[1][SDID_START] = CONTROL_DEFAULT2_START,
	.keys[1][SDID_SCREENSHOT] = CONTROL_DEFAULT2_SCREENSHOT,
	.keys[2][SDID_MOVEUP] = CONTROL_DEFAULT3_UP,
	.keys[2][SDID_MOVEDOWN] = CONTROL_DEFAULT3_DOWN,
	.keys[2][SDID_MOVELEFT] = CONTROL_DEFAULT3_LEFT,
	.keys[2][SDID_MOVERIGHT] = CONTROL_DEFAULT3_RIGHT,
	.keys[2][SDID_ATTACK] = CONTROL_DEFAULT3_FIRE1,
	.keys[2][SDID_ATTACK2] = CONTROL_DEFAULT3_FIRE2,
	.keys[2][SDID_ATTACK3] = CONTROL_DEFAULT3_FIRE3,
	.keys[2][SDID_ATTACK4] = CONTROL_DEFAULT3_FIRE4,
	.keys[2][SDID_JUMP] = CONTROL_DEFAULT3_FIRE5,
	.keys[2][SDID_SPECIAL] = CONTROL_DEFAULT3_FIRE6,
	.keys[2][SDID_START] = CONTROL_DEFAULT3_START,
	.keys[2][SDID_SCREENSHOT] = CONTROL_DEFAULT3_SCREENSHOT,
	.keys[3][SDID_MOVEUP] = CONTROL_DEFAULT4_UP,
	.keys[3][SDID_MOVEDOWN] = CONTROL_DEFAULT4_DOWN,
	.keys[3][SDID_MOVELEFT] = CONTROL_DEFAULT4_LEFT,
	.keys[3][SDID_MOVERIGHT] = CONTROL_DEFAULT4_RIGHT,
	.keys[3][SDID_ATTACK] = CONTROL_DEFAULT4_FIRE1,
	.keys[3][SDID_ATTACK2] = CONTROL_DEFAULT4_FIRE2,
	.keys[3][SDID_ATTACK3] = CONTROL_DEFAULT4_FIRE3,
	.keys[3][SDID_ATTACK4] = CONTROL_DEFAULT4_FIRE4,
	.keys[3][SDID_JUMP] = CONTROL_DEFAULT4_FIRE5,
	.keys[3][SDID_SPECIAL] = CONTROL_DEFAULT4_FIRE6,
	.keys[3][SDID_START] = CONTROL_DEFAULT4_START,
	.keys[3][SDID_SCREENSHOT] = CONTROL_DEFAULT4_SCREENSHOT,
	
};
