"""The remote-board test rig: supply CH3 on the 5 V rail, scope setup A, USB serial.

Shared by the T3 / T5 scripts. Boot classification relies on the original remote firmware:
  "Failed radio init, trying again (n/10)"  on every failed attempt (10 attempts, then the
                                            "radio init failed" screen, no further output)
  two joystick lines ("0.007 0.001")       once the firmware is past radio init
"""

import json
import os
import re
import time

from capture import SKILL, save_capture
from serial_log import SerialLogger

import sys
sys.path.insert(0, SKILL)
from owon import ODP3063  # noqa: E402
from rigol import DHO914S  # noqa: E402

CHANNELS = {"VCC": 1, "TX_ON": 2, "RX_ON": 3, "SDN": 4}           # setup A
CHANNELS_GPIO2 = {"VCC": 1, "TX_ON": 2, "RX_ON": 3, "GPIO2": 4}   # setup A with CH4 on RFM23BP GPIO_2
# V/div, offset per signal (10x probes): 5 V signals centred on 2.5 V, 3.3 V logic on 1.65 V
RANGES = {"VCC": (1.0, -2.5), "TX_ON": (1.0, -1.65), "RX_ON": (1.0, -1.65), "SDN": (1.0, -2.5), "GPIO2": (1.0, -1.65),
          "SCK": (1.0, -2.5), "MISO": (1.0, -2.5), "NIRQ": (1.0, -2.5)}
SUPPLY_CH = 3
CAPS = {SUPPLY_CH: (5.5, 2.0)}          # 5 V rail: never above 5.5 V / 2 A
NOMINAL = {"volt": 5.0, "curr": 1.0}
FIRMWARE = "remote, original firmware as flashed (never hardened, commit unknown)"

FAIL_RE = re.compile(r"Failed radio init, trying again \((\d+)/(\d+)\)")
JOY_RE = re.compile(r"^-?\d+\.\d+ -?\d+\.\d+$")


class Rig:

    def __init__(self, test_name, channels=None):
        self.channels = channels or CHANNELS
        here = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
        self.run = os.path.join(here, "data", time.strftime("%Y-%m-%d"), test_name + "_" + time.strftime("%H%M%S"))
        os.makedirs(self.run)
        self.serial = SerialLogger(os.path.join(self.run, "serial.log"))
        self.serial.start()
        self.psu = ODP3063()
        self.scope = DHO914S()
        self.scope.sock.settimeout(30)
        self.rows = []
        self.firmware = FIRMWARE

    def close(self, power_off=True):
        """power_off=False only when a run holds a reproduced failure for measurement."""
        self.serial.stop()
        try:
            if power_off:
                self.psu.channel_off(SUPPLY_CH)
                print("board power off (supply CH%d)" % SUPPLY_CH, flush=True)
            else:
                print("board LEFT POWERED to hold the failure state", flush=True)
            self.scope.run()
        finally:
            self.psu.close()
            self.scope.close()

    # --- setup ------------------------------------------------------------------------

    def setup_scope(self):
        s = self.scope
        for name, ch in self.channels.items():
            scale, offset = RANGES[name]
            s.setup_channel(ch, scale, offset, probe=10)
        s.set_memory_depth("1M")
        s.setup_timebase(0.02, 0.15)     # 20 ms/div: about -0.05 .. +0.35 s around the trigger

    def check(self, on=True):
        self.psu.check_caps(CAPS)
        self.psu.expect_outputs(False, False, on)

    def ensure_on(self, settle_s=3.0):
        """Switch the board on if a previous run left it off (they power off at the end)."""
        self.psu.check_caps(CAPS)
        if not self.psu.outputs()[SUPPLY_CH - 1]:
            self.psu.channel_on(SUPPLY_CH)
            print("board power on (supply CH%d)" % SUPPLY_CH, flush=True)
            time.sleep(settle_s)
        self.check(on=True)

    def cross_check(self, tolerance_v=0.3):
        """The supply readback can freeze until a mains power cycle (seen twice on 2026-10-26):
        compare it with the scope's VCC average. Call with the board powered and settled."""
        volts, amps = self.psu.measure()
        scope_v = self.scope.measure("VAVG", self.channels["VCC"])
        if scope_v is None or abs(volts[SUPPLY_CH - 1] - scope_v) > tolerance_v:
            raise RuntimeError("supply readback %.3f V / %.3f A disagrees with scope VCC %s V: readback "
                               "frozen? Stop and power cycle the supply at the mains" % (volts[SUPPLY_CH - 1], amps[SUPPLY_CH - 1], scope_v))
        return volts[SUPPLY_CH - 1], amps[SUPPLY_CH - 1], scope_v

    # --- actions ------------------------------------------------------------------------

    def power_cycle(self, off_s, arm_init_capture=True):
        """CH3 off for off_s, then on. Optionally arms the scope on the SDN init pulse first."""
        self.check(on=True)
        if arm_init_capture:
            self.scope.setup_edge_trigger(self.channels["SDN"], 2.5, "POS", "NORM")
            self.scope.single()
        self.psu.channel_off(SUPPLY_CH)
        t_off = time.time()
        time.sleep(max(0.0, off_s - (time.time() - t_off)))
        self.psu.channel_on(SUPPLY_CH)
        return t_off, time.time()

    def watch_boot(self, t_on, timeout_s=8.0):
        """Supply log + serial until the firmware is past radio init (or timeout)."""
        supply, result = [], None
        while time.time() - t_on < timeout_s:
            volts, amps = self.psu.measure()
            supply.append((round(time.time() - t_on, 3), volts[SUPPLY_CH - 1], amps[SUPPLY_CH - 1]))
            result = self.classify(t_on)
            if result["state"] in ("OK", "RETRY_OK", "FAILED") and time.time() - t_on > 2.0:
                break
        result = self.classify(t_on)
        result["supply"] = supply
        return result

    def classify(self, t_on):
        lines = [(t - t_on, s) for t, s in self.serial.lines if t >= t_on - 0.5]
        fails = [int(m.group(1)) for _, s in lines for m in [FAIL_RE.search(s)] if m]
        joy = [t for t, s in lines if JOY_RE.match(s.strip())]
        if joy:
            state = "RETRY_OK" if fails else "OK"
        elif fails and max(fails) >= 9:
            state = "FAILED"
        else:
            state = "PENDING"
        return {"state": state, "failed_attempts": len(fails), "first_output_s": round(joy[0], 3) if joy else None,
                "serial": lines}

    def tx_alive(self, timeout_s=2.5):
        """Arm on TX_ON falling: True if the radio transmitted within timeout_s (pings are at 2 Hz)."""
        self.scope.setup_edge_trigger(self.channels["TX_ON"], 1.65, "NEG", "NORM")
        self.scope.single()
        return self.scope.wait_stopped(timeout_s)

    def save(self, name, meta):
        """Saves the stopped capture under the run folder and returns that capture's folder."""
        folder = os.path.join(self.run, name)
        save_capture(self.scope, folder, self.channels, dict(meta, firmware=self.firmware))
        return folder

    def log_row(self, row):
        self.rows.append(row)
        with open(os.path.join(self.run, "rows.json"), "w") as f:
            json.dump(self.rows, f, indent=1)
