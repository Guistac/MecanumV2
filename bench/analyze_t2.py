#!/usr/bin/env python3
"""Analyse a T2 run folder (bench/data/<date>/t2_<time>/) from saved data only.

Usage: python3 -I bench/analyze_t2.py <run folder>
"""

import glob
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "lib"))

from capture import load_capture  # noqa: E402
from states import first_crossing, merge_glitches, timeline  # noqa: E402

GLITCH_S = 20e-6   # shorter state changes are treated as edge noise


def main():
    run = sys.argv[1]
    with open(os.path.join(run, "t2.json")) as f:
        t2 = json.load(f)
    print("firmware:", t2["firmware"])
    for cyc in t2["cycles"]:
        amps = [a for _, _, a in cyc["supply"]]
        print("\n== cycle %d (%s), off %.1f s, scope stopped after %s s" % (cyc["n"], cyc["kind"], cyc["off_s"], cyc["scope_stopped_after_s"]))
        print("   supply current: max %.3f A, last %.3f A, samples %s" % (max(amps), amps[-1], [round(a, 3) for a in amps]))
        t_on = cyc["t_on"]
        for t, s in cyc["serial"]:
            print("   serial %+7.3f s  %s" % (t - t_on, s))
        folders = glob.glob(os.path.join(run, "cycle%02d_*" % cyc["n"]))
        if not folders:
            continue
        record, signals = load_capture(folders[0])
        t0, dt, vcc = signals["VCC"]
        steps = {k: round(c["volts_per_step"], 4) for k, c in record["settings"]["channels"].items()}
        print("   capture: %d points, %.0f ns per point, window %.3f .. %.3f s, volts per step %s"
              % (len(vcc), dt * 1e9, t0, t0 + len(vcc) * dt, steps))
        clipped = {k: n for k, n in record["clipped"].items() if n}
        if clipped:
            print("   WARNING clipped points (levels invalid there): %s" % clipped)
        for level in (0.5, 4.5):
            t = first_crossing(signals["VCC"], level, rising=True)
            print("   VCC first above %.1f V at %s" % (level, "%.4f s" % t if t is not None else "-"))
        segs = merge_glitches(timeline(signals), GLITCH_S)
        totals = {}
        for state, start, length in segs:
            totals[state] = totals.get(state, 0.0) + length
        print("   time per state: " + ", ".join("%s %.2f ms" % (k, v * 1e3) for k, v in sorted(totals.items())))
        print("   timeline (state, start ms, length ms):")
        for state, start, length in segs[:40]:
            print("     %-9s %9.3f %9.3f" % (state, start * 1e3, length * 1e3))
        if len(segs) > 40:
            print("     ... %d more segments" % (len(segs) - 40))


if __name__ == "__main__":
    main()
