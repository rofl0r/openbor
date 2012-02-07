/*

Written by rofl0r with parts taken from borplay.
LICENSE: GPL v2 
 
 gcc -Wall -Wextra bor2wav.c ../borplay/adpcm.c -o bor2wav
 
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include "../borplay/adpcm.h"

#define     BOR_MUSIC_VERSION   0x00010000
#define     NEW_MUSIC_VERSION   0x00010001
#define     BOR_IDENTIFIER      "BOR music"
#define     MUSIC_BUF_SIZE      8192

typedef struct {
	unsigned int   pns_len;
	unsigned int   filestart;
	unsigned int   filesize;
	char           namebuf[80];
} pnamestruct;

typedef struct {
	char    	 identifier[16];
	char    	 artist[64];
	char    	 title[64];
	unsigned int version;
	int     	 frequency;
	int     	 channels;
	int     	 datastart;
} bor_header;


typedef union {
	unsigned char chars[2];
	unsigned short shrt;
} u_char2;


//all values are LITTLE endian unless otherwise specified...
typedef struct {
	char text_RIFF[4];
	/*
	ChunkSize:  36 + SubChunk2Size, or more precisely:
	4 + (8 + SubChunk1Size) + (8 + SubChunk2Size)
	This is the size of the rest of the chunk 
	following this number.  This is the size of the 
	entire file in bytes minus 8 bytes for the
	two fields not included in this count:
	ChunkID and ChunkSize.	 * */
	unsigned int filesize_minus_8;
	char text_WAVE[4];
} RIFF_HEADER;

typedef struct {
	char text_fmt[4];
	unsigned int formatheadersize; /* Subchunk1Size    16 for PCM.  This is the size of the
                               rest of the Subchunk which follows this number. */
	u_char2 format; /* big endian, PCM = 1 (i.e. Linear quantization)
                               Values other than 1 indicate some 
                               form of compression. */
	unsigned short channels; //Mono = 1, Stereo = 2, etc
	unsigned int samplerate;
	unsigned int bytespersec; // ByteRate == SampleRate * NumChannels * BitsPerSample/8
	unsigned short blockalign; /* == NumChannels * BitsPerSample/8
                               The number of bytes for one sample including
                               all channels. */
	unsigned short bitwidth; //BitsPerSample    8 bits = 8, 16 bits = 16, etc.
} WAVE_HEADER;

typedef struct {
	char text_data[4]; //Contains the letters "data"
	unsigned int data_size; /* == NumSamples * NumChannels * BitsPerSample/8
                               This is the number of bytes in the data.
                               You can also think of this as the size
                               of the read of the subchunk following this 
                               number.*/
} RIFF_SUBCHUNK2_HEADER;

typedef struct {
	RIFF_HEADER riff_hdr;
	WAVE_HEADER wave_hdr;
	RIFF_SUBCHUNK2_HEADER sub2;
} WAVE_HEADER_COMPLETE;

size_t getfilesize(char *filename) {
	struct stat st;
	if(!stat(filename, &st)) {
		return st.st_size;
	} else
		return 0;
}

static WAVE_HEADER_COMPLETE wave_hdr = {
	{
		{ 'R', 'I', 'F', 'F'},
		0,
		{ 'W', 'A', 'V', 'E'},
	},
	{
		{ 'f', 'm', 't', ' '},
		16,
		{1, 0},
		0,
		44100,
		0,
		0,
		16,
	},
	{
		{ 'd', 'a', 't', 'a' },
		0
	},
};

int bor2wav(char *borname, char* wavname) {
	FILE* fd, *outfd;
	bor_header bh;
	int len, kb = 0;
	char *p, *artist, *title;
	unsigned int size = getfilesize(borname);
	unsigned char in[MUSIC_BUF_SIZE];
	unsigned char out[MUSIC_BUF_SIZE * 4];
	
	fd = fopen(borname, "r");
	
	if(!fread(&bh, sizeof(bh), 1, fd)) return -1;
	size -= sizeof(bh);
	
	outfd = fopen(wavname, "w");
	

	if(strncmp(bh.identifier, BOR_IDENTIFIER, sizeof(bh.identifier))) return -1;

	if((bh.version != BOR_MUSIC_VERSION) && (bh.version != NEW_MUSIC_VERSION)) {
		printf("- warning: unknown file version (%08x)\n", bh.version);
	}
	
	// fix title
	for(p = bh.title; *p; p++) {
		if (*p == '_') *p = ' ';
	}
	
	// fix artist
	for(p = bh.artist; *p; p++) {
		if (*p == '_') *p = ' ';
	}
	
	// force mono if it's a v1 file
	if ((bh.version == BOR_MUSIC_VERSION) && (bh.channels != 1))
	{
		printf("- warning: forcing mono playback because of file version; "
				"use the newest Wav2Bor from LavaLit.com to create BOR files "
				"with more than 1 channel of audio\n");
		bh.channels = 1;
	}

	printf("Decoding: 16bit, %d hz %d channels) %s\n", bh.frequency, bh.channels, borname);

	if(bh.datastart > sizeof(bh)) {
		bh.datastart -= sizeof(bh);
		if(fseek(fd, bh.datastart, SEEK_CUR) < 0) perror("fseek");
		size -= bh.datastart;
	}
	
	wave_hdr.wave_hdr.channels = bh.channels;
	wave_hdr.wave_hdr.samplerate = bh.frequency;
	wave_hdr.wave_hdr.blockalign = bh.channels * (wave_hdr.wave_hdr.bitwidth / 8);
	wave_hdr.wave_hdr.bytespersec = wave_hdr.wave_hdr.samplerate * bh.channels * (wave_hdr.wave_hdr.bitwidth / 8);
	wave_hdr.riff_hdr.filesize_minus_8 = sizeof(WAVE_HEADER_COMPLETE) + (size * 4) - 8; 
	wave_hdr.sub2.data_size = size * 4;
	
	fwrite(&wave_hdr, 1, sizeof(wave_hdr), outfd);
	

	while(size && (len = fread(in, 1, MUSIC_BUF_SIZE, fd))) {
		if(size < len) len = size;
		size -= len;
		
		adpcm_decode(in, (short int*) out, len, bh.channels);
		fwrite(out, 1, len * 4, outfd);
	}

	
	fclose(outfd);
	fclose(fd);
	
	return 0;
}

int syntax(void) {
	puts("bor2wav 1.0 by rofl0r\nsyntax: bor2wav borfile wavfile");
	return 1;
}


int main(int argc, char** argv) {
	if(argc != 3) return syntax();
	return !!bor2wav(argv[1], argv[2]);
}

