/*
 * OpenBOR - http://www.LavaLit.com
 * -
 ----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in OpenBOR root for details.
 *
 * Copyright (c) 2004 - 2011 OpenBOR Team
 */

/////////////////////////////////////////////////////////////////////////////
//      Beats of Rage                                                          //
//      Side-scrolling beat-'em-up                                             //
/////////////////////////////////////////////////////////////////////////////

#ifndef OPENBOR_H
#define OPENBOR_H


/////////////////////////////////////////////////////////////////////////////

#include	"types.h"
#include	"video.h"
#include	"vga.h"
#include	"screen.h"
#include	"loadimg.h"
#include	"bitmap.h"
#include	"sprite.h"
#include	"spriteq.h"
#include	"font.h"
#include	"timer.h"
#include	"rand32.h"
#include	"sblaster.h"
#include	"soundmix.h"
#include	"control.h"
#include	"draw.h"
#include	"packfile.h"
#include	"palette.h"
#include	"anigif.h"
#include	"texture.h"
#include    "openborscript.h"
#include    "globals.h"
#include    "version.h"

#ifdef SDL
#include    "gfx.h"
#endif

/////////////////////////////////////////////////////////////////////////////

// FIXME wow. this needs to be fixed. does rename every occurence of those variables/functions.

#define		exit				borExit
#define		kill				borKill
#define		pause				borPause
#define		shutdown(ec, fmt, args...)	borShutdown(__FUNCTION__, ec, fmt, ## args)

#define		COMPATIBLEVERSION	0x00030000
#define		CV_SAVED_GAME		0x00021276
#define		CV_HIGH_SCORE		0x00020048
#define     GAME_SPEED          200
#define		THINK_SPEED			2
#define		COUNTER_SPEED		(GAME_SPEED*2)
#define		MAX_ENTS			150
#define		MAX_PANELS			52
#define		MAX_WEAPONS			10
#define		MAX_COLOUR_MAPS		30
#define		MAX_NAME_LEN		40
#define		LEVEL_MAX_SPAWNS	600
#define		LEVEL_MAX_PANELS	100
#define		LEVEL_MAX_HOLES		40
#define		LEVEL_MAX_WALLS		40
#define     LEVEL_MAX_BGLAYERS  100
#define     LEVEL_MAX_FGLAYERS  10
#define     LEVEL_MAX_TEXTOBJS  50
#define     LEVEL_MAX_FILESTREAMS 50
#define     LEVEL_MAX_PALETTES  40	// altered palettes
#define		MAX_LEVELS			100
#define		MAX_DIFFICULTIES	10
#define		MAX_SPECIALS		8	// Added for customizable freespecials
#define     MAX_SPECIAL_INPUTS  27	// max freespecial input steps, MAX_SPECIAL_INPUTS-1 is reserved, MAX_SPECIAL_INPUTS-2 is animation index, MAX_SPECIAL_INPUTS-3 is reserved. OX -4 , -5 , -6 , -7 , -8 , -9 , -10 also for cancels
#define		MAX_ATCHAIN			12	// Maximum attack chain length
#define     MAX_IDLES           1	// Idle animations.
#define     MAX_WALKS           1	// Walk animations.
#define     MAX_BACKWALKS       1	// Backwalk animations.
#define     MAX_UPS             1	// Walk up animations.
#define     MAX_DOWNS           1	// Walk down animations.
#define		MAX_ATTACKS			4	// Total number of attacks players have
#define     MAX_FOLLOWS         4	// For followup animations
#define     MAX_PLAYERS         4
#define		MAX_ARG_LEN			512
#define		MAX_PAL_SIZE		1024
#define		MAX_CACHED_BACKGROUNDS 9

#define		FLAG_ESC			0x00000001
#define		FLAG_START			0x00000002
#define		FLAG_MOVELEFT		0x00000004
#define		FLAG_MOVERIGHT		0x00000008
#define		FLAG_MOVEUP			0x00000010
#define		FLAG_MOVEDOWN		0x00000020
#define		FLAG_ATTACK			0x00000040
#define		FLAG_JUMP			0x00000080
#define		FLAG_SPECIAL		0x00000100
#define		FLAG_SCREENSHOT		0x00000200
#define		FLAG_ATTACK2		0x00000400
#define		FLAG_ATTACK3		0x00000800
#define		FLAG_ATTACK4		0x00001000
#define		FLAG_ANYBUTTON		(FLAG_START|FLAG_SPECIAL|FLAG_ATTACK|FLAG_ATTACK2|FLAG_ATTACK3|FLAG_ATTACK4|FLAG_JUMP)
#define		FLAG_FORWARD		0
#define		FLAG_BACKWARD		1

#define		SDID_MOVEUP			0
#define		SDID_MOVEDOWN		1
#define		SDID_MOVELEFT		2
#define		SDID_MOVERIGHT		3
#define		SDID_ATTACK 		4
#define		SDID_ATTACK2		5
#define		SDID_ATTACK3		6
#define		SDID_ATTACK4		7
#define		SDID_JUMP			8
#define		SDID_SPECIAL		9
#define		SDID_START			10
#define		SDID_SCREENSHOT		11

#define		TYPE_NONE			0
#define		TYPE_PLAYER			1
#define		TYPE_ENEMY			2
#define		TYPE_ITEM			4
#define		TYPE_OBSTACLE		8
#define		TYPE_STEAMER		16
#define		TYPE_SHOT			32	// 7-1-2005 type to use for player projectiles
#define		TYPE_TRAP			64	// 7-1-2005 lets face it enemies are going to just let you storm in without setting a trap or two!
#define		TYPE_TEXTBOX		128	// New textbox type for displaying messages
#define		TYPE_ENDLEVEL		256	// New endlevel type that ends the level when touched
#define     TYPE_NPC            512	// A character can be an ally or enemy.
#define     TYPE_PANEL          1024	// Fake panel, scroll with screen using model speed

#define		SUBTYPE_NONE		0
#define		SUBTYPE_BIKER		1
#define		SUBTYPE_NOTGRAB		2	//7-1-2005 new subtype for those ungrabbable enemies
#define		SUBTYPE_ARROW		3	//7-1-2005  subtype for an "enemy" that flies across the screen and dies
#define		SUBTYPE_TOUCH		4	// ltb 1-18-05  new Item subtype for a more platformer feel.
#define		SUBTYPE_WEAPON		5
#define		SUBTYPE_NOSKIP		6	// Text type that can't be skipped
#define		SUBTYPE_FLYDIE		7	// Now obstacles can be hit and fly like on Simpsons/TMNT
#define		SUBTYPE_BOTH		8	// Used with TYPE_ENDLEVEL to force both players to reach the point before ending level
#define		SUBTYPE_PROJECTILE	9	// New weapon projectile type that can be picked up by players/enemies
#define     SUBTYPE_FOLLOW      10	// Used by NPC character, if set, they will try to follow players
#define     SUBTYPE_CHASE       11	// Used by enemy always chasing you

//------------reserved for A.I. types-------------------------
//  A.I. move1, affect movement path
#define     AIMOVE1_NORMAL      0	// Current default style
#define     AIMOVE1_CHASE       0x00000001	// alway move towards target, and can run to them if target is farway
#define     AIMOVE1_CHASEZ      0x00000002	// only try to get close in z direction
#define     AIMOVE1_CHASEX      0x00000004	// only try to get colse in x direction
#define     AIMOVE1_AVOID       0x00000008	// try to avoid target
#define     AIMOVE1_AVOIDZ      0x00000010	// only try to avoid target in z direction
#define     AIMOVE1_AVOIDX      0x00000020	// only try to avoid target in x direction
#define     AIMOVE1_WANDER      0x00000040	// ignore the target's position completely, wander everywhere, long idle time
#define     AIMOVE1_BIKER       0x00000080	// move like a biker
#define     AIMOVE1_ARROW       0x00000100	// fly like an arrow
#define     AIMOVE1_STAR        0x00000200	// fly like a star, subject to ground
#define     AIMOVE1_BOMB        0x00000400	// fly like a bomb, subject to ground/wall etc
#define     AIMOVE1_NOMOVE      0x00000800	// don't move at all
#define     AIMOVE1_BOOMRANG    0x00001000	// boomrang
#define     MASK_AIMOVE1        0x0000FFFF

// A.I move2, affect terrain reflect
#define     AIMOVE2_NORMAL      0	// Current default style
#define     AIMOVE2_IGNOREHOLES 0x00010000	// don't avoid holes
#define     MASK_AIMOVE2        0xFFFF0000

// A.I. attack1, affect attacking style
#define     AIATTACK1_NORMAL      0	// Current default style
#define     AIATTACK1_LONG        0x00000001	// Long range first
#define     AIATTACK1_MELEE       0x00000002	// Melee attack first
#define     AIATTACK1_NOATTACK    0x00000004	// dont attack at all
#define     MASK_AIATTACK1        0x0000FFFF

//  A.I. attack2, affect Defending style
#define     AIATTACK2_NORMAL      0	// Current default style, don't dodge at all
#define     AIATTACK2_DODGE       0x00010000	// Use dodge animation to avoid attack
#define     AIATTACK2_DODGEMOVE   0x00020000	// Try to move in z direction if a jump attack is about to hit him
													  // and try to step back if a melee attack is about to hit him
#define     MASK_AIATTACK2        0xFFFF0000


typedef struct {
	short player_min_z;
	short player_max_z;
	short bgheight;
} s_player_min_max_z_bgheight;

extern s_player_min_max_z_bgheight player_min_max_z_bgheight;
#define PLAYER_MIN_Z (player_min_max_z_bgheight.player_min_z)
#define PLAYER_MAX_Z (player_min_max_z_bgheight.player_max_z)
#define BGHEIGHT (player_min_max_z_bgheight.bgheight)

// Note: the minimum Z coordinate of the player is important
// for several other drawing operations.
// movement restirctions are here!
//int                   PLAYER_MIN_Z            = 160;                          // 2-10-05  adjustable walking area
//int                   PLAYER_MAX_Z            = 232;                          // 2-10-05  adjustable walking area
//int                   BGHEIGHT                        = 160;                          // 2-10-05  adjustable BGHeight
//int         MAX_WALL_HEIGHT     =  1000;            // max wall height that an entity can be spawned on
#define		FRONTPANEL_Z		(PLAYER_MAX_Z+50)
#define     HUD_Z               (FRONTPANEL_Z+10000)
#define		HOLE_Z				(PLAYER_MIN_Z-46)
#define		NEONPANEL_Z			(PLAYER_MIN_Z-47)
#define		SHADOW_Z			(PLAYER_MIN_Z-48)
#define		SCREENPANEL_Z		(PLAYER_MIN_Z-49)
#define		PANEL_Z				(PLAYER_MIN_Z-50)
#define		MIRROR_Z			(PLAYER_MIN_Z-5)
#define		PIT_DEPTH			-250
#define		P2_STATS_DIST		180
#define		CONTACT_DIST_H		30	// Distance to make contact
#define		CONTACT_DIST_V		12
#define		GRAB_DIST			36	// Grabbing ents will be placed this far apart.
#define		GRAB_STALL			(GAME_SPEED * 8 / 10)

typedef enum {
	ATK_NORMAL = 0,
	ATK_NORMAL2,
	ATK_NORMAL3,
	ATK_NORMAL4,
	ATK_BLAST,
	ATK_BURN,
	ATK_FREEZE,
	ATK_SHOCK,
	ATK_STEAL, // Steal opponents life
	ATK_NORMAL5,
	ATK_NORMAL6,
	ATK_NORMAL7,
	ATK_NORMAL8,
	ATK_NORMAL9,
	ATK_NORMAL10,
	ATK_ITEM,
	MAX_ATKS
} attack_types;

#define     STA_ATKS            6	// default special attack types: blast burn shock freeze steal
#define     MAX_DOTS            10	// Max active dot effects.

#define		SCROLL_RIGHT		2
#define		SCROLL_DOWN			4
#define		SCROLL_LEFT			8
#define		SCROLL_UP			16
#define		SCROLL_BACK			1
#define		SCROLL_BOTH			(SCROLL_BACK|SCROLL_RIGHT)
#define		SCROLL_RIGHTLEFT	SCROLL_BOTH
#define		SCROLL_LEFTRIGHT    (SCROLL_LEFT|SCROLL_BACK)
#define		SCROLL_INWARD       32
#define     SCROLL_OUTWARD      64
#define		SCROLL_OUTIN		(SCROLL_OUTWARD|SCROLL_BACK)
#define		SCROLL_INOUT		(SCROLL_INWARD|SCROLL_BACK)
#define		SCROLL_UPWARD       128
#define     SCROLL_DOWNWARD     256
// blah, blah,

typedef enum {
	ANI_IDLE = 0,
	ANI_WALK,
	ANI_JUMP,
	ANI_LAND,
	ANI_ATTACK,
	ANI_ATTACK1 = ANI_ATTACK,
	ANI_ATTACK2,
	ANI_ATTACK3,
	ANI_ATTACK4, // Very important
	ANI_UPPER,
	ANI_BLOCK, // New block animation
	ANI_JUMPATTACK,
	ANI_JUMPATTACK1 = ANI_JUMPATTACK,
	ANI_JUMPATTACK2,
	ANI_JUMPATTACK3,
	ANI_GET,
	ANI_GRAB,
	ANI_GRABATTACK,
	ANI_GRABATTACK1 = ANI_GRABATTACK,
	ANI_GRABATTACK2,
	ANI_THROW,
	ANI_SPECIAL,
	ANI_SPECIAL1 = ANI_SPECIAL,
	ANI_SPECIAL2,	// Animation played for when pressing forward special
	ANI_SPECIAL3,	// special3 is an alias for jumpspecial
	ANI_JUMPSPECIAL = ANI_SPECIAL3,
	ANI_FREESPECIAL,
	ANI_FREESPECIAL1 = ANI_FREESPECIAL,
	ANI_FREESPECIAL2,
	ANI_FREESPECIAL3,
	ANI_FREESPECIAL4,	// More freespecials added
	ANI_FREESPECIAL5,	// More freespecials added
	ANI_FREESPECIAL6,	// More freespecials added
	ANI_FREESPECIAL7,	// More freespecials added
	ANI_FREESPECIAL8,	// More freespecials added
	ANI_SPAWN,	// 26-12-2004 new animation added here ani_spawn
	ANI_PICK,	// 7-1-2005 used when players select their character at the select screen
	ANI_UP,	// Mar 2, 2005 - Animation for when going up
	ANI_DOWN,	// Mar 2, 2005 - Animation for when going down
	ANI_SHOCK,	// Animation played when knocked down by shock attack
	ANI_BURN,	// Animation played when knocked down by burn attack
	ANI_SHOCKPAIN,	// Animation played when not knocked down by shock attack
	ANI_BURNPAIN,	// Animation played when not knocked down by shock attack
	ANI_GRABBED,	// Animation played when grabbed
	ANI_RUN,	// Animation played when a player is running
	ANI_RUNATTACK,	// Animation played when a player is running and presses attack
	ANI_RUNJUMPATTACK,	// Animation played when a player is running and jumps and presses attack
	ANI_ATTACKUP,	// u u animation
	ANI_ATTACKDOWN,	// d d animation
	ANI_ATTACKFORWARD,	// f f animation
	ANI_ATTACKBACKWARD,	// Used for attacking backwards
	ANI_DODGE,	// Used for up up / down down SOR3 dodge moves for players
	ANI_ATTACKBOTH,	// Used for when a player holds down attack and presses jump
	ANI_GRABFORWARD,	// New grab attack for when a player holds down forward/attack
	ANI_GRABFORWARD2,	// New second grab attack for when a player holds down forward/attack
	ANI_JUMPFORWARD,	// Attack when a player is moving and jumps
	ANI_GRABDOWN,	// Attack when a player has grabbed an opponent and presses down/attack
	ANI_GRABDOWN2,	// Attack when a player has grabbed an opponent and presses down/attack
	ANI_GRABUP,	// Attack when a player has grabbed an opponent and presses up/attack
	ANI_GRABUP2,	// Attack when a player has grabbed an opponent and presses up/attack
	ANI_SELECT,	// Animation that is displayed at the select screen
	ANI_DUCK,	// Animation that is played when pressing down in "platform" type levels
	ANI_FAINT,	// Faint animations for players/enemys by tails
	ANI_CANT,	// Can't animation for players(animation when mp is less than mpcost) by tails.
	ANI_THROWATTACK,	// Added for subtype projectile
	ANI_CHARGEATTACK,	// Plays when player releases attack1 after holding >= chargetime.
	ANI_VAULT,	// Now you can flip over people like in SOR.
	ANI_JUMPCANT,
	ANI_BURNDIE,
	ANI_SHOCKDIE,
	ANI_PAIN, //the pain anims must be in a sequence as they're used to calculate offsets in lcmHandleCommandAnim()
	ANI_PAIN1 = ANI_PAIN,
	ANI_PAIN2,
	ANI_PAIN3,
	ANI_PAIN4,
	ANI_PAIN5,
	ANI_PAIN6,
	ANI_PAIN7,
	ANI_PAIN8,
	ANI_PAIN9,
	ANI_PAIN10,
	ANI_FALL,
	ANI_FALL1 = ANI_FALL,
	ANI_FALL2,
	ANI_FALL3,
	ANI_FALL4,
	ANI_FALL5,
	ANI_FALL6,
	ANI_FALL7,
	ANI_FALL8,
	ANI_FALL9,
	ANI_FALL10,
	ANI_DIE,	// 29-12-2004 new animation added here ani_die
	ANI_DIE1 = ANI_DIE,
	ANI_DIE2,
	ANI_DIE3,
	ANI_DIE4,
	ANI_DIE5,
	ANI_DIE6,
	ANI_DIE7,
	ANI_DIE8,
	ANI_DIE9,
	ANI_DIE10,
	ANI_CHARGE,
	ANI_BACKWALK,
	ANI_SLEEP,
	ANI_FOLLOW,
	ANI_FOLLOW1 = ANI_FOLLOW,
	ANI_FOLLOW2,
	ANI_FOLLOW3,
	ANI_FOLLOW4,
	ANI_TURN,	// turn back/flip
	ANI_RESPAWN,	//now spawn works for players
	ANI_FORWARDJUMP,
	ANI_RUNJUMP,
	ANI_JUMPLAND,
	ANI_JUMPDELAY,
	ANI_HITWALL,
	ANI_GRABBACKWARD,
	ANI_GRABBACKWARD1 = ANI_GRABBACKWARD,
	ANI_GRABBACKWARD2,
	ANI_GRABWALK,
	ANI_GRABBEDWALK,
	ANI_GRABWALKUP,
	ANI_GRABBEDWALKUP,
	ANI_GRABWALKDOWN,
	ANI_GRABBEDWALKDOWN,
	ANI_GRABTURN,
	ANI_GRABBEDTURN,
	ANI_GRABBACKWALK,
	ANI_GRABBEDBACKWALK,
	ANI_SLIDE,	//Down + Jump animation.
	ANI_RUNSLIDE,	//Down + Jump while running.
	ANI_DUCKATTACK,
	ANI_RISE,
	ANI_RISE1 = ANI_RISE,
	ANI_RISE2,
	ANI_RISE3,
	ANI_RISE4,
	ANI_RISE5,
	ANI_RISE6,
	ANI_RISE7,
	ANI_RISE8,
	ANI_RISE9,
	ANI_RISE10,
	ANI_RISEB,
	ANI_RISES,
	ANI_BLOCKPAIN,	//If entity has this, it will play in place of "pain" when its blockpain is 1 and incoming attack is blocked.
	ANI_BLOCKPAIN1 = ANI_BLOCKPAIN,
	ANI_BLOCKPAIN2,
	ANI_BLOCKPAIN3,
	ANI_BLOCKPAIN4,
	ANI_BLOCKPAIN5,
	ANI_BLOCKPAIN6,
	ANI_BLOCKPAIN7,
	ANI_BLOCKPAIN8,
	ANI_BLOCKPAIN9,
	ANI_BLOCKPAIN10,
	ANI_BLOCKPAINB,
	ANI_BLOCKPAINS,
	ANI_CHIPDEATH,
	ANI_GUARDBREAK,
	ANI_RISEATTACK,	// Attack used for enemies when players are crowding around after knocking them down
	ANI_RISEATTACK1 = ANI_RISEATTACK,
	ANI_RISEATTACK2,
	ANI_RISEATTACK3,
	ANI_RISEATTACK4,
	ANI_RISEATTACK5,
	ANI_RISEATTACK6,
	ANI_RISEATTACK7,
	ANI_RISEATTACK8,
	ANI_RISEATTACK9,
	ANI_RISEATTACK10,
	ANI_RISEATTACKB,
	ANI_RISEATTACKS,
	ANI_WALKOFF,
	MAX_ANIS
} ani_types;
#define     ARG_FLOAT            0
#define     ARG_STRING           1
#define     ARG_INT              2

// perhaps outdated, now use separted flags for entity
#define     SUBJECT_TO_WALL      1
#define     SUBJECT_TO_HOLE      2
#define     SUBJECT_TO_OBSTACLE  4
#define     SUBJECT_TO_BORDER    8
#define     SUBJECT_TO_SCREEN    16
#define     SUBJECT_TO_MINZ      32
#define     SUBJECT_TO_MAXZ      48

//macros for drawing menu text, fits different font size

#ifdef _MSC_VER
#define _strmidx(f,s, ...) ((videomodes.hRes-font_string_width((f), s, __VA_ARGS__))/2)
#else
#define _strmidx(f,s, args...) ((videomodes.hRes-font_string_width((f), s, ##args))/2)
#endif
#define _colx(f,c) ((int)(videomodes.hRes/2+(c)*(font_monowidths[(f)]+1)))
#define _liney(f,l) ((int)(videomodes.vRes/2+(l)*(font_heights[(f)]+1)))
#ifdef _MSC_VER
#define _menutextm(f, l, shift, s, ...) font_printf(_strmidx(f,s, __VA_ARGS__)+(int)((shift)*(font_monowidths[(f)]+1)), _liney(f,l), (f), 0, s, __VA_ARGS__)
#define _menutext(f, c, l, s, ...) font_printf(_colx(f,c), _liney(f,l), (f), 0, s, __VA_ARGS__)
#else
#define _menutextm(f, l, shift, s, args...) font_printf(_strmidx(f,s, ##args)+(int)((shift)*(font_monowidths[(f)]+1)), _liney(f,l), (f), 0, s, ##args)
#define _menutext(f, c, l, s, args...) font_printf(_colx(f,c), _liney(f,l), (f), 0, s, ##args)
#endif

/*
#define     ICO_NORMAL           0
#define     ICO_PAIN             1
#define     ICO_DIE              2
#define     ICO_GET              3
#define     ICO_WEAPON           4*/

// model flags
#define     MODEL_NO_COPY         0x00000001	//dont copy anything from original model
#define     MODEL_NO_WEAPON_COPY  0x00000002	//dont copy weapon list from original model

#define lut_mul ((level && current_palette)?(level->blendings[current_palette-1][BLEND_MULTIPLY]):(blendings[BLEND_MULTIPLY]))
#define lut_screen ((level && current_palette)?(level->blendings[current_palette-1][BLEND_SCREEN]):(blendings[BLEND_SCREEN]))
#define lut_overlay ((level && current_palette)?(level->blendings[current_palette-1][BLEND_OVERLAY]):(blendings[BLEND_OVERLAY]))
#define lut_hl ((level && current_palette)?(level->blendings[current_palette-1][BLEND_HARDLIGHT]):(blendings[BLEND_HARDLIGHT]))
#define lut_dodge ((level && current_palette)?(level->blendings[current_palette-1][BLEND_DODGE]):(blendings[BLEND_DODGE]))
#define lut_half ((level && current_palette)?(level->blendings[current_palette-1][BLEND_HALF]):(blendings[BLEND_HALF]))
#define lut ((level && current_palette)?(level->blendings[current_palette-1]):(blendings))

#define ABS(x) ((x)>0?(x):(-(x)))

#define set_attacking(e) e->attacking = 1;\
						 e->idling = 0;

#define set_jumping(e)   e->jumping = 1;\
						 e->idling = 0;

#define set_charging(e)  e->charging = 1;\
						 e->idling = 0;

#define set_getting(e)   e->getting = 1;\
						 e->idling = 0;

#define set_blocking(e)  e->blocking = 1;\
						 e->idling = 0;

#define set_turning(e)  e->turning = 1;\
						e->idling = 0;

#define is_frozen(e)     ((textbox && e->modeldata.type != TYPE_TEXTBOX) || \
						 (smartbomber && e != smartbomber && e->modeldata.type != TYPE_TEXTBOX) ||(self->frozen&&self->freezetime > borTime))

#define expand_time(e)   if(e->stalltime>0) e->stalltime++;\
						 if(e->releasetime>0)e->releasetime++;\
						 if(e->nextanim>0)e->nextanim++;\
						 if(e->nextthink>0)e->nextthink++;\
						 if(e->magictime>0)e->magictime++;\
						 if(e->guardtime>0)e->guardtime++;\
						 if(e->toss_time>0)e->toss_time++;\
						 if(e->freezetime>0 && (textbox || smartbomber))e->freezetime++;\
						 if(e->mpchargetime>0)e->mpchargetime++;\
						 if(e->invinctime>0) e->invinctime++;\
						 if(e->sealtime>0) e->sealtime++;
/*                       if(e->dot_time>0) e->dot_time++;\
						 if(e->dot_cnt>0) e->dot_cnt++;
*/

#define freezeall        (smartbomber || textbox)

#define is_projectile(e) (e->modeldata.type == TYPE_SHOT || e->model->subtype == SUBTYPE_ARROW || e->owner)

#define check_range(self, target, animnum) \
		 ( target && \
		  (self->direction ? \
		  target->x > self->x+self->modeldata.animation[animnum]->range[0] &&\
		  target->x < self->x+self->modeldata.animation[animnum]->range[1]\
		:\
		  target->x < self->x-self->modeldata.animation[animnum]->range[0] &&\
		  target->x > self->x-self->modeldata.animation[animnum]->range[1])\
		  && target->z - self->z > self->modeldata.animation[animnum]->range[2] \
		  && target->z - self->z < self->modeldata.animation[animnum]->range[3] \
		  && (target->a - self->a) > self->modeldata.animation[animnum]->range[4] \
		  && (target->a - self->a) < self->modeldata.animation[animnum]->range[5] \
		  && (target->base - self->base) > self->modeldata.animation[animnum]->range[6] \
		  && (target->base - self->base) < self->modeldata.animation[animnum]->range[7] \
		  )\

#define check_range_both(self, target, animnum) \
		 ( target && \
		  ((target->x > self->x+self->modeldata.animation[animnum]->range[0] &&\
			target->x < self->x+self->modeldata.animation[animnum]->range[1])\
		||\
		   (target->x < self->x-self->modeldata.animation[animnum]->range[0] &&\
			target->x > self->x-self->modeldata.animation[animnum]->range[1]))\
		  && target->z - self->z > self->modeldata.animation[animnum]->range[2] \
		  && target->z - self->z < self->modeldata.animation[animnum]->range[3] \
		  && (target->a - self->a) > self->modeldata.animation[animnum]->range[4] \
		  && (target->a - self->a) < self->modeldata.animation[animnum]->range[5] \
		  && (target->base - self->base) > self->modeldata.animation[animnum]->range[6] \
		  && (target->base - self->base) < self->modeldata.animation[animnum]->range[7] \
		  )\


#define tobounce(e) (e->animation->bounce && diff(0, e->tossv) > 2 && \
					 !((autoland == 1 && e->damage_on_landing == -1) ||e->damage_on_landing == -2))

#define getpal ((current_palette&&level)?(level->palettes[current_palette-1]):pal)

#define canbegrabbed(self, other) \
		(other->animation->vulnerable[other->animpos] && \
		 (!self->animation->move || self->animation->move[self->animpos] == 0) && \
		 (!self->animation->movez || self->animation->movez[self->animpos] == 0 ) && \
		 !(other->nograb || other->invincible || other->link || \
		   other->model->animal || inair(other) || \
		  (self->modeldata.type == TYPE_PLAYER && other->modeldata.type == TYPE_PLAYER && savedata.mode)))

#define cangrab(self, other) \
		((other->model->antigrab - self->model->grabforce + \
		  (other->model->paingrab?(other->model->paingrab-other->inpain):0)<=0) &&\
		 canbegrabbed(self, other) && \
		 !inair(self) && \
		 diff(other->a, self->a) <= 0.1)

#define unfrozen(e) \
		ent_set_colourmap(e, e->map);\
		e->frozen = 0;\
		e->freezetime = 0;

#define validanim(e, a) ((e)->modeldata.animation[a]&&(e)->modeldata.animation[a]->numframes)

//#define     MAX_MOVES             16
//#define     MAX_MOVE_STEPS        16

#define MENU_PACK_FILENAME "Menu.xxx"

#pragma pack (4)

typedef struct {
	unsigned int compatibleversion;
	short gamma;
	short brightness;
	char usesound;	// Use SB
	unsigned short soundrate;	// SB freq
	short soundvol;	// SB volume
	char usemusic;	// Play music
	short musicvol;	// Music volume
	short effectvol;	// Sound fx volume
	char soundbits;	// SB bits
	char usejoy;
	char mode;		// Mode now saves
	char windowpos;
	int keys[MAX_PLAYERS][12];
	char showtitles;
	char videoNTSC;
	char screen[7][2];	// Screen Filtering/Scaling Effects
	char logo;
	char uselog;
	char debuginfo;	// FPS, Memory, etc...
	char fullscreen;	// Window or Full Screen Mode
	char stretch;		// Stretch (1) or preserve aspect ratio (0) in fullscreen mode
	char usegl;		// 1 if OpenGL is preferred over SDL software blitting
	float glscale;		// Scale factor for OpenGL
	char glfilter;	// Simple or bilinear scaling
} s_savedata;

typedef struct {
	unsigned int compatibleversion:32;
	char dName[MAX_NAME_LEN + 1];	// Difficulty Name
	unsigned short level:16;	// Level Number
	unsigned short stage:16;	// Stage
	unsigned char pLives[MAX_PLAYERS];	// Player Lives Left
	unsigned char pCredits[MAX_PLAYERS];	// Player Credits Left
	unsigned int pScores[MAX_PLAYERS];	// Player Scores
	unsigned char credits:8;	// Number Of Credits
	unsigned short times_completed:16;
	unsigned short which_set:16;
	//-------------------new strict save features-----------------------
	char flag:8;		// 0 useless slot 1 only load level number 2 load player info and level
	char pName[MAX_PLAYERS][MAX_NAME_LEN + 1];	// player names
	int pSpawnhealth[MAX_PLAYERS];	// hit points left
	int pSpawnmp[MAX_PLAYERS];	// magic points left
	char pWeapnum[MAX_PLAYERS];	// weapon
	char pColourmap[MAX_PLAYERS];	// colour map
} s_savelevel;


typedef struct {
	unsigned int compatibleversion:32;
	unsigned int highsc[10];
	char hscoren[10][MAX_NAME_LEN + 1];
} s_savescore;


typedef struct {
	float dropv[3];		// fly height/x/z if the target is knoced down
	float grab_distance;	// suck target near by

	int staydown[3];	// [0] = Add to rise delay. [1] = Add to rise attack delay.
	int freezetime;
	int maptime;
	int sealtime;
	int dot;		//Dot mode.
	int dot_index;		//Dot index.
	int dot_time;		//Dot time to expire.
	int dot_force;		//Dot amount per tick.
	int dot_rate;		//Dot tick delay.
	int hitsound;		// Sound effect to be played when attack hits opponent
	int hitflash;		// Custom flash for each animation, model id
	int blockflash;		// Custom bflash for each animation, model id
	int blocksound;		// Custom sound for when an attack is blocked

	short attack_force;
	short attack_coords[5];	// stick on the only one victim
	short jugglecost;	// cost for juggling a falling ent
	short guardcost;	// cost for blocking an attack
	short pause_add;	// Flag to determine if an attack adds a pause before updating the animation
	
	signed char counterattack;
	signed char no_pain;
	signed char no_flash;	// Flag to determine if an attack spawns a flash or not
	signed char no_block;	// Flag to determine if an attack is blockable (default 0 - blockable)
	signed char grab;
	signed char force_direction;	// 0 dont care, 1 same direction as attacker, -1 opposite drection as attacker, 2 right, -2 left
	signed char blast;
	signed char freeze;
	signed char steal;
	signed char forcemap;
	signed char seal;
	signed char otg;		// Over The Ground. Gives ground projectiles the ability to hit lying ents.
	signed char attack_drop;	// now be a knock-down factor, how many this attack will knock victim down
	signed char attack_type;
	signed char damage_on_landing;	// same as throw damage type
	
} s_attack;

typedef struct {
	int model_index;
	short numframes:16;
	int loop[3];		// Animation loop (0 = loop on/off, 1 = Loop to frame, 2 = Loop end frame).
	short height:16;	// entity's height during animation
	short tossframe:16;	// Used to determine which frame will toss a bomb/grenade
	short shootframe:16;
	short throwframe:16;
	short throwa:16;	//      Used for setting the "a" at which weapons are spawned
	// various entity model id, knife/star/bomb etc
	int custknife;
	int custstar;
	int custbomb;		// Used for new projectile bomb
	int custpshotno;
	int subentity;		// Store the sub-entity's name for further use
	char fastattack:8;	// Flag to determine if the opponent uses their pain time
	short energycost[3];	// 1-10-05 to adjust the amount of energy used for specials. 05072010: Made array with mponly. 0 = Energycost, 1 = MPonly, 2 = Disable flag (see check_energy function).
	float chargetime;	// charge time for an animation
	short jumpframe:16;
	float jumpv;		// moveflag                  // So movement forward can be specified for jumpframes
	float jumpx;		// override move forward value
	float jumpz;		// override move z value
	int jumpd;		// Index of dust entity to spawn on liftoff of jumpframe.
	float bounce;		// -tossv/bounce = new tossv
	float dive[2];		// new dive kick by tails
	int *soundtoplay;	// each frame can have a sound
	int *sprite;		// sprite[set][framenumber]
	short *delay;
	short *move;
	short *movez;
	short *movea;
	short *seta;		// Now characters can have a custom "a" value
	short *vulnerable;
	short (*bbox_coords)[5];
	int *shadow;
	unsigned char *idle;	// Allow free move
	short (*shadow_coords)[2];	// x, z offset of shadow
	s_drawmethod **drawmethods;
	char attackone:8;
	s_attack **attacks;
	float (*platform)[8];	// Now entities can have others land on them
	int range[8];		// Use for attacks; xmin, xmax, zmin, zmax, amin, amax, basemin, basemax
	short flipframe:16;	// Turns entities around on the desired frame
	short followanim:16;	// use which FOLLOW anim?
	char followcond:8;	// conditions under which to use a followup
	short counterframe[4];	// 0,1; Counter frame, 2 counter cond, 3 counterdam
	char cancel:8;		// Cancel anims with freespecial
	short *weaponframe;	// Specify with a frame when to switch to a weapon model
	short quakeframe[4];	// Specify with a frame, repeat, quake (4 is highest)
	float *spawnframe;	// Spawn the subentity as its default type. {frame} {x} {z} {a} {relative?}
	float *summonframe;	// Summon the subentity as an ally, only one though {frame} {x} {z} {a} {relative?}
	short unsummonframe:16;	// Un-summon the entity
	short landframe[2];	// 0 frame switch to when land, 1 dust to spawn.
	short dropframe:16;	// if tossv < 0, this frame will be set
	short animhits:16;	// Does the attack need to hit before cancel is allowed?
} s_anim;

typedef struct {
	int mode;
	float factor;
	int cap_min;
	int cap_max;
	int range_min;
	int range_max;
} s_edelay;

struct animlist {
	s_anim *anim;
	struct animlist *next;
};
typedef struct animlist s_anim_list;
extern s_anim_list *anim_list;

typedef enum {
	horizontalbar = 0,
	verticalbar = 1,
} barorient;

typedef enum {
	valuebar = 0,
	percentagebar = 1,
} bartype;

typedef struct {
	int (*colourtable)[11];	//0 default backfill 1-10 foreground colours
	int barlayer;
	int backlayer;
	int borderlayer;
	int shadowlayer;
	
	short offsetx;
	short offsety;
	short sizex;
	short sizey;
	char noborder:1;
	char direction:1;	//0) left to right or botom to top 1) reversed
	bartype type:1;
	barorient orientation:1;
} s_barstatus;

typedef enum {
	LSTYPE_NONE = 0,
	LSTYPE_BAR = 1,
	LSTYPE_BACKGROUND = 2,
} loadingScreenType;

typedef struct {
	loadingScreenType set;
	/*set determines how loading screen would be.
	   - 0 = no loading screen.
	   - 1 = background and status bar.
	   - 2 = background only.
	   - 3 = status bar only.
	 */
	char tf;		//determines used font number for "LOADING" text (last element in command, moved here because of alignment)
	/*
	   - 0 = font.gif
	   - 1 = font2.gif
	   - 2 = font3.gif
	   - 3 = font4.gif */
	short bx;		//determines x and y coordinates of loading bar top left's location respectively
	short by;
	short bsize;		// length of bar in pixels
	short tx;		//determines x and y coordinates of "LOADING" text location respectively.
	short ty;
	short refreshMs;	// modder defined number of milliseconds in which the screen is updated while loading
} s_loadingbar;

typedef struct {
	Script *animation_script;	//system generated script
	Script *update_script;	//execute when update_ents
	Script *think_script;	//execute when entity thinks.
	Script *takedamage_script;	//execute when taking damage.
	Script *ondeath_script;	//execute when killed in game.
	Script *onkill_script;	//execute when removed from play.
	Script *onpain_script;	//Execute when put in pain animation.
	Script *onfall_script;	//execute when falling.
	Script *onblocks_script;	//execute when blocked by screen.
	Script *onblockw_script;	//execute when blocked by wall.
	Script *onblocko_script;	//execute when blocked by obstacle.
	Script *onblockz_script;	//execute when blocked by Z.
	Script *onblocka_script;	//execute when "hit head".
	Script *onmovex_script;	//execute when moving along X axis.
	Script *onmovez_script;	//execute when moving along Z axis.
	Script *onmovea_script;	//execute when moving along A axis.
	Script *didhit_script;	//execute when attack hits another.
	Script *onspawn_script;	//execute when spawned.
	Script *key_script;	//execute when entity's player presses a key
	Script *didblock_script;	//execute when blocking attack.
	Script *ondoattack_script;	//execute when attack passes do_attack checks.
} s_scripts;

typedef struct {
	//global script
	Script level_script;		//execute when level start
	Script endlevel_script;		//execute when level finished
	Script update_script;		//execute when ingame update
	Script updated_script;		//execute when ingame update finished
	Script key_script_all;		//keyscript for all players
	Script timetick_script;		//time tick script.
	//player script
	Script score_script[4];		//execute when add score, 4 players
	Script key_script[4];		//key listeners, lol
	Script join_script[4];		//player join scripts
	Script respawn_script[4];	//player respawn scripts
	Script pdie_script[4];		//player death scripts
} s_game_scripts;

extern s_game_scripts game_scripts;

typedef enum {
	MF_NONE = 0,
	MF_ANIMLIST = 1,
	MF_COLOURMAP = 2,
	MF_PALETTE = 4,
	MF_WEAPONS = 8,
	MF_BRANCH = 16,
	MF_ANIMATION = 32,
	MF_DEF_FACTORS = 64,
	MF_DEF_PAIN = 128,
	MF_DEF_KNOCKDOWN = 256,
	MF_DEF_BLOCKPOWER = 512,
	MF_DEF_BLOCKTRESHOLD = 1024,
	MF_DEF_BLOCKRATIO = 2048,
	MF_DEF_BLOCKTYPE = 4096,
	MF_OFF_FACTORS = 8192,
	MF_SPECIAL = 16384,
	MF_SMARTBOMB = 32768,
	MF_SCRIPTS = 65536,
} ModelFreetype;
#define MF_ALL 0x1FFFF

typedef struct {
	int (*special)[MAX_SPECIAL_INPUTS];	// Stores freespecials
	int (*weapon)[MAX_WEAPONS];	// weapon model list

	unsigned char *palette;	// original palette for 32/16bit mode
	unsigned char *colourmap[MAX_COLOUR_MAPS];
	char *name;
	char *path;		// Path, so scripts can dynamically get files, sprites, sounds, etc.
	char *branch;		//level branch name
	float *defense_factors;	//basic defense factors: damage = damage*(1-def)
	float *defense_pain;	//Pain factor (like nopain) for defense type.
	float *defense_knockdown;	//Knockdowncount (like knockdowncount) for attack type.
	float *defense_blockpower;	//If > unblockable, this attack type is blocked.
	float *defense_blockthreshold;	//Strongest attack from this attack type that can be blocked.
	float *defense_blockratio;	//% of damage still taken from this attack type when blocked.
	float *defense_blocktype;	//0 = HP, 1=MP, 2=both taken when this attack type is blocked.
	float *offense_factors;	//basic offense factors: damage = damage*(1+def)
	s_attack *smartbomb;
	s_anim **animation;
	
	int health;
	int mp;			// mp's variable for mpbar by tails
	int makeinv;		// Option to spawn player invincible >0 blink <0 noblink
	int riseinv;		// how many seconds will the character become invincible after rise >0 blink, <0 noblink
	int multiple;		// So you can control how many points are given for hitting opponents
	int icon;
	int iconpain;		// 20-1-2005   New icons
	int iconget;		// 20-1-2005   New icons
	int icondie;		// 20-1-2005   New icons
	int iconw;		// icon for the weapon like in beat of fighting by tails
	int iconmp[3];		// icon for the mpbar 3 levels
	int parrow[MAX_PLAYERS][3];	// Image to be displayed when player spawns invincible
	int setlayer;		// Used for forcing enities to be displayed behind
	int diesound;
	
	int index;
	// these are model id of various stuff
	int project;
	int rider;		// 7-1-2005 now every "biker" can have a new driver!
	int knife;		// 7-1-2005 now every enemy can have their own "knife" projectile
	int pshotno;		// 7-1-2005 now every enemy can have their own "knife" projectile
	int star;		// 7-1-2005 now every enemy can have their own "ninja star" projectiles
	int bomb;		// New projectile type for exploding bombs/grenades/dynamite
	int flash;		// Now each entity can have their own flash
	int bflash;		// Flash that plays when an attack is blocked
	int dust[3];		// Dust spawn (0 = Fall land, 1 = Jumpland, 2 = Jumpstart.)
	int grabforce;		// grab factor, antigrab - grabforce <= 0 means can grab
	int sight[6];		// Sight ranges, xmin, xmax, zmin, zmax, amin, amax
	int jugglepoints[2];	// juggle points. [0] = current [1] = max total
	int guardpoints[2];	// guard points. [0] = current [1] = max total

	unsigned int aiattack;	// attack/defend style
	unsigned int aimove;	// move style
	unsigned int offscreenkill;	// for biker, arrow, etc
	unsigned int score;
	
	float stats[20];	// Parameters that do nothing on their own.
	float scroll;		// Autoscroll like panel entity.

	float speed;
	float grabdistance;	// 30-12-2004   grabdistance varirable adder per character
	float jumpspeed;	// normal jump foward speed, default to max(1, speed)
	float jumpheight;	// 28-12-2004   Jump height variable added per character
	float grabwalkspeed;
	float runspeed;		// The speed the character runs at
	float runjumpheight;	// The height the character jumps when running
	float runjumpdist;	// The distance the character jumps when running
	float throwheight;	// The height at which an opponent can now be adjusted
	float throwdist;	// The distance an opponent can now be adjusted
	float lifespan;		// lifespan count down
	float knockdowncount;	// the knock down count for this entity
	float antigravity;	//antigravity : gravity * (1- antigravity)
	
	short mpstableval;	// MP Stable target.
	short aggression;	// For enemy A.I.
	short risetime[2];	// 0 = Rise delay, 1 = Riseattack delay.
	short sleepwait;
	short counter;	// counter of weapons by tails
	short type;
	short thold;		// The entities threshold for block
	short blockodds;	// Odds that an enemy will block an attack (1 : blockodds)
	short throwframewait;	// The frame victim is thrown during ANIM_THROW, added by kbandressen 10/20/06
	short specials_loaded;	// Stores how many specials have been loaded
	short valid_special;	// Used for setting when a valid special has been found
	short height;	// Used to set height of player in pixels
	short turndelay;	// turn delay
	short stealth[2];	// 0 = Entity's invisibility to AI. 1 = AI ability to see through stealth.

	//---------------new A.I. switches-----------
	short hostile;	// specify hostile types
	short candamage;	// specify types that can be damaged by this entity
	short projectilehit;	// specify types that can be hit by this entity if it is thrown
	
	short throwdamage;	// 1-14-05  adjust throw damage
	short hpx;
	short hpy;
	short iconx;
	short icony;
	short namex;
	short namey;
	
	unsigned char shootnum;	// counter of shots by tails
	unsigned char reload;	// reload max shots by tails
	char weapnum;
	char secret;
	char weaploss[2];	// Determines possibility of losing weapon.
	char ownweapons;	// is the weapon list own or share with others
	char reactive;	// Used for setting the "a" at which weapons are spawned
	char typeshot;	// see if weapon is a gun or knife by tails
	char animal;		// see is the weapon is a animal by tails
	char nolife;		// Feb 25, 2005 - Variable flag to show life 0 = no, else yes
	char dofreeze;	// Flag to freeze all enemies/players while special is executed
	char noquake;		// Flag to make the screen shake when entity lands 1 = no, else yes
	char ground;		// Flag to determine if enemy projectiles only hit the enemy when hitting the ground
	char bounce;		// Flag to determine if bounce/quake is to be used.
	char subtype;
	char fmap;		// Corresponds to which remap to use for when a character is frozen
	char komap[2];		// Remap to use when KO'd
	char hmap1;		//Bottom range of remaps unavailable at select screen.
	char hmap2;		//Top range of remaps unavailable at select screen.
	char alpha;		// New alpha variable to determine if the entity uses alpha transparency
	char toflip;		// Flag to determine if flashes flip or not
	char shadow;
	char gfxshadow;	// use current frame to create a shadow
	char aironly;		// Used to determine if shadows will be shown when jumping only
	char nomove;		// Flag for static enemies
	char noflip;		// Flag to determine if static enemies flip or stay facing the same direction
	char nodrop;		// Flag to determine if enemies can be knocked down
	char nodieblink;	// Flag to determine if blinking while playing die animation
	char holdblock;	// Continue the block animation as long as the player holds the button down
	char nopassiveblock;	// Don't auto block randomly
	char blockback;	// Able to block attacks from behind
	char noatflash;	// Flag to determine if attacking characters attack spawns a flash
	char runupdown;	// Flag to determine if a player will continue to run while pressing up or down
	char runhold;		// Flag to determine if a player will continue to run if holding down forward when landing
	char remove;		// Flag to remove a projectile on contact or not
	char jumpmovex;	// low byte: 0 default 1 flip in air, 2 move in air, 3 flip and move
	char jumpmovez;	// 2nd byte: 0 default 1 zjump with flip(not implemented yet) 2 z jump move in air, 3 1+2
	char grabfinish;	// wait for grab animation to finish before do other actoins
	char antigrab;	// anti-grab factor
	char facing;		// 0 no effect, 1 alway right, 2 always left, 3, affected by level dir
	char grabback;	// Flag to determine if entities grab images display behind opponenets
	char grabturn;
	char paingrab;	// Can only be grabbed when in pain
	char maps_loaded;	// Used for player colourmap selecting
	char unload;		// Unload model after level completed?
	char falldie;		// Play die animation?
	char globalmap;	// use global palette for its colour map in 24bit mode
	char nopain;
	char summonkill;	// kill it's summoned entity when died;  0. dont kill 1. kill summoned only 2. kill all spawned entity
	char combostyle;
	char blockpain;
	char atchain[MAX_ATCHAIN];
	char chainlength;
	char credit;
	char escapehits;	// Escape spammers!
	char chargerate;	// For the charge animation
	char guardrate;	// Rate for guardpoints recover.
	char mprate;		// For time-based mp recovery.
	char mpdroprate;	// Time based MP loss.
	char mpstable;	// MP stable type.
	char riseattacktype;
	char mpswitch;	// switch between reduce or gain mp for mpstabletype 4
	char model_flag;	//used to judge some copy method when setting new model to an entity
	// movement flags
	char subject_to_wall;
	char subject_to_platform;
	char subject_to_obstacle;
	char subject_to_hole;
	char subject_to_gravity;
	char subject_to_screen;
	char subject_to_minz;
	char subject_to_maxz;
	char no_adjust_base;	// dont change base to 0 automatically
	char instantitemdeath;	// no delay before item suicides
	char isSubclassed;
	
	ModelFreetype freetypes;
	s_scripts scripts;
	s_barstatus hpbarstatus;
	s_edelay edelay;	// Entity level delay adjustment.
} s_model;

typedef struct {
	char *name;
	char *path;
	s_model *model;
	char loadflag;
	char selectable;
} s_modelcache;
extern s_modelcache *model_cache;


typedef struct entity {
	struct entity *link;	// Used to link 2 entities together.
	struct entity *owner;	// Added for "hitenemy" flag so projectile recognizes its owner
	struct entity *grabbing;	// Added for "platform level" layering
	struct entity *weapent;
	struct entity *parent;	//Its spawner
	struct entity *subentity;	//store the sub entity
	struct entity *opponent;
	struct entity *lasthit;
	struct entity *hithead;	// when a player jumps and hits head on the bottom of a platform
	struct entity *bound;	// ignore trymove method, follow this entity
	struct entity *landed_on_platform;
	struct entity *dot_owner[MAX_DOTS];	//Dot owner.

	void (*think) ();
	void (*takeaction) ();
	int (*takedamage) (struct entity *, s_attack *);
	int (*trymove) (float, float);
	
	unsigned char *colourmap;
	
	ScriptVariant *entvars;

	s_model *defaultmodel;	// this is the default model
	s_model *model;		// current model
	
	s_anim *animation;

	
	float *defense_factors;	//defense factors: damage = damage*(1-def)
	float *defense_pain;	//Pain factor (like nopain) for defense type.
	float *defense_knockdown;	//Knockdowncount (like knockdowncount) for attack type.
	float *defense_blockpower;	//If > unblockable, this attack type is blocked.
	float *defense_blockthreshold;	//Strongest attack from this attack type that can be blocked.
	float *defense_blockratio;	//% of damage still taken from this attack type when blocked.
	float *defense_blocktype;	//0 = HP, 1=MP, 2=both taken when this attack type is blocked.
	float *offense_factors;	//offense factors: damage = damage*(1+def)
	
	float x;		// X
	float z;		// Depth
	float a;		// Altitude
	float xdir;
	float zdir;
	float base;		// Default altitude
	float altbase;		// Altitude affected by movea
	float tossv;		// Effect of gravity
	float jumpz;
	float jumpx;
	float jumpv;
	float stats[20];	// Parameters that do nothing on their own.
	float lifespancountdown;	// life span count down
	float antigravity;	// gravity*(1-antigravity)
	float knockdowncount;
	
	int health;		// current hp
	int mp;			// current mp
	int oldhealth;
	int oldmp;		//mp's variable for mp for players by tails
	int sortid;		// id for sprite queue sort
	int itemhealth;		// Now items spawned can have their properties changed
	int dot[MAX_DOTS];	//Dot mode.
	int dot_atk[MAX_DOTS];	//Dot attack type.
	int dot_force[MAX_DOTS];	//Dot amount.
	int dot_rate[MAX_DOTS];	//Dot delay per tick.
	int dot_cnt[MAX_DOTS];	//Dot time of next tick.
	
	// ---------------------- action times -------------------------------
	unsigned int lastmove;
	unsigned int lastdir;
	unsigned int timestamp;
	unsigned int releasetime;
	unsigned int toss_time;	// Used by gravity code
	unsigned int stalltime;
	unsigned int combotime;	// For multiple-hit combo
	unsigned int movetime;	// For special move
	unsigned int freezetime;	// Used to store at what point the a frozen entity becomes unfrozen
	unsigned int maptime;	// used by forcemap
	unsigned int sealtime;	// used by seal (stops special moves).
	unsigned int dot_time[MAX_DOTS];	//Dot time to expire.
	unsigned int magictime;
	unsigned int guardtime;
	unsigned int nextanim;
	unsigned int nextthink;
	unsigned int pain_time;
	unsigned int mpchargetime;	// For the CHARGE animation
	unsigned int sleeptime;	// For the SLEEP animation
	unsigned int rushtime;	// rush combo timer
	unsigned int knockdowntime;	// count knock down hit
	unsigned int invinctime;	// Used to set time for invincibility to expire
	unsigned int turntime;
	unsigned int staydown[3];	// [0] = Extra time before next rise. [1] = Extra time before next rise attack. [3] = Stalltime placeholder for riseattack.

	s_drawmethod drawmethod;
	s_scripts scripts;
	s_model modeldata;	// model data copyied here
	
	short itemplayer_count;
	short boss;
	short item;		// item model id

	short jumpid;
	short animpos;
	short lastanimpos;	// Used by AI
	short animnum;	// animation id
	
	short attack_id;
	short hit_by_attack_id;
	//struct entity   *thrower;
	short bindoffset[4];	// x, z, a, dir; int is ok
	short bindanim;	// keep the bound entities same animation id
	
	unsigned short rush[2];	// rush combo and max combo
	
	char escapecount;	// For escapehits
	char update_mark;
	char damage_on_landing;
	char damagetype;	// used for set death animation or pain animation
	char map;		// Stores the colourmap for restoring purposes
	
	
	char spawntype;	// Type of spawn. 1 = Level spawn. 0 for all else (subject to change).
	char exists;		// flag to determine if it is a valid entity.
	char reactive;	// Used for setting the "a" at which weapons are spawned
	char ptype;
	char playerindex;
	
	char dying;		// Coresponds with which remap is to be used for the dying flash
	char per1;		// Used to store at what health value the entity begins to flash
	char per2;		// Used to store at what health value the entity flashes more rapidly
	char direction;	// 0=left 1=right
	char nograb;		// Some enemies cannot be grabbed (bikes) - now used with cantgrab as well
	char movestep;
	char name[MAX_NAME_LEN + 1];	// this is display name
	
	char itemmap;		// Now items spawned can have their properties changed
	char itemtrans;	// alpha effect of item
	char itemalias[MAX_NAME_LEN + 1];	// Now items spawned can have their properties changed
	unsigned char combostep[MAX_SPECIAL_INPUTS];	// merge into an array to clear up some code

	
	
	//------------------------- a lot of flags ---------------------------
	//TODO check if these can be set to 1 bit.

	char seal;		//1 = No specials.
	char dead;
	char jumping;		// Stuff useful for AI
	char idling;
	char drop;
	char attacking;
	char getting;
	char turning;
	char charging;
	char blocking;
	char falling;
	char running;		// Flag to determine if a player is running
	char grabwalking;	// a flag for grabwalk check
	char inpain;		// playing pain animation
	char frozen;		// Flag to determine if an entity is frozen
	char blink;
	char invincible;	// Flag used to determine if player is currently invincible
	char autokill;	// Kill on end animation
	char remove_on_attack;
	char cantfire;	// Flag to determine if another shot can be fired that costs energy
	char tocost;		// Flag to determine if special costs life if doesn't hit an enemy
	char noaicontrol;	// pause A.I. control
	char projectile;
	char toexplode;	// Needed to determine if the projectile is a type that will explode (bombs, dynamite, etc)
	char animating;	// Set by animation code
	char arrowon;		// Flag to display parrow/parrow2 or not
	char pathblocked;
	
	
} entity;



typedef struct {
	char name[MAX_NAME_LEN + 1];
	char colourmap:8;
	unsigned int score:32;
	unsigned char lives:8;
	unsigned char credits:8;
	entity *ent;
	u32 keys;
	u32 newkeys;
	u32 playkeys;
	u32 releasekeys;
	int spawnhealth;
	int spawnmp;
	char joining:8;
	char hasplayed:8;
	char weapnum:8;
} s_player;


typedef struct {
	s_sprite *sprite_normal;
	s_sprite *sprite_neon;
	s_sprite *sprite_screen;
} s_panel;

typedef struct {
	char *sprite_normal;
	char *sprite_neon;
	char *sprite_screen;
} s_panel_filenames;

typedef struct s_spawn_script_cache_node {
	char *filename;
	Script *cached_spawn_script;
	struct s_spawn_script_cache_node *next;
} s_spawn_script_cache_node;

typedef struct s_spawn_script_list_node {
	Script *spawn_script;
	struct s_spawn_script_list_node *next;
} s_spawn_script_list_node;


typedef struct {
	short at:16;
	char wait:8;
	char nojoin:8;		// dont allow new hero to join
	char spawnplayer_count:8;	// spawn this entity according to the amount of players
	char palette:8;		//change system palette to ...
	short groupmin:16;
	short groupmax:16;
	short scrollminz:16;	// new scroll limit
	short scrollmaxz:16;
	short blockade:16;	//limit how far you can go back
	short light[2];		// x, z  light direction, for gfx shadow
	char shadowcolor:8;	// -1 no shadow
	int shadowalpha:8;
	char music[128];
	float musicfade;
	u32 musicoffset;
	char *name;		// must be a name in the model list, so just reference
	int index;		// model id
	int itemindex;		// item model id
	int weaponindex;	// the spawned entity with an weapon item, this is the id of the item model
	char alpha:8;		// Used for alpha effects
	char boss:8;
	char flip:8;
	char itemtrans:8;
	char itemmap:8;
	char colourmap:8;
	char dying:8;		// Used for the dying flash animation
	char per1:8;		// Used to store at what health value the entity begins to flash
	char per2:8;		// Used to store at what health value the entity flashes more rapidly
	char nolife:8;		// So nolife can be overriden for all characters
	char itemplayer_count:8;	// spawn the item according to the amount of players
	s_model *itemmodel;
	s_model *model;
	char alias[MAX_NAME_LEN + 1];
	char *item;		// must be a name in the model list, so just reference
	char itemalias[MAX_NAME_LEN + 1];
	int itemhealth;
	int health[MAX_PLAYERS];
	int mp;			// mp's variable for mpbar by tails
	unsigned int score:32;	// So score can be overridden for enemies/obstacles
	int multiple;		// So score can be overridden for enemies/obstacles
	// coords
	float x;
	float z;
	float a;
	unsigned char credit:8;
	short aggression:16;	// For enemy A.I.
	char spawntype;		// Pass 1 when a level spawn.
	char *weapon;		// spawn with a weapon, since it should be in the model list, so the model must be loaded, just reference its name
	s_model *weaponmodel;
	s_spawn_script_list_node *spawn_script_list_head;
} s_spawn_entry;

typedef enum {
	normal_level = 0,
	cut_scene = 1,
	select_screen = 2,
} le_type;

typedef struct {
	char *branchname;	// Use a name so we can find this level in branches
	char *filename;
	le_type type;		// see le_type
	int z_coords[3];	// Used for setting custom "z"
	int gonext;		// 0. dont complete this level and display score,
	// 1. complete level and display score,
	// 2. complete game, show hall of fame
} s_level_entry;


typedef enum {
	bg_sprite,
	bg_screen
} bglayer_type;

typedef enum {
	fg_sprite,
	fg_screen
} fglayer_type;

typedef struct {
	union {
		s_sprite *sprite;
		s_screen *screen;
		void *handle;
	};
	bglayer_type type;
	int width;
	int height;
	float xratio;
	float zratio;
	int xoffset;
	int zoffset;
	int xspacing;
	int zspacing;
	int xrepeat;
	int zrepeat;
	int transparency;
	int alpha;
	int watermode;
	int amplitude;
	int wavelength;
	float wavespeed;
	float bgspeedratio;
	int enabled;
} s_bglayer;

typedef struct {
	union {
		s_sprite *sprite;
		s_screen *screen;
		void *handle;
	};
	fglayer_type type;
	int width;
	int height;
	float xratio;
	float zratio;
	int xoffset;
	int zoffset;
	int xspacing;
	int zspacing;
	int xrepeat;
	int zrepeat;
	int transparency;
	int alpha;
	int watermode;
	int amplitude;
	int wavelength;
	float wavespeed;
	float bgspeedratio;
	int enabled;
	int z;
} s_fglayer;

typedef struct {
	char *text;
	int t;			//Time to expire.
	int x;
	int y;
	int font;
	int z;
} s_textobj;

typedef struct {
	int pos;
	char *buf;
	size_t size;
} s_filestream;

typedef struct {
	float x;
	float z;
	float upperleft;
	float lowerleft;
	float upperright;
	float lowerright;
	float depth;
} s_hole;

// attention, s_wall must have all the members of s_hole in the same order
// as it is used in checkwall_or_hole()
typedef struct {
	float x;
	float z;
	float upperleft;
	float lowerleft;
	float upperright;
	float lowerright;
	float depth;
	float alt;
} s_wall;

typedef struct {
	char *name;
	int numspawns;
	s_spawn_entry spawnpoints[LEVEL_MAX_SPAWNS];
	int numpanels;
	int order[LEVEL_MAX_PANELS];
	s_bglayer bglayers[LEVEL_MAX_BGLAYERS];
	s_fglayer fglayers[LEVEL_MAX_FGLAYERS];
	s_textobj textobjs[LEVEL_MAX_TEXTOBJS];
	int numbglayers;
	int numfglayers;
	s_filestream filestreams[LEVEL_MAX_FILESTREAMS];
	int numfilestreams;
	int cameraxoffset;
	int camerazoffset;
	int numholes;
	int numwalls;		// Stores number of walls loaded
	s_hole holes[LEVEL_MAX_HOLES];
	int holesfound[LEVEL_MAX_HOLES];
	s_wall walls[LEVEL_MAX_WALLS];	// Now you can have walls for different walkable areas
	int wallsfound[LEVEL_MAX_WALLS];
	int exit_blocked;
	int exit_hole;
	int scrolldir;
	int width;
	int rocking;
	float bgspeed;		// Used to make autoscrolling backgrounds
	int bgdir;		// Used to set which direction the backgrounds scroll for autoscrolling backgrounds
	int mirror;
	int bosses;
	char bossmusic[256];
	unsigned int bossmusic_offset;
	int numpalettes;
	unsigned char palettes[LEVEL_MAX_PALETTES][1024];	//dynamic palettes
	unsigned char *blendings[LEVEL_MAX_PALETTES][MAX_BLENDINGS];	//blending tables
	int settime;		// Set time limit per level
	int notime;		// Used to specify if the time is displayed 1 = no, else yes
	int noreset;		// If set, clock will not reset when players spawn/die
	int type;		// Used to specify which level type (1 = bonus, else regular)
	int nospecial;		// Used to specify if you can use your special during bonus levels
	int nohurt;		// Used to specify if you can hurt the other player during bonus levels
	int noslow;		// Flag so the level doesn't slow down after a boss is defeated
	int nohit;		// Not able to grab / hit other player on a per level basis
	int spawn[MAX_PLAYERS][4];	// Used to determine the spawn position of players
	int setweap;		// Levels can now specified which weapon will be used by default
	int facing;		// Force the players to face to ... 0 no effects, 1 right, 2 left, 3 affected by level dir
//--------------------gravity system-------------------------
	float maxfallspeed;
	float maxtossspeed;
	float gravity;
//---------------------scripts-------------------------------
	Script update_script;
	Script updated_script;
	Script key_script;
	Script level_script;
	Script endlevel_script;
	s_spawn_script_cache_node *spawn_script_cache_head;
	int pos;
	u32 advancetime;
	u32 quaketime;
	int quake;
	int waiting;

} s_level;


typedef struct ArgList {
	size_t count;
	size_t arglen[18];
	char *args[18];
} ArgList;

typedef struct {
	int black;
	int red;
	int orange;
	int yellow;
	int white;
	int blue;
	int green;
	int pink;
	int purple;
	int magic;
	int magic2;
	int shadow;
} s_colors;
#define s_colors_itemcount 12

typedef struct {
	int r; int g; int b;
} s_rgb;

typedef struct {
	s_rgb black;
	s_rgb red;
	s_rgb orange;
	s_rgb yellow;
	s_rgb white;
	s_rgb blue;
	s_rgb green;
	s_rgb pink;
	s_rgb purple;
	s_rgb magic;
	s_rgb magic2;
	s_rgb shadow;	
} s_colors_rgb;

extern s_colors colors;

typedef struct {
	int go;
	int beat;
	int block;
	int indirect;
	int get;
	int get2;
	int fall;
	int jump;
	int punch;
	int oneup;
	int timeover;
	int beep;
	int beep2;
	int bike;
} s_samples;

extern s_samples samples;
#define s_samples_itemcount 14

typedef struct {
	char* go;
	char* beat;
	char* block;
	char* indirect;
	char* get;
	char* get2;
	char* fall;
	char* jump;
	char* punch;
	char* oneup;
	char* timeover;
	char* beep;
	char* beep2;
	char* bike;
} s_samples_strings;

extern List *scriptConstantsCommandList;
typedef struct {
	VARTYPE vt;
	intptr_t value;
} s_script_args_tuple;

typedef struct {
	s_script_args_tuple ent;
	s_script_args_tuple attacker;
	s_script_args_tuple drop;
	s_script_args_tuple type;
	s_script_args_tuple noblock;
	s_script_args_tuple guardcost;
	s_script_args_tuple jugglecost;
	s_script_args_tuple pauseadd;
	s_script_args_tuple which;
	s_script_args_tuple atkid;
	s_script_args_tuple blocked;
	s_script_args_tuple animnum;
	s_script_args_tuple frame;
	s_script_args_tuple player;
	s_script_args_tuple attacktype;
	s_script_args_tuple reset;
	s_script_args_tuple plane;
	s_script_args_tuple height;
	s_script_args_tuple obstacle;
	s_script_args_tuple time;
	s_script_args_tuple gotime;
	s_script_args_tuple damage;
	s_script_args_tuple damagetaker;
	s_script_args_tuple other;
} s_script_args;

typedef struct {
	char* ent;
	char* attacker;
	char* drop;
	char* type;
	char* noblock;
	char* guardcost;
	char* jugglecost;
	char* pauseadd;
	char* which;
	char* atkid;
	char* blocked;
	char* animnum;
	char* frame;
	char* player;
	char* attacktype;
	char* reset;
	char* plane;
	char* height;
	char* obstacle;
	char* time;
	char* gotime;
	char* damage;
	char* damagetaker;
	char* other;
} s_script_args_names;
#define s_script_args_membercount (sizeof(s_script_args_names) / sizeof(char*))

typedef enum {
	CB_UP = 0,
	CB_DOWN,
	CB_LEFT,
	CB_RIGHT,
	CB_ATK1,
	CB_ATK2,
	CB_ATK3,
	CB_ATK4,
	CB_JUMP,
	CB_SPECIAL,
	CB_START,
	CB_SCREENSHOT,
	CB_MAX
} control_buttons;

int buffer_pakfile(char *filename, char **pbuffer, size_t * psize);
int getsyspropertybyindex(ScriptVariant * var, int index);
int changesyspropertybyindex(int index, ScriptVariant * value);
int load_script(Script * script, char *path);
void init_scripts();
void load_scripts();
void execute_animation_script(entity * ent);
void execute_takedamage_script(entity * ent, entity * other, int force, int drop, int type, int noblock, int guardcost,
			       int jugglecost, int pauseadd);
void execute_ondeath_script(entity * ent, entity * other, int force, int drop, int type, int noblock, int guardcost,
			    int jugglecost, int pauseadd);
void execute_onkill_script(entity * ent);
void execute_onpain_script(entity * ent, int iType, int iReset);
void execute_onfall_script(entity * ent, entity * other, int force, int drop, int type, int noblock, int guardcost,
			   int jugglecost, int pauseadd);
void execute_onblocks_script(entity * ent);
void execute_onblockw_script(entity * ent, int plane, float height);
void execute_onblocko_script(entity * ent, entity * other);
void execute_onblockz_script(entity * ent);
void execute_onblocka_script(entity * ent, entity * other);
void execute_onmovex_script(entity * ent);
void execute_onmovez_script(entity * ent);
void execute_onmovea_script(entity * ent);
void execute_didblock_script(entity * ent, entity * other, int force, int drop, int type, int noblock, int guardcost,
			     int jugglecost, int pauseadd);
void execute_ondoattack_script(entity * ent, entity * other, int force, int drop, int type, int noblock, int guardcost,
			       int jugglecost, int pauseadd, int iWhich, int iAtkID);
void execute_updateentity_script(entity * ent);
void execute_think_script(entity * ent);
void execute_didhit_script(entity * ent, entity * other, int force, int drop, int type, int noblock, int guardcost,
			   int jugglecost, int pauseadd, int blocked);
void execute_onspawn_script(entity * ent);
void clearsettings(void);
void savesettings(void);
void saveasdefault(void);
void loadsettings(void);
void loadfromdefault(void);
void clearSavedGame(void);
void clearHighScore(void);
void saveGameFile(void);
int loadGameFile(void);
void saveHighScoreFile(void);
void loadHighScoreFile(void);
int music(char *filename, int loop, long offset);
#if DC
void check_music_opened(void);
#endif
char *findarg(char *command, int which);
float diff(float a, float b);
int inair(entity * e);
float randf(float max);
int _makecolour(int r, int g, int b);
int load_colourmap(s_model * model, char *image1, char *image2);
int load_palette(unsigned char *pal, char *filename);
void standard_palette();
void change_system_palette(int palindex);
void unload_background();
void lifebar_colors();
void load_background(char *filename, int createtables);
void unload_texture();
void load_texture(char *filename);
void freepanels();
s_sprite *loadpanel2(char *filename);
int loadpanel(s_panel_filenames*);
int loadfrontpanel(char *filename);
void resourceCleanUp(void);
void freesprites();
s_sprite *loadsprite2(char *filename, int *width, int *height);
int loadsprite(char *filename, int ofsx, int ofsy, int bmpformat);
void load_special_sprites();
int load_special_sounds();
s_model *find_model(char *name);
s_model *nextplayermodel(s_model * current);
s_model *prevplayermodel(s_model * current);
void free_anim(s_anim * anim);
void free_models();
s_anim *alloc_anim();
int addframe(s_anim * a, int spriteindex, int framecount, short delay, unsigned char idle,
	     short *bbox, s_attack * attack, short move, short movez,
	     short movea, short seta, float *platform, int frameshadow,
	     short *shadow_coords, int soundtoplay, s_drawmethod * drawmethod);
void cache_model(char *name, char *path, int flag);
void remove_from_cache(char *name);
void free_modelcache();
int get_cached_model_index(char *name);
char *get_cached_model_path(char *name);
s_model *load_cached_model(char *name, char *owner, char unload);
int is_set(s_model * model, int m);
int load_script_setting();
int load_models();
void unload_levelorder();
void add_level(char *filename, int diff);
void add_scene(char *filename, int diff);
void load_levelorder();
void unload_level();
void load_level(char *filename);
void drawlifebar(int x, int y, int h, int maxh);
void drawmpbar(int x, int y, int m, int maxm);
void update_loading(s_loadingbar * s, int value, int max);
void spawnplayer(int);
void drop_all_enemies();
void kill_all_enemies();
void ent_set_colourmap(entity * ent, unsigned int which);
void predrawstatus();
void drawstatus();
void addscore(int playerindex, int add);
void free_ent(entity * e);
void free_ents();
int alloc_ents();
entity *smartspawn(s_spawn_entry * p);
int adjust_grabposition(entity * ent, entity * other, float dist, int grabin);
int player_trymove(float xdir, float zdir);
void toss(entity * ent, float lift);
void player_think(void);
void subtract_shot(void);
void set_model_ex(entity * ent, char *modelname, int index, s_model * newmodel, int flag);
void dropweapon(int flag);
void biker_drive(void);
void trap_think(void);
void steamer_think(void);
void text_think(void);
void anything_walk(void);
void adjust_walk_animation(entity * other);
void kill(entity *);
int player_takedamage(entity * other, s_attack * attack);
int biker_takedamage(entity * other, s_attack * attack);
int obstacle_takedamage(entity * other, s_attack * attack);
void ent_set_anim(entity *, int, int);
void suicide(void);
void player_blink(void);
void common_prejump();
void common_dot();
void tryjump(float, float, float, int);
void dojump(float, float, float, int);
void biker_drive(void);
void ent_default_init(entity * e);
void ent_spawn_ent(entity * ent);
void ent_summon_ent(entity * ent);
void ent_set_anim(entity * ent, int aninum, int resetable);
void ent_set_colourmap(entity * ent, unsigned int which);
void ent_set_model(entity * ent, char *modelname);
entity *spawn(float x, float z, float a, int direction, char *name, int index, s_model * model);
void ent_unlink(entity * e);
void ents_link(entity * e1, entity * e2);
void kill(entity * victim);
void kill_all();
int checkhit(entity * attacker, entity * target, int counter);
int checkhole(float x, float z);
int testplatform(entity *, float, float);
int testhole(int, float, float);
int testwall(int, float, float);
int checkwalls(float x, float z, float a1, float a2);
int checkholes(float, float);
int checkwall_below(float x, float z, float a);
int checkwall(float x, float z);
entity *check_platform_below(float x, float z, float a);
entity *check_platform(float x, float z);
void do_attack(entity * e);
void update_ents();
entity *find_ent_here(entity * exclude, float x, float z, int types);
void display_ents();
void toss(entity * ent, float lift);
entity *findent(int types);
int count_ents(int types);
entity *find_ent_here(entity * exclude, float x, float z, int types);
int set_idle(entity * ent);
int set_death(entity * iDie, int type, int reset);
int set_fall(entity * iFall, int type, int reset, entity * other, int force, int drop, int noblock, int guardcost,
	     int jugglecost, int pauseadd);
int set_rise(entity * iRise, int type, int reset);
int set_riseattack(entity * iRiseattack, int type, int reset);
int set_blockpain(entity * iBlkpain, int type, int reset);
int set_pain(entity * iPain, int type, int reset);
void set_weapon(entity * ent, int wpnum, int anim_flag);
entity *melee_find_target();
entity *long_find_target();
entity *normal_find_target(int anim);
entity *normal_find_item();
int long_attack();
int melee_attack();
void dothrow();
void doprethrow();
void dograbattack(int);
int check_special();
void normal_prepare();
void common_jump();
void common_spawn(void);
void common_drop(void);
void common_jumpattack();
void common_turn();
void common_fall();
void common_lie();
void common_rise();
void common_pain();
void common_vault();
void common_get();
void common_land();
void common_grab(void);
void common_grabattack();
void common_grabbed();
void common_block(void);
int arrow_takedamage(entity * other, s_attack * attack);
int common_takedamage(entity * other, s_attack * attack);
int normal_attack();
void common_throw(void);
void common_throw_wait(void);
void common_prethrow(void);
void npc_warp();
int common_trymove(float xdir, float zdir);
void normal_runoff();
void common_stuck_underneath();
void common_attack_proc();
void normal_attack_finish();
entity *common_find_target();
int common_attack(void);
int common_try_jump(void);
int common_try_pick(entity * other);
int common_try_chase(entity * target);
int common_try_follow();
int common_try_avoid(entity * target);
int common_try_wandercompletely();
int common_try_wander(entity * target);
void common_pickupitem(entity * other);
int common_walk_anim(entity * ent);
int common_idle_anim(entity * ent);
int common_up_anim(entity * ent);
int common_down_anim(entity * ent);
int common_backwalk_anim(entity * ent);
int bomb_move(void);
int arrow_move(void);
int normal_move(void);
int avoid_move(void);
int chase_move(void);
int wander_move(void);
int common_move(void);
void common_think(void);
void suicide(void);
void prethrow(void);
void player_die();
int player_trymove(float xdir, float zdir);
int check_energy(int which, int ani);
void player_preinput();
int player_check_special();
void runanimal(void);
void player_blink(void);
int check_combo(int m);
int check_costmove(int s, int fs);
void didfind_item(entity * other);
void player_think(void);
void subtract_shot();
void dropweapon(int flag);
void drop_all_enemies();
void kill_all_enemies();
void smart_bomb(entity * e, s_attack * attack);
void anything_walk(void);
entity *knife_spawn(char *name, int index, float x, float z, float a, int direction, int type, int map);
entity *bomb_spawn(char *name, int index, float x, float z, float a, int direction, int map);
void bomb_explode(void);
int star_spawn(float x, float z, float a, int direction);
void steam_think(void);
void trap_think(void);
void steam_spawn(float x, float z, float a);
void steamer_think(void);
void text_think(void);
entity *homing_find_target(int type);
void biker_drive(void);
void bike_crash(void);
void obstacle_fall(void);
void obstacle_fly(void);
entity *smartspawn(s_spawn_entry * props);
void spawnplayer(int index);
void time_over();
void update_scroller();
void draw_scrolled_bg();
void update(int ingame, int usevwait);
void fade_out(int type, int speed);
void apply_controls();
void display_credits(void);
void borShutdown(const char *caller, int status, char *msg, ...);
void guistartup(void);
void startup(void);
int playgif(char *filename, int x, int y, int noskip);
void playscene(char *filename);
void gameover();
void hallfame(int addtoscore);
void showcomplete(int num);
int playlevel(char *filename);
int selectplayer(int *players, char *filename);
void playgame(int *players, unsigned which_set, int useSavedGame);
int choose_difficulty();
int load_saved_game();
void term_videomodes();
int init_videomodes(void);
void safe_set(int *arr, int index, int newkey, int oldkey);
void keyboard_setup(int player);
void input_options();
void inputrefresh();
void soundvol_options();
void config_options();
void cheatoptions();
void system_options();
void video_options();
void options();
void soundcard_options();
void openborMain(int argc, char **argv);
int getValidInt(char *text, char *file, char *cmd);
float getValidFloat(char *text, char *file, char *cmd);
int set_color_correction(int gm, int br);

extern s_videomodes videomodes;
extern unsigned int videoMode;
extern int current_palette;
extern unsigned char pal[MAX_PAL_SIZE];

extern s_savelevel savelevel[MAX_DIFFICULTIES];
extern s_savescore savescore;
extern s_savedata savedata;

extern int quit_game;
extern u32 bothkeys;
extern u32 bothnewkeys;
extern u32 interval;
extern s_playercontrols *playercontrolpointers[];
extern int cheats;
extern int livescheat;
extern int creditscheat;
extern int healthcheat;
extern int keyscriptrate;
extern int versusdamage;
extern int ctrlmaxplayers[MAX_DIFFICULTIES];
extern int maxplayers[MAX_DIFFICULTIES];
extern int current_set;
extern int pause;		//FIXME rename to borPause and remove pause macro
extern int forcecheatsoff;
extern unsigned int ifcomplete[MAX_DIFFICULTIES];
extern unsigned int num_difficulties;
extern int bonus;
extern int saveslot;
extern char set_names[MAX_DIFFICULTIES][MAX_NAME_LEN + 1];
extern s_player player[4];
extern u32 borTime;
extern int shadowsprites[6];
extern int gosprite;
extern int golsprite;
extern char *custBkgrds;
extern char *custLevels;
extern char *custModels;
#endif
