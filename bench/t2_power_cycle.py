#!/usr/bin/env python3
"""T2 - power cycles of the remote board, watched closely (docs/radio/05-bench-test-plan.md).

Setup A: scope CH1 VCC, CH2 TX_ON, CH3 RX_ON, CH4 SDN (10x probes); supply CH3 on the 5 V rail.
Each cycle: CH3 off for --off seconds, then on, and one scope capture of either
  powerup: trigger VCC rising (what the control lines do before the firmware takes over), or
  init:    trigger SDN rising, armed --arm-delay s after power-on (the radio init pulse).
Cycles alternate powerup / init. Supply current and the USB serial are logged throughout.
Saves everything under bench/data/<date>/t2_<time>/. Analyse with analyze_t2.py.

Usage: python3 -I bench/t2_power_cycle.py [--cycles 4] [--off 5] [--on 6] [--arm-delay 0.8]
"""

import argparse
import json
import os
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "lib"))

from capture import SKILL, save_capture  # noqa: E402
from serial_log import SerialLogger  # noqa: E402

sys.path.insert(0, SKILL)
from owon import ODP3063  # noqa: E402
from rigol import DHO914S  # noqa: E402

CHANNELS = {"VCC": 1, "TX_ON": 2, "RX_ON": 3, "SDN": 4}
SUPPLY_CH = 3
CAPS = {SUPPLY_CH: (5.5, 2.0)}          # 5 V rail: never above 5.5 V / 2 A
FIRMWARE = "remote, original firmware as flashed (never hardened, commit unknown)"


def setup_scope(scope):
    scope.setup_channel(CHANNELS["VCC"], 1.0, -2.5, probe=10)
    scope.setup_channel(CHANNELS["TX_ON"], 1.0, -1.65, probe=10)
    scope.setup_channel(CHANNELS["RX_ON"], 1.0, -1.65, probe=10)
    scope.setup_channel(CHANNELS["SDN"], 1.0, -2.5, probe=10)
    scope.set_memory_depth("1M")
    scope.setup_timebase(0.02, 0.15)     # 20 ms/div, 1M points: about -0.05 .. +0.35 s around the trigger


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--cycles", type=int, default=4)
    ap.add_argument("--off", type=float, default=5.0)
    ap.add_argument("--on", type=float, default=6.0)
    ap.add_argument("--arm-delay", type=float, default=0.8)
    args = ap.parse_args()

    run = os.path.join(HERE, "data", time.strftime("%Y-%m-%d"), "t2_" + time.strftime("%H%M%S"))
    os.makedirs(run)
    serial = SerialLogger(os.path.join(run, "serial.log"))
    serial.start()
    psu, scope = ODP3063(), DHO914S()
    scope.sock.settimeout(30)
    cycles = []
    try:
        psu.check_caps(CAPS)
        psu.expect_outputs(False, False, True)
        setup_scope(scope)
        time.sleep(1.0)
        for n in range(args.cycles):
            kind = "powerup" if n % 2 == 0 else "init"
            cyc = {"n": n, "kind": kind, "off_s": args.off, "supply": []}
            psu.check_caps(CAPS)
            psu.expect_outputs(False, False, True)
            if kind == "powerup":
                scope.setup_edge_trigger(CHANNELS["VCC"], 2.5, "POS", "NORM")
            else:
                scope.setup_edge_trigger(CHANNELS["SDN"], 2.5, "POS", "NORM")
            psu.channel_off(SUPPLY_CH)
            cyc["t_off"] = time.time()
            time.sleep(args.off)
            if kind == "powerup":
                scope.single()
                time.sleep(0.3)
            psu.channel_on(SUPPLY_CH)
            cyc["t_on"] = t_on = time.time()
            armed = kind == "powerup"
            triggered = None
            while time.time() - t_on < args.on:
                if not armed and time.time() - t_on >= args.arm_delay:
                    scope.single()
                    cyc["t_armed"] = time.time()
                    armed = True
                volts, amps = psu.measure()
                cyc["supply"].append((round(time.time() - t_on, 3), volts[SUPPLY_CH - 1], amps[SUPPLY_CH - 1]))
                if armed and triggered is None and scope.query(":TRIG:STAT?") == "STOP":
                    triggered = time.time() - t_on
            if triggered is None and armed and scope.query(":TRIG:STAT?") == "STOP":
                triggered = time.time() - t_on
            cyc["scope_stopped_after_s"] = triggered
            cyc["serial"] = serial.lines_between(cyc["t_off"], time.time())
            if triggered is not None:
                save_capture(scope, os.path.join(run, "cycle%02d_%s" % (n, kind)), CHANNELS,
                             {"cycle": n, "kind": kind, "firmware": FIRMWARE, "t_on": t_on})
            else:
                scope.stop()
            psu.expect_outputs(False, False, True)
            failed = [s for _, s in cyc["serial"] if "Failed radio init" in s]
            print("cycle %d %-7s triggered %s | supply A max %.3f last %.3f | serial lines %d, init failures %d"
                  % (n, kind, "%.2fs" % triggered if triggered is not None else "NO",
                     max(a for _, _, a in cyc["supply"]), cyc["supply"][-1][2], len(cyc["serial"]), len(failed)))
            cycles.append(cyc)
    finally:
        with open(os.path.join(run, "t2.json"), "w") as f:
            json.dump({"firmware": FIRMWARE, "args": vars(args), "cycles": cycles,
                       "supply_snapshot_end": psu.snapshot()}, f, indent=1)
        serial.stop()
        scope.run()
        psu.close()
        scope.close()
        print("saved", run)


if __name__ == "__main__":
    main()
