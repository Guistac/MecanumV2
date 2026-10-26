#!/usr/bin/env python3
"""T9b - SDN during power-up with the startup_middle_hook (docs/radio/05-bench-test-plan.md).

Setup E (SDN on CH4, 10x). Per power cycle one capture triggered on the first SDN rising edge after
power-on, 20 ms/div, -0.02 .. +0.18 s. Without the hook SDN floated high for about 319 ms from the
start of the boot (T3); with it the first high should be the 10 ms init pulse of Radio::initialize.
Reports every SDN high interval in the window and the boot result.

Usage: python3 -I bench/t9_boot_sdn.py [--cycles 5]
"""

import argparse
import os
import sys
import time

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "lib"))
from capture import load_capture  # noqa: E402
from rig import Rig  # noqa: E402

CHANNELS_SPI = {"SCK": 1, "MISO": 2, "NIRQ": 3, "SDN": 4}


def high_intervals(signal, threshold_v=2.0):
    t0, dt, v = signal
    out, start = [], None
    for i, x in enumerate(v):
        if x >= threshold_v and start is None:
            start = i
        elif x < threshold_v and start is not None:
            out.append((t0 + start * dt, (i - start) * dt))
            start = None
    if start is not None:
        out.append((t0 + start * dt, None))       # still high at the end of the window
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--cycles", type=int, default=5)
    args = ap.parse_args()
    rig = Rig("t9b_boot_sdn", channels=CHANNELS_SPI)
    rig.firmware = "env:remote_diag (RadioHead_RFM23BP, startup_middle_hook drives SDN low)"
    try:
        rig.ensure_on(settle_s=4.0)
        rig.setup_scope()
        rig.scope.setup_timebase(0.02, 0.08)
        rig.scope.run()
        time.sleep(1.0)
        for n in range(args.cycles):
            t_off, t_on = rig.power_cycle(1.5, arm_init_capture=True)
            boot = rig.watch_boot(t_on, timeout_s=8.0)
            highs = None
            if rig.scope.query(":TRIG:STAT?") == "STOP":
                folder = rig.save("cycle%03d" % n, {"cycle": n, "t_on": t_on})
                _, sig = load_capture(folder)
                highs = [(round(s * 1e3, 2), round(l * 1e3, 2) if l is not None else "past window") for s, l in high_intervals(sig["SDN"])]
            row = {"n": n, "state": boot["state"], "failed_attempts": boot["failed_attempts"], "sdn_high_ms": highs}
            rig.log_row(row)
            print("#%d %s failed %d  SDN high (start ms, length ms): %s" % (n, boot["state"], boot["failed_attempts"], highs), flush=True)
            time.sleep(1.0)
    finally:
        rig.close(power_off=True)
        print("saved", rig.run)


if __name__ == "__main__":
    main()
