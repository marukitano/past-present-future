#pragma once

#include <stdint.h>

#define PPF_SYMBOL_COUNT 2
#define PPF_SYMBOL_WIDTH 24
#define PPF_SYMBOL_HEIGHT 21

typedef enum {
  PPF_SYMBOL_C = 0,
  PPF_SYMBOL_DEGREE = 1
} PpfSymbol;

/*
 * Jede horizontale Pixelzeile steckt in den
 * unteren 24 Bit.
 */
extern const uint32_t PPF_SYMBOLS
    [PPF_SYMBOL_COUNT]
    [PPF_SYMBOL_HEIGHT];
