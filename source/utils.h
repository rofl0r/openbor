/*
 * OpenBOR - http://www.LavaLit.com
 * -----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in OpenBOR root for details.
 *
 * Copyright (c) 2004 - 2011 OpenBOR Team
 */

#ifndef UTILS_H
#define UTILS_H

#include "debug.h"

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof(arr[0]))

// *** INCLUDES ***
#include "types.h"
#include "stringptr.h"

// *** TYPE DECLARATIONS ***
#define SCRIPT_LOG 0
#define OPENBOR_LOG 1

// *** VARIABLE DECLARATIONS ***
extern char debug_msg[2048];
extern unsigned long debug_time;

// *** FUNCTIONS DECLARATIONS ***
#define LOGHANDLE stderr
#define writeToLogFile(fmt...) fprintf(LOGHANDLE, fmt)
#define SCRIPTLOGHANDLE stderr
#define writeToScriptLog(fmt...) fprintf(SCRIPTLOGHANDLE, fmt)

int fileExists(char *fnam);
int dirExists(char *dname, int create);
int is_dir(char* fn);
stringptr *readFromLogFile(int which);
void freeAndNull(void** data);
void lc(char *buf, size_t size);
size_t getNewLineStart(char *buf);
void debugBuf(unsigned char *buf, size_t size, int columns);
void debug_printf(char *, ...);

typedef enum {
	ST_SAVE = 0,
	ST_HISCORE,
	ST_SCRIPT,
	ST_INP,
	ST_CFG,
	ST_MAX
} savefile_type;

void getSaveFileName(char name[256], unsigned int type);
void screenshot(s_screen * vscreen, unsigned char *pal, int ingame);
void getBasePath(char *newName, char *name, int type);
unsigned readlsb32(const unsigned char *src);
int searchList(const char *list[], const char *value, int length);
char *commaprint(u64 n);
void char_to_lower(char *dst, char *src, size_t maxlen);
void int_min_max(int* candidate, int min, int max);
void short_min_max(short* candidate, short min, short max);

#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif


#endif
