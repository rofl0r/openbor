#include "openbor.h"
#include "movie.h"

//movie log stuffs
int movielog = 0;
int movieplay = 0;
int moviebufptr = 0;
int movielen = 0;
int movieloglen = 0;
FILE *moviefile = NULL;
u32(*moviebuffer)[5][2] = NULL;	//keyflags, newkeyflags;


void movie_openfile(int save) {
	char path[256] = { "" };
	char tmpname[256] = { "" };
	moviebuffer = malloc(sizeof(*moviebuffer) * MOVIEBUF_LEN);
	if(!moviebuffer)
		return;
	memset(moviebuffer, 0, sizeof(*moviebuffer) * MOVIEBUF_LEN);
	getBasePath(path, "Saves", 0);
	getSaveFileName(tmpname, ST_INP); // TODO shouldnt this be ST_SCR ?
	strcat(path, tmpname);
	if(save)
		moviefile = fopen(path, "wb");
	else
		moviefile = fopen(path, "rb");
	if(moviefile == NULL)
		return;
	if(save) {
		movielog = 1;
		movieplay = 0;
		movieloglen = 0;
		moviebufptr = 0;
	} else {
		fseek(moviefile, 0, SEEK_END);
		movielen = ftell(moviefile);
		movielen /= sizeof(*moviebuffer);
		fseek(moviefile, 0, SEEK_SET);
		movielog = 0;
		movieplay = 1;
		movieloglen = 0;
		moviebufptr = MOVIEBUF_LEN;
	}
}

void movie_flushbuf() {
	int disCcWarns;
	if(!moviefile || !moviebuffer)
		return;
	disCcWarns = fwrite(moviebuffer, sizeof(*moviebuffer), MOVIEBUF_LEN, moviefile);
	memset(moviebuffer, 0, sizeof(*moviebuffer) * MOVIEBUF_LEN);
	moviebufptr = 0;
}

void movie_closefile() {
	if(moviefile)
		fclose(moviefile);
	if(moviebuffer)
		free(moviebuffer);
	moviebuffer = NULL;
	moviefile = NULL;
	moviebufptr = 0;
	movielog = 0;
	movieplay = 0;
}

void movie_update(s_playercontrols ** pctrls) {
	int p;
	int disCcWarns;
	if(!moviefile || !moviebuffer)
		return;
	if(moviebufptr == MOVIEBUF_LEN) {
		moviebufptr = 0;
		if(movieloglen <= movielen) {
			disCcWarns = fread(moviebuffer, sizeof(*moviebuffer), MOVIEBUF_LEN, moviefile);
			movieloglen += MOVIEBUF_LEN;
		} else {
			movie_closefile();
			return;
		}
	}
	for(p = 0; p < 4; p++) {
		playercontrolpointers[p]->keyflags = moviebuffer[moviebufptr][p][0];
		playercontrolpointers[p]->newkeyflags = moviebuffer[moviebufptr][p][1];
	}
	seed = moviebuffer[moviebufptr][4][0];
	interval = moviebuffer[moviebufptr][4][1];
	moviebufptr++;
}

void movie_save(s_playercontrols ** pctrls) {
	int p;
	if(!moviefile || !moviebuffer)
		return;
	if(moviebufptr == MOVIEBUF_LEN) {
		movie_flushbuf();
		movieloglen += MOVIEBUF_LEN;
	}
	for(p = 0; p < 4; p++) {
		moviebuffer[moviebufptr][p][0] = playercontrolpointers[p]->keyflags;
		moviebuffer[moviebufptr][p][1] = playercontrolpointers[p]->newkeyflags;
	}
	moviebuffer[moviebufptr][4][0] = seed;
	moviebuffer[moviebufptr][4][1] = interval;
	moviebufptr++;
}
