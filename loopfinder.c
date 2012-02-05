//#include "List.h"

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <string.h>
#include <assert.h>

#include "stringptr.h"


typedef struct {
	char text_RIFF[4];
	unsigned int filesize_minus_8;
	char text_WAVE[4];
	char text_fmt[4];
	unsigned int formatheadersize;
	unsigned short format;
	unsigned short channels;
	unsigned int samplerate;
	unsigned int bytespersec;
	unsigned short blockalign;
	unsigned short bitwidth;
} WAVE_HEADER;

size_t getfilesize(char *filename) {
	struct stat st;
	if(!stat(filename, &st)) {
		return st.st_size;
	} else
		return 0;
}

stringptr *readfile(char *filename) {
	FILE *f;
	stringptr *buf;
	size_t size = getfilesize(filename);
	size_t bufpos = 0;
	size_t bread = 0;
	if(!size)
		return NULL;
	f = fopen(filename, "r");
	if(!f)
		return NULL;
	//buf = malloc(size);
	buf = new_string(size);

	if(!buf)
		goto FEXIT;

	while(bufpos < size) {
		bread = fread(buf->ptr + bufpos, 1, 64 * 1024, f);
		bufpos += bread;
		if(!bread) {
			printf(strerror(errno));
			break;
		}
	}
	FEXIT:
	fclose(f);
	return buf;
}

void searchBiggestRepeatingBlock(stringptr * buf) {
	size_t samplesize = 8;
	size_t startpos = 0;
	size_t scanpos = 0;
	size_t lastfoundoffset = 0;
	size_t lastfoundsamplesize = 0;

	while(startpos < buf->size - samplesize) {
		printf("approaching startpos %d with samplesize %d\n", startpos, samplesize);
		scanpos = startpos + samplesize;
		while(scanpos < buf->size - samplesize) {
			if(buf->ptr[startpos] == buf->ptr[scanpos] &&
			   buf->ptr[startpos + samplesize - 1] == buf->ptr[scanpos + samplesize - 1] &&
			   memcmp(buf->ptr + startpos, buf->ptr + scanpos, samplesize) == 0) {
				lastfoundoffset = scanpos;
				lastfoundsamplesize = samplesize;
				samplesize++;
				while(scanpos + samplesize < buf->size
				      && buf->ptr[startpos + samplesize - 1] == buf->ptr[scanpos + samplesize - 1])
					samplesize++;
				scanpos++;
				printf("#");
			} else {
				scanpos++;
			}
		}
		startpos++;
	}
	printf("\nlastfoundoffset: %d, lastfoundsamplesize: %d\n", lastfoundoffset, lastfoundsamplesize);

}

void searchLoop(char *buf, size_t bufsize, size_t startpos, size_t minsize, size_t blocksize) {
	size_t samplesize = minsize;
	size_t save = 0;
	size_t scanpos = 0;

	//align startpos to blocksize
	startpos += startpos % blocksize;

	while(startpos < bufsize - samplesize) {
		if(startpos % 1000 == 0)
			printf("approaching startpos %d with samplesize %d\n", startpos, samplesize);
		assert(startpos % blocksize == 0);
		samplesize = minsize;
		scanpos = startpos + minsize;
		save = 0;
		//scanning the buffer from scanpos till eof, comparing with the junk from startpos.
		while(scanpos < bufsize - samplesize) {
			if(!memcmp(buf + startpos, buf + scanpos, samplesize)) {
				while(scanpos + samplesize < bufsize && startpos + samplesize < scanpos
				      && !memcmp(buf + startpos + samplesize, buf + scanpos + samplesize, blocksize))
					samplesize += blocksize;
				if(startpos + samplesize == scanpos || scanpos + samplesize == bufsize) {
					printf("(possible) loop found at offset %d, length %d, repeats at %d!\n",
					       startpos, samplesize, scanpos);
					// lets search for a bigger loop, which includes everything here
					save = samplesize;
					samplesize *= 2;
					scanpos += save;
				} else {
					printf("match of length %d found at %d and %d\n", samplesize, startpos,
					       scanpos);
					save = samplesize;
					samplesize = scanpos - startpos;
					scanpos += save;
				}
			} else
				scanpos += blocksize;
		}
		if(save)
			startpos += save;
		else
			startpos += blocksize;
	}
	printf("no loops found :-/\n");
}

int checkWaveValid(WAVE_HEADER * wave) {
	if(!memcmp((char *) wave->text_RIFF, "RIFF", 4) &&
	   !memcmp((char *) wave->text_WAVE, "WAVE", 4) &&
	   !memcmp((char *) wave->text_fmt, "fmt ", 4) && !memcmp((char *) &wave->format, "\x01\x00", 2))
		return 1;
	else
		return 0;
}

size_t findWaveDataStart(WAVE_HEADER * wave) {
	if(checkWaveValid(wave)) {
		if(!memcmp
		   ((char *) &wave->formatheadersize + sizeof(wave->formatheadersize) + wave->formatheadersize, "data",
		    4))
			return ((size_t) & wave->formatheadersize - (size_t) wave) + sizeof(wave->formatheadersize) +
			    wave->formatheadersize + sizeof(int);
	}
	return 0;
}

int main(int argc, char **argv) {
	// gcc -Wall -g -Isource loopfinder.c source/stringptr.c -o loopfinder
	stringptr *buf;
	char *data = NULL;
	WAVE_HEADER *wave = NULL;
	size_t datasize;
	size_t startpos;
	size_t minsize;
	if(argc < 4) {
		puts("need a valid filename as argv1, scanstartoffset as argv2, minimum matchsize in bytes as argv3");
		return 1;
	}

	buf = readfile(argv[1]);
	startpos = atoi(argv[2]);
	minsize = atoi(argv[3]);
	if(buf->size > sizeof(WAVE_HEADER)) {
		wave = (WAVE_HEADER *) buf->ptr;
		data = buf->ptr + findWaveDataStart(wave);
		datasize = buf->size - ((size_t) data - (size_t) buf->ptr);
		if(data == buf->ptr)
			printf("no valid WAVE file or compressed format. scanning whole file.\n");
		else
			printf("skipping WAVE header of length %d\n", (size_t) data - (size_t) buf->ptr);
	}

	if(wave && datasize % wave->blockalign != 0) {
		printf("error, filesize doesnt match blockalign!");
		return 1;
	}

	searchLoop(data, datasize, startpos, minsize, wave ? wave->blockalign : 1);

	free_string(buf);
	return 0;
}
