"""Background logger for the Teensy USB serial port.

With the VUSB/VIN pad cut the port disappears at every power cycle: the logger keeps looking for
/dev/cu.usbmodem*, reopens it when it comes back, and timestamps every line with host time.
No pyserial needed (the Teensy ignores the baud rate).
"""

import glob
import os
import select
import termios
import threading
import time


class SerialLogger(threading.Thread):

    def __init__(self, path, pattern="/dev/cu.usbmodem*"):
        super().__init__(daemon=True)
        self.path = path
        self.pattern = pattern
        self.lines = []            # (host time, text); events have text starting with "#"
        self._stop_event = threading.Event()
        self._file = open(path, "a", buffering=1)

    def stop(self):
        self._stop_event.set()
        self.join(timeout=2)
        self._file.close()

    def lines_between(self, t_from, t_to):
        return [(t, s) for t, s in self.lines if t_from <= t <= t_to]

    def _emit(self, text):
        t = time.time()
        self.lines.append((t, text))
        self._file.write("%.3f\t%s\n" % (t, text))

    def run(self):
        while not self._stop_event.is_set():
            ports = sorted(glob.glob(self.pattern))
            if not ports:
                time.sleep(0.05)
                continue
            try:
                fd = os.open(ports[0], os.O_RDONLY | os.O_NOCTTY | os.O_NONBLOCK)
            except OSError:
                time.sleep(0.05)
                continue
            try:
                attrs = termios.tcgetattr(fd)
                attrs[3] &= ~(termios.ICANON | termios.ECHO)
                termios.tcsetattr(fd, termios.TCSANOW, attrs)
            except termios.error:
                pass
            self._emit("# port open %s" % ports[0])
            pending = b""
            while not self._stop_event.is_set():
                try:
                    ready, _, _ = select.select([fd], [], [], 0.2)
                    if not ready:
                        if not os.path.exists(ports[0]):
                            break
                        continue
                    chunk = os.read(fd, 4096)
                    if not chunk:
                        # macOS reports an idle Teensy port readable with 0 bytes: only a port
                        # that vanished is a disconnect (a real one also raises OSError)
                        if not os.path.exists(ports[0]):
                            break
                        time.sleep(0.05)
                        continue
                except (OSError, ValueError):
                    break
                pending += chunk
                while b"\n" in pending:
                    line, pending = pending.split(b"\n", 1)
                    self._emit(line.decode(errors="replace").rstrip("\r"))
            try:
                os.close(fd)
            except OSError:
                pass
            self._emit("# port closed")
