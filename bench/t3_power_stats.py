#!/usr/bin/env python3
"""T3 - power-cycle statistics (docs/radio/05-bench-test-plan.md, diagnostic D4).

Off times 0.2 / 0.5 / 1 / 2 / 5 / 10 s, --repeat each, in a shuffled order (seed saved).
Per cycle: init capture (SDN rising), boot result from serial, supply current, ping check.
Stops at the first failure and leaves the board powered in that state.

Usage: python3 -I bench/t3_power_stats.py [--repeat 5] [--seed 1]
"""

import argparse
import os
import random
import sys
import time

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "lib"))
from rig import Rig  # noqa: E402

OFF_TIMES = [0.2, 0.5, 1.0, 2.0, 5.0, 10.0]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--repeat", type=int, default=5)
    ap.add_argument("--seed", type=int, default=1)
    args = ap.parse_args()
    order = OFF_TIMES * args.repeat
    random.Random(args.seed).shuffle(order)
    rig = Rig("t3")
    try:
        rig.check(on=True)
        rig.setup_scope()
        time.sleep(1.0)
        for n, off_s in enumerate(order):
            t_off, t_on = rig.power_cycle(off_s)
            boot = rig.watch_boot(t_on)
            stopped = rig.scope.query(":TRIG:STAT?") == "STOP"
            if stopped:
                rig.save("cycle%02d_init" % n, {"cycle": n, "off_s": off_s, "t_on": t_on})
            alive = rig.tx_alive() if boot["state"] in ("OK", "RETRY_OK") else None
            amps = [a for _, _, a in boot["supply"]]
            row = {"n": n, "off_s": off_s, "state": boot["state"], "failed_attempts": boot["failed_attempts"],
                   "first_output_s": boot["first_output_s"], "init_captured": stopped, "pinging": alive,
                   "amps_max": max(amps), "amps_last": amps[-1], "supply": boot["supply"], "serial": boot["serial"],
                   "t_off": t_off, "t_on": t_on}
            rig.log_row(row)
            print("#%02d off %4.1fs  %-8s attempts failed %d  output at %s s  init capture %s  pinging %s  A max %.3f last %.3f"
                  % (n, off_s, row["state"], row["failed_attempts"], row["first_output_s"], stopped, alive,
                     row["amps_max"], row["amps_last"]), flush=True)
            rig.setup_scope()
            rig.scope.setup_edge_trigger(1, 2.0, "POS", "AUTO")
            rig.scope.run()
            time.sleep(0.5)
            row["cross_check"] = rig.cross_check()
            if row["state"] not in ("OK", "RETRY_OK") or alive is False:
                print("STOP: failure reproduced, board left powered in this state", flush=True)
                break
    finally:
        rig.close()
        print("saved", rig.run)


if __name__ == "__main__":
    main()
