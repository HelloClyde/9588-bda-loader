# SPDX-License-Identifier: Apache-2.0
from __future__ import annotations

import argparse
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


GLYPH_WIDTH = 12
GLYPH_HEIGHT = 12
CATEGORY_LABELS = "其他听说语法阅读游戏考试背诵词典娱乐工具"
GB2312_HIGH_FIRST = 0xB0
GB2312_HIGH_LAST = 0xF7
GB2312_LOW_FIRST = 0xA1
GB2312_LOW_LAST = 0xFE


def title_from_bda(path: Path) -> str:
    header = path.read_bytes()[:0x88]
    if len(header) < 0x3C:
        return ""
    raw = header[0x2C:0x3C].split(b"\0", 1)[0]
    try:
        return raw.decode("gbk")
    except UnicodeDecodeError:
        return ""


def collect_characters(app_directory: Path | None) -> list[tuple[int, str]]:
    characters = set(CATEGORY_LABELS)

    for high in range(GB2312_HIGH_FIRST, GB2312_HIGH_LAST + 1):
        for low in range(GB2312_LOW_FIRST, GB2312_LOW_LAST + 1):
            pair = bytes((high, low))
            try:
                characters.add(pair.decode("gbk"))
            except UnicodeDecodeError:
                pass

    if app_directory is not None and app_directory.is_dir():
        for path in sorted(app_directory.iterdir()):
            if path.is_file() and path.suffix.lower() == ".bda":
                characters.update(
                    character
                    for character in title_from_bda(path)
                    if ord(character) >= 0x80
                )

    encoded: list[tuple[int, str]] = []
    for character in characters:
        try:
            pair = character.encode("gbk")
        except UnicodeEncodeError:
            continue
        if len(pair) == 2:
            encoded.append(((pair[0] << 8) | pair[1], character))
    return sorted(encoded)


def render_glyph(font: ImageFont.FreeTypeFont, character: str) -> bytes:
    image = Image.new("L", (GLYPH_WIDTH, GLYPH_HEIGHT), 0)
    draw = ImageDraw.Draw(image)
    left, top, right, bottom = draw.textbbox((0, 0), character, font=font)
    width = right - left
    height = bottom - top
    x = (GLYPH_WIDTH - width) // 2 - left
    y = (GLYPH_HEIGHT - height) // 2 - top

    draw.text((x, y), character, font=font, fill=255)
    pixels = image.load()
    rows = bytearray()
    for row in range(GLYPH_HEIGHT):
        bits = 0
        for column in range(GLYPH_WIDTH):
            if pixels[column, row] >= 96:
                bits |= 0x8000 >> column
        rows.extend(((bits >> 8) & 0xFF, bits & 0xFF))
    return bytes(rows)


def generate(
    font_path: Path,
    app_directory: Path | None,
    output: Path,
) -> None:
    font = ImageFont.truetype(str(font_path), 12)
    entries: list[str] = []

    for key, character in collect_characters(app_directory):
        glyph = render_glyph(font, character)
        rows = ", ".join(f"0x{value:02x}" for value in glyph)
        entries.append(
            f"    {{0x{key:04x}u, {{{rows}}}}}, /* {character} */"
        )

    body = "\n".join(
        [
            "/* SPDX-License-Identifier: OFL-1.1 */",
            "/* Generated from Noto Sans CJK SC Regular by "
            "tools/generate_small_font.py. */",
            "static const small_gbk_glyph_t k_small_gbk_glyphs[] = {",
            *entries,
            "};",
            "",
            "#define SMALL_GBK_GLYPH_COUNT \\",
            "    ((u32)(sizeof(k_small_gbk_glyphs) / "
            "sizeof(k_small_gbk_glyphs[0])))",
            "",
        ]
    )
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(body, encoding="utf-8", newline="\n")


def main() -> None:
    parser = argparse.ArgumentParser(
        description="从开放字体生成 BDA Loader 的 12×12 GBK 字体表。",
    )
    parser.add_argument("--font", type=Path, required=True)
    parser.add_argument("--apps", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    generate(args.font, args.apps, args.output)


if __name__ == "__main__":
    main()
