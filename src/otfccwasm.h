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

void fontToJSON(uint8_t *dataIn, size_t lenIn, uint8_t **dataOut, size_t *lenOut);
void JSONtoFont(uint8_t *dataIn, size_t lenIn,  uint8_t **dataOut, size_t *lenOut);

#endif // OTFCC_WASM_H
