#pragma once

#include <stdint.h>

#define PPF_DIGIT_COUNT 10
#define PPF_DIGIT_WIDTH 24
#define PPF_DIGIT_HEIGHT 21

/*
 * Jede horizontale Pixelzeile steckt in den unteren 24 Bit.
 * Das höchstwertige dieser 24 Bit ist der linke Pixel.
 */
extern const uint32_t PPF_DIGITS[PPF_DIGIT_COUNT][PPF_DIGIT_HEIGHT];
