# 05 - Bench test plan and results

Bench tests on the remote board (Teensy 4.1 + RFM23BP) to confirm the causes in
[01](01-issues-and-root-causes.md) and to validate the firmware in [02](02-firmware-hardening.md).
They implement the diagnostics D1 to D8 from [03](03-diagnostics-to-run.md).

Started 2026-10-26. Only the remote board is on the bench; the robot board shows the same issues.

## Bench

| Item | Setup |
| --- | --- |
| Board | Remote board, 18650 cell removed, Teensy VUSB/VIN pad cut |
| Power | OWON ODP3063 CH3 on the 5 V rail (VCC): 5.0 V, current limit 1.0 A, OCP 2.0 A, OVP 5.5 V. The boost converter REG1 is bypassed (it is back-fed from its output, its input floats behind D2/D3). |
| USB | Connected to the Mac, data only (pad cut): the serial port disappears at every power cycle |
| Antenna | fitted |
| Scope | Rigol DHO914S, 10x probes, grounds on the module GND wire |
| Control | Scripts on the Mac over LAN (supply 10.0.0.10, scope 10.0.0.11), helpers in the `bench-instruments` skill |

Test wires (R0 = RFM23BP footprint, board pad numbering): VCC at the module (pad 11), TX_ON (pad 13),
RX_ON (pad 12), SDN (pad 6), nIRQ (pad 5), SCK (pad 3), MISO (pad 1), MOSI (pad 2), GND (pad 7/14).

| Setup | CH1 | CH2 | CH3 | CH4 | Used by |
| --- | --- | --- | --- | --- | --- |
| A | VCC | TX_ON | RX_ON | SDN | T0 to T7 |
| B | VCC | TX_ON | RX_ON | nIRQ | T8 |
| C | SCK | MISO | MOSI | nIRQ (nSEL if a wire is added on pad 4) | T9 |

## Test cases

Phase 1 runs the firmware already on the board (original remote code, never hardened).
Phase 2 runs `radio-hardening` built for the remote.

| Test | Question | Switches | Diagnostic |
| --- | --- | --- | --- |
| T0 Probe check | Each probe on the right wire, DC levels plausible | nothing | - |
| T1 Normal operation | TX length and rate, VCC during TX, TX_ON/RX_ON states, any time both are low | nothing | D2, D5 |
| T2 One power cycle | TX_ON/RX_ON from power-up (GPIO POR defaults) and during init; init result on serial; boot current | CH3 off 5 s, on (2 to 3 times) | D2, D4 |
| T3 Power-cycle statistics | Init failure rate versus off time (0.2 / 0.5 / 1 / 2 / 5 / 10 s), about 30 cycles | CH3 | D4, D3 |
| T4 Rail decay | VCC fall time to 0 V, with and without USB (back-powering) | CH3, USB unplugged once | D3 |
| T5 Weak supply | Lower current limit (1.0 / 0.6 / 0.4 A), then voltage (5.0 / 4.5 / 4.2 V): when does a TX reset the chip, does the old firmware hang | CH3 setpoints | D5, root cause 1 |
| T6 Heat and stuck init | If init gets stuck: current and temperature in that state; 30 min run with current logged | none or one cycle | D1 |
| T7 Hardened firmware | T1 to T5 again, compared with phase 1 | as above | D8 |
| T8 Fault injection | Debug build with a serial console: forced hang (watchdog), forced radio restart, nIRQ held low | firmware commands | D8 |
| T9 SPI quality | Ringing, edges, glitches on the module side of the TXB0108 | nothing | D6 |

The firmware on the board cannot be read back from a Teensy. Going back to phase 1 after flashing
means rebuilding the remote firmware from the repo.

## How results are recorded

- Every capture is read from the scope's acquisition memory as data (`:WAV:DATA?`, RAW mode),
  never from a screenshot. Saved per capture: raw bytes plus the scope's preamble (scaling),
  and a metadata file (time, scope settings, probe factor, volts per step, supply snapshot,
  firmware). Screenshots are only kept as a visual check.
- Analysis scripts read only saved data and write the result tables; every number in this
  document comes from such a table, with its resolution (volts per step, time per point) and
  sample count.
- Channel ranges are chosen for the question: logic levels at 1 V/div (34 mV per step), VCC zoomed
  at 200 mV/div around 5 V (7 mV per step) for sag measurements.
- Before absolute-voltage tests: scope self-calibration, 10x probe compensation check, one DMM
  reading of VCC as reference.
- T0 and T1 predate this: T1 kept per-capture summaries only, plus the raw CSV of its last
  capture (`t1_last_capture.csv`, 1M points, 80 ns).

## Results

### T0 - Probe check (2026-10-26)

Running normally, no robot board (remote screen shows the robot offline), supply 4.999 V, 0.16 to 0.17 A.

| Signal | Measured | Reading |
| --- | --- | --- |
| VCC | 5.00 V, about +-0.15 V noise | ok |
| TX_ON | 3.3 V | high = PA off. GPIO high level is 3.3 V, not VCC (module runs the chip from an internal 3.3 V supply, presumably) |
| RX_ON | 0.1 V | low = LNA on, receiving |
| SDN | about 0 V | radio enabled |

The supply current (about 160 mA for the whole board) shows no PA running while idle.

### T1 - Normal operation (2026-10-26)

Trigger TX_ON falling at 1.65 V, 5 ms/div, 1M points (80 ns per point), 10 transmissions.
Without the robot board the remote sends a ping at 2 Hz (`pingSendFrequency`), 20 Hz once connected.
All 10 transmissions were identical:

```
RX  ->  both low 158 us  ->  TX 7.43 ms  ->  both low 230 us  ->  RX
```

- **PA enabled without drive for about 390 us per packet**: TX_ON and RX_ON are both low for 158 us
  before and 230 us after each transmission (chip in TUNE / READY while switching). This is the
  state predicted in 01, root cause 2, but here it is part of every RX/TX switch, not idle time.
  Duty cycle 0.08 % at 2 Hz, 0.8 % at 20 Hz. The hardened firmware does not remove it (the
  pins are driven by the chip's TX state / RX state outputs in that window); only the PCB change
  (inverted or pulled-up control) does.
- **Weak levels during TX** (from the raw data of the last capture, edges excluded, median and
  1st to 99th percentile, 68 mV per scope step):

  | Signal | RX (894 241 points) | TX (88 911 points) |
  | --- | --- | --- |
  | TX_ON | 3.277 V (3.209 to 3.345) | 0.614 V (0.478 to 0.683) |
  | RX_ON | -0.068 V (-0.205 to 0.000) | 2.185 V (2.048 to 2.253) |

  The module's control inputs seem to load the pins in TX. RadioHead
  writes the GPIO configuration as 0x12 / 0x15, i.e. the lowest drive strength (bits 7:6 = 00).
  Firmware experiment to add: highest drive strength, then measure again (check the Si4432
  register description first).
- **VCC during TX** (stiff bench supply, boost converter bypassed): median 4.983 V in RX, 4.915 V in
  TX, minimum 4.64 to 4.71 V at TX start. **Not resolved**: at 2 V/div one scope step is 68 mV, so
  the median drop is one step and the minimum 4 to 5 steps. Repeat with VCC at 200 mV/div around
  5 V (about 7 mV per step) before quoting a sag. Absolute values carry the uncalibrated scope
  offset (about -0.08 V, see the bench-instruments skill).
- **TX length 7.43 ms** (03 estimated about 13 ms for an 8 byte frame).
- **SDN levels invalid in T0 to T2 (and T3)**: the CH4 probe switch was on 1x while the scope was
  set to 10x (confirmed on the probe 2026-10-26), so SDN read 10 times too high: the 5 V pulse
  clipped above 6.8 V for its whole 10 ms in T2, and the "spikes up to 1.5 V during TX" are
  about 0.15 V. SDN timing and the shutdown detection (2.5 V threshold) are not affected.
  Probe set to 10x afterwards; the analysis now flags clipped points.
- Capture intervals (2 to 3 s) are set by the readout time, not by the remote.

### T2 - Power cycles, watched closely (2026-10-26)

Run `bench/data/2026-10-26/t2_171207/` (raw captures, serial log, supply log; `bench/analyze_t2.py`).
4 cycles, supply CH3 off 5 s then on. Cycles 0 and 2 trigger on VCC rising (2.5 V), cycles 1 and 3
on SDN rising (the init pulse), armed 0.8 s after power-on. 1M points, 400 ns per point,
window -50 .. +350 ms around the trigger, 34 mV per step. Original firmware.
Both captures of each kind gave the same timeline to within 0.1 ms.

- **All 4 inits succeeded** (no "Failed radio init" on serial). Supply current: 0.13 to 0.18 A at
  every 0.5 s reading, settling at 0.157 A. Serial port back 0.56 to 0.65 s after power-on, first
  output at 1.45 s.
- **Power-up** (t = 0 at VCC 2.5 V rising; VCC above 0.5 V at -13 ms, above 4.5 V at +32 ms, the
  supply's own ramp). SDN stayed low: the radio is not held in shutdown at power-up with this
  firmware and runs its power-on reset while VCC ramps.

  | From (ms) | Length (ms) | TX_ON / RX_ON | Meaning |
  | --- | --- | --- | --- |
  | -50 | 45.5 | both low | VCC still below about 1 V, lines not driven |
  | -4.5 | 15.3 | low / high | **PA enabled, no drive**: chip in power-on reset, VCC ramping |
  | +10.9 | 339 (to window end) | high / low | GPIO power-on defaults after reset: PA off, LNA on |

- **Radio init** (t = 0 at SDN rising):

  | From (ms) | Length (ms) | TX_ON / RX_ON | Meaning |
  | --- | --- | --- | --- |
  | 0 | 10.0 | (shutdown) | SDN pulse from `Radio::initialize()` |
  | 10.0 | 0.33 | high / low | |
  | 10.3 | 19.7 | low / high | **PA enabled, no drive**: power-on reset after SDN |
  | 30.1 | 92.2 | high / low | power-on defaults, PA off (RadioHead's blind 100 ms delay) |
  | 122.2 | 0.22 + 0.32 | both low (twice) | **PA enabled, no drive**: GPIO configuration being written |
  | 122.9 | 22.1 | high / low | RX (the firmware enters RX right after init) |
  | 145.1 | 0.16 / 7.43 / 0.23 | both low / TX / both low | first ping |
  | 152.9 | 197 (to window end) | high / low | RX |

- **Answer to D2 (GPIO power-on polarity)**: during the chip's power-on reset the default GPIO
  functions give TX_ON = 0, RX_ON = 1, i.e. **PA enabled without drive**, for about 15 ms at power-up
  and about 20 ms after every SDN release (so on every init attempt). After the reset the defaults
  give TX_ON = 1, RX_ON = 0 (PA off), which is safe until the firmware reconfigures the pins.
- **CORRECTED by T3 (probe error)**: the CH4 probe was on 1x in T2; its 1 MOhm load held SDN low
  during boot. With a 10x probe SDN floats high for about 319 ms at every power-up and TX_ON stays
  low meanwhile (see T3). The exposure estimate below is therefore wrong.
- ~~Exposure is small with this firmware on a stiff supply~~ (superseded): about 15 + 20 + 1 ms per boot and
  0.39 ms per packet. The original firmware does not leave the chip idle after init (it enters RX
  within 0.5 ms of writing the GPIO configuration). Even 100 failing init attempts add about 2 s.
  The PA-overheating explanation for the persistent init failures (01, root cause 2) is **not
  supported** by normal operation; T3 to T6 need to reproduce a failure and measure it.
- **Consequences for the fixes**:
  - The hardened firmware cannot remove the power-on-reset window (the chip drives the pins from
    its own reset logic before any SPI access). It does remove the two short both-low windows.
  - **04 recommendation 1 needs revising**: pull-ups do not help during the power-on reset, because
    the GPIO actively drives TX_ON low. Inverters would make it worse: after the reset the defaults
    (TX_ON = 1, RX_ON = 0) would become PA on until the firmware configures the pins. Only
    controlling TX_ON / RX_ON from Teensy pins (04's alternative) keeps the PA off in every state.
- Serial logger artefact (fixed after the run): the logger closed and reopened an idle port every
  0.5 s; no reset of the board was involved (no boot output followed, supply current unchanged).

### T3 - Power-cycle statistics (2026-10-26)

Run `bench/data/2026-10-26/t3_171820/` (`bench/t3_power_stats.py --repeat 5 --seed 1`). 30 power cycles,
off times 0.2 / 0.5 / 1 / 2 / 5 / 10 s, 5 each, shuffled. Original firmware.

- **30 of 30 boots OK, no failed init attempt at any off time** (0.2 s included; every cycle
  printed its boot output 1.40 to 1.47 s after power-on, so even 0.2 s off did reset the board).
  Every boot started pinging. Idle current 0.11 to 0.21 A (one 0.000 A reading in cycle 8: stale readback).
  Power cycling alone on a stiff 5 V supply does not reproduce the init failure.
- **Probe switched from 1x to 10x between cycles 22 and 23.** Cycles 0 to 22: SDN levels invalid and
  the scope sometimes triggered at power-up instead of the init pulse (10 times too sensitive). The
  init timelines of all valid captures match T2 to within 0.1 ms.
- **SDN floats high during boot** (cycles 23, 24, probe 10x; t = 0 at SDN rising; medians, 1 ms
  trimmed at each edge, 34 mV per step):

  | From (ms) | Length (ms) | State | VCC | TX_ON | RX_ON | SDN |
  | --- | --- | --- | --- | --- | --- | --- |
  | -50.0 | 18.5 | VCC ramping | 0.27 | 0.21 | -0.10 | -0.07 |
  | -31.5 | 14.6 | chip power-on reset: TX_ON low | 2.66 | 0.34 | 2.63 | -0.07 |
  | -16.9 | 16.9 | power-on defaults (PA off) | 3.89 | 3.28 | -0.07 | -0.07 |
  | 0.0 | 318.6 | **shutdown (SDN floating high)** | 4.95 | **0.72** | **-0.03** | 3.96 |
  | 319.1 | 19.7 | power-on reset after SDN low (constructor) | 4.95 | 0.38 | 3.28 | -0.03 |
  | 338.8 | 11.2+ | power-on defaults (PA off) | 4.98 | 3.28 | -0.07 | -0.07 |

  SDN is not driven while the Teensy boots (its pins float for about 300 ms, 01 contributing
  factor): with a 10x probe it floats to about 3.96 V, with a 1x probe (1 MOhm) it stays low. The
  radio's shutdown during boot therefore depends on loading, i.e. it is undefined.
- **In shutdown the control lines are not driven and sit low: TX_ON 0.72 V, RX_ON about 0 V.**
  TX_ON is close to its level while transmitting (0.61 V), about a transistor base-emitter voltage.
  If the module reads that as low, **the PA is enabled without drive during the whole shutdown**:
  about 319 ms at every power-up and 10 ms at every init pulse. Per boot the PA-enabled time is then
  about 15 + 319 + 20 + 10 + 20 = 384 ms, not 36 ms as estimated from T2.
  **Not yet proven by a current measurement**: the supply readback reads 0 A for the first 1.7 s
  after switching on.
- **Consequence for the hardened firmware (02)**: it holds the radio in shutdown on purpose (startup
  hook from early boot, after every fault, between init retries with pauses up to 2 s). If shutdown
  enables the PA, that makes the exposure longer, not shorter. **Do not flash it before the PA
  current in shutdown is measured.** The same applies to 04 recommendation 3 (SDN pull-up).

### T6a - Teensy halted in its bootloader (2026-10-26)

Run `bench/data/2026-10-26/t6a_173052/` (`bench/t6_bootloader_shutdown.py`). Goal: hold the
"pins floating" state of the first 300 ms of boot long enough to measure the current (PROGRAM
button pressed, current limit 0.5 A, 5.8 s in the bootloader, then power cycle).

| State | Supply current | SDN | TX_ON | RX_ON |
| --- | --- | --- | --- | --- |
| Normal operation (20 readings) | 0.157 to 0.163 A, single 0.214 / 0.246 A (pings) | -0.03 V | 3.32 V | 0.15 V |
| Teensy in bootloader (11 readings) | 0.116 A steady (first reading 0.204 A) | -0.03 V | 3.31 V | -0.03 V |

- **Not the intended state**: in the bootloader SDN floated **low**, so the radio stayed configured
  in RX (TX_ON high, RX_ON low). The PA current in shutdown is still unmeasured.
- Confirms that SDN is undefined while the Teensy does not drive it: high for 319 ms during a
  power-up (T3), low in the bootloader.
- Board booted normally afterwards (no failed attempt, pinging).

### T5 - Weak supply (2026-10-26)

Run `bench/data/2026-10-26/t5_173320/` (`bench/t5_weak_supply.py --watch 20`). At each level: power cycle,
boot, one capture of the first transmission (2 ms/div, VCC at 1 V/div = 34 mV per step), then 20 s
of ping checks (TX_ON falling within 2.5 s). Original firmware. All probes 10x.

  | Supply setting | Boot | Pings | VCC before TX (median) | VCC in TX (median) | VCC in TX (min) |
  | --- | --- | --- | --- | --- | --- |
  | 5.0 V / 1.00 A | OK | 25/25 | 4.983 | 4.949 | 4.676 |
  | 5.0 V / 0.50 A | OK | 24/24 | 4.983 | 4.949 | 4.676 |
  | 5.0 V / 0.35 A | OK | 24/24 | 4.983 | 4.949 | 4.642 |
  | 5.0 V / 0.25 A | OK | 24/24 | 4.983 | 4.949 | 4.676 |
  | 5.0 V / 0.20 A | OK | 24/24 | 4.983 | 4.949 | 4.642 |
  | 4.5 V / 1.00 A | OK | 26/26 | 4.471 | 4.471 | 4.164 |
  | 4.2 V / 1.00 A | OK | 26/26 | 4.198 | 4.164 | 3.891 |
  | 4.0 V / 1.00 A | OK | 26/26 | 3.994 | 3.959 | 3.720 |
  | 3.8 V / 1.00 A | OK | 25/25 | 3.789 | 3.755 | 3.550 |
  | 3.6 V / 1.00 A | OK | 25/25 | 3.584 | 3.550 | 3.345 |

- **No failure at any level**: every boot without a failed attempt, pings never stopped, no reboot,
  down to 3.6 V and down to a 0.2 A current limit.
- **Voltage sweep valid** (from the scope): the board works from 3.6 V; the dip at TX start is
  0.24 to 0.31 V at every voltage, the median during the packet 0.03 V lower than before it.
- **Current-limit sweep inconclusive**: the limits were applied (setpoint readback), but VCC during
  TX was the same at 0.2 A as at 1.0 A. The supply's output capacitance most likely carries the
  8 ms bursts before its current limit acts, so this does not emulate a weak boost converter.
  The TX current itself is still unmeasured: needs a shunt in the 5 V feed and the scope.
- **Supply readback frozen for the whole run** (4.999 V / 0.211 A at every reading, even at a 3.6 V
  setpoint and 0.07 s after power-on), the second freeze of the day; it froze between T6a and T5.
  No supply current data in T5. The scripts now cross-check the supply voltage against the scope's
  VCC average and stop when they disagree by more than 0.3 V.
- The TX length here (7.82 ms, first to last TX_ON sample below 1.65 V) uses a different definition
  from T1 (7.43 ms, TX state only): the difference is the two both-low windows (0.16 + 0.23 ms), in
  which TX_ON is already / still low.
