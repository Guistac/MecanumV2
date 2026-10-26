#!/usr/bin/env python3
"""T6c - init under heat (docs/radio/05-bench-test-plan.md). Needs the remote_diag build.

The owner warms the RFM23BP module with hot air in steps while this script power cycles the board
(--off s off, then on) about every --period s and logs: boot result, failed attempts with what
the chip answered (diag init_fail lines), the radio's own temperature (diag temp lines, uncalibrated)
and the supply current. Runs for --minutes or until a failure; on a failure it keeps power cycling
every --period s (no heating, the module cools) and records when the radio starts again.
Powers the board off at the end.

Usage: python3 -I bench/t6c_heat.py [--minutes 15] [--period 12] [--off 1]
"""

import argparse
import os
import re
import sys
import time

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "lib"))
from rig import CHANNELS, Rig  # noqa: E402

TEMP_RE = re.compile(r"diag temp_raw=(-?\d+) temp_c=(-?[\d.]+)")
FAIL_DIAG_RE = re.compile(r"diag init_fail .*")


def last_temp(rig, since):
    temps = [float(m.group(2)) for t, s in rig.serial.lines if t >= since for m in [TEMP_RE.search(s)] if m]
    return temps[-1] if temps else None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--minutes", type=float, default=15.0)
    ap.add_argument("--period", type=float, default=12.0)
    ap.add_argument("--off", type=float, default=1.0)
    args = ap.parse_args()
    rig = Rig("t6c")
    t_start = time.time()
    first_failure = None
    try:
        rig.ensure_on()
        rig.setup_scope()
        n = 0
        while time.time() - t_start < args.minutes * 60:
            t_cycle = time.time()
            t_off, t_on = rig.power_cycle(args.off, arm_init_capture=False)
            boot = rig.watch_boot(t_on, timeout_s=8.0)
            alive = rig.tx_alive(3.0) if boot["state"] in ("OK", "RETRY_OK") else None
            while time.time() - t_cycle < args.period - 1.0:       # stay on, collect temperature lines
                time.sleep(0.5)
            diag = [s for t, s in boot["serial"] if FAIL_DIAG_RE.search(s)]
            temp = last_temp(rig, t_on)
            amps = [a for _, _, a in boot["supply"]]
            row = {"n": n, "t_min": round((t_on - t_start) / 60, 2), "state": boot["state"],
                   "failed_attempts": boot["failed_attempts"], "pinging": alive, "radio_temp_c": temp,
                   "init_fail_diag": diag, "amps_last": amps[-1] if amps else None, "serial": boot["serial"],
                   "phase": "cooling" if first_failure is not None else "heating"}
            rig.log_row(row)
            print("#%03d t=%5.2f min  %-8s failed %d  pinging %s  radio temp %s C  A %.3f  %s%s"
                  % (n, row["t_min"], row["state"], row["failed_attempts"], alive, temp,
                     row["amps_last"] or 0, row["phase"], ("  " + diag[-1]) if diag else ""), flush=True)
            ok = row["state"] in ("OK", "RETRY_OK") and alive
            if not ok and first_failure is None:
                first_failure = t_on
                print(">>> FAILURE at radio temp %s C: stop heating now; cycling on while it cools" % temp, flush=True)
            elif ok and first_failure is not None:
                print(">>> radio back %.1f s after the first failure, radio temp %s C" % (t_on - first_failure, temp), flush=True)
                break
            n += 1
    finally:
        rig.close(power_off=True)
        print("saved", rig.run)


if __name__ == "__main__":
    main()
