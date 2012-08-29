/*
 * OpenBOR - http://www.LavaLit.com
 * -----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in OpenBOR root for details.
 *
 * Copyright (c) 2004 - 2011 OpenBOR Team
 */

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <malloc.h>
#include <locale.h>
#include <math.h>

#include "stringptr.h"
#include "utils.h"
#include "stristr.h"
#include "openbor.h"
#include "packfile.h"
#include "savepng.h"

#include <dirent.h>
#include <sys/stat.h>

#ifdef SDL
#  include <unistd.h>
#  include "sdlport.h"
#endif

#define MKDIR(x) mkdir(x, 0777)

#define CHECK_LOGFILE(type)  type ? fileExists("./Logs/OpenBorLog.txt") : fileExists("./Logs/ScriptLog.txt")
#define OPEN_LOGFILE(type)   type ? fopen("./Logs/OpenBorLog.txt", "wt") : fopen("./Logs/ScriptLog.txt", "wt")
#define APPEND_LOGFILE(type) type ? fopen("./Logs/OpenBorLog.txt", "at") : fopen("./Logs/ScriptLog.txt", "at")
#define READ_LOGFILE(type)   type ? fopen("./Logs/OpenBorLog.txt", "rt") : fopen("./Logs/ScriptLog.txt", "rt")
#define COPY_ROOT_PATH(buf, name) strcpy(buf, "./"); strcat(buf, name); strcat(buf, "/");
#define COPY_PAKS_PATH(buf, name) strcpy(buf, "./Paks/"); strcat(buf, name);

void debugBuf(unsigned char *buf, size_t size, int columns) {
	size_t pos = 0;
	int i;
	while(pos < size) {
		for(i = 0; i < columns; i++) {
			if(pos >= size)
				break;
			printf("%02x", buf[pos]);
			pos++;
		}
		printf("\n");
	}
}

//lowercases a buffer inplace
void lc(char *buf, size_t size) {
	ptrdiff_t i;
	for(i = 0; i < size; i++)
		buf[i] = tolower((int) buf[i]);
}

// returns position after next newline in buf
size_t getNewLineStart(char *buf) {
	size_t res = 0;
	while(buf[res] && buf[res] != '\n' && buf[res] != '\r')
		++res;
	while(buf[res] && (buf[res] == '\n' || buf[res] == '\r'))
		++res;
	return res;
}

void freeAndNull(void** data) {
	if(data && *data) {
		free(*data);
		*data = NULL;
	}
}

int is_dir(char* fn) {
	struct stat st;
	if(stat(fn, &st) != -1 && S_ISDIR(st.st_mode))
		return 1;
	return 0;
}

FILE *openborLog = NULL;
FILE *scriptLog = NULL;
char debug_msg[2048];
unsigned long debug_time = 0xFFFFFFFF;

void getBasePath(char *newName, char *name, int type) {
	char buf[128] = { "" };
	if(type) 
		snprintf(buf, sizeof(buf) - 1, "%s/%s", paksDir, name);
	else 
		snprintf(buf, sizeof(buf) - 1, "%s/", name);
	
	if(is_dir(buf)) 
		strcat(buf, "/");
	strncpy(newName, buf, sizeof(buf));
}

int dirExists(char *dname, int create) {
	char realName[128] = { "" };
	DIR *fd1 = NULL;
	int fd2 = -1;
	strncpy(realName, dname, 128);
	fd1 = opendir(realName);
	if(fd1 != NULL) {
		closedir(fd1);
		return 1;
	}
	if(create) {
		fd2 = MKDIR(realName);
		if(fd2 < 0)
			return 0;
		return 1;
	}
	return 0;
}

int fileExists(char *fnam) {
	FILE *handle = NULL;
	if((handle = fopen(fnam, "rb")) == NULL)
		return 0;
	fclose(handle);
	return 1;
}

void debug_printf(char *format, ...) {
	va_list arglist;

	va_start(arglist, format);
	vsprintf(debug_msg, format, arglist);
	va_end(arglist);

	debug_time = 0xFFFFFFFF;
}

const char* savefile_exts[] = {
	[ST_SAVE] = ".sav",
	[ST_HISCORE] = ".hi",
	[ST_SCRIPT] = ".scr",
	[ST_INP] = ".inp",
	[ST_CFG] = ".cfg",
};

int packfile_is_dir(void) {
	return packfile[strlen(packfile) - 1] == '/';
}

void getSaveFileName(char name[256], savefile_type type) {

	char mod[256] = { "" };
	const char* ext = "";
	char* fn;
	unsigned s, l;
	
	if(type < ST_MAX)
		ext = savefile_exts[type];
	
	l = strlen(paksDir);
	if(memcmp(packfile, paksDir, l))
		s = 0;
	else {
		s = l;
		if(paksDir[l - 1] != '/')
			s++;
	}
	
	l = strlen(packfile) - s;
	if(!packfile_is_dir())
		l -= 4; // strip off ".pak"
	else 
		l-=1; // strip off "/"
		
	
	memcpy(mod, packfile + s, l);
	memcpy(mod + l, ext, strlen(ext));
	
	if((fn = strrchr(mod, '/')) || (fn = strrchr(mod, '\\')))
		strcpy(name, fn + 1);
	else
		strcpy(name, mod);
}

void screenshot(s_screen * vscreen, unsigned char *pal, int ingame) {
	int shotnum = 0;
	char shotname[128] = { "" };
	char modname[128] = { "" };

	getSaveFileName(modname, 99);
	do {
		sprintf(shotname, "./ScreenShots/%s - %04u.png", modname, shotnum);
		++shotnum;
	} while(fileExists(shotname) && shotnum < 100);

	if(shotnum < 10000)
		savepng(shotname, vscreen, pal);
	if(ingame)
		debug_printf("Saved %s", shotname);
}

unsigned readlsb32(const unsigned char *src) {
	return
	    ((((unsigned) (src[0])) & 0xFF) << 0) |
	    ((((unsigned) (src[1])) & 0xFF) << 8) |
	    ((((unsigned) (src[2])) & 0xFF) << 16) | ((((unsigned) (src[3])) & 0xFF) << 24);
}

// Optimized search in an arranged string table, return the index
int searchList(const char *list[], const char *value, int length) {
	int i;
	int a = 0;
	int b = length / 2;
	int c = length - 1;
	int v = value[0];

	// We must convert uppercase values to lowercase,
	// since this is how every command is written in
	// our source.  Refer to an ASCII Chart
	if(v >= 0x41 && v <= 0x5A)
		v += 0x20;

	// Index value equals middle value,
	// Lets search starting from center.
	if(v == list[b][0]) {
		if(stricmp(list[b], value) == 0)
			return b;

		// Search Down the List.
		if(v == list[b - 1][0]) {
			for(i = b - 1; i >= 0; i--) {
				if(stricmp(list[i], value) == 0)
					return i;
				if(v != list[i - 1][0])
					break;
			}
		}
		// Search Up the List.
		if(v == list[b + 1][0]) {
			for(i = b + 1; i < length; i++) {
				if(stricmp(list[i], value) == 0)
					return i;
				if(v != list[i + 1][0])
					break;
			}
		}
		// No match, return failure.
		goto searchListFailed;
	}
	// Define the starting point.
	if(v >= list[b + 1][0])
		a = b + 1;
	else if(v <= list[b - 1][0])
		c = b - 1;
	else
		goto searchListFailed;

	// Search Up from starting point.
	for(i = a; i <= c; i++) {
		if(v == list[i][0]) {
			if(stricmp(list[i], value) == 0)
				return i;
			if(v != list[i + 1][0])
				break;
		}
	}

	searchListFailed:

	// The search failed!
	// On five reasons for failure!
	// 1. Is the list in alphabetical order?
	// 2. Is the first letter lowercase in list?
	// 3. Does the value exist in the list?
	// 4. Is it a typo?
	// 5. Is it a text file error?
	return -1;
}

char *commaprint(u64 n) {
	static int comma = '\0';
	static char retbuf[30];
	char *p = &retbuf[sizeof(retbuf) - 1];
	int i = 0;

	if(comma == '\0') {
		struct lconv *lcp = localeconv();
		if(lcp != NULL) {
			if(lcp->thousands_sep != NULL && *lcp->thousands_sep != '\0')
				comma = *lcp->thousands_sep;
			else
				comma = ',';
		}
	}

	*p = '\0';

	do {
		if(i % 3 == 0 && i != 0)
			*--p = comma;
		*--p = '0' + n % 10;
		n /= 10;
		i++;
	} while(n != 0);

	return p;
}

void char_to_lower(char *dst, char *src, size_t maxlen) {
	unsigned i;
	for(i = 0; i < maxlen; i++) {
		dst[i] = tolower(src[i]);
		if(!src[i])
			break;
	}
	if(i == maxlen)
		dst[maxlen - 1] = 0;
}

void int_min_max(int* candidate, int min, int max) {
	if(*candidate < min)
		*candidate = min;
	else if (*candidate > max)
		*candidate = max;
}

void short_min_max(short* candidate, short min, short max) {
	if(*candidate < min)
		*candidate = min;
	else if (*candidate > max)
		*candidate = max;
}
