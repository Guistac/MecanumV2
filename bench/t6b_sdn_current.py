#!/usr/bin/env python3
"""T6b - supply current and control-line levels with the radio on and in shutdown
(docs/radio/05-bench-test-plan.md, D1/D2). Needs bench/firmware/sdn_test flashed.

The question: in shutdown TX_ON sits low (T3); does that enable the PA, i.e. does the board draw
more current in shutdown than with the radio on? Shutdown is held --hold seconds at a time, at most
twice, with the current limit at 0.5 A and an abort above --abort-amps. The second shutdown is
skipped if the first already shows more than --clear-amps above the on state.

Usage: python3 -I bench/t6b_sdn_current.py [--hold 3] [--abort-amps 0.45] [--clear-amps 0.05]
"""

import argparse
import json
import os
import sys
import time

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "lib"))
from capture import SKILL  # noqa: E402
from rig import CAPS, CHANNELS, NOMINAL, SUPPLY_CH  # noqa: E402
from serial_cmd import SerialCommand  # noqa: E402

sys.path.insert(0, SKILL)
from owon import ODP3063  # noqa: E402
from rigol import DHO914S  # noqa: E402

TEST_CURR = 0.5
FIRMWARE = "bench/firmware/sdn_test (radio-bench)"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--hold", type=float, default=3.0)
    ap.add_argument("--on", type=float, default=4.0)
    ap.add_argument("--abort-amps", type=float, default=0.45)
    ap.add_argument("--clear-amps", type=float, default=0.05)
    args = ap.parse_args()
    run = os.path.join(os.path.dirname(os.path.abspath(__file__)), "data", time.strftime("%Y-%m-%d"), "t6b_" + time.strftime("%H%M%S"))
    os.makedirs(run)
    psu, scope, ser = ODP3063(), DHO914S(), SerialCommand()
    log = {"firmware": FIRMWARE, "args": vars(args), "phases": []}

    def phase(name, sdn_char, duration, abort):
        t_cmd, answer = ser.command(sdn_char)
        readings = []
        while time.time() - t_cmd < duration:
            volts, amps = psu.measure()
            readings.append((round(time.time() - t_cmd, 2), volts[SUPPLY_CH - 1], amps[SUPPLY_CH - 1]))
            print("  %-9s t=%4.2f s  %.3f V  %.3f A" % ((name,) + readings[-1]), flush=True)
            if abort and readings[-1][2] > args.abort_amps:
                ser.command("0")
                print("  ABORT: %.3f A, radio switched back on" % readings[-1][2], flush=True)
                break
        levels = {k: scope.measure("VAVG", ch) for k, ch in CHANNELS.items()}
        device = ser.command("r")[1]
        settled = [a for t, _, a in readings if t >= 1.2]       # readback lags about 0.7 s
        p = {"name": name, "answer": answer, "readings": readings, "levels": levels, "device": device,
             "amps_settled_mean": sum(settled) / len(settled) if settled else None}
        log["phases"].append(p)
        print("  %-9s settled %s A | levels %s | %s" % (name, "%.3f" % p["amps_settled_mean"] if settled else "-",
              {k: round(v, 3) for k, v in levels.items() if v is not None}, device), flush=True)
        return p

    try:
        psu.check_caps(CAPS)
        psu.expect_outputs(False, False, True)
        for ch in CHANNELS.values():
            scope.setup_channel(ch, 1.0, -2.5 if ch in (CHANNELS["VCC"], CHANNELS["SDN"]) else -1.65, probe=10)
        scope.setup_timebase(0.01)
        scope.setup_edge_trigger(CHANNELS["VCC"], 2.0, "POS", "AUTO")
        scope.run()
        psu.set_current(SUPPLY_CH, TEST_CURR)
        psu.check_caps(CAPS)
        time.sleep(1.0)
        volts, amps = psu.measure()
        scope_v = scope.measure("VAVG", CHANNELS["VCC"])
        if scope_v is None or abs(volts[SUPPLY_CH - 1] - scope_v) > 0.3:
            raise RuntimeError("supply readback %.3f V vs scope %s V: frozen?" % (volts[SUPPLY_CH - 1], scope_v))
        on1 = phase("on", "0", args.on, False)
        off1 = phase("shutdown", "1", args.hold, True)
        on2 = phase("on", "0", args.on, False)
        delta = (off1["amps_settled_mean"] or 0) - (on1["amps_settled_mean"] or 0)
        if delta > args.clear_amps:
            print("shutdown draws %.3f A more than on: second shutdown skipped" % delta, flush=True)
        else:
            phase("shutdown", "1", args.hold, True)
            phase("on", "0", args.on, False)
    finally:
        ser.command("0")
        log["final_device"] = ser.command("r")[1]
        psu.set_current(SUPPLY_CH, NOMINAL["curr"])
        psu.channel_off(SUPPLY_CH)
        log["supply_end"] = psu.snapshot()
        with open(os.path.join(run, "t6b.json"), "w") as f:
            json.dump(log, f, indent=1)
        print("radio on (%s), current limit back to %.1f A, board power off; saved %s" % (log["final_device"], NOMINAL["curr"], run), flush=True)
        ser.close()
        psu.close()
        scope.close()


if __name__ == "__main__":
    main()
