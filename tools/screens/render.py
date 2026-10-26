#!/usr/bin/env python3
"""Renders the remote's screens on the Mac: builds tools/screens/render_screens.cpp with the firmware's drawing code
(lib/RemoteUi, lib/RemoteLogic, Adafruit_GFX) and turns each 128 x 32 picture into an enlarged PNG that looks like the
OLED, plus one sheet with all of them.

    python3 tools/screens/render.py [output folder]     (default: tools/screens/out, not in git)
"""
import pathlib
import subprocess
import sys

from PIL import Image, ImageDraw, ImageFont

ROOT = pathlib.Path(__file__).resolve().parents[2]
OUT = pathlib.Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "tools/screens/out"
SCALE = 6
LIT = (120, 200, 255)
DARK = (14, 18, 26)


def build():
    OUT.mkdir(parents=True, exist_ok=True)
    binary = OUT / "render_screens"
    sources = [
        "lib/Adafruit-GFX/Adafruit_GFX.cpp",
        "lib/RemoteLogic/src/StatusView.cpp",
        "lib/RemoteUi/src/RemoteScreens.cpp",
        "tools/screens/render_screens.cpp",
    ]
    includes = ["tools/screens/host", "lib/Adafruit-GFX", "lib/RemoteLogic/src", "lib/RemoteUi/src"]
    command = ["clang++", "-std=c++17", "-O1", "-w", "-DARDUINO=100", "-o", str(binary)]
    command += [f"-I{ROOT / i}" for i in includes] + [str(ROOT / s) for s in sources]
    subprocess.run(command, check=True)
    subprocess.run([str(binary), str(OUT)], check=True)


def read_rows(pbm):
    lines = pbm.read_text().split("\n")
    return [[int(v) for v in line.split()] for line in lines[2:] if line.strip()]


# Rows that must stay empty, so nothing touches (docs: 1 px gap between elements). By file name prefix:
# 0/1 status screen, 2 page, 3 menu, 4 calibration. x 127 is the firmware alive dot.
def components(rows):
    """Connected shapes (8-connected lit pixels): returns a label per pixel and each label's lowest and highest row."""
    labels = [[0] * 128 for _ in range(32)]
    spans = {}
    label = 0
    for y in range(32):
        for x in range(128):
            if not rows[y][x] or labels[y][x]:
                continue
            label += 1
            stack = [(x, y)]
            labels[y][x] = label
            top, bottom = y, y
            while stack:
                cx, cy = stack.pop()
                top, bottom = min(top, cy), max(bottom, cy)
                for dx in (-1, 0, 1):
                    for dy in (-1, 0, 1):
                        nx, ny = cx + dx, cy + dy
                        if 0 <= nx < 128 and 0 <= ny < 32 and rows[ny][nx] and not labels[ny][nx]:
                            labels[ny][nx] = label
                            stack.append((nx, ny))
            spans[label] = (top, bottom)
    return labels, spans


def check_gaps(name, rows):
    """Elements must not touch: no connected shape may reach across a gap row from the element above it into the
    element below it. A descender crossing a gap without reaching the next element's own rows is fine."""
    labels, spans = components(rows)
    kind = name[0]
    # (gap row, x from, x to, the element above ends at or before, the element below starts at or after)
    gaps = []
    if kind in "01":
        gaps = [(7, 0, 126, 6, 9), (22, 0, 126, 21, 28), (23, 0, 126, 22, 28)]
    elif kind == "2":
        gaps = [(7, 0, 126, 6, 9), (15, 0, 126, 14, 17), (23, 0, 126, 22, 25)]
    elif kind == "3":
        gaps = [(9, 0, 122, 8, 10), (19, 0, 122, 18, 20)]
    elif kind == "4":
        gaps = [(7, 31, 95, 6, 9), (15, 31, 95, 14, 17), (22, 31, 95, 21, 24)]
    problems = []
    for y, x0, x1, above, below in gaps:
        for x in range(x0, x1 + 1):
            label = labels[y][x]
            if label and spans[label][0] <= above and spans[label][1] >= below:
                problems.append(f"row {y}: a shape joins the rows above and below at x {x}")
                break
    return problems


def to_png(pbm):
    rows = read_rows(pbm)
    image = Image.new("RGB", (128 * SCALE, 32 * SCALE), DARK)
    draw = ImageDraw.Draw(image)
    for y, row in enumerate(rows):
        for x, value in enumerate(row):
            if value:
                draw.rectangle([x * SCALE, y * SCALE, x * SCALE + SCALE - 2, y * SCALE + SCALE - 2], fill=LIT)
    image.save(pbm.with_suffix(".png"))
    pbm.unlink()
    return image


def main():
    build()
    pictures = []
    failures = 0
    for pbm in sorted(OUT.glob("*.pbm")):
        for problem in check_gaps(pbm.stem, read_rows(pbm)):
            print(f"GAP {pbm.stem}: {problem}")
            failures += 1
        pictures.append((pbm.stem, to_png(pbm)))
    # one sheet, two columns, with names
    width = 128 * SCALE
    height = 32 * SCALE
    label = 18
    columns = 2
    sheet_rows = (len(pictures) + columns - 1) // columns
    sheet = Image.new("RGB", (columns * (width + 20) + 20, sheet_rows * (height + label + 20) + 20), (40, 40, 40))
    draw = ImageDraw.Draw(sheet)
    font = ImageFont.load_default()
    for i, (name, picture) in enumerate(pictures):
        x = 20 + (i % columns) * (width + 20)
        y = 20 + (i // columns) * (height + label + 20)
        draw.text((x, y), name, fill=(220, 220, 220), font=font)
        sheet.paste(picture, (x, y + label))
    sheet.save(OUT / "sheet.png")
    print(f"{len(pictures)} screens in {OUT}, all of them in {OUT / 'sheet.png'}; gap problems: {failures}")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
