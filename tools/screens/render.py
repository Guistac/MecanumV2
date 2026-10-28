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
        "lib/RemoteLogic/src/RemoteSounds.cpp",
        "lib/RemoteUi/src/RemoteScreens.cpp",
        "lib/RemoteUi/src/RemoteTransitions.cpp",
        "lib/RemoteUi/src/RemoteSplash.cpp",
        "tools/screens/render_screens.cpp",
    ]
    includes = ["tools/screens/host", "lib/Adafruit-GFX", "lib/RemoteLogic/src", "lib/RemoteUi/src", "lib/RemoteAudio/src", "lib/RemoteGames/src"]
    command = ["clang++", "-std=c++17", "-O1", "-w", "-DARDUINO=100", "-o", str(binary)]
    command += [f"-I{ROOT / i}" for i in includes] + [str(ROOT / s) for s in sources]
    subprocess.run(command, check=True)
    subprocess.run([str(binary), str(OUT)], check=True)


def read_rows(pbm):
    lines = pbm.read_text().split("\n")
    return [[int(v) for v in line.split()] for line in lines[2:] if line.strip()]


# Rows that must stay empty, so nothing touches (docs: 1 px gap between elements). By file name prefix:
# 0/1 status screen, 2 page, 3 menu, 4 calibration, 5 the Motion page live, 6 the Motion page's text lines, 7 the
# Motors page, 8 the Tracking page (dark rings, check_tracking_margins; its compass sits on its own dark disc top
# right). x 127 is the firmware alive dot.
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
    elif kind in "56":
        # the Motion page [D-161, D-163]: the title line; row 8 the countdown bar (it notches over the tops in row 9);
        # two columns (velocity x 0..61, pose x 66..126) of three text lines from row 9, or the text lines without
        # motion data and while editing the countdown
        gaps = [(7, 0, 126, 6, 9), (16, 0, 126, 15, 18), (24, 0, 126, 23, 26)]
    elif kind == "7":
        # the Motors page [D-163]: front wheels and their two text lines in rows 0..15, rear in rows 17..31 (row 16
        # free but for the alive dot in the centre, rows 14..16); the text lines apart (rows 7 and 24) beside the
        # wheels, whose bodies run through
        gaps = [(16, 0, 127, 15, 17)]
        for x0, x1 in [(0, 48), (78, 126)]:
            gaps += [(7, x0, x1, 6, 8), (24, x0, x1, 23, 25)]
    if name.startswith("20"):
        # the Link page: the title line; the history graph (x 0..59, rows 9..31) runs through the rows, the numbers'
        # column (x 64..126) has its three lines from rows 9, 17 and 25
        gaps = [(7, 0, 126, 6, 9), (16, 64, 126, 15, 17), (24, 64, 126, 23, 25)]
    elif kind == "9":
        # the Tracking page's views (owner, 2026-10-28): 9l the mark list (pill lines rows 0..8, 11..19, 22..30),
        # 9t the trip (text rows 0, 9, 18), 9e the editor (title rows 0..8, field pills 10..18 and 21..29)
        if name.startswith("9l"):
            gaps = [(9, 0, 127, 8, 11), (20, 0, 127, 19, 22)]
        elif name.startswith("9t"):
            gaps = [(8, 0, 127, 7, 9), (17, 0, 127, 16, 18)]
        elif name.startswith("9e"):
            gaps = [(9, 0, 127, 8, 10), (20, 0, 127, 19, 21)]
    if name.startswith("24e"):
        # the games' loading screen: title pill rows 0..8, blocks 11..19, text from row 24
        gaps = [(10, 0, 126, 9, 11), (22, 0, 126, 21, 24)]
    problems = []
    if kind == "8":
        problems += check_tracking_margins(rows, name.startswith("8r"))
    for y, x0, x1, above, below in gaps:
        for x in range(x0, x1 + 1):
            label = labels[y][x]
            if label and spans[label][0] <= above and spans[label][1] >= below:
                problems.append(f"row {y}: a shape joins the rows above and below at x {x}")
                break
    return problems


def ring_lit(rows, x0, y0, x1, y1):
    """The lit pixels on the 1 px ring round the box x0..x1, y0..y1 (inside the screen)."""
    lit = []
    for y in range(y0 - 1, y1 + 2):
        for x in range(x0 - 1, x1 + 2):
            inside = x0 <= x <= x1 and y0 <= y <= y1
            if not inside and 0 <= x < 128 and 0 <= y < 32 and rows[y][x]:
                lit.append((x, y))
    return lit


def run_bottom_boxes(rows):
    """The run's bottom row (8r): the time left's glyphs (shapes of two or more pixels within rows 24..30 from x 0, the
    digits and their dots within 2 px of each other) and a filled pill (a shape spanning rows 23..31 with at least 30
    pixels: the pace word or the split), or plain words in its place (PAUSED, a blinking pill's off phase: shapes
    within rows 24..30 further right)."""
    labels, spans = components(rows)
    sizes = {}
    for y in range(32):
        for x in range(128):
            if labels[y][x]:
                sizes[labels[y][x]] = sizes.get(labels[y][x], 0) + 1
    glyphs = {}
    for y in range(23, 32):
        for x in range(0, 128):     # the full width: the turn pill reaches the right edge (the drive arrow is its own shape)
            l = labels[y][x]
            if not l:
                continue
            top, bottom = spans[l]
            if (top >= 24 and bottom <= 30 and sizes[l] >= 2) or (top == 23 and bottom == 31 and sizes[l] >= 30):
                glyphs.setdefault(l, []).append(x)
    # group the shapes into runs of columns no more than 2 px apart
    columns = sorted({x for xs in glyphs.values() for x in xs})
    boxes = []
    start = None
    for i, x in enumerate(columns):
        if start is None:
            start = x
        if i + 1 == len(columns) or columns[i + 1] - x > 3:
            boxes.append(("run bottom text", start, 24 if not any(rows[23][c] for c in range(start, x + 1)) else 23, x,
                          30 if not any(rows[31][c] for c in range(start, x + 1)) else 31))
            start = None
    return boxes


def check_tracking_margins(rows, b_run=False):
    """The Tracking page: the grid, the trail and the origin must not touch the rover (x 62..66, rows 13..19), the
    alive dot (x 0..2, rows 0..2), the scale label (bottom left: the bar
    from x 0 along row 30, then "1m" / "5m" 3 px after its end) or the top line's text: a dark ring of 1 px round
    each."""
    problems = []
    boxes = [("rover", 62, 13, 66, 19), ("alive dot", 0, 0, 2, 2)]
    if rows[30][0] and rows[28][0]:
        length = 0
        while length + 1 < 128 and rows[30][length + 1]:
            length += 1
        boxes.append(("scale label", 0, 24, length + 3 + 11 - 1, 31))
    else:
        # no map: the top line's text, if any (x 4..114: clear of the alive dot and the compass). A timed run or its
        # arrival (8r) has no scale label, no compass and no top line (owner, 2026-10-28): its bottom row is checked
        # instead, the time left (plain digits from x 0) and the pace, PAUSED or split pill; each on a dark 1 px
        # ring. A refused play (8m_refused_*) is a pill in the compass's place, checked like a question
        if b_run:
            boxes += run_bottom_boxes(rows)
        else:
            columns = [x for x in range(4, 115) if any(rows[y][x] for y in range(0, 8))]
            if columns:
                boxes.append(("text line", columns[0], 0, columns[-1], 7))
    for label, x0, y0, x1, y1 in boxes:
        lit = ring_lit(rows, x0, y0, x1, y1)
        if lit:
            problems.append(f"{label}: something touches it at {lit[0]}")
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


def transitions_sheet(strips):
    """Per transition idea an animated GIF at real speed (a frame per 20 ms, pauses between), and one sheet with the
    forward frames of every idea, one row each."""
    if not strips:
        return
    scale = 3
    width, height, gap, label = 128 * scale, 32 * scale, 8, 18
    for idea, pictures in strips.items():
        forward = [p for key, p in sorted(pictures) if key[0] == "f"]
        back = [p for key, p in sorted(pictures) if key[0] == "b"]
        sequence = forward + back
        durations = [700] + [20] * (len(forward) - 2) + [700] + [700] + [20] * (len(back) - 2) + [700]
        sequence[0].save(OUT / f"transition_{idea}.gif", save_all=True, append_images=sequence[1:],
                         duration=durations, loop=0)
    frames = max(len([k for k, _ in p if k[0] == "f"]) for p in strips.values())
    sheet = Image.new("RGB", (frames * (width + gap) + gap, len(strips) * (height + label + gap) + gap), (40, 40, 40))
    draw = ImageDraw.Draw(sheet)
    font = ImageFont.load_default()
    for row, (idea, pictures) in enumerate(sorted(strips.items())):
        y = gap + row * (height + label + gap)
        draw.text((gap, y), idea.replace("_", " "), fill=(220, 220, 220), font=font)
        for column, picture in enumerate(p for key, p in sorted(pictures) if key[0] == "f"):
            x = gap + column * (width + gap)
            sheet.paste(picture.resize((width, height), Image.NEAREST), (x, y + label))
    sheet.save(OUT / "transitions.png")
    for png in OUT.glob("t_*.png"):
        png.unlink()


def main():
    build()
    pictures = []
    failures = 0
    strips = {}
    animations = {}
    for pbm in sorted(OUT.glob("*.pbm")):
        if pbm.stem.startswith("g_"):
            name, frame = pbm.stem[2:].rsplit("_", 1)
            animations.setdefault(name, []).append((int(frame), to_png(pbm)))
            continue
        if pbm.stem.startswith("t_"):
            idea, frame = pbm.stem[2:].rsplit("_", 1)
            strips.setdefault(idea, []).append(((frame[0], int(frame[1:])), to_png(pbm)))
            continue
        for problem in check_gaps(pbm.stem, read_rows(pbm)):
            print(f"GAP {pbm.stem}: {problem}")
            failures += 1
        pictures.append((pbm.stem, to_png(pbm)))
    transitions_sheet(strips)
    for name, frames in animations.items():
        gif_frames = [p for _, p in sorted(frames)]
        gif_frames[0].save(OUT / f"{name}.gif", save_all=True, append_images=gif_frames[1:], duration=20, loop=0)
        for png in OUT.glob(f"g_{name}_*.png"):
            png.unlink()
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
