#!/usr/bin/env python3
"""Records the remote's games on the Mac: builds tools/screens/render_games.cpp with lib/RemoteGames and
Adafruit_GFX, lets each game play itself (its demo player) and writes a GIF at real speed that looks like the OLED,
plus a sheet of every 10th frame.

    python3 tools/screens/render_games.py [game name|all|arcade] [seconds] [seed] [output folder]

Defaults: all, 20 s, seed 12345, tools/screens/out/games (not in git).
"""
import pathlib
import subprocess
import sys

from PIL import Image

ROOT = pathlib.Path(__file__).resolve().parents[2]
WHICH = sys.argv[1] if len(sys.argv) > 1 else "all"
SECONDS = sys.argv[2] if len(sys.argv) > 2 else "20"
SEED = sys.argv[3] if len(sys.argv) > 3 else "12345"
OUT = pathlib.Path(sys.argv[4]) if len(sys.argv) > 4 else ROOT / "tools/screens/out/games"
SCALE = 4
LIT = (120, 200, 255)
DARK = (14, 18, 26)


def build():
    OUT.mkdir(parents=True, exist_ok=True)
    binary = OUT / "render_games"
    sources = ["lib/Adafruit-GFX/Adafruit_GFX.cpp", "tools/screens/render_games.cpp"]
    sources += sorted(str(p.relative_to(ROOT)) for p in (ROOT / "lib/RemoteGames/src").glob("*.cpp"))
    includes = ["tools/screens/host", "lib/Adafruit-GFX", "lib/RemoteGames/src"]
    command = ["clang++", "-std=c++17", "-O1", "-w", "-DARDUINO=100", "-o", str(binary)]
    command += [f"-I{ROOT / i}" for i in includes] + [str(ROOT / s) for s in sources]
    subprocess.run(command, check=True)
    subprocess.run([str(binary), str(OUT), WHICH, SECONDS, SEED], check=True)


def frame_image(data):
    image = Image.new("RGB", (128 * SCALE, 32 * SCALE), DARK)
    pixels = image.load()
    for y in range(32):
        for x in range(128):
            byte = data[y * 16 + x // 8]
            if byte & (0x80 >> (x % 8)):
                for dy in range(SCALE - 1):
                    for dx in range(SCALE - 1):
                        pixels[x * SCALE + dx, y * SCALE + dy] = LIT
    return image


def main():
    build()
    for frames_file in sorted(OUT.glob("*.frames")):
        raw = frames_file.read_bytes()
        frames = [frame_image(raw[i:i + 512]) for i in range(0, len(raw), 512)]
        frames_file.unlink()
        if not frames:
            continue
        gif = frames_file.with_suffix(".gif")
        frames[0].save(gif, save_all=True, append_images=frames[1:], duration=20, loop=0, optimize=False)
        picked = frames[::10][:60]
        columns = 4
        width, height, gap = 128 * 2, 32 * 2, 4
        rows = (len(picked) + columns - 1) // columns
        sheet = Image.new("RGB", (columns * (width + gap) + gap, rows * (height + gap) + gap), (60, 60, 60))
        for i, picture in enumerate(picked):
            sheet.paste(picture.resize((width, height), Image.NEAREST),
                        (gap + (i % columns) * (width + gap), gap + (i // columns) * (height + gap)))
        sheet.save(frames_file.with_name(frames_file.stem + "_sheet.png"))
        print(f"{gif} ({len(frames)} frames), sheet {frames_file.stem}_sheet.png")


if __name__ == "__main__":
    main()
