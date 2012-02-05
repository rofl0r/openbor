#ifndef MOVIE_H
#define MOVIE_H

#include "types.h"
#include "control.h"

#define MOVIEBUF_LEN 2048

extern int movielog;
extern int movieplay;
extern int moviebufptr;
extern int movielen;
extern int movieloglen;
extern FILE *moviefile;
extern u32(*moviebuffer)[5][2];	//keyflags, newkeyflags;

void movie_openfile(int save);
void movie_flushbuf();
void movie_closefile();
void movie_update(s_playercontrols ** pctrls);
void movie_save(s_playercontrols ** pctrls);

#endif
