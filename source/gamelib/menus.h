#ifndef MENUS_H
#define MENUS_H

#include "types.h"

void options(void);
void video_options(void);
void system_options(void);
void input_options(void);
void sound_options(void);
void soundcard_options(void);
void cheatoptions(void);
void config_settings(void);
void movie_options(void);
int choose_difficulty(void);
int choose_mode(int *players);
int load_saved_game(void);
int main_menu(int* started, u32* introtime, int* players);
void controller_options(int player_nr, int *quit, char** buttonnames, char* disabledkey);

#endif
