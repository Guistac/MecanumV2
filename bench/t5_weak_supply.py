#!/usr/bin/env python3
"""T5 - weak supply (docs/radio/05-bench-test-plan.md, diagnostic D5 and root cause 1).

Emulates a weak source on the 5 V rail: first the current limit is lowered at 5.0 V, then the
voltage at 1.0 A. At each level: one power cycle (init capture), the boot result, a capture of the
first transmission (VCC sag), then --watch seconds of ping checks (the remote pings at 2 Hz without
a robot). Stops at the first failure (init failed, pings stop = hang, or a reboot) and leaves the
supply at that level so the state can be measured (T6); otherwise restores 5.0 V / 1.0 A.

Usage: python3 -I bench/t5_weak_supply.py [--watch 20]
"""

import argparse
import os
import sys
import time

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "lib"))
from capture import load_capture  # noqa: E402
from rig import CHANNELS, JOY_RE, NOMINAL, SUPPLY_CH, Rig  # noqa: E402

LEVELS = [(5.0, 1.0), (5.0, 0.5), (5.0, 0.35), (5.0, 0.25), (5.0, 0.2),
          (4.5, 1.0), (4.2, 1.0), (4.0, 1.0), (3.8, 1.0), (3.6, 1.0)]


def set_level(rig, volt, curr):
    # lower first whichever goes down, so no intermediate state exceeds the target
    rig.psu.set_current(SUPPLY_CH, min(curr, NOMINAL["curr"]))
    rig.psu.set_voltage(SUPPLY_CH, volt)
    rig.psu.set_current(SUPPLY_CH, curr)
    rig.check(on=True)


def tx_capture_stats(folder):
    _, sig = load_capture(folder)
    t0, dt, tx = sig["TX_ON"]
    _, _, vcc = sig["VCC"]
    low = [i for i, v in enumerate(tx) if v < 1.65]
    if not low:
        return {}
    i0, i1 = low[0], low[-1]
    before = vcc[:max(1, i0 - int(1e-3 / dt))]
    during = vcc[i0:i1 + 1]
    return {"tx_ms": round((i1 - i0) * dt * 1e3, 3), "vcc_before_median": sorted(before)[len(before) // 2],
            "vcc_tx_min": min(during), "vcc_tx_median": sorted(during)[len(during) // 2]}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--watch", type=float, default=20.0)
    args = ap.parse_args()
    rig = Rig("t5")
    failed = False
    try:
        rig.check(on=True)
        for n, (volt, curr) in enumerate(LEVELS):
            set_level(rig, volt, curr)
            rig.setup_scope()
            t_off, t_on = rig.power_cycle(5.0)
            boot = rig.watch_boot(t_on)
            if rig.scope.query(":TRIG:STAT?") == "STOP":
                rig.save("level%02d_init" % n, {"level": n, "volt": volt, "curr": curr, "t_on": t_on})
            row = {"n": n, "volt": volt, "curr": curr, "boot": boot["state"], "failed_attempts": boot["failed_attempts"],
                   "supply_boot": boot["supply"], "t_on": t_on}
            if boot["state"] not in ("OK", "RETRY_OK"):
                row["result"] = "INIT " + boot["state"]
            else:
                # first transmission, 2 ms/div: about -4 .. +16 ms around TX_ON falling
                rig.scope.setup_timebase(0.002, 0.006)
                if rig.tx_alive(3.0):
                    folder = rig.save("level%02d_tx" % n, {"level": n, "volt": volt, "curr": curr})
                    row["tx"] = tx_capture_stats(folder)
                rig.scope.setup_timebase(0.02, 0.15)
                rig.scope.setup_edge_trigger(CHANNELS["VCC"], 2.0, "POS", "AUTO")
                rig.scope.run()
                time.sleep(0.5)
                row["cross_check"] = rig.cross_check()
                checks, ok, supply = 0, 0, []
                t_watch = time.time()
                while time.time() - t_watch < args.watch:
                    checks += 1
                    ok += bool(rig.tx_alive(2.5))
                    volts, amps = rig.psu.measure()
                    supply.append((round(time.time() - t_on, 3), volts[SUPPLY_CH - 1], amps[SUPPLY_CH - 1]))
                joy = [s for t, s in rig.serial.lines if t >= t_on and JOY_RE.match(s.strip())]
                row.update({"ping_checks": checks, "ping_ok": ok, "boot_prints": len(joy), "supply_watch": supply})
                reboots = max(0, len(joy) // 2 - 1)
                row["reboots"] = reboots
                row["result"] = "OK" if ok == checks and reboots == 0 else \
                    ("REBOOT" if reboots else "PINGS STOPPED (hang?)")
            row["serial"] = [(round(t - t_on, 3), s) for t, s in rig.serial.lines if t >= t_off]
            rig.log_row(row)
            amps = [a for _, _, a in row.get("supply_watch", row["supply_boot"])]
            print("#%d %.1f V %.2f A: boot %-8s (failed attempts %d)  %s  pings %s/%s  A max %.3f  tx %s"
                  % (n, volt, curr, row["boot"], row["failed_attempts"], row["result"], row.get("ping_ok"),
                     row.get("ping_checks"), max(amps), row.get("tx")), flush=True)
            if row["result"] != "OK":
                failed = True
                print("STOP: failure reproduced at %.1f V / %.2f A, supply left at this level" % (volt, curr), flush=True)
                break
    finally:
        if not failed:
            set_level(rig, NOMINAL["volt"], NOMINAL["curr"])
            print("supply restored to %.1f V / %.1f A" % (NOMINAL["volt"], NOMINAL["curr"]))
        rig.close()
        print("saved", rig.run)


if __name__ == "__main__":
    main()
