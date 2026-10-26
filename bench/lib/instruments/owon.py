#!/usr/bin/env python3
"""OWON ODP3063 triple-output supply over LAN (raw SCPI, TCP 3000).

Tested on OWON,ODP3063,23450067,FV:V3.6.0 on 2026-10-26. Standard library only.

Safety rules built into this module:
- `OUTP ON` switches ALL channels: it is refused. Use set_outputs() / channel_on().
- Limits (OVP/OCP) are ceilings: set them first, then setpoints, then read back.
  The supply silently rejects a setpoint above its limit, so every write is verified.
- Measurements refresh about every 0.5 s; polling faster freezes them, so measure()
  is rate limited.
- check_caps() refuses to run when a channel's limits exceed the caller's fixed caps.

Usage as a library:
    from owon import ODP3063
    psu = ODP3063()
    psu.check_caps({3: (5.5, 1.0)})           # CH3 must never exceed 5.5 V / 1 A
    psu.configure(3, volt=5.0, curr=1.0, ovp=5.5, ocp=1.0)
    psu.channel_on(3); print(psu.measure()); psu.channel_off(3)

Command line:
    python3 owon.py status      read-only snapshot
    python3 owon.py off         all outputs off (CHAN:OUTP:ALL 0,0,0)
"""

import json
import re
import socket
import sys
import time

DEFAULT_HOST = "10.0.0.10"
DEFAULT_PORT = 3000
CHANNELS = (1, 2, 3)
MEASURE_INTERVAL_S = 0.5    # faster polling freezes the readings
WRITE_SETTLE_S = 0.15       # the supply needs a moment before the next command
TOLERANCE = 0.0015          # readback rounding (3 decimals)

# OUTP / OUTP:STAT / OUTP:STAT:ALL with ON or 1: switches every channel
_MASTER_ON = re.compile(r"^\s*OUTP(UT)?(:STAT(E)?)?(:ALL)?\s+(ON|1)\s*$", re.IGNORECASE)


class OwonError(RuntimeError):
    pass


class ODP3063:

    def __init__(self, host=DEFAULT_HOST, port=DEFAULT_PORT, timeout=1.0):
        self.sock = socket.create_connection((host, port), timeout=2.0)
        self.sock.settimeout(timeout)
        self._last_measure = 0.0

    def close(self):
        self.sock.close()

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        self.close()

    # --- transport --------------------------------------------------------------

    def write(self, command):
        if _MASTER_ON.match(command):
            raise OwonError("refused: '%s' switches ALL channels, use set_outputs()" % command)
        self.sock.sendall((command + "\n").encode())
        time.sleep(WRITE_SETTLE_S)

    def query(self, command):
        """Unsupported queries get no reply at all: that raises instead of hanging."""
        self.sock.sendall((command + "\n").encode())
        buf = b""
        try:
            while not buf.endswith(b"\n"):
                chunk = self.sock.recv(256)
                if not chunk:
                    break
                buf += chunk
        except socket.timeout:
            pass
        if not buf:
            raise OwonError("no reply to '%s' (unsupported command?)" % command)
        return buf.decode().strip()

    @staticmethod
    def _floats(reply):
        return [float(x) for x in reply.split(",")]

    # --- state --------------------------------------------------------------------

    def idn(self):
        return self.query("*IDN?")

    def outputs(self):
        """Per-channel output state, e.g. (False, False, True). (OUTP? only says 'any on'.)"""
        return tuple(int(x) == 1 for x in self.query("CHAN:OUTP:ALL?").split(","))

    def setpoints(self):
        return self._floats(self.query("APP:VOLT?")), self._floats(self.query("APP:CURR?"))

    def limits(self):
        """(OVP volts, OCP amps) per channel."""
        return self._floats(self.query("VOLT:LIM:ALL?")), self._floats(self.query("CURR:LIM:ALL?"))

    def measure(self):
        """Measured (volts, amps) per channel, rate limited to one refresh period."""
        wait = MEASURE_INTERVAL_S - (time.monotonic() - self._last_measure)
        if wait > 0:
            time.sleep(wait)
        volts = self._floats(self.query("MEAS:VOLT:ALL?"))
        amps = self._floats(self.query("MEAS:CURR:ALL?"))
        self._last_measure = time.monotonic()
        return volts, amps

    def snapshot(self):
        volts, amps = self.setpoints()
        ovp, ocp = self.limits()
        return {"volt": volts, "curr": amps, "ovp": ovp, "ocp": ocp, "outputs": list(self.outputs())}

    def restore(self, snap):
        """Restore a snapshot(): outputs off first, then limits, setpoints, outputs."""
        self.set_outputs(False, False, False)
        self.write("VOLT:LIM:ALL " + ",".join("%.3f" % v for v in snap["ovp"]))
        self.write("CURR:LIM:ALL " + ",".join("%.3f" % v for v in snap["ocp"]))
        self.write("APP:VOLT " + ",".join("%.3f" % v for v in snap["volt"]))
        self.write("APP:CURR " + ",".join("%.3f" % v for v in snap["curr"]))
        self.set_outputs(*snap["outputs"])
        now = self.snapshot()
        for key in ("volt", "curr", "ovp", "ocp"):
            if any(abs(a - b) > TOLERANCE for a, b in zip(now[key], snap[key])):
                raise OwonError("restore mismatch on %s: %s != %s" % (key, now[key], snap[key]))

    # --- control ------------------------------------------------------------------

    def set_outputs(self, ch1, ch2, ch3):
        self.write("CHAN:OUTP:ALL %d,%d,%d" % (bool(ch1), bool(ch2), bool(ch3)))
        wanted = (bool(ch1), bool(ch2), bool(ch3))
        if self.outputs() != wanted:
            raise OwonError("output state %s, expected %s" % (self.outputs(), wanted))

    def channel_on(self, channel):
        state = list(self.outputs())
        state[channel - 1] = True
        self.set_outputs(*state)

    def channel_off(self, channel):
        state = list(self.outputs())
        state[channel - 1] = False
        self.set_outputs(*state)

    def all_off(self):
        self.set_outputs(False, False, False)

    def expect_outputs(self, ch1, ch2, ch3):
        """Call during a run: an OVP/OCP trip only shows as the output switching itself off
        (no trip flag over LAN; OCP reacted after 1.4 to 2.5 s in the 2026-10-26 short test)."""
        state = self.outputs()
        wanted = (bool(ch1), bool(ch2), bool(ch3))
        if state != wanted:
            raise OwonError("outputs %s, expected %s: protection trip (OVP/OCP) or front panel?" % (state, wanted))

    def configure(self, channel, volt, curr, ovp, ocp):
        """Limits first, then setpoints; every value is read back (rejections are silent)."""
        if channel not in CHANNELS:
            raise OwonError("bad channel %r" % channel)
        if volt > ovp or curr > ocp:
            raise OwonError("setpoint above its limit: %.3f V/%.3f A vs %.3f V/%.3f A" % (volt, curr, ovp, ocp))
        self.write("INST:NSEL %d" % channel)
        self.write("VOLT:LIM %.3f" % ovp)
        self.write("CURR:LIM %.3f" % ocp)
        self.write("VOLT %.3f" % volt)
        self.write("CURR %.3f" % curr)
        got = [float(self.query(q)) for q in ("VOLT:LIM?", "CURR:LIM?", "VOLT?", "CURR?")]
        for name, want, have in zip(("OVP", "OCP", "volt", "curr"), (ovp, ocp, volt, curr), got):
            if abs(want - have) > TOLERANCE:
                raise OwonError("CH%d %s reads %.3f, wanted %.3f (rejected by the supply?)" % (channel, name, have, want))

    def set_voltage(self, channel, volt):
        """Change only the voltage setpoint (e.g. a low-battery sweep), verified."""
        self.write("INST:NSEL %d" % channel)
        self.write("VOLT %.3f" % volt)
        have = float(self.query("VOLT?"))
        if abs(have - volt) > TOLERANCE:
            raise OwonError("CH%d volt reads %.3f, wanted %.3f (above OVP?)" % (channel, have, volt))

    def set_current(self, channel, curr):
        """Change only the current setpoint (constant-current limit), verified."""
        self.write("INST:NSEL %d" % channel)
        self.write("CURR %.3f" % curr)
        have = float(self.query("CURR?"))
        if abs(have - curr) > TOLERANCE:
            raise OwonError("CH%d curr reads %.3f, wanted %.3f (above OCP?)" % (channel, have, curr))

    def check_caps(self, caps):
        """caps = {channel: (max_volts, max_amps)}. Raises if a limit or setpoint is above its cap.
        Call before every run and every cycle: the front panel stays usable while connected."""
        (volts, amps), (ovp, ocp) = self.setpoints(), self.limits()
        for channel, (max_v, max_a) in caps.items():
            i = channel - 1
            if ovp[i] > max_v + TOLERANCE or volts[i] > max_v + TOLERANCE:
                raise OwonError("CH%d voltage OVP %.3f / set %.3f above cap %.3f" % (channel, ovp[i], volts[i], max_v))
            if ocp[i] > max_a + TOLERANCE or amps[i] > max_a + TOLERANCE:
                raise OwonError("CH%d current OCP %.3f / set %.3f above cap %.3f" % (channel, ocp[i], amps[i], max_a))


def _main(argv):
    command = argv[1] if len(argv) > 1 else "status"
    with ODP3063() as psu:
        if command == "status":
            print(psu.idn())
            print(json.dumps(psu.snapshot()))
            volts, amps = psu.measure()
            print(json.dumps({"meas_volt": volts, "meas_curr": amps}))
        elif command == "off":
            psu.all_off()
            print("outputs:", psu.outputs())
        else:
            print(__doc__)
            return 2
    return 0


if __name__ == "__main__":
    sys.exit(_main(sys.argv))
