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

The results below do not follow this numbering from T6 on: the heat tests became T6a to T6e, the race
reproduction T7a, the fixed firmware T8 (plan T7) and the fault injection T9 (plan T8). Plan T4 was run as
T4a; plan T9 (SPI quality) was covered by the SPI captures of T6e and T8.

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

### T6b - Current with the radio on and in shutdown (2026-10-26)

Run `bench/data/2026-10-26/t6b_174752/` (`bench/t6b_sdn_current.py`), firmware `bench/firmware/sdn_test`
(branch `radio-bench`; SDN switched on serial commands, display held in reset, nothing else
running). Current limit 0.5 A, shutdown held 3.2 s once (the second shutdown was skipped because
the first already answered the question). Settled readings = readings 1.2 s or more after the
command (the readback lags about 0.7 s). Readback cross-checked against the scope before the run.

| Phase | Supply current (settled) | TX_ON | RX_ON | SDN | Device type |
| --- | --- | --- | --- | --- | --- |
| On (SDN low) | 0.144 A | 3.31 V | -0.03 V | -0.03 V | 0x08 |
| **Shutdown (SDN high)** | **0.298 A** (0.288 to 0.299) | **0.725 V** | -0.04 V | 3.98 V | 0x00 |
| On again | 0.126 A | 3.32 V | -0.04 V | -0.03 V | 0x08 |

- **Confirmed: the PA is on in shutdown.** The board draws 0.154 A (about 0.77 W) more with the
  radio in shutdown than with it on; the chip itself draws almost nothing in shutdown, and TX_ON
  sits at 0.725 V. This answers the open D2 item "PA state while in shutdown".
- Combined with T3: every power-up holds the PA on for about 319 ms while SDN floats high during
  the Teensy boot, plus about 15 + 20 ms in the chip's power-on resets.
- **The hardened firmware on `radio-hardening` must not be flashed as is**: its parking puts the
  radio in shutdown (startup hook, faults, between retries), which enables the PA. The safe idle
  state is SDN low with the GPIOs parked high (TX_ON 3.3 V, 0.13 to 0.14 A for the board).
  The startup hook must drive SDN **low**; the PCB needs a pull-down on SDN (not the pull-up of
  04 recommendation 3) and PA control that is off when the chip is in shutdown or reset.
- Open: the on current was 18 mA lower after the shutdown than before (0.126 vs 0.144 A).
- The display was blank during this test (sketch holds it in reset); board powered off afterwards.

### T4a - Brown-out hold (2026-10-26)

Run `bench/data/2026-10-26/t4a_175817/` (`bench/t4a_brownout_hold.py --hold 20`). Baseline firmware
reflashed first (`bench/data/firmware/remote_baseline_eff32f8.hex`, built from eff32f8).
Hypothesis: after switch-off the rail lingers at an intermediate voltage, so a power cycle is no
real reset. From normal operation the 5 V rail was held at V_hold for 20 s, then set back to 5 V.

| V_hold (scope) | Board current in the hold | Result |
| --- | --- | --- |
| 2.99 V | 58 mA | Teensy reset, init OK, pinging |
| 2.49 V | 55 mA | Teensy reset, init OK, pinging |
| 1.99 V | 42 mA | Teensy reset, init OK, pinging |
| 1.49 V | 29 mA | Teensy reset, init OK, pinging |
| 0.98 V | 1 mA | Teensy reset, init OK, pinging |
| 0.48 V | 0 mA | Teensy reset, init OK, pinging |

- **Not reproduced**: no failed init attempt after any hold. With the original firmware (SDN pulse
  before every init) a rail that does not reach 0 V does not prevent the radio from starting.
- The board stops drawing current below about 1 V: after a real switch-off the rail falls fast to
  about 1 V and may then decay slowly (T4b, real power path, still to measure).
- Summary of the baseline so far: 55 boots (T2, T3, T5, T6a, T4a), no failed init attempt.

### T6c - Init under heat: FAILURE REPRODUCED (2026-10-26)

Run `bench/data/2026-10-26/t6c_180635/` (`bench/t6c_heat.py`), firmware `env:remote_diag` (branch
`radio-bench`: the original remote firmware plus, behind `RADIO_BENCH_DIAG`, the radio
temperature once a second and the chip's answer after each failed init attempt). The owner heated
the module with hot air (100 C, later 160 C, low airflow), no thermocouple. A power cycle
(1 s off) every 12 s. Radio temperature from the chip's own sensor, RadioHead default range
(-64 to +64 C, 0.5 C per step, uncalibrated, **saturates at 63.5 C**).

| Cycles | Time (min) | Radio temperature | Result |
| --- | --- | --- | --- |
| #000 to #052 | 0.07 to 9.82 | 50.5, up to the 63.5 ceiling (5.0 to 7.8 min), cooling to 49.0 | 53 boots OK, no failed attempt |
| **#053, #054, #055** | 10.00 to 10.37 | cooling, last reading before 49.0 C | **all 10 init attempts failed, three boots in a row** |
| #056 | 10.55 | 45.5 C | OK again, 33 s after the first failure |

- **First reproduction of the persistent init failure.** It happened **while cooling, at about 45 to
  49 C**, not at the peak, persisted over three power cycles, and cleared by itself after about
  30 s of further cooling: the field pattern ("stays failed across reboots, minutes later fine").
- **Signature** (identical in the three failed boots, read after the 10th attempt):
  `device_type=0x08 status1=0x20 status2=0x00`.
  - Device type 0x08: SPI, level shifter and the chip's digital part work.
  - Interrupt status 2 = 0x00: **the chip ready flag (ICHIPRDY) is not set**. RadioHead's init (with
    the earlier local edit) waits up to 200 ms for it after the software reset and fails without it.
    The flag reports that the 30 MHz crystal oscillator is running and stable.
  - Status 1 = 0x20 (TX FIFO almost empty) is the normal idle value.
- **Working hypothesis: at some temperatures the module's crystal oscillator does not start, or not
  within 200 ms.** One event (three boots) so far: to be repeated and confirmed directly.
- Next: wire to the RFM23BP GPIO_2 (R0 pad 8, unconnected on the PCB; it outputs the crystal-derived
  microcontroller clock, 1 MHz, after reset) on scope CH4, temperature sensor on a wider range,
  chip-ready wait time per attempt, crystal load capacitance register 0x09; same heating profile.
- Consequences to check for the fix: the chip-ready wait (200 ms here, 100 ms on `radio-hardening`),
  retry policy, crystal load capacitance setting.

### T6d - Init under heat with the crystal clock on GPIO_2 (2026-10-26)

Run `bench/data/2026-10-26/t6d_182545/` (`bench/t6d_heat_gpio2.py`), firmware `env:remote_diag`
extended (temperature -64 to +192 C at 1 C per step; per attempt the chip ready time after the SDN
pulse; after a failed attempt all status and crystal registers, then an own software reset with up to
1 s for chip ready). CH4 on the RFM23BP GPIO_2 (R0 pad 8), which outputs the crystal-derived
microcontroller clock after every reset.

- **Failure reproduced again** (cycle #017, radio temperature 74 to 85 C while heating, all 10
  attempts), recovered 11.2 s later at 74 C.
- **The crystal runs** (GPIO_2 clock active 95 % of the failed attempt's capture, gap only during the
  next SDN pulse), the chip answers (device type 0x08), and **an own single software reset after each
  failed attempt gets chip ready in 604 us**. So the chip is fine; RadioHead's sequence fails.
- RadioHead's init (framework copy): `RHSPIDriver::init()` with a blind 100 ms delay, then
  `reset()` (software reset + 1 ms), device type read, software reset #2, then polls interrupt
  status 2 back to back until ICHIPRDY, giving up after 200 ms (local edit).
- Crystal load capacitance register 0x09 = 0x7F (chip default), register 0x62 = 0x24.

### T6e - Init under heat with SPI and nIRQ on the scope: ROOT CAUSE (2026-10-26)

Run `bench/data/2026-10-26/t6e_184907/` (`bench/t6e_heat_spi.py`, decoder `bench/lib/spi_decode.py`),
same firmware. Setup E: CH1 SCK, CH2 MISO (first wired to MOSI by mistake, which validated the
decoder: it decoded exactly RadioHead's command sequence 87 80 / 00 00 / 87 80 / 04 00...), CH3 nIRQ,
CH4 SDN, all on the module side. One capture per boot of RadioHead's init (32 ns per point,
109 to 141 ms after SDN falls). SPI runs at 1 MHz: 16 clocks per register access, 1.5 us between
the bytes, about 1.9 us between accesses, one status poll every 17.4 us. The scope's own SPI
decoder (`:BUS1`, set up by `bench/t6e_decode_check.py`) decodes only the on-screen part and
did not match the code sequence; not used.

| Boot | Radio temp | nIRQ falls (chip ready) after reset #2 | ... at this point of a 17.4 us poll read | Result |
| --- | --- | --- | --- | --- |
| #0 | 30 C | 607.0 us | 16.7 us | OK (next poll reads 0x02) |
| #1 | 56 C | 604.0 us | 13.7 us | OK |
| #2 | 70 C | 588.7 us | 15.7 us | OK |
| #3 | 82 C | 581.7 us | 8.7 us | OK |
| **#4** | hotter | **577.6 us** | **4.6 us** | **FAILED, 10 attempts** |
| **#5** | hotter | **578.1 us** | **5.2 us** | **FAILED, 10 attempts** |
| #6 | 66 C (cooling) | 583.3 us | 10.4 us | OK again, 29.5 s after the first failure |

- **Root cause of the persistent init failure: a race in RadioHead's chip-ready polling.** Interrupt
  status 2 is clear-on-read. The chip snapshots the value during the address byte and clears the
  flags at the start of the data byte (nIRQ rises about 8.5 us into the access). When chip ready is
  set between the two (boots #4 and #5: nIRQ low for only 3.4 to 3.9 us, inside one poll), the read
  returns 0x00 **and** clears the flag. No second chip ready event comes, the loop times out after
  200 ms, `init()` returns false. In the failed boots all 1719 polls read 0x00 (one 0x01).
- **Why it persists and then goes away**: the loop timing is deterministic, so at a given temperature
  every attempt and every boot hits the same phase. The chip ready time shifts by about -0.5 us
  per C (607 us at 30 C, 578 us when failing): the board works again once the module has warmed or
  cooled by a few degrees. With a 17.4 us poll period a bad band recurs about every 35 C, and each
  module has its own offset, so the failing temperature differs between boards (first reproduction
  T6c at about 45 to 49 C on the old scale; here above about 85 C).
- **Not a hardware fault**: SPI, TXB0108, crystal and chip all work; the chip reports ready every time.
- **Fix (firmware)**: do not catch a clear-on-read flag with back-to-back reads. Either wait for the
  nIRQ pin (Teensy pin 9) to go low after the software reset and then read the status once, or wait a
  fixed time (for example 2 ms) and read once; one software reset instead of two. On
  `radio-hardening` the fork polls every 200 us, which makes the race rarer but does not remove it.

### T7a - Race induced at room temperature, fix verified (2026-10-26)

Run `bench/data/2026-10-26/t7a_185621/` (`bench/t7a_race_sweep.py`), firmware `bench/firmware/race_sweep`
(flashed on the remote; the display is blank while it runs). Each trial repeats RadioHead's sequence
with RadioHead's own `spiRead`/`spiWrite` (software reset #1, 1 ms, device type read, software
reset #2), then waits an added delay d (CPU cycle counter) before the chip ready check. nIRQ is
recorded by a pin interrupt. d swept 0 to 20 us in 250 ns steps, 20 trials per step, at room
temperature, no heat.

| d | RadioHead method (back-to-back polls, 5 ms timeout) | Fix: wait 2 ms, read once |
| --- | --- | --- |
| 0 to 9.25 us | 0 / 20 failed at every step | 0 failed |
| 9.5 us | 9 / 20 failed | 0 failed |
| **9.75 to 11.5 us** | **20 / 20 failed at every step** | 0 failed |
| 11.75 us | 15 / 20 failed | 0 failed |
| 12 to 20 us | 0 / 20 failed at every step | 0 failed |

- **The failure is induced at will by the poll phase alone**: one contiguous band about 2 us wide in
  the 17.4 us poll period, 100 % failure inside, 0 % outside. RadioHead method: 184 of 1620 trials
  failed, all inside the band; nIRQ showed chip ready in all 1620 trials (about 0.6 ms after reset #2).
  Failed trials made about 342 reads in the 5.95 ms timeout, i.e. one read every 17.4 us as in T6e.
- **The fix "wait 2 ms after the reset, then read status 2 once" never failed**: 0 of 1620 trials, at
  every phase. (Its trials record no nIRQ edge because nIRQ stays low from reset #1: nothing reads the
  status during the wait. The single read found chip ready every time.)
- This confirms the T6e mechanism independently of temperature; heat only shifts the chip ready time
  (about -0.5 us per C) and so the phase. Room temperature on this module is about 4 to 5 us away
  from the band, i.e. roughly 8 to 10 C.
- Next (done in T8): the fix in the real init path, verified with a heat run; then the stall.

### T8 - Fixes in the real driver: init, stall, heat (2026-10-26)

RadioHead now lives in the repo as `lib/RadioHead_RFM23BP` (the framework copy of this Mac, with its
earlier local edits, unchanged), and `platformio.ini` ignores the framework's RadioHead, so the build
no longer depends on a machine's framework folder. Changes, marked "RFM23BP fork" (see its README):

- `init()`: one software reset, `delay(2)`, one read of interrupt status 2 for chip ready, then the
  device type check. No back-to-back polls.
- `send()`: waits for the previous packet at most `RH_RF22_TX_TIMEOUT_MS` (30 ms; a packet takes about
  7 ms) instead of forever; a timeout returns false and counts in `txTimeouts()`. `Radio::send` then
  re-initializes the radio (`initialize(1)`, keeps the channel). `Radio::initialize` detaches the
  interrupt first: on a re-init RadioHead's handler would otherwise clear the chip ready flag first.
- `adcRead()` (radio temperature) gives up after 5 ms instead of waiting forever for a chip that does
  not answer.

Bench hook (diag builds only): serial `k` pulls SDN high for 1 ms right after the next send, so the
chip resets during a transmit and the packet sent interrupt never comes. `env:remote_diag_stock` is
the same diag firmware with the framework's unmodified RadioHead. Script `bench/t8_stall.py`,
setup E.

| Test | Firmware | Run | Result |
| --- | --- | --- | --- |
| Chip reset during transmit, 1 trial | `remote_diag_stock` | `t8_stall_stock_191621` | **stalled**: no output from 0.6 s after the pulse (the next send), display frozen; 0.158 A |
| Chip reset during transmit, 20 trials | `remote_diag` | `t8_stall_fixed_191654` | **20 / 20 recovered**: "re-init ok" 0.65 s after each pulse (next send + 30 ms + init), firmware kept running |
| nIRQ activity before and after a recovery | `remote_diag` | `t8b_tx_after_recovery_*` | same interrupt pattern before and after (3 of 3 triggers within 2 s each, 10 us low per interrupt): transmitting resumed |
| Power cycles, room temperature | `remote_diag` | `t6e_191942` | **61 / 61 OK** on the first attempt, 29 to 31 C |
| Power cycles under heat | `remote_diag` | `t6e_193035` | **127 / 127 OK** on the first attempt, 30 to 99 C: 19 boots at 82 C or more, 17 at 85 C or more, 7 at 90 C or more |

- SPI per init (capture 109 to 141 ms after SDN falls): software reset at 110.09 ms, the single read
  of status 2 at 112.10 ms returns 0x03 (chip ready and power-on reset), device type 0x08 right after.
  nIRQ is low from before the window until that read clears it: chip ready is latched, so the phase
  of the read no longer matters.
- In T6e the old code failed on this module at the boots above about 82 C; with the fix, every boot
  through 99 C passed.
- Not covered yet: the robot (receiver) board, which uses the same `Radio.h` and driver
  (`env:teensy41` builds); a chip reset while not transmitting still leaves the chip at its default
  registers until the next send times out (no timeout without a send); the PA-on-in-shutdown and
  SDN boot float findings (T6a, T6b) are unchanged.

### T9 - Recovery after any chip reset, SDN driven from early boot (2026-10-26)

T8 left one gap: the robot board only sends in reply to a received packet, so a chip reset while it is
receiving (nearly always) would leave it on its default frequency and modem settings, never hearing the
remote again, and no send could time out. The link would stay down until a power cycle.

Changes (`env:remote_diag`, both boards build):

- Driver: the interrupt handler counts power-on reset interrupts in `chipResets()` (a chip reset sets
  the interrupt enables back to their defaults, which include power-on reset).
- `Radio::update()`, called every loop on both boards: re-init when `chipResets()` changed, and once a
  second when the frequency band register (0x75) differs from what `setChannel()` wrote. A re-init
  blocks for about 140 ms; while a chip stays dead it is retried every 2 s, not every loop.
  Also once a second: re-init when nIRQ is low and still low 200 us later. nIRQ is edge triggered, so
  a flag pending when the interrupt is attached, or a lost edge, would otherwise silence the radio for
  good with its configuration intact (01, root cause 1).
- `startup_middle_hook` in `main.cpp` drives SDN (pin 8 on both boards) low in the Teensy startup code,
  before its 300 ms USB wait.
- Bench hooks: serial `i` pulses SDN for 1 ms at once (chip receiving), `f` overwrites register 0x75
  with its default (a reset the interrupt missed), `n` detaches the nIRQ interrupt (the next event
  leaves nIRQ low, as a lost edge would). Script `bench/t8_stall.py --command i|f|k|n`; SDN at
  power-up `bench/t9_boot_sdn.py`.

| Test | Run | Result |
| --- | --- | --- |
| Chip reset while receiving (`i`), 10 trials | `t8_stall_fixed_i_200924` | **10 / 10 recovered**, re-init done 0.14 s after the pulse; 9 found by the interrupt, 1 by the register check |
| Configuration lost (`f`), 10 trials | `t8_stall_fixed_f_201022` | **10 / 10 recovered** by the register check, 0.2 to 1.0 s later (once a second) |
| Chip reset during a transmit (`k`), 10 trials | `t8_stall_fixed_k_201108` | **10 / 10 recovered**, 0.14 s after the pulse, before the next send: 8 by the interrupt, 2 by the register check |
| nIRQ left low (`n`), 10 trials | `t8_stall_fixed_n_202232` | **10 / 10 recovered**: 6 by the stuck nIRQ check (0.24 to 0.85 s), 4 by the remote's send timeout first; on the robot only the check applies |
| `i`, `f`, `k` again with the nIRQ check, 5 trials each | `t8_stall_fixed_{i,f,k}_2023*` | **15 / 15 recovered** |
| SDN at power-up, 5 power cycles | `t9b_boot_sdn_201224` | first SDN high is the 10.0 ms init pulse in all 5; the 319 ms float of T3 is gone |
| Power cycles, room temperature, 20 | `t6e_201322` | radio `init_ok try=1` in **20 / 20** |

- The interrupt alone missed 3 of 20 SDN pulses (found by the register check within a second); the
  check is needed, not only a backup.
- With the float gone, the PA-on time per boot drops from about 384 ms to the 10 ms init pulse plus the
  chip's own power-on resets (about 15 + 20 ms, T3).
- In `t6e_201322` boots 2 to 19 stopped in the remote's startup check for centred joysticks (left X raw
  0.589 instead of 0.007; cause not checked yet: joystick moved or something resting on it?). Not radio related; the script
  classifies those boots OK because it looks for the joystick lines.
