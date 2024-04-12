#!/usr/bin/env bash

SRC=`find dep lib -name '*.c' -not -path "*msvc*"`
INCLUDE="-Iinclude -Ilib -Ideps -Iinclude/dep"
OPTS="-g -sMODULARIZE -sALLOW_MEMORY_GROWTH -sEXPORTED_FUNCTIONS=['_malloc','_free']"

emcc $INCLUDE src/otfccwasm.c $SRC $OPTS -o wasm-poc/otfccwasm.js
