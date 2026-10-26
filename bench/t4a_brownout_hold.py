#!/usr/bin/env python3
"""T4a - brown-out hold (docs/radio/05-bench-test-plan.md, D3).

Hypothesis: after switch-off the 5 V rail discharges quickly until the loads stop conducting, then
lingers at 1 to 2 V for minutes; a "power cycle" from there is no real reset, and the radio may
fail to initialise until the rail has leaked away. Emulated here: from normal operation the rail is
set to V_hold for hold_s, then back to 5.0 V, and the result is classified:
  REBOOT_OK / REBOOT_RETRY_OK / REBOOT_INIT_FAILED   the Teensy reset during the hold
  NO_REBOOT_PINGING / NO_REBOOT_STALLED             the Teensy survived (stalled = pings stopped)
Stops at the first failure and leaves the board powered in that state; otherwise powers it off.

Usage: python3 -I bench/t4a_brownout_hold.py [--hold 20] [--levels 3.0,2.5,2.0,1.5,1.0,0.5]
"""

import argparse
import os
import sys
import time

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "lib"))
from rig import CHANNELS, NOMINAL, SUPPLY_CH, Rig  # noqa: E402


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--hold", type=float, default=20.0)
    ap.add_argument("--levels", default="3.0,2.5,2.0,1.5,1.0,0.5")
    ap.add_argument("--arm-delay", type=float, default=0.8)
    args = ap.parse_args()
    levels = [float(x) for x in args.levels.split(",")]
    rig = Rig("t4a")
    failed = False
    try:
        rig.ensure_on()
        rig.setup_scope()
        rig.scope.setup_edge_trigger(CHANNELS["VCC"], 2.0, "POS", "AUTO")
        rig.scope.run()
        time.sleep(1.0)
        rig.cross_check()
        if not rig.tx_alive(3.0):
            raise RuntimeError("board not pinging before the test")
        for n, v_hold in enumerate(levels):
            rig.check(on=True)
            rig.setup_scope()
            rig.scope.setup_edge_trigger(CHANNELS["VCC"], 2.0, "POS", "AUTO")
            rig.scope.run()
            t_hold = time.time()
            rig.psu.set_voltage(SUPPLY_CH, v_hold)
            hold_vcc = []
            while time.time() - t_hold < args.hold:
                time.sleep(2.0)
                hold_vcc.append((round(time.time() - t_hold, 1), rig.scope.measure("VAVG", CHANNELS["VCC"]),
                                 rig.psu.measure()[1][SUPPLY_CH - 1]))
            # back to 5 V; arm the scope on the SDN init pulse once the boot float is over
            rig.scope.setup_edge_trigger(CHANNELS["SDN"], 2.5, "POS", "NORM")
            rig.psu.set_voltage(SUPPLY_CH, NOMINAL["volt"])
            t_on = time.time()
            time.sleep(args.arm_delay)
            rig.scope.single()
            boot = rig.watch_boot(t_on)
            hold_events = [s for t, s in rig.serial.lines if t_hold <= t <= t_on + 8]
            rebooted = any(s.startswith("# port closed") for s in hold_events)
            init_captured = rig.scope.query(":TRIG:STAT?") == "STOP"
            if init_captured:
                rig.save("level%02d_init" % n, {"level": n, "v_hold": v_hold, "hold_s": args.hold, "t_on": t_on})
            rig.scope.setup_timebase(0.02, 0.15)
            alive = rig.tx_alive(3.0)
            if rebooted:
                result = {"OK": "REBOOT_OK", "RETRY_OK": "REBOOT_RETRY_OK"}.get(boot["state"], "REBOOT_INIT_" + boot["state"])
            else:
                result = "NO_REBOOT_PINGING" if alive else "NO_REBOOT_STALLED"
            if result in ("REBOOT_OK", "REBOOT_RETRY_OK") and not alive:
                result += "_BUT_NOT_PINGING"
            row = {"n": n, "v_hold": v_hold, "hold_s": args.hold, "result": result, "rebooted": rebooted,
                   "boot": boot["state"], "failed_attempts": boot["failed_attempts"], "pinging": alive,
                   "init_captured": init_captured, "hold_vcc_scope_amps": hold_vcc, "supply_boot": boot["supply"],
                   "serial": [(round(t - t_hold, 3), s) for t, s in rig.serial.lines if t >= t_hold]}
            rig.log_row(row)
            print("#%d hold %.1f V for %.0f s: VCC on scope %s V, %s A | %s (failed attempts %d, pinging %s)"
                  % (n, v_hold, args.hold, hold_vcc[-1][1], hold_vcc[-1][2], result, row["failed_attempts"], alive), flush=True)
            if result not in ("REBOOT_OK", "NO_REBOOT_PINGING"):
                failed = True
                print("STOP: %s after a %.1f V hold; board left powered at 5 V in this state" % (result, v_hold), flush=True)
                break
    finally:
        if not failed:
            rig.psu.set_voltage(SUPPLY_CH, NOMINAL["volt"])
        rig.close(power_off=not failed)
        print("saved", rig.run)


if __name__ == "__main__":
    main()
