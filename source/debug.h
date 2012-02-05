#ifndef DEBUG_H
#define DEBUG_H

#ifdef DEBUG
#include <stdio.h>
# define PDEBUG(fmt, args...) do { fprintf(stderr,"DEBUG: "fmt, ## args); fflush(stderr); } while (0)
#else
# define PDEBUG(fmt, args...)
#endif

#endif
