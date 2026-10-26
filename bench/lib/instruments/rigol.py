#!/usr/bin/env python3
"""Rigol DHO914S oscilloscope over LAN (raw SCPI, TCP 5555).

Tested on RIGOL TECHNOLOGIES,DHO914S,DHO9S254603418,00.01.01 on 2026-10-26. Standard library only.

Tested: identity, error queue, settings queries, waveform read (screen and raw memory, BYTE),
screenshot (PNG), single / force trigger / stop / run, trigger status, channel / timebase /
trigger / memory depth writes, measurements, generator setup and output.
Not tested yet: memory depths above 1M, peak-detect acquisition, setup save/recall. The
setup_* helpers, measure() and generator*() were tested in the loopback on 2026-10-26.

Usage as a library:
    from rigol import DHO914S
    with DHO914S() as scope:
        scope.single()                       # arm; the harness then switches power
        scope.wait_stopped(timeout=5)        # False if nothing triggered
        t0, dt, volts = scope.waveform(1, raw=True)
        scope.screenshot("capture.png")

Command line:
    python3 rigol.py status            read-only settings snapshot
    python3 rigol.py shot out.png      screenshot
"""

import json
import socket
import sys
import time

DEFAULT_HOST = "10.0.0.11"
DEFAULT_PORT = 5555
RAW_CHUNK = 1000000         # points per :WAV:DATA? in RAW mode (1M in one read tested: 0.42 s)


class RigolError(RuntimeError):
    pass


def to_volts(preamble, data):
    """Raw BYTE data -> volts, with the scope's own scaling (yinc, yorig, yref from :WAV:PRE?)."""
    yinc, yorg, yref = float(preamble[7]), float(preamble[8]), float(preamble[9])
    return [(b - yorg - yref) * yinc for b in data]


def preamble_time(preamble):
    """(time of the first point relative to the trigger, time per point) in seconds."""
    return float(preamble[5]), float(preamble[4])


class DHO914S:

    def __init__(self, host=DEFAULT_HOST, port=DEFAULT_PORT, timeout=10.0):
        self.sock = socket.create_connection((host, port), timeout=3.0)
        self.sock.settimeout(timeout)

    def close(self):
        self.sock.close()

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        self.close()

    # --- transport --------------------------------------------------------------

    def write(self, command):
        self.sock.sendall((command + "\n").encode())

    def query(self, command):
        self.write(command)
        buf = b""
        while not buf.endswith(b"\n"):
            chunk = self.sock.recv(4096)
            if not chunk:
                raise RigolError("connection closed during '%s'" % command)
            buf += chunk
        return buf.decode().strip()

    def query_block(self, command):
        """IEEE 488.2 definite-length block: #<n><n digits of size><data>\\n"""
        self.write(command)
        buf = b""
        while len(buf) < 2:
            buf += self.sock.recv(4096)
        if buf[:1] != b"#":
            raise RigolError("'%s' did not return a block: %r" % (command, buf[:40]))
        digits = int(buf[1:2])
        while len(buf) < 2 + digits:
            buf += self.sock.recv(4096)
        size = int(buf[2:2 + digits])
        end = 2 + digits + size
        while len(buf) < end + 1:          # + trailing newline
            buf += self.sock.recv(65536)
        return buf[2 + digits:end]

    def check(self, context=""):
        """Raise if the error queue is not empty (the DHO reports errors, unlike the OWON)."""
        err = self.query(":SYST:ERR?")
        if not err.startswith("0,"):
            raise RigolError("%s: %s" % (context or "scope error", err))

    # --- state --------------------------------------------------------------------

    def idn(self):
        return self.query("*IDN?")

    def status(self):
        snap = {
            "trigger_status": self.query(":TRIG:STAT?"),
            "trigger_mode": self.query(":TRIG:MODE?"),
            "trigger_sweep": self.query(":TRIG:SWE?"),
            "sample_rate": float(self.query(":ACQ:SRAT?")),
            "memory_depth": float(self.query(":ACQ:MDEP?")),
            "timebase_scale": float(self.query(":TIM:MAIN:SCAL?")),
        }
        for ch in (1, 2, 3, 4):
            snap["ch%d" % ch] = {
                "display": self.query(":CHAN%d:DISP?" % ch) == "1",
                "scale": float(self.query(":CHAN%d:SCAL?" % ch)),
                "probe": float(self.query(":CHAN%d:PROB?" % ch)),
            }
        return snap

    # --- acquisition (tested) -----------------------------------------------------

    def run(self):
        self.write(":RUN")

    def stop(self):
        self.write(":STOP")

    def single(self):
        """Arm one capture; :TRIG:STAT? goes WAIT, then STOP once triggered."""
        self.write(":SING")

    def force_trigger(self):
        self.write(":TFOR")

    def wait_stopped(self, timeout=5.0, poll=0.1):
        end = time.monotonic() + timeout
        while time.monotonic() < end:
            if self.query(":TRIG:STAT?") == "STOP":
                return True
            time.sleep(poll)
        return False

    def waveform(self, channel, raw=False):
        """Returns (t_first_s, dt_s, volts). raw=True reads capture memory and needs the scope
        stopped (after single() + wait_stopped()); raw=False reads the 1000 screen points."""
        preamble, data = self.waveform_bytes(channel, raw)
        return preamble_time(preamble) + (to_volts(preamble, data),)

    def waveform_bytes(self, channel, raw=False):
        """Like waveform(), but returns (preamble list, raw bytes) exactly as the scope sent them,
        for saving; convert later with to_volts() / preamble_time()."""
        self.write(":WAV:SOUR CHAN%d" % channel)
        self.write(":WAV:MODE %s" % ("RAW" if raw else "NORM"))
        self.write(":WAV:FORM BYTE")
        if raw:
            # The preamble's point count is the current :WAV:STAR/:WAV:STOP window, which persists
            # between reads: take the capture size from the memory depth and set the window.
            points = int(float(self.query(":ACQ:MDEP?")))
            self.write(":WAV:STAR 1")
            self.write(":WAV:STOP %d" % min(RAW_CHUNK, points))
        pre = self.query(":WAV:PRE?").split(",")
        if not raw:
            points = int(float(pre[2]))
        data = b""
        if raw:
            start = 1
            while start <= points:
                stop = min(start + RAW_CHUNK - 1, points)
                self.write(":WAV:STAR %d" % start)
                self.write(":WAV:STOP %d" % stop)
                data += self.query_block(":WAV:DATA?")
                start = stop + 1
        else:
            data = self.query_block(":WAV:DATA?")
        if not data:
            raise RigolError("no waveform data (nothing captured yet?)")
        pre[2] = str(len(data))     # the point count actually read, not the last window
        return pre, data

    def screenshot(self, path):
        png = self.query_block(":DISP:DATA? PNG")
        if png[:4] != bytes([0x89, 0x50, 0x4E, 0x47]):
            raise RigolError("screenshot is not a PNG")
        with open(path, "wb") as f:
            f.write(png)
        return len(png)

    # --- setup (tested 2026-10-26 by write + readback) -------------------------------
    # Every refused command pops up "remote Cmd Error" on the scope screen: only send
    # commands listed in reference.md, and never probe command variants during a run.

    def _set_checked(self, command, query, want=None):
        self.write(command)
        self.check(command)
        if want is not None:
            have = self.query(query)
            try:
                same = abs(float(have) - float(want)) <= 1e-9 + 1e-6 * abs(float(want))
            except ValueError:
                # the scope answers short forms (SQUare -> SQU, CHANnel2 -> CHAN2)
                have_u, want_u = have.upper(), str(want).upper()
                same = have_u.startswith(want_u) or want_u.startswith(have_u)
            if not same:
                raise RigolError("%s reads back %s" % (command, have))

    def setup_channel(self, channel, scale_v, offset_v=0.0, probe=10, coupling="DC"):
        """Order matters: a hidden channel silently ignores scale/offset writes, and the
        valid V/div range depends on the probe factor, so: show, probe, coupling, offset, scale."""
        c = ":CHAN%d" % channel
        self._set_checked(c + ":DISP 1", c + ":DISP?", 1)
        self._set_checked(c + ":PROB %g" % probe, c + ":PROB?", probe)
        self._set_checked(c + ":COUP %s" % coupling, c + ":COUP?", coupling)
        self._set_checked(c + ":SCAL %g" % scale_v, c + ":SCAL?", scale_v)
        self._set_checked(c + ":OFFS %g" % offset_v, c + ":OFFS?", offset_v)

    def hide_channel(self, channel):
        self._set_checked(":CHAN%d:DISP 0" % channel, ":CHAN%d:DISP?" % channel, 0)

    def setup_timebase(self, scale_s, offset_s=0.0):
        self._set_checked(":TIM:MAIN:SCAL %g" % scale_s, ":TIM:MAIN:SCAL?", scale_s)
        self._set_checked(":TIM:MAIN:OFFS %g" % offset_s, ":TIM:MAIN:OFFS?", offset_s)

    def set_memory_depth(self, depth):
        """depth as the scope spells it: '10k', '100k', '1M', ... (a plain 10000 is refused)."""
        self._set_checked(":ACQ:MDEP %s" % depth, ":ACQ:MDEP?")

    def setup_edge_trigger(self, channel, level_v, slope="POS", sweep="NORM"):
        """slope POS / NEG / RFAL; sweep NORM waits for a real edge (SING then captures once)."""
        self._set_checked(":TRIG:MODE EDGE", ":TRIG:MODE?", "EDGE")
        self._set_checked(":TRIG:EDGE:SOUR CHAN%d" % channel, ":TRIG:EDGE:SOUR?", "CHAN%d" % channel)
        self._set_checked(":TRIG:EDGE:SLOP %s" % slope, ":TRIG:EDGE:SLOP?", slope)
        self._set_checked(":TRIG:EDGE:LEV %g" % level_v, ":TRIG:EDGE:LEV?", level_v)
        self._set_checked(":TRIG:SWE %s" % sweep, ":TRIG:SWE?", sweep)

    def measure(self, item, channel):
        """e.g. measure('VMIN', 1) -> volts, or None when the scope has no valid value
        (it answers 9.9E+37, e.g. FREQ on a flat line)."""
        value = float(self.query(":MEAS:ITEM? %s,CHAN%d" % (item, channel)))
        return None if value > 9e37 else value

    # --- generator (S models; tested 2026-10-26, output switched with nothing connected) ---
    # Owner rule: say before switching the generator output on, even with nothing connected.

    def generator(self, function="SQU", freq_hz=1000.0, ampl_vpp=1.0, offset_v=0.0):
        """function SIN / SQU / RAMP / DC / ... ('PULS' is refused on this firmware)."""
        self._set_checked(":SOUR:FUNC %s" % function, ":SOUR:FUNC?", function)
        if function != "DC":
            self._set_checked(":SOUR:FREQ %g" % freq_hz, ":SOUR:FREQ?", freq_hz)
        self._set_checked(":SOUR:VOLT:AMPL %g" % ampl_vpp, ":SOUR:VOLT:AMPL?", ampl_vpp)
        self._set_checked(":SOUR:VOLT:OFFS %g" % offset_v, ":SOUR:VOLT:OFFS?", offset_v)

    def generator_output(self, on):
        self._set_checked(":SOUR:OUTP:STAT %d" % bool(on), ":SOUR:OUTP:STAT?", int(bool(on)))


def _main(argv):
    command = argv[1] if len(argv) > 1 else "status"
    with DHO914S() as scope:
        if command == "status":
            print(scope.idn())
            print(json.dumps(scope.status(), indent=1))
            scope.check("status")
        elif command == "shot" and len(argv) > 2:
            print("%d bytes -> %s" % (scope.screenshot(argv[2]), argv[2]))
        else:
            print(__doc__)
            return 2
    return 0


if __name__ == "__main__":
    sys.exit(_main(sys.argv))
