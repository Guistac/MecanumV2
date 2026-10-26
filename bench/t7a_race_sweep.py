#!/usr/bin/env python3
"""T7a - run the race_sweep firmware (bench/firmware/race_sweep must be flashed) and save its output.

Usage: python3 -I bench/t7a_race_sweep.py [--sweeps 1]
"""

import argparse
import os
import sys
import time

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "lib"))
from rig import SUPPLY_CH, CAPS  # noqa: E402
from capture import SKILL  # noqa: E402
from serial_cmd import SerialCommand  # noqa: E402

sys.path.insert(0, SKILL)
from owon import ODP3063  # noqa: E402


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--sweeps", type=int, default=1)
    args = ap.parse_args()
    run = os.path.join(os.path.dirname(os.path.abspath(__file__)), "data", time.strftime("%Y-%m-%d"), "t7a_" + time.strftime("%H%M%S"))
    os.makedirs(run)
    psu = ODP3063()
    try:
        psu.check_caps(CAPS)
        if not psu.outputs()[SUPPLY_CH - 1]:
            psu.channel_on(SUPPLY_CH)
            time.sleep(3.0)
        ser = SerialCommand(wait_s=10)
        with open(os.path.join(run, "sweep.txt"), "w") as out:
            out.write("# firmware bench/firmware/race_sweep, %s\n" % time.strftime("%Y-%m-%d %H:%M:%S"))
            for k in range(args.sweeps):
                t, line = ser.command("s", timeout_s=5.0)
                end = time.time() + 120
                while line is not None and not line.startswith("done") and time.time() < end:
                    out.write(line + "\n")
                    print(line, flush=True)
                    line = ser.read_line(timeout_s=5.0)
                out.write("%s\n" % line)
                print(line, flush=True)
        ser.close()
    finally:
        psu.channel_off(SUPPLY_CH)
        print("board power off; saved", run, flush=True)
        psu.close()


if __name__ == "__main__":
    main()
