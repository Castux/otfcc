#include <stdio.h>

#include "otfcc/sfnt.h"
#include "otfcc/font.h"
#include "otfcc/sfnt-builder.h"

#include "aliases.h"
#include "platform.h"
#include "stopwatch.h"

void readEntireFile(char *inPath, uint8_t **_buffer, size_t *_length) {
	uint8_t *buffer = NULL;
	size_t length = 0;
	FILE *f = u8fopen(inPath, "rb");
	if (!f) {
		fprintf(stderr, "Cannot read file \"%s\". Exit.\n", inPath);
		exit(EXIT_FAILURE);
	}
	fseek(f, 0, SEEK_END);
	length = ftell(f);
	fseek(f, 0, SEEK_SET);
	buffer = malloc(length);
	if (buffer) { fread(buffer, 1, length, f); }
	fclose(f);

	if (!buffer) {
		fprintf(stderr, "Cannot read file \"%s\". Exit.\n", inPath);
		exit(EXIT_FAILURE);
	}
	*_buffer = buffer;
	*_length = length;
}

otfcc_SplineFontContainer *readSFNTfromBuffer(uint8_t *buffer)
{
	return NULL;
}

void fontToJSON(char *inPath)
{
	uint8_t *dataIn;
	size_t length;

	readEntireFile(inPath, &dataIn, &length);
	printf("Read %s: (%ld bytes)\n", inPath, length);

	otfcc_SplineFontContainer *sfnt = readSFNTfromBuffer(dataIn);

	free(dataIn);
}

int main(int argc, char *argv[])
{
	if(argc < 2)
	{
		fprintf(stderr, "Missing file name\n");
		exit(EXIT_FAILURE);
	}

	fontToJSON(argv[1]);

	return 0;
}
