#include <stdio.h>
#include "otfccwasm.h"

void readEntireFile(char *inPath, uint8_t **_buffer, size_t *_length) {
	uint8_t *buffer = NULL;
	size_t length = 0;
	FILE *f = fopen(inPath, "rb");
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

int main(int argc, char *argv[]) {

	if(argc < 3) {
		fprintf(stderr, "Usage: wasm <input.[otf|ttf|otc]> <output.json>\n");
		exit(EXIT_FAILURE);
	}

	char *inPath = argv[1];
	char *outPath = argv[2];

	// Read the raw font file

	uint8_t *dataIn = NULL;
	size_t lenIn = 0;

	readEntireFile(inPath, &dataIn, &lenIn);
	printf("Read %s: (%ld bytes)\n", inPath, lenIn);

	uint8_t *dataOut = NULL;
	size_t lenOut = 0;

//	fontToJSON(dataIn, lenIn, &dataOut, &lenOut);
	JSONtoFont(dataIn, lenIn, &dataOut, &lenOut);

	FILE *outFile = fopen(outPath, "wb");
	if(!outFile) {
		fprintf(stderr, "Cannot write to file \"%s\". Exit.\n", outPath);
		exit(EXIT_FAILURE);
	}
	fwrite(dataOut, sizeof(uint8_t), lenOut, outFile);
	fclose(outFile);

	free(dataIn);
	free(dataOut);

	return 0;
}
