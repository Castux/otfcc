#include <stdio.h>

#include "otfcc/sfnt.h"
#include "otfcc/font.h"
#include "otfcc/sfnt-builder.h"

#include "support/util.h"
#include "otfcc/sfnt.h"

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

uint8_t parse_8u(uint8_t **buffer)
{
	uint8_t value = read_8u(*buffer);
	*buffer += 1;
	return value;
}

uint16_t parse_16u(uint8_t **buffer)
{
	uint16_t value = read_16u(*buffer);
	*buffer += 2;
	return value;
}

uint32_t parse_32u(uint8_t **buffer)
{
	uint32_t value = read_32u(*buffer);
	*buffer += 4;
	return value;
}

void readPacketsFromBuffer(otfcc_SplineFontContainer *font, uint8_t *buffer)
{
	uint8_t *head;

	for (uint32_t count = 0; count < font->count; count++) {
		head = buffer + font->offsets[count];

		font->packets[count].sfnt_version = parse_32u(&head);
		font->packets[count].numTables = parse_16u(&head);
		font->packets[count].searchRange = parse_16u(&head);
		font->packets[count].entrySelector = parse_16u(&head);
		font->packets[count].rangeShift = parse_16u(&head);

		printf("readPacketsFromBuffer %d %d %d %d %d\n",
			font->packets[count].sfnt_version,
			font->packets[count].numTables,
			font->packets[count].searchRange,
			font->packets[count].entrySelector,
			font->packets[count].rangeShift
		);

		NEW(font->packets[count].pieces, font->packets[count].numTables);

		for (uint32_t i = 0; i < font->packets[count].numTables; i++) {
			font->packets[count].pieces[i].tag = parse_32u(&head);
			font->packets[count].pieces[i].checkSum = parse_32u(&head);
			font->packets[count].pieces[i].offset = parse_32u(&head);
			font->packets[count].pieces[i].length = parse_32u(&head);
			NEW(font->packets[count].pieces[i].data, font->packets[count].pieces[i].length);
		}

		for (uint32_t i = 0; i < font->packets[0].numTables; i++) {
			head = buffer + font->packets[count].pieces[i].offset;
			memcpy(font->packets[count].pieces[i].data, head, font->packets[count].pieces[i].length);
		}
	}
}

otfcc_SplineFontContainer *readSFNTfromBuffer(uint8_t *buffer) {
	otfcc_SplineFontContainer *font;
	NEW(font);

	uint8_t *head = buffer;

	font->type = parse_32u(&head);

	switch (font->type) {
		case 'OTTO':
		case 0x00010000:
		case 'true':
		case 'typ1':
			font->count = 1;
			NEW(font->offsets, font->count);
			NEW(font->packets, font->count);
			font->offsets[0] = 0;
			readPacketsFromBuffer(font, buffer);
			break;

		case 'ttcf':
			parse_32u(&head);
			font->count = parse_32u(&head);
			NEW(font->offsets, font->count);
			NEW(font->packets, font->count);

			for (uint32_t i = 0; i < font->count; i++) {
				font->offsets[i] = parse_32u(&head);
			}

			readPacketsFromBuffer(font, buffer);
			break;

		default:
			font->count = 0;
			font->offsets = NULL;
			font->packets = NULL;
			break;
	}

	return font;
}

// This is a rewrite of otfccdump.c main()

void fontToJSON(char *inPath, char *outputPath)
{
	uint32_t ttcindex = 0;

	bool show_pretty = true;
	bool show_ugly = false;
	bool add_bom = false;

	otfcc_Options *options = otfcc_newOptions();
	options->logger = otfcc_newLogger(otfcc_newStdErrTarget());
	options->logger->indent(options->logger, "wasm");
	options->decimal_cmap = true;

	// Read the raw font file

	uint8_t *dataIn;
	loggedStep("Load file") {
		size_t length;
		readEntireFile(inPath, &dataIn, &length);
		printf("Read %s: (%ld bytes)\n", inPath, length);
	}

	// Parse the container

	otfcc_SplineFontContainer *sfnt;
	loggedStep("Read SFNT") {
		sfnt = readSFNTfromBuffer(dataIn);
		if (!sfnt || sfnt->count == 0) {
			logError("Cannot read SFNT file \"%s\". Exit.\n", inPath);
			exit(EXIT_FAILURE);
		}
		if (ttcindex >= sfnt->count) {
			logError("Subfont index %d out of range for \"%s\" (0 -- %d). Exit.\n", ttcindex,
					 inPath, (sfnt->count - 1));
			exit(EXIT_FAILURE);
		}
		free(dataIn);
	}

	// Build font structure

	otfcc_Font *font;
	loggedStep("Read Font") {
		otfcc_IFontBuilder *reader = otfcc_newOTFReader();
		font = reader->read(sfnt, ttcindex, options);
		if (!font) {
			logError("Font structure broken or corrupted \"%s\". Exit.\n", inPath);
			exit(EXIT_FAILURE);
		}
		reader->free(reader);
		if (sfnt) otfcc_deleteSFNT(sfnt);
	}

	loggedStep("Consolidate") {
		otfcc_iFont.consolidate(font, options);
	}

	// Export to JSON

	json_value *root;
	loggedStep("Dump") {
		otfcc_IFontSerializer *dumper = otfcc_newJsonWriter();
		root = (json_value *)dumper->serialize(font, options);
		if (!root) {
			logError("Font structure broken or corrupted \"%s\". Exit.\n", inPath);
			exit(EXIT_FAILURE);
		}
		dumper->free(dumper);
	}

	char *buf;
	size_t buflen;
	loggedStep("Serialize to JSON") {
		json_serialize_opts jsonOptions;
		jsonOptions.mode = json_serialize_mode_packed;
		jsonOptions.opts = 0;
		jsonOptions.indent_size = 4;
		if (show_pretty || (!outputPath && isatty(fileno(stdout)))) {
			jsonOptions.mode = json_serialize_mode_multiline;
		}
		if (show_ugly) jsonOptions.mode = json_serialize_mode_packed;
		buflen = json_measure_ex(root, jsonOptions);
		buf = calloc(1, buflen);
		json_serialize_ex(buf, root, jsonOptions);
	}

	loggedStep("Output") {
		FILE *outputFile = u8fopen(outputPath, "wb");
		if (!outputFile) {
			logError("Cannot write to file \"%s\". Exit.", outputPath);
			exit(EXIT_FAILURE);
		}
		if (add_bom) {
			fputc(0xEF, outputFile);
			fputc(0xBB, outputFile);
			fputc(0xBF, outputFile);
		}
		size_t actualLen = buflen - 1;
		while (!buf[actualLen])
			actualLen -= 1;
		fwrite(buf, sizeof(char), actualLen + 1, outputFile);
		fclose(outputFile);
	}

	// Clean up

	loggedStep("Finalize") {
		free(buf);
		if (font) otfcc_iFont.free(font);
		if (root) json_builder_free(root);
	}
	otfcc_deleteOptions(options);
}

int main(int argc, char *argv[])
{
	if(argc < 3)
	{
		fprintf(stderr, "Usage: wasm <input.[otf|ttf|otc]> <output.json>\n");
		exit(EXIT_FAILURE);
	}

	fontToJSON(argv[1], argv[2]);

	return 0;
}
