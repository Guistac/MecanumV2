#!/usr/bin/env python3
"""Takes a short recording apart into its partials for the synth [D-158]: for each pitch given, the measured pitch,
when it starts (10 % of its peak), when it peaks, its peak level (as a sine's amplitude), its decay time constant
after the peak (a log-linear fit down to 5 %) and when it has faded. The result is a score (SynthScoreNote: start,
attack = peak - start, seconds = end - start, decay), as in lib/RemoteUi/src/StartupScore.h.

    python3 tools/sounds/partials.py tools/sounds/startup.m4a 468 705 883 1050 1324 1572 2089 4177

Find the pitches first from a spectrogram (the strongest peaks over time). Needs numpy and afconvert (macOS).
"""
import pathlib
import subprocess
import sys
import tempfile
import wave

import numpy as np


def load(path):
    with tempfile.TemporaryDirectory() as folder:
        source = pathlib.Path(folder) / "source.wav"
        subprocess.run(["afconvert", "-f", "WAVE", "-d", "LEI16", "-c", "1", str(path), str(source)], check=True)
        w = wave.open(str(source))
        return np.frombuffer(w.readframes(w.getnframes()), dtype=np.int16).astype(float) / 32768.0, w.getframerate()


if __name__ == "__main__":
    x, rate = load(sys.argv[1])
    size, hop = 8192, 256
    window = np.hanning(size)
    spectra = np.array([np.abs(np.fft.rfft(x[i:i + size] * window)) for i in range(0, len(x) - size, hop)])
    f = np.fft.rfftfreq(size, 1 / rate)
    t = np.arange(len(spectra)) * hop / rate + size / 2 / rate
    print("start   Hz      level  attack seconds decay")
    for target in map(float, sys.argv[2:]):
        band = (f > target * 0.985) & (f < target * 1.015)
        envelope = spectra[:, band].max(axis=1) / (window.sum() / 2)
        pitch = f[band][np.argmax(spectra[:, band].sum(axis=0))]
        k = int(np.argmax(envelope))
        peak = envelope[k]
        start = t[np.nonzero(envelope > 0.1 * peak)[0][0]]
        after = np.arange(k, len(envelope))
        after = after[envelope[after] > 0.05 * peak]
        decay = -1 / np.polyfit(t[after], np.log(envelope[after]), 1)[0]
        end = t[after[-1]]
        print(f"{start:.3f}  {pitch:7.1f}  {peak:.4f}  {t[k] - start:.3f}  {end - start:.3f}  {decay:.3f}")
