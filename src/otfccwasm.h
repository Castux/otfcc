#ifndef OTFCC_WASM_H
#define OTFCC_WASM_H

#include "otfcc/sfnt.h"
#include "otfcc/font.h"
#include "otfcc/sfnt-builder.h"

#include "support/util.h"
#include "otfcc/sfnt.h"

#include "aliases.h"
#include "platform.h"
#include "stopwatch.h"

struct blob
{
	uint8_t *data;
	size_t length;
};

struct blob* fontToJSON(uint8_t *dataIn, size_t lenIn);
struct blob* JSONtoFont(uint8_t *dataIn, size_t lenIn);
void free_blob(struct blob *blob);

#endif // OTFCC_WASM_H
