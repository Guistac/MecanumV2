#!/usr/bin/env python3
"""T6e preparation - the scope's own SPI decoder versus bench/lib/spi_decode on the same capture.

Setup E (CH1 SCK, CH2 MISO, CH3 nIRQ, CH4 SDN). One power cycle at room temperature, one capture
of the first init attempt (as in t6e_heat_spi.py), then: the scope's decode event table
(:BUS1:DATA?, commands from the DHO800/DHO900 programming guide, section 3.4) and our decoder.
Leaves the scope's decoder on (bus 1, SPI, timeout framing) and powers the board off.

Usage: python3 -I bench/t6e_decode_check.py [--timeout-us 1.7] [--scale 0.002] [--offset 0.125]
"""

import argparse
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "lib"))
from capture import load_capture  # noqa: E402
from rig import Rig  # noqa: E402
from spi_decode import frames  # noqa: E402
from t6e_heat_spi import CHANNELS_SPI, setup_capture  # noqa: E402

# :BUS1 setup for SPI decoding of RadioHead's register accesses (16 bits = address + data byte)
DECODER = [
    (":BUS1:MODE SPI", ":BUS1:MODE?", "SPI"),
    (":BUS1:SPI:SCLK:SOURce CHANnel1", ":BUS1:SPI:SCLK:SOURce?", "CHAN1"),
    (":BUS1:SPI:SCLK:SLOPe POSitive", ":BUS1:SPI:SCLK:SLOPe?", "POS"),
    (":BUS1:SPI:MISO:SOURce CHANnel2", ":BUS1:SPI:MISO:SOURce?", "CHAN2"),
    (":BUS1:SPI:MOSI:SOURce OFF", ":BUS1:SPI:MOSI:SOURce?", "OFF"),
    (":BUS1:SPI:POLarity HIGH", ":BUS1:SPI:POLarity?", "HIGH"),
    (":BUS1:SPI:DBITs 16", ":BUS1:SPI:DBITs?", "16"),
    (":BUS1:SPI:ENDian MSB", ":BUS1:SPI:ENDian?", "MSB"),
    (":BUS1:SPI:MODE TIMeout", ":BUS1:SPI:MODE?", "TIM"),
    (":BUS1:FORMat HEX", ":BUS1:FORMat?", "HEX"),
    (":BUS1:DISPlay ON", ":BUS1:DISPlay?", "1"),
    (":BUS1:EVENt ON", ":BUS1:EVENt?", "1"),
]


def setup_decoder(scope, timeout_s):
    for cmd, query, want in DECODER:
        scope.write(cmd)
        time.sleep(0.05)
        scope.check(cmd)
        have = scope.query(query)
        if not have.upper().startswith(want.upper()):
            raise RuntimeError("%s reads back %s" % (cmd, have))
    scope.write(":BUS1:SPI:TIMeout:TIME %g" % timeout_s)
    scope.check("timeout")
    scope.write(":BUS1:THReshold 2.0,CLK")
    scope.check("threshold CLK")
    scope.write(":BUS1:THReshold 2.0,MISO")
    scope.check("threshold MISO")
    return {"timeout": scope.query(":BUS1:SPI:TIMeout:TIME?"),
            "thr_clk": scope.query(":BUS1:THReshold? CLK"), "thr_miso": scope.query(":BUS1:THReshold? MISO")}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--timeout-us", type=float, default=1.7)
    ap.add_argument("--scale", type=float, default=0.002)
    ap.add_argument("--offset", type=float, default=0.125)
    args = ap.parse_args()
    rig = Rig("t6e_decode", channels=CHANNELS_SPI)
    rig.firmware = "env:remote_diag (radio-bench)"
    try:
        rig.ensure_on()
        rig.setup_scope()
        print("decoder:", setup_decoder(rig.scope, args.timeout_us * 1e-6), flush=True)
        setup_capture(rig, args.scale, args.offset)
        rig.scope.run()
        time.sleep(1.0)
        t_off, t_on = rig.power_cycle(1.0, arm_init_capture=False)
        time.sleep(max(0.0, 0.8 - (time.time() - t_on)))
        rig.scope.single()
        boot = rig.watch_boot(t_on, timeout_s=8.0)
        print("boot:", boot["state"], "failed attempts", boot["failed_attempts"], flush=True)
        if rig.scope.query(":TRIG:STAT?") != "STOP":
            print("no capture", flush=True)
            return
        time.sleep(1.0)    # let the scope finish decoding the stopped capture
        table = rig.scope.query_block(":BUS1:DATA?").decode(errors="replace")
        with open(os.path.join(rig.run, "scope_decode.csv"), "w") as f:
            f.write(table)
        rig.scope.screenshot(os.path.join(rig.run, "screen.png"))
        folder = rig.save("cycle000_init", {"t_on": t_on})
        _, sig = load_capture(folder)
        ours = frames(sig["SCK"], sig["MISO"], gap_s=args.timeout_us * 1e-6)
        rows = [r for r in table.splitlines() if r.strip()]
        print("scope table: %d lines; first lines:" % len(rows), flush=True)
        for r in rows[:14]:
            print("   ", r)
        print("our decoder (gap %.1f us): %d frames; first frames:" % (args.timeout_us, len(ours)))
        for f in ours[:12]:
            print("    %.4f ms  edges %2d  %s" % (f["t_s"] * 1e3, f["edges"], " ".join("%02X" % b for b in f["bytes"])))
    finally:
        rig.close(power_off=True)
        print("saved", rig.run)


if __name__ == "__main__":
    main()
