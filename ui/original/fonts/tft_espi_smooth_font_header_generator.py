#!/usr/bin/env python3
"""
Generate TFT_eSPI smooth-font FLASH array headers directly from a TTF/OTF font.

Output is a byte-for-byte .vlw-style payload wrapped as:
    const uint8_t FontName[] PROGMEM = { ... };

It is intended for TFT_eSPI's:
    tft.loadFont(FontName);

Dependencies:
    pip install pillow fonttools

Example:
    python tft_espi_smooth_font_header_generator.py ^
      --font "C:\\Windows\\Fonts\\Roboto-Regular.ttf" ^
      --size 16 --size 24 --size 36 ^
      --name-prefix Roboto ^
      --glyphs "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz .:/%-₂°+" ^
      --out-dir ui\\fonts
"""

from __future__ import annotations

import argparse
import re
import struct
import unicodedata
from pathlib import Path
from typing import Iterable

from PIL import Image, ImageDraw, ImageFont
from fontTools.ttLib import TTFont

DEFAULT_GLYPHS = (
    "0123456789"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    " .:/%-_+()"
    "₂°"
)

PROJECT_STRINGS = [
    "Not available",
    "Temp/Humidity",
    "Indoor",
    "Outside",
    "VOC",
    "PM25",
    "NOx",
    "PM 2.5",
    "VOC Level",
    "NOx Level",
    "Recent CO2 Values",
    "NA",
    "ppm",
    "No data",
    "Good",
    "Fair",
    "Poor",
    "Bad",
    "Awaiting samples",
]


def u32be(value: int) -> bytes:
    return struct.pack(">I", value & 0xFFFFFFFF)


def sanitize_c_identifier(name: str) -> str:
    name = re.sub(r"[^0-9A-Za-z_]", "_", name)
    if not name or name[0].isdigit():
        name = "font_" + name
    return name


def unique_chars(text: str) -> list[str]:
    seen: set[str] = set()
    out: list[str] = []
    for ch in text:
        cp = ord(ch)
        if cp in (0xFE0F, 0xFE0E):  # variation selectors
            continue
        if cp > 0xFFFF:
            raise ValueError(f"TFT_eSPI smooth fonts support BMP only; got U+{cp:04X} {ch!r}")
        if ch not in seen:
            seen.add(ch)
            out.append(ch)
    return out


def font_names(font_path: Path) -> tuple[str, str]:
    try:
        tt = TTFont(str(font_path))
        names = tt["name"].names
        family = None
        ps = None
        for n in names:
            if n.nameID == 1 and family is None:
                family = n.toUnicode()
            elif n.nameID == 6 and ps is None:
                ps = n.toUnicode()
        return family or font_path.stem, ps or font_path.stem
    except Exception:
        return font_path.stem, font_path.stem


def glyph_to_bitmap(font: ImageFont.FreeTypeFont, ch: str) -> tuple[int, int, int, int, int, bytes]:
    """Return unicode, height, width, advance, gdY, gdX, bitmap bytes."""
    cp = ord(ch)

    # Spaces and some combining marks may have no drawable bitmap. TFT_eSPI handles ASCII space itself.
    bbox = font.getbbox(ch, anchor="ls")
    advance = int(round(font.getlength(ch)))

    if bbox is None:
        return cp, 0, 0, max(advance, 0), 0, 0, b""

    left, top, right, bottom = bbox
    width = max(0, int(right - left))
    height = max(0, int(bottom - top))

    if width == 0 or height == 0:
        return cp, 0, 0, max(advance, 0), int(-top), int(left), b""

    if width > 255 or height > 255 or advance > 255:
        raise ValueError(
            f"Glyph {ch!r} U+{cp:04X} too large for TFT_eSPI uint8 metrics: "
            f"w={width}, h={height}, advance={advance}"
        )
    if left < -128 or left > 127:
        raise ValueError(f"Glyph {ch!r} has gdX={left}, outside TFT_eSPI int8 range")

    img = Image.new("L", (width, height), 0)
    draw = ImageDraw.Draw(img)
    draw.text((-left, -top), ch, font=font, fill=255, anchor="ls")
    return cp, height, width, max(advance, 0), int(-top), int(left), img.tobytes()


def build_vlw(font_path: Path, size: int, chars: Iterable[str], font_label: str | None = None) -> bytes:
    font = ImageFont.truetype(str(font_path), size=size, layout_engine=ImageFont.Layout.BASIC)
    family, postscript = font_names(font_path)
    label = font_label or f"{family}-{size}"

    glyphs = []
    bitmaps = bytearray()

    for ch in sorted(unique_chars("".join(chars)), key=ord):
        cp, h, w, adv, gdy, gdx, bmp = glyph_to_bitmap(font, ch)
        glyphs.append((cp, h, w, adv, gdy, gdx, 0))
        bitmaps.extend(bmp)

    # Approximate Processing/TFT_eSPI expectations:
    # ascent = baseline to top of 'd'; descent = baseline to bottom of 'p'.
    d_bbox = font.getbbox("d", anchor="ls")
    p_bbox = font.getbbox("p", anchor="ls")
    ascent = int(-d_bbox[1]) if d_bbox else size
    descent = int(p_bbox[3]) if p_bbox else max(1, size // 4)

    data = bytearray()
    data += u32be(len(glyphs))
    data += u32be(11)          # VLW encoder version used by TFT_eSPI examples
    data += u32be(size)        # nominal point/pixel size
    data += u32be(0)           # deprecated mboxY
    data += u32be(ascent)
    data += u32be(descent)

    for cp, h, w, adv, gdy, gdx, pad in glyphs:
        data += u32be(cp)
        data += u32be(h)
        data += u32be(w)
        data += u32be(adv)
        data += u32be(gdy)
        data += u32be(gdx)
        data += u32be(pad)

    data += bitmaps

    # Trailer expected by TFT_eSPI comments, though TFT_eSPI does not need it for rendering.
    label_b = label.encode("utf-8")[:255]
    ps_b = postscript.encode("utf-8")[:255]
    data += bytes([len(label_b)]) + label_b + b"\x00"
    data += bytes([len(ps_b)]) + ps_b + b"\x00"
    data += b"\x01"  # anti-aliased flag

    return bytes(data)


def write_header(array_name: str, data: bytes, out_path: Path) -> None:
    array_name = sanitize_c_identifier(array_name)
    lines = []
    lines.append("#pragma once")
    lines.append("#include <Arduino.h>")
    lines.append("")
    lines.append(f"const uint8_t {array_name}[] PROGMEM = {{")
    for i in range(0, len(data), 12):
        chunk = data[i:i + 12]
        lines.append("  " + ", ".join(f"0x{b:02X}" for b in chunk) + ",")
    lines.append("};")
    lines.append("")
    out_path.write_text("\n".join(lines), encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser(description="Generate TFT_eSPI smooth-font flash-array headers from TTF/OTF.")
    parser.add_argument("--font", required=True, type=Path, help="Path to .ttf or .otf font file")
    parser.add_argument("--size", required=True, type=int, action="append", help="Font size; repeat for multiple sizes")
    parser.add_argument("--name-prefix", default=None, help="C array/header prefix, e.g. Roboto")
    parser.add_argument("--glyphs", default=DEFAULT_GLYPHS, help="Characters to include")
    parser.add_argument("--include-project-strings", action="store_true", help="Add glyphs from this project's screen strings")
    parser.add_argument("--out-dir", default=Path("."), type=Path, help="Output directory")
    parser.add_argument("--write-vlw", action="store_true", help="Also write raw .vlw files next to headers")
    args = parser.parse_args()

    if not args.font.exists():
        raise FileNotFoundError(args.font)

    glyphs = args.glyphs
    if args.include_project_strings:
        glyphs += "".join(PROJECT_STRINGS)
    chars = unique_chars(glyphs)

    prefix = args.name_prefix or sanitize_c_identifier(args.font.stem)
    args.out_dir.mkdir(parents=True, exist_ok=True)

    for size in args.size:
        array_name = sanitize_c_identifier(f"{prefix}_{size}")
        data = build_vlw(args.font, size, chars, font_label=f"{prefix}_{size}")
        header_path = args.out_dir / f"{array_name}.h"
        write_header(array_name, data, header_path)
        if args.write_vlw:
            (args.out_dir / f"{array_name}.vlw").write_bytes(data)
        print(f"Wrote {header_path} ({len(data):,} bytes, {len(chars)} glyphs)")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
