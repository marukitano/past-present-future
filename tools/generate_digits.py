#!/usr/bin/env python3

from pathlib import Path
from PIL import Image

SOURCE = Path("assets/font/ppf-digits.png")
HEADER = Path("src/digit_font.h")
IMPLEMENTATION = Path("src/digit_font.c")

DIGIT_COUNT = 10
DIGIT_WIDTH = 24
DIGIT_HEIGHT = 21
DIGIT_GAP = 2
DIGIT_STRIDE = DIGIT_WIDTH + DIGIT_GAP

EXPECTED_WIDTH = DIGIT_COUNT * DIGIT_WIDTH + (DIGIT_COUNT - 1) * DIGIT_GAP
EXPECTED_SIZE = (EXPECTED_WIDTH, DIGIT_HEIGHT)


def pixel_is_set(pixel: tuple[int, int, int, int]) -> bool:
    red, green, blue, alpha = pixel

    if alpha == 0:
        return False

    # Erlaubt nur schwarze, deckende Pixel.
    if alpha != 255 or (red, green, blue) != (0, 0, 0):
        raise ValueError(
            f"Ungültiger Pixel: RGBA{pixel}. "
            "Erlaubt sind nur transparent oder vollständig schwarz."
        )

    return True


def validate_separator_columns(image: Image.Image) -> None:
    for digit in range(DIGIT_COUNT - 1):
        separator_start = digit * DIGIT_STRIDE + DIGIT_WIDTH

        for x in range(separator_start, separator_start + DIGIT_GAP):
            for y in range(DIGIT_HEIGHT):
                if image.getpixel((x, y))[3] != 0:
                    raise ValueError(
                        f"Die Trennspalte bei x={x}, y={y} ist nicht transparent."
                    )


def read_digits(image: Image.Image) -> list[list[int]]:
    digits: list[list[int]] = []

    for digit in range(DIGIT_COUNT):
        start_x = digit * DIGIT_STRIDE
        rows: list[int] = []

        for y in range(DIGIT_HEIGHT):
            row_bits = 0

            for x in range(DIGIT_WIDTH):
                row_bits <<= 1

                if pixel_is_set(image.getpixel((start_x + x, y))):
                    row_bits |= 1

            rows.append(row_bits)

        digits.append(rows)

    return digits


def write_header() -> None:
    HEADER.write_text(
        """#pragma once

#include <stdint.h>

#define PPF_DIGIT_COUNT 10
#define PPF_DIGIT_WIDTH 24
#define PPF_DIGIT_HEIGHT 21

/*
 * Jede horizontale Pixelzeile steckt in den unteren 24 Bit.
 * Das höchstwertige dieser 24 Bit ist der linke Pixel.
 */
extern const uint32_t PPF_DIGITS[PPF_DIGIT_COUNT][PPF_DIGIT_HEIGHT];
""",
        encoding="utf-8",
    )


def write_implementation(digits: list[list[int]]) -> None:
    lines = [
        '#include "digit_font.h"',
        "",
        "const uint32_t PPF_DIGITS[PPF_DIGIT_COUNT][PPF_DIGIT_HEIGHT] = {",
    ]

    for digit, rows in enumerate(digits):
        lines.append(f"  // {digit}")
        lines.append("  {")

        for row in rows:
            lines.append(f"    0x{row:06X}u,")

        lines.append("  },")

    lines.append("};")
    lines.append("")

    IMPLEMENTATION.write_text("\n".join(lines), encoding="utf-8")


def print_preview(digits: list[list[int]]) -> None:
    for digit, rows in enumerate(digits):
        print(f"\nZiffer {digit}:")

        for row in rows:
            print(
                "".join(
                    "██" if row & (1 << (DIGIT_WIDTH - 1 - x)) else "  "
                    for x in range(DIGIT_WIDTH)
                )
            )


def main() -> None:
    if not SOURCE.exists():
        raise SystemExit(f"Datei nicht gefunden: {SOURCE}")

    image = Image.open(SOURCE).convert("RGBA")

    if image.size != EXPECTED_SIZE:
        raise SystemExit(
            f"Falsche Bildgröße: {image.size[0]} × {image.size[1]} Pixel. "
            f"Erwartet werden {EXPECTED_SIZE[0]} × {EXPECTED_SIZE[1]} Pixel."
        )

    validate_separator_columns(image)
    digits = read_digits(image)

    HEADER.parent.mkdir(parents=True, exist_ok=True)
    write_header()
    write_implementation(digits)

    print(f"Erzeugt: {HEADER}")
    print(f"Erzeugt: {IMPLEMENTATION}")
    print_preview(digits)


if __name__ == "__main__":
    main()
