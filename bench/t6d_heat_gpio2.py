#!/usr/bin/env python3
"""T6d - init under heat with the crystal clock on the scope (docs/radio/05-bench-test-plan.md).

Like T6c, plus: CH4 on the RFM23BP GPIO_2 (R0 pad 8), which outputs the crystal-derived
microcontroller clock (1 MHz by default) after every reset; one capture per boot of the first init
attempt (triggered on TX_ON falling at the start of the SDN pulse, armed --arm-delay s after power-on,
past the boot float), and the extended diag lines of env:remote_diag (chip ready times after the
SDN pulse and after an own software reset, crystal registers, temperature on a -64..192 C range).

Usage: python3 -I bench/t6d_heat_gpio2.py [--minutes 20] [--period 12] [--off 1]
"""

import argparse
import os
import re
import sys
import time

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "lib"))
from capture import load_capture  # noqa: E402
from rig import CHANNELS_GPIO2, Rig  # noqa: E402

TEMP_RE = re.compile(r"diag temp_raw=(-?\d+) temp_c=(-?[\d.]+)")
OK_RE = re.compile(r"diag init_ok .*")
FAIL_RE = re.compile(r"diag (init_fail|swres_chip_ready).*")
BIN_S = 0.5e-3
ACTIVE_V = 1.0     # peak-to-peak within a bin: the 1 MHz clock is sampled at 2.5 MSa/s, so a toggling
                   # output spans most of 0..3.3 V in every bin, a stopped one stays flat


def gpio2_activity(signal):
    t0, dt, v = signal
    per_bin = max(1, int(BIN_S / dt))
    bins = []
    for i in range(0, len(v) - per_bin, per_bin):
        chunk = v[i:i + per_bin]
        bins.append((t0 + i * dt, max(chunk) - min(chunk) > ACTIVE_V))
    active = [t for t, a in bins if a]
    # runs of activity, as (start ms, length ms)
    runs, start, prev = [], None, None
    for t, a in bins:
        if a and start is None:
            start = t
        if not a and start is not None:
            runs.append((round(start * 1e3, 1), round((t - start) * 1e3, 1)))
            start = None
    if start is not None:
        runs.append((round(start * 1e3, 1), round((bins[-1][0] + BIN_S - start) * 1e3, 1)))
    return {"active_fraction": round(len(active) / len(bins), 3) if bins else None, "runs_ms": runs[:12]}


def lines(rig, since, regex):
    return [s for t, s in rig.serial.lines if t >= since and regex.search(s)]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--minutes", type=float, default=20.0)
    ap.add_argument("--period", type=float, default=12.0)
    ap.add_argument("--off", type=float, default=1.0)
    ap.add_argument("--arm-delay", type=float, default=0.8)
    args = ap.parse_args()
    rig = Rig("t6d", channels=CHANNELS_GPIO2)
    rig.firmware = "env:remote_diag (radio-bench, extended diag: chip ready times, xtal registers, temp -64..192 C)"
    t_start = time.time()
    first_failure = None
    try:
        rig.ensure_on()
        rig.setup_scope()
        rig.scope.setup_edge_trigger(CHANNELS_GPIO2["TX_ON"], 1.65, "NEG", "NORM")
        n = 0
        while time.time() - t_start < args.minutes * 60:
            t_cycle = time.time()
            t_off, t_on = rig.power_cycle(args.off, arm_init_capture=False)
            time.sleep(max(0.0, args.arm_delay - (time.time() - t_on)))
            rig.scope.single()
            boot = rig.watch_boot(t_on, timeout_s=8.0)
            captured = rig.scope.query(":TRIG:STAT?") == "STOP"
            act = None
            if captured:
                folder = rig.save("cycle%03d_init" % n, {"cycle": n, "t_on": t_on})
                _, sig = load_capture(folder)
                act = gpio2_activity(sig["GPIO2"])
            ok_now = boot["state"] in ("OK", "RETRY_OK")
            if ok_now:
                rig.scope.setup_timebase(0.02, 0.15)
            alive = rig.tx_alive(3.0) if ok_now else None
            rig.scope.setup_edge_trigger(CHANNELS_GPIO2["TX_ON"], 1.65, "NEG", "NORM")
            while time.time() - t_cycle < args.period - 1.0:
                time.sleep(0.5)
            temps = [float(m.group(2)) for s in lines(rig, t_on, TEMP_RE) for m in [TEMP_RE.search(s)]]
            row = {"n": n, "t_min": round((t_on - t_start) / 60, 2), "state": boot["state"],
                   "failed_attempts": boot["failed_attempts"], "pinging": alive, "radio_temp_c": temps[-1] if temps else None,
                   "init_ok": lines(rig, t_on, OK_RE), "init_fail": lines(rig, t_on, FAIL_RE), "gpio2": act,
                   "captured": captured, "serial": boot["serial"],
                   "phase": "cooling" if first_failure is not None else "heating"}
            rig.log_row(row)
            print("#%03d t=%5.2f min  %-8s failed %d  pinging %s  temp %s C  gpio2 %s  %s"
                  % (n, row["t_min"], row["state"], row["failed_attempts"], alive, row["radio_temp_c"],
                     (act["active_fraction"], act["runs_ms"][:3]) if act else "-",
                     (row["init_ok"] or row["init_fail"] or ["-"])[-1]), flush=True)
            if not (ok_now and alive) and first_failure is None:
                first_failure = t_on
                print(">>> FAILURE at radio temp %s C: stop heating; cycling on while it cools" % row["radio_temp_c"], flush=True)
            elif ok_now and alive and first_failure is not None:
                print(">>> radio back %.1f s after the first failure, temp %s C" % (t_on - first_failure, row["radio_temp_c"]), flush=True)
                break
            n += 1
    finally:
        rig.close(power_off=True)
        print("saved", rig.run)


if __name__ == "__main__":
    main()
