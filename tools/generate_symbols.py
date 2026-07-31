#!/usr/bin/env python3

from pathlib import Path
from PIL import Image

SOURCE = Path("assets/font/ppf-symbols.png")
HEADER = Path("src/symbol_font.h")
IMPLEMENTATION = Path("src/symbol_font.c")

SYMBOL_NAMES = [
    "C",
    "DEGREE",
]

SYMBOL_COUNT = len(SYMBOL_NAMES)
SYMBOL_WIDTH = 24
SYMBOL_HEIGHT = 21
SYMBOL_GAP = 2
SYMBOL_STRIDE = SYMBOL_WIDTH + SYMBOL_GAP

EXPECTED_WIDTH = (
    SYMBOL_COUNT * SYMBOL_WIDTH
    + (SYMBOL_COUNT - 1) * SYMBOL_GAP
)

EXPECTED_SIZE = (
    EXPECTED_WIDTH,
    SYMBOL_HEIGHT,
)


def pixel_is_set(
    pixel: tuple[int, int, int, int]
) -> bool:
    red, green, blue, alpha = pixel

    if alpha == 0:
        return False

    if (
        alpha != 255
        or (red, green, blue) != (0, 0, 0)
    ):
        raise ValueError(
            f"Ungültiger Pixel: RGBA{pixel}. "
            "Erlaubt sind nur transparente oder "
            "vollständig schwarze Pixel."
        )

    return True


def validate_separator_columns(
    image: Image.Image
) -> None:
    separator_start = SYMBOL_WIDTH

    for x in range(
        separator_start,
        separator_start + SYMBOL_GAP,
    ):
        for y in range(SYMBOL_HEIGHT):
            if image.getpixel((x, y))[3] != 0:
                raise ValueError(
                    "Die Trennspalte ist nicht transparent: "
                    f"x={x}, y={y}"
                )


def read_symbols(
    image: Image.Image
) -> list[list[int]]:
    symbols: list[list[int]] = []

    for symbol in range(SYMBOL_COUNT):
        start_x = symbol * SYMBOL_STRIDE
        rows: list[int] = []

        for y in range(SYMBOL_HEIGHT):
            row_bits = 0

            for x in range(SYMBOL_WIDTH):
                row_bits <<= 1

                if pixel_is_set(
                    image.getpixel(
                        (start_x + x, y)
                    )
                ):
                    row_bits |= 1

            rows.append(row_bits)

        symbols.append(rows)

    return symbols


def write_header() -> None:
    HEADER.write_text(
        """#pragma once

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
""",
        encoding="utf-8",
    )


def write_implementation(
    symbols: list[list[int]]
) -> None:
    lines = [
        '#include "symbol_font.h"',
        "",
        "const uint32_t PPF_SYMBOLS",
        "    [PPF_SYMBOL_COUNT]",
        "    [PPF_SYMBOL_HEIGHT] = {",
    ]

    for name, rows in zip(
        SYMBOL_NAMES,
        symbols,
        strict=True,
    ):
        lines.append(f"  // {name}")
        lines.append("  {")

        for row in rows:
            lines.append(
                f"    0x{row:06X}u,"
            )

        lines.append("  },")

    lines.append("};")
    lines.append("")

    IMPLEMENTATION.write_text(
        "\n".join(lines),
        encoding="utf-8",
    )


def print_preview(
    symbols: list[list[int]]
) -> None:
    for name, rows in zip(
        SYMBOL_NAMES,
        symbols,
        strict=True,
    ):
        print(f"\nSymbol {name}:")

        for row in rows:
            print(
                "".join(
                    "██"
                    if row
                    & (
                        1
                        << (
                            SYMBOL_WIDTH
                            - 1
                            - x
                        )
                    )
                    else "  "
                    for x in range(
                        SYMBOL_WIDTH
                    )
                )
            )


def main() -> None:
    if not SOURCE.exists():
        raise SystemExit(
            f"Datei nicht gefunden: {SOURCE}"
        )

    image = Image.open(SOURCE).convert(
        "RGBA"
    )

    if image.size != EXPECTED_SIZE:
        raise SystemExit(
            "Falsche Bildgröße: "
            f"{image.size[0]} × {image.size[1]}. "
            "Erwartet werden "
            f"{EXPECTED_SIZE[0]} × "
            f"{EXPECTED_SIZE[1]} Pixel."
        )

    validate_separator_columns(image)

    symbols = read_symbols(image)

    HEADER.parent.mkdir(
        parents=True,
        exist_ok=True,
    )

    write_header()
    write_implementation(symbols)
    print_preview(symbols)

    print(f"\nErzeugt: {HEADER}")
    print(f"Erzeugt: {IMPLEMENTATION}")


if __name__ == "__main__":
    main()
