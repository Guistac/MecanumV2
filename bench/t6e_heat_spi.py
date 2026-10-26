#!/usr/bin/env python3
"""T6e - init under heat with the SPI traffic and nIRQ on the scope (docs/radio/05-bench-test-plan.md).

Setup E: CH1 SCK, CH2 MISO, CH3 nIRQ, CH4 SDN (5 V side, 10x). Firmware env:remote_diag unchanged.
Per boot one capture of the first init attempt, triggered on SDN falling (end of the init pulse,
armed --arm-delay s after power-on, past the boot float), positioned on RHSPIDriver::init()'s end:
software reset #1, device type read, software reset #2 and the chip ready polls. Decoded from the
raw data: every SPI frame's MISO bytes, and when nIRQ is low. Answers whether the chip never reports
ready, or reports it and the reads lose it.

Usage: python3 -I bench/t6e_heat_spi.py [--minutes 20] [--period 15] [--off 1] [--window-start 0.105]
"""

import argparse
import os
import re
import sys
import time

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "lib"))
from capture import load_capture  # noqa: E402
from rig import Rig  # noqa: E402
from spi_decode import summarize  # noqa: E402

CHANNELS_SPI = {"SCK": 1, "MISO": 2, "NIRQ": 3, "SDN": 4}
TEMP_RE = re.compile(r"diag temp_raw=(-?\d+) temp_c=(-?[\d.]+)")
DIAG_RE = re.compile(r"diag (init_ok|init_fail|swres_chip_ready).*")


def setup_capture(rig, scale_s, offset_s):
    rig.scope.setup_timebase(scale_s, offset_s)
    rig.scope.setup_edge_trigger(CHANNELS_SPI["SDN"], 2.5, "NEG", "NORM")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--minutes", type=float, default=20.0)
    ap.add_argument("--period", type=float, default=15.0)
    ap.add_argument("--off", type=float, default=1.0)
    ap.add_argument("--arm-delay", type=float, default=0.8)
    ap.add_argument("--scale", type=float, default=0.002, help="s/div")
    ap.add_argument("--offset", type=float, default=0.125, help="timebase offset (window centre after SDN falling)")
    args = ap.parse_args()
    rig = Rig("t6e", channels=CHANNELS_SPI)
    rig.firmware = "env:remote_diag (radio-bench, extended diag), unchanged since T6d"
    t_start = time.time()
    first_failure = None
    try:
        rig.ensure_on()
        rig.setup_scope()
        setup_capture(rig, args.scale, args.offset)
        rig.scope.run()
        time.sleep(1.0)
        n = 0
        while time.time() - t_start < args.minutes * 60:
            t_cycle = time.time()
            t_off, t_on = rig.power_cycle(args.off, arm_init_capture=False)
            time.sleep(max(0.0, args.arm_delay - (time.time() - t_on)))
            rig.scope.single()
            boot = rig.watch_boot(t_on, timeout_s=8.0)
            summary = None
            if rig.scope.query(":TRIG:STAT?") == "STOP":
                folder = rig.save("cycle%03d_init" % n, {"cycle": n, "t_on": t_on})
                rec, sig = load_capture(folder)
                summary = summarize(sig)
                summary["window_ms"] = (round(sig["SCK"][0] * 1e3, 3), round((sig["SCK"][0] + len(sig["SCK"][2]) * sig["SCK"][1]) * 1e3, 3))
                summary["dt_ns"] = round(sig["SCK"][1] * 1e9, 1)
                summary["clipped"] = {k: v for k, v in rec["clipped"].items() if v}
            while time.time() - t_cycle < args.period - 1.0:
                time.sleep(0.5)
            temps = [float(m.group(2)) for t, s in rig.serial.lines if t >= t_on for m in [TEMP_RE.search(s)] if m]
            ok = boot["state"] in ("OK", "RETRY_OK")
            row = {"n": n, "t_min": round((t_on - t_start) / 60, 2), "state": boot["state"],
                   "failed_attempts": boot["failed_attempts"], "radio_temp_c": temps[-1] if temps else None,
                   "diag": [s for t, s in rig.serial.lines if t >= t_on and DIAG_RE.search(s)],
                   "spi": summary, "serial": boot["serial"], "phase": "cooling" if first_failure is not None else "heating"}
            rig.log_row(row)
            if summary:
                spi = "frames %d edges %s | dev %s | polls %s | ready in MISO at %s ms | nIRQ low %d %s" % (
                    summary["frames"], summary["frame_edges"], summary["device_type_byte"], summary["poll_values_after_frame3"],
                    summary["first_ready_in_miso_ms"], summary["nirq_low_count"], summary["nirq_low"][:3])
            else:
                spi = "no capture"
            print("#%03d t=%5.2f min  %-8s failed %d  temp %s C  %s" % (n, row["t_min"], row["state"], row["failed_attempts"],
                  row["radio_temp_c"], spi), flush=True)
            if not ok and first_failure is None:
                first_failure = t_on
                print(">>> FAILURE at radio temp %s C: stop heating; cycling on while it cools" % row["radio_temp_c"], flush=True)
            elif ok and first_failure is not None:
                print(">>> radio back %.1f s after the first failure, temp %s C" % (t_on - first_failure, row["radio_temp_c"]), flush=True)
                break
            setup_capture(rig, args.scale, args.offset)
            n += 1
    finally:
        rig.close(power_off=True)
        print("saved", rig.run)


if __name__ == "__main__":
    main()
