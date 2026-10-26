#!/usr/bin/env python3
"""T6a - PA current while the radio's control lines float (docs/radio/05-bench-test-plan.md, D1/D2).

Pressing the Teensy PROGRAM button stops it in the bootloader with its pins floating, as during the
first 300 ms of every boot: SDN floats high (radio shutdown) and TX_ON / RX_ON float low (T3).
This holds that state long enough for the supply to measure the current.

Safety: current limit lowered to 0.5 A for the test (hard cap about 2.5 W), the state is held for
--hold seconds only, and the board is power cycled at once if a reading exceeds --abort-amps.

Usage: python3 -I bench/t6_bootloader_shutdown.py [--hold 5] [--abort-amps 0.45]
"""

import argparse
import glob
import os
import sys
import time

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "lib"))
from rig import CHANNELS, NOMINAL, SUPPLY_CH, Rig  # noqa: E402

TEST_CURR = 0.5


def levels(rig):
    return {name: rig.scope.measure("VAVG", ch) for name, ch in CHANNELS.items()}


def reading(rig, t_ref, tag):
    volts, amps = rig.psu.measure()
    row = (tag, round(time.time() - t_ref, 2), volts[SUPPLY_CH - 1], amps[SUPPLY_CH - 1])
    print("  %-9s t=%6.2f s  %.3f V  %.3f A" % row, flush=True)
    return row


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--hold", type=float, default=5.0)
    ap.add_argument("--abort-amps", type=float, default=0.45)
    ap.add_argument("--wait", type=float, default=180.0)
    args = ap.parse_args()
    rig = Rig("t6a")
    log = {"readings": [], "levels": {}}
    try:
        rig.check(on=True)
        rig.psu.set_current(SUPPLY_CH, TEST_CURR)
        rig.check(on=True)
        rig.setup_scope()
        rig.scope.setup_timebase(0.01)
        rig.scope.setup_edge_trigger(CHANNELS["VCC"], 2.5, "POS", "AUTO")
        rig.scope.run()
        print("current limit %.2f A; baseline (normal operation):" % TEST_CURR, flush=True)
        t0 = time.time()
        while time.time() - t0 < 10:
            log["readings"].append(reading(rig, t0, "baseline"))
        log["levels"]["baseline"] = levels(rig)
        print("  levels:", log["levels"]["baseline"], flush=True)
        print(">>> PRESS PROGRAM NOW (waiting up to %.0f s for the serial port to disappear)" % args.wait, flush=True)
        t_wait = time.time()
        while glob.glob("/dev/cu.usbmodem*"):
            if time.time() - t_wait > args.wait:
                print("no PROGRAM press detected, ending", flush=True)
                return
            time.sleep(0.1)
        t_halt = time.time()
        print("bootloader detected (serial port gone); holding %.1f s" % args.hold, flush=True)
        aborted = False
        while time.time() - t_halt < args.hold:
            row = reading(rig, t_halt, "halted")
            log["readings"].append(row)
            if row[3] > args.abort_amps:
                aborted = True
                print("  ABORT: %.3f A above %.2f A" % (row[3], args.abort_amps), flush=True)
                break
        if not aborted:
            log["levels"]["halted"] = levels(rig)
            print("  levels:", log["levels"]["halted"], flush=True)
        rig.psu.channel_off(SUPPLY_CH)
        t_off = time.time()
        print("power off after %.1f s in the bootloader" % (t_off - t_halt), flush=True)
        time.sleep(3.0)
        rig.psu.set_current(SUPPLY_CH, NOMINAL["curr"])
        rig.psu.channel_on(SUPPLY_CH)
        t_on = time.time()
        boot = rig.watch_boot(t_on)
        alive = rig.tx_alive() if boot["state"] in ("OK", "RETRY_OK") else None
        print("after power cycle: boot %s, failed attempts %d, pinging %s" % (boot["state"], boot["failed_attempts"], alive), flush=True)
        log.update({"boot_after": boot["state"], "pinging_after": alive, "aborted": aborted, "t_halt": t_halt, "t_off": t_off})
    finally:
        rig.psu.set_current(SUPPLY_CH, NOMINAL["curr"])
        rig.log_row(log)
        rig.close()
        print("current limit back to %.1f A; saved %s" % (NOMINAL["curr"], rig.run), flush=True)


if __name__ == "__main__":
    main()
