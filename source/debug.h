#ifndef DEBUG_H
#define DEBUG_H

#include <stdio.h>

#ifdef DEBUG
# define PDEBUG(fmt, args...) do { fprintf(stderr,"DEBUG: "fmt, ## args); fflush(stderr); } while (0)
#else
# define PDEBUG(fmt, args...) do { } while(0)
#endif

#endif
