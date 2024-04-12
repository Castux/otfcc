#!/usr/bin/env bash

SRC=`find dep lib -name '*.c' -not -path "*msvc*"`
INCLUDE="-Iinclude -Ilib -Ideps -Iinclude/dep"
OPTS="-O3 -sALLOW_MEMORY_GROWTH"

emcc $INCLUDE src/otfccwasm.c $SRC $OPTS -o otfccwasm.js
