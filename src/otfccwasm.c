#include "otfccwasm.h"

#ifdef EMSCRIPTEN
#include "emscripten.h"
#else
#define EMSCRIPTEN_KEEPALIVE
#endif

uint8_t parse_8u(uint8_t **buffer) {
	uint8_t value = read_8u(*buffer);
	*buffer += 1;
	return value;
}

uint16_t parse_16u(uint8_t **buffer) {
	uint16_t value = read_16u(*buffer);
	*buffer += 2;
	return value;
}

uint32_t parse_32u(uint8_t **buffer) {
	uint32_t value = read_32u(*buffer);
	*buffer += 4;
	return value;
}

void readPacketsFromBuffer(otfcc_SplineFontContainer *font, uint8_t *buffer) {
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

struct blob* EMSCRIPTEN_KEEPALIVE fontToJSON(uint8_t *dataIn, size_t lenIn) {
	uint32_t ttcindex = 0;

	bool show_pretty = true;
	bool show_ugly = false;

	otfcc_Options *options = otfcc_newOptions();
	options->logger = otfcc_newLogger(otfcc_newStdErrTarget());
	options->logger->indent(options->logger, "wasm");
	options->decimal_cmap = true;

	// Parse the container

	otfcc_SplineFontContainer *sfnt;
	loggedStep("Read SFNT") {
		sfnt = readSFNTfromBuffer(dataIn);
		if (!sfnt || sfnt->count == 0) {
			logError("Cannot read SFNT file. Exit.\n");
			exit(EXIT_FAILURE);
		}
		if (ttcindex >= sfnt->count) {
			logError("Subfont index %d out of range (0 -- %d). Exit.\n",
				ttcindex, (sfnt->count - 1));
			exit(EXIT_FAILURE);
		}
	}

	// Build font structure

	otfcc_Font *font;
	loggedStep("Read Font") {
		otfcc_IFontBuilder *reader = otfcc_newOTFReader();
		font = reader->read(sfnt, ttcindex, options);
		if (!font) {
			logError("Font structure broken or corrupted. Exit.\n");
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
			logError("Font structure broken or corrupted. Exit.\n");
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
		if (show_pretty) {
			jsonOptions.mode = json_serialize_mode_multiline;
		}
		if (show_ugly) jsonOptions.mode = json_serialize_mode_packed;
		buflen = json_measure_ex(root, jsonOptions);
		buf = calloc(1, buflen);
		json_serialize_ex(buf, root, jsonOptions);

		// Remove trailing 0's
		while (!buf[buflen])
			buflen -= 1;
		buflen++;
	}

	struct blob *result = malloc(sizeof(struct blob));
	result->data = (uint8_t *)buf;
	result->length = buflen;

	// Clean up

	loggedStep("Finalize") {
		if (font) otfcc_iFont.free(font);
		if (root) json_builder_free(root);
	}
	otfcc_deleteOptions(options);

	return result;
}

struct blob* EMSCRIPTEN_KEEPALIVE JSONtoFont(uint8_t *dataIn, size_t lenIn) {
	otfcc_Options *options = otfcc_newOptions();
	options->logger = otfcc_newLogger(otfcc_newStdErrTarget());
	options->logger->indent(options->logger, "wasm");
	otfcc_Options_optimizeTo(options, 1);

	json_value *jsonRoot = NULL;
	loggedStep("Parse into JSON") {
		jsonRoot = json_parse((char *) dataIn, lenIn);
		if (!jsonRoot) {
			logError("Cannot parse JSON file. Exit.\n");
			exit(EXIT_FAILURE);
		}
	}

	otfcc_Font *font;
	loggedStep("Parse") {
		otfcc_IFontBuilder *parser = otfcc_newJsonReader();
		font = parser->read(jsonRoot, 0, options);
		if (!font) {
			logError("Cannot parse JSON file as a font. Exit.\n");
			exit(EXIT_FAILURE);
		}
		parser->free(parser);
		json_value_free(jsonRoot);
	}
	loggedStep("Consolidate") {
		otfcc_iFont.consolidate(font, options);
	}

	struct blob *result = malloc(sizeof(struct blob));
	loggedStep("Build") {
		otfcc_IFontSerializer *writer = otfcc_newOTFWriter();
		caryll_Buffer *otf = (caryll_Buffer *)writer->serialize(font, options);

		result->length = buflen(otf);
		result->data = malloc(result->length);
		memcpy(result->data, otf->data, result->length);

		buffree(otf), writer->free(writer), otfcc_iFont.free(font);
	}
	otfcc_deleteOptions(options);

	return result;
}

struct blob* EMSCRIPTEN_KEEPALIVE test(size_t len)
{
	struct blob *res = malloc(sizeof(struct blob));
	res->data = malloc(len);
	res->length = len;
	for(size_t i = 0; i < len; i++)
		res->data[i] = 0x69;

	return res;
}

uint8_t* EMSCRIPTEN_KEEPALIVE blob_data(struct blob *blob)
{
	return blob->data;
}

size_t EMSCRIPTEN_KEEPALIVE blob_length(struct blob *blob)
{
	return blob->length;
}

void EMSCRIPTEN_KEEPALIVE free_blob(struct blob *blob)
{
	free(blob->data);
	free(blob);
}
