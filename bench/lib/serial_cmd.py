"""Send single-character commands to a bench test firmware and read its one-line answers."""

import glob
import os
import select
import termios
import time


class SerialCommand:

    def __init__(self, pattern="/dev/cu.usbmodem*", wait_s=5.0):
        end = time.time() + wait_s
        while not glob.glob(pattern):
            if time.time() > end:
                raise RuntimeError("no serial port %s" % pattern)
            time.sleep(0.05)
        self.port = sorted(glob.glob(pattern))[0]
        self.fd = os.open(self.port, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
        attrs = termios.tcgetattr(self.fd)
        attrs[3] &= ~(termios.ICANON | termios.ECHO)
        termios.tcsetattr(self.fd, termios.TCSANOW, attrs)
        self.pending = b""

    def close(self):
        os.close(self.fd)

    def read_line(self, timeout_s=1.0):
        end = time.time() + timeout_s
        while b"\n" not in self.pending:
            left = end - time.time()
            if left <= 0:
                return None
            ready, _, _ = select.select([self.fd], [], [], left)
            if ready:
                try:
                    self.pending += os.read(self.fd, 4096)
                except BlockingIOError:
                    pass
        line, self.pending = self.pending.split(b"\n", 1)
        return line.decode(errors="replace").strip()

    def command(self, char, timeout_s=1.0):
        """-> (host time the command was sent, answer line or None)"""
        # drop anything already received (boot banner, late answers) so the answer matches
        while select.select([self.fd], [], [], 0)[0]:
            try:
                if not os.read(self.fd, 4096):
                    break
            except BlockingIOError:
                break
        self.pending = b""
        t = time.time()
        os.write(self.fd, char.encode())
        return t, self.read_line(timeout_s)
