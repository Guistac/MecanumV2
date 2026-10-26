"""Save and load scope captures as raw data.

A capture is a folder with one <signal>.bin per channel (the bytes exactly as the scope sent them)
and capture.json (preambles = the scope's scaling, settings, host times, anything the caller adds).
Analysis only ever reads these folders, never the scope.
"""

import json
import os
import sys
import time

SKILL = os.path.expanduser("~/.claude/skills/bench-instruments")
if SKILL not in sys.path:
    sys.path.insert(0, SKILL)

from rigol import preamble_time, to_volts  # noqa: E402


def scope_settings(scope, channels):
    settings = {
        "idn": scope.idn(),
        "timebase_s_div": float(scope.query(":TIM:MAIN:SCAL?")),
        "timebase_offset_s": float(scope.query(":TIM:MAIN:OFFS?")),
        "memory_depth": float(scope.query(":ACQ:MDEP?")),
        "sample_rate": float(scope.query(":ACQ:SRAT?")),
        "trigger": {
            "source": scope.query(":TRIG:EDGE:SOUR?"),
            "slope": scope.query(":TRIG:EDGE:SLOP?"),
            "level_v": float(scope.query(":TRIG:EDGE:LEV?")),
            "sweep": scope.query(":TRIG:SWE?"),
        },
        "channels": {},
    }
    for name, ch in channels.items():
        settings["channels"][name] = {
            "ch": ch,
            "scale_v_div": float(scope.query(":CHAN%d:SCAL?" % ch)),
            "offset_v": float(scope.query(":CHAN%d:OFFS?" % ch)),
            "probe": float(scope.query(":CHAN%d:PROB?" % ch)),
        }
    return settings


def save_capture(scope, folder, channels, meta=None):
    """channels = {"VCC": 1, ...}. The scope must be stopped on the capture to save."""
    os.makedirs(folder, exist_ok=True)
    record = {"saved_at": time.time(), "settings": scope_settings(scope, channels), "preambles": {}}
    for name, ch in channels.items():
        preamble, data = scope.waveform_bytes(ch, raw=True)
        with open(os.path.join(folder, name + ".bin"), "wb") as f:
            f.write(data)
        record["preambles"][name] = preamble
        record["settings"]["channels"][name]["volts_per_step"] = float(preamble[7])
    record.update(meta or {})
    with open(os.path.join(folder, "capture.json"), "w") as f:
        json.dump(record, f, indent=1)
    return record


def load_capture(folder):
    """-> (record, {name: (t_first_s, dt_s, volts)}). record["clipped"][name] counts the points at
    the ends of the 8-bit range (code 0 or 255): those are outside the channel's window, the real
    value is unknown (wrong offset or V/div, or a probe factor that does not match the probe switch)."""
    with open(os.path.join(folder, "capture.json")) as f:
        record = json.load(f)
    signals, record["clipped"] = {}, {}
    for name, preamble in record["preambles"].items():
        with open(os.path.join(folder, name + ".bin"), "rb") as f:
            data = f.read()
        t0, dt = preamble_time(preamble)
        signals[name] = (t0, dt, to_volts(preamble, data))
        record["clipped"][name] = data.count(0) + data.count(255)
    return record, signals
