#!/usr/bin/env python3
"""Renders the remote's sounds to WAV files on the Mac [D-158]: builds tools/sounds/render_cues.cpp with the real
sound engine (lib/RemoteAudio), the remote's cues (lib/RemoteLogic), the arcade and the games (lib/RemoteGames), and
runs it. Listen before flashing; check lengths and loops.

    python3 tools/sounds/render_cues.py list
    python3 tools/sounds/render_cues.py cue <name|all> [seconds]
    python3 tools/sounds/render_cues.py music <game|all> [seconds]
    python3 tools/sounds/render_cues.py game <game|all> <seconds> [seed] [mode 0..3]

`cue`: one of the named cues (remote_*, arcade_*, kit_*; `list` names them). `music`: a game's playMusic() alone.
`game`: the game played by its demo player with its sound on (the picker's demo is silent; this bypasses it), the
arcade's game over phrase, then the next game; mode 0 SOUND ON, 1 MUSIC OFF, 2 FX OFF, 3 MUTE.
Each render prints its length, peak and disc loudness: the RMS in dB after a model of the remote's piezo disc
(900 Hz high-pass, 6 kHz low-pass) over the 50 ms windows where something sounds, and its loudest window. A game's
music should come within about 3 to 4 dB of its effects (docs/arcade/README.md section 3).
Output: tools/screens/out/sounds (not in git), or OUT=<folder> in the environment.
"""
import os
import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
OUT = pathlib.Path(os.environ.get("OUT", ROOT / "tools/screens/out/sounds"))


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 2
    OUT.mkdir(parents=True, exist_ok=True)
    binary = OUT / "render_cues"
    sources = ["lib/Adafruit-GFX/Adafruit_GFX.cpp", "tools/sounds/render_cues.cpp",
               "lib/RemoteLogic/src/RemoteSounds.cpp"]
    sources += sorted(str(p.relative_to(ROOT)) for p in (ROOT / "lib/RemoteGames/src").glob("*.cpp"))
    includes = ["tools/screens/host", "lib/Adafruit-GFX", "lib/RemoteGames/src", "lib/RemoteAudio/src",
                "lib/RemoteLogic/src"]
    command = ["clang++", "-std=c++17", "-O2", "-w", "-DARDUINO=100", "-o", str(binary)]
    command += [f"-I{ROOT / i}" for i in includes] + [str(ROOT / s) for s in sources]
    subprocess.run(command, check=True)
    return subprocess.run([str(binary), str(OUT)] + sys.argv[1:]).returncode


if __name__ == "__main__":
    sys.exit(main())
