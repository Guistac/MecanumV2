#!/usr/bin/env python3
"""T8/T9 - radio chip reset: stall (stock driver) or recovery (RadioHead_RFM23BP + Radio::update).

Firmware env:remote_diag or env:remote_diag_stock. Serial commands (--command):
  k  pulse SDN for 1 ms right after the next send (pings every 500 ms): packet sent never comes (T8)
  i  pulse SDN for 1 ms now, while the chip is receiving (T9)
  f  overwrite the frequency band register with its default: a reset the interrupt missed (T9)
  n  detach the nIRQ interrupt: the next radio event leaves nIRQ low, as a lost edge would (T9)
Setup E (CH1 SCK, CH2 MISO, CH3 nIRQ, CH4 SDN, 10x): one capture per trial, triggered on the
SDN pulse, 20 ms/div, showing the following sends and the re-init.

Per trial, from the serial log: the pulse line, "radio tx timeout, re-init ok|failed", and whether
the firmware still prints its once a second diag line (stalled when none for 3 s). A stall ends the
run: supply current is logged for a few seconds, then the board is switched off.

Usage: python3 -I bench/t8_stall.py [--trials 20] [--label fixed]
"""

import argparse
import glob
import os
import re
import sys
import time

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "lib"))
from rig import Rig, SUPPLY_CH  # noqa: E402

CHANNELS_SPI = {"SCK": 1, "MISO": 2, "NIRQ": 3, "SDN": 4}
TEMP_RE = re.compile(r"diag temp_raw=(-?\d+) temp_c=(-?[\d.]+)(?: recoveries=(\d+))?")
REINIT_RE = re.compile(r"radio (.+), re-init (ok|failed) \(recovery (\d+)\)")
EVENT_RE = re.compile(r"diag (sdn_pulse_during_tx|sdn_pulse_now|config_corrupted|interrupt_dropped)")


def send_char(char):
    """Write only: the rig's serial logger keeps reading the same port."""
    port = sorted(glob.glob("/dev/cu.usbmodem*"))[0]
    fd = os.open(port, os.O_WRONLY | os.O_NOCTTY | os.O_NONBLOCK)
    try:
        os.write(fd, char.encode())
    finally:
        os.close(fd)
    return time.time()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--trials", type=int, default=20)
    ap.add_argument("--label", default="fixed", help="firmware label for the run folder, e.g. stock or fixed")
    ap.add_argument("--period", type=float, default=4.0, help="s between trials")
    ap.add_argument("--command", default="k", choices=["k", "i", "f", "n"])
    args = ap.parse_args()
    rig = Rig("t8_stall_%s_%s" % (args.label, args.command), channels=CHANNELS_SPI)
    rig.firmware = "env:remote_diag_stock (framework RadioHead)" if args.label == "stock" else "env:remote_diag (RadioHead_RFM23BP)"
    stalled = False
    try:
        rig.ensure_on(settle_s=4.0)
        rig.setup_scope()
        rig.scope.setup_timebase(0.02, 0.12)        # about -0.02 .. +0.26 s around the pulse
        rig.scope.run()
        time.sleep(1.0)
        for n in range(args.trials):
            if args.command in ("k", "i"):
                rig.scope.setup_edge_trigger(CHANNELS_SPI["SDN"], 2.5, "POS", "NORM")
                rig.scope.single()
                time.sleep(0.3)
            t_k = send_char(args.command)
            time.sleep(3.0)
            lines = [(t - t_k, s) for t, s in rig.serial.lines if t >= t_k]
            pulse = [round(t, 3) for t, s in lines if EVENT_RE.search(s)]
            reinit = [(round(t, 3), m.group(1), m.group(2), int(m.group(3))) for t, s in lines for m in [REINIT_RE.search(s)] if m]
            alive = [t for t, s in lines if TEMP_RE.search(s)]
            last_alive = max(alive) if alive else None
            stalled = bool(pulse) and (last_alive is None or last_alive < 3.0 - 1.5)
            volts, amps = rig.psu.measure()
            captured = args.command in ("k", "i") and rig.scope.query(":TRIG:STAT?") == "STOP"
            folder = rig.save("trial%03d" % n, {"trial": n, "t_k": t_k}) if captured and n < 5 else None
            row = {"n": n, "pulse_s": pulse, "reinit": reinit, "alive_lines": len(alive),
                   "last_alive_s": round(last_alive, 3) if last_alive is not None else None,
                   "stalled": stalled, "supply_v": volts[SUPPLY_CH - 1], "supply_a": amps[SUPPLY_CH - 1],
                   "capture": os.path.basename(folder) if folder else ("captured" if captured else None),
                   "serial": [(round(t, 3), s) for t, s in lines]}
            rig.log_row(row)
            print("#%02d pulse %s  re-init %s  diag lines %d (last %s s)  %s  %.3f A  capture %s" % (
                n, pulse, reinit, len(alive), row["last_alive_s"], "STALLED" if stalled else "running",
                row["supply_a"], row["capture"]), flush=True)
            if not pulse:
                print(">>> no pulse line: firmware did not take the command (wrong firmware?)", flush=True)
                break
            if stalled:
                print(">>> STALL: logging supply current for 5 s, then power off", flush=True)
                end = time.time() + 5.0
                while time.time() < end:
                    volts, amps = rig.psu.measure()
                    print("    %.3f V  %.3f A" % (volts[SUPPLY_CH - 1], amps[SUPPLY_CH - 1]), flush=True)
                    time.sleep(0.6)
                break
            time.sleep(max(0.0, args.period - 3.3))
    finally:
        rig.close(power_off=True)
        print("saved", rig.run)


if __name__ == "__main__":
    main()
