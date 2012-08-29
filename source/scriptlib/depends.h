/*
 * OpenBOR - http://www.LavaLit.com
 * -----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in OpenBOR root for details.
 *
 * Copyright (c) 2004 - 2011 OpenBOR Team
 */

#ifndef DEPENDS_H
#define DEPENDS_H

#include "types.h"
#include "globals.h"
#include <stdbool.h>

#ifndef COMPILED_SCRIPT
#define COMPILED_SCRIPT 1
#endif

#ifndef NULL
#define NULL 0
#endif

#ifndef XBOX
#ifdef S_OK
#undef S_OK
#endif
#define S_OK   ((s32)0)

#ifdef E_FAIL
#undef E_FAIL
#endif
#define E_FAIL ((s32)-1)

#ifdef FAILED
#undef FAILED
#endif
#define FAILED(status) (((s32)(status))<0)

#ifdef SUCCEEDED
#undef SUCCEEDED
#endif
#define SUCCEEDED(status) (((s32)(status))>=0)
#endif

#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE 1
#endif

#define MAX_STR_LEN    128
#define MAX_STR_VAR_LEN    64

#endif
