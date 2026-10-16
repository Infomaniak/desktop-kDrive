#!/usr/bin/env python3
"""Generate the badge PNG of the nightly vulnerability scan.

The badge is drawn locally at 2x resolution with Pillow so it stays crisp
when kChat scales it in the conversation: shields.io only serves 1x rasters,
which end up blurry. The file is published as a static asset by the workflow
so every kChat message keeps its own result.
"""

import argparse
import pathlib

from PIL import Image, ImageDraw, ImageFont

# Supersampling factor: kChat downscales the image, which stays crisp
SCALE = 2
FONT_CANDIDATES = [
    "C:/Windows/Fonts/segoeuib.ttf",
    "C:/Windows/Fonts/arialbd.ttf",
    "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
    "/System/Library/Fonts/Supplemental/Arial Bold.ttf",
]


def load_font(size):
    for path in FONT_CANDIDATES:
        if pathlib.Path(path).exists():
            return ImageFont.truetype(path, size)
    return ImageFont.load_default(size)


def spaced_width(draw, text, font, spacing):
    return sum(draw.textlength(char, font=font) for char in text) + spacing * max(len(text) - 1, 0)


def draw_spaced(draw, xy, text, font, fill, spacing):
    x, y = xy
    for char in text:
        draw.text((x, y), char, font=font, fill=fill, anchor="lm")
        x += draw.textlength(char, font=font) + spacing


def make_badge(left, right, color, output, rounded=True, left_bg="#555"):
    font_size, padding, height, spacing = 11 * SCALE, 6 * SCALE, 28 * SCALE, SCALE
    font = load_font(font_size)
    probe = ImageDraw.Draw(Image.new("RGBA", (10, 10)))
    left_width = spaced_width(probe, left, font, spacing) + 2 * padding
    right_width = spaced_width(probe, right, font, spacing) + 2 * padding
    width = int(left_width + right_width)
    image = Image.new("RGBA", (width, int(height)))
    draw = ImageDraw.Draw(image)
    if rounded:
        radius = 12
        draw.rounded_rectangle([0, 0, width - 1, height - 1], radius=radius, fill=color)
        draw.rounded_rectangle([0, 0, left_width - 1, height - 1], radius=radius,
                               corners=(True, False, False, True), fill=left_bg)
    else:
        draw.rectangle([0, 0, width - 1, height - 1], fill=color)
        draw.rectangle([0, 0, left_width - 1, height - 1], fill=left_bg)
    draw_spaced(draw, (padding, height / 2), left, font, "white", spacing)
    draw_spaced(draw, (left_width + padding, height / 2), right, font, "white", spacing)
    pathlib.Path(output).parent.mkdir(parents=True, exist_ok=True)
    image.save(output)


def main():
    parser = argparse.ArgumentParser(description="Draw the kChat badge PNG of the vulnerability scan")
    parser.add_argument("--left", default="VULNERABILITY SCAN", help="Text of the left segment")
    parser.add_argument("--right", required=True, help="Text of the colored segment")
    parser.add_argument("--color", required=True, help="Hex color of the right segment")
    parser.add_argument("--output", required=True, help="Path of the PNG to write")
    parser.add_argument("--square", action="store_true", help="Use square corners instead of rounded ones")
    args = parser.parse_args()
    make_badge(args.left, args.right, args.color, args.output, rounded=not args.square)


if __name__ == "__main__":
    main()
