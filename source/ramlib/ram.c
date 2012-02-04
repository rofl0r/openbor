/*
 * OpenBOR - http://www.LavaLit.com
 * -----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in OpenBOR root for details.
 *
 * Copyright (c) 2004 - 2011 OpenBOR Team
 */

/*
 * This library is used for calculating how much memory is available/used.
 * Certain platforms offer physical memory statistics, we obviously wrap
 * around those functions.  For platforms where we can't retrieve this
 * information we then calculate the estimated sizes based on a few key
 * variables and symbols.  These estimated values should tolerable.......
 */

/////////////////////////////////////////////////////////////////////////////
// Libraries

#include <sys/sysinfo.h>
#include <unistd.h>

#include <malloc.h>
#include <string.h>
#include <stdio.h>
#include "globals.h"
#include "utils.h"
#include "ram.h"

/////////////////////////////////////////////////////////////////////////////
// Globals

static u64 systemRam = 0x00000000;


/////////////////////////////////////////////////////////////////////////////
//  Functions

u64 getFreeRam(int byte_size)
{
	struct sysinfo info;
	sysinfo(&info);
	return ((u64)info.freeram) * info.mem_unit;
}

void setSystemRam()
{
	struct sysinfo info;
	sysinfo(&info);
	systemRam = ((u64)info.totalram) * info.mem_unit;
	getRamStatus(BYTES);
}

u64 getSystemRam(int byte_size)
{
	return systemRam/byte_size;
}

u64 getUsedRam(int byte_size)
{
	unsigned long vm = 0;
	FILE *file = fopen("/proc/self/statm", "r");
	if (file == NULL) return 0;
	if (fscanf (file, "%lu", &vm) <= 0) return 0;
	fclose (file);
	return (vm * getpagesize() / 8)/byte_size;
}

void getRamStatus(int byte_size)
{
	printf("Total Ram: %"PRIu64" Bytes\n Free Ram: %"PRIu64" Bytes\n Used Ram: %"PRIu64" Bytes\n\n",
		   getSystemRam(byte_size),
		   getFreeRam(byte_size),
		   getUsedRam(byte_size));
}

