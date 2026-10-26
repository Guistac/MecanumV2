# Radio reliability (RFM23BP)

Teensy 4.1 + RFM23BP (Si4432) behind a TXB0108 level shifter, the same circuit on the remote and the
robot (receiver) board. Two field problems, seen on several boards:

1. The radio sometimes would not initialize, even across many power cycles, until it had been left
   alone for minutes.
2. The firmware sometimes stalled.

Both are firmware bugs in RadioHead and are fixed on branch `radio-bench` (2026-10-26), verified on the
remote board on the bench. Branch `mavlink` is still the deployed firmware; `radio-hardening` is an earlier,
untested attempt that must not be flashed as is (see [02](02-firmware-hardening.md)).

## Root causes

| Problem | Cause | Evidence |
| --- | --- | --- |
| Init fails until minutes later | RadioHead polls the chip ready flag back to back. Interrupt status 2 is clear-on-read: the chip latches the value during the address byte and clears it during the data byte, so a flag set between the two is lost and init times out. The chip ready time drifts with temperature (about -0.5 us per C), so at a given temperature every attempt hits the same phase; the board works again once the module has drifted a few degrees. | 05 T6c to T6e (reproduced under heat, SPI and nIRQ captured), T7a (induced at room temperature by shifting the poll phase: 100 % failure in a 2 us band of the 17.4 us poll period, 0 % elsewhere) |
| Stall | `RH_RF22::send()` waits forever for the previous packet. When the chip resets during a transmit (brown-out, SDN glitch), "packet sent" never comes. `adcRead()` (temperature) had the same kind of wait. | 05 T8 (reproduced with the stock driver) |
| Link dead until a power cycle (robot) | A chip reset leaves the chip on its default frequency and modem settings, and a lost nIRQ edge silences it. The robot only sends in reply to the remote, so nothing ever times out. Found in code, not seen in the field as such. | 05 T9 (induced: reset while receiving, lost configuration, nIRQ left low) |

Not the cause: overheating, the supply (55 boots through power cycles, brown-out holds and low voltage on a
stiff bench supply never failed, T2 to T5), SPI or the TXB0108 (decoded clean in every capture).

## What changed (branch `radio-bench`)

- `lib/RadioHead_RFM23BP`: RadioHead 1.112 in the repo; `platformio.ini` ignores the framework's copy,
  whose local edits had lived outside version control. Changes are marked "RFM23BP fork" ([README](../../lib/RadioHead_RFM23BP/README.md)):
  init with one software reset, 2 ms wait and a single chip ready read; `send()` waits at most 30 ms;
  `adcRead()` at most 5 ms; power-on resets counted in the interrupt handler.
- `src/Hardware/Radio.h`: re-init after a send timeout, and from `Radio::update()` (every loop, both boards)
  after a chip reset interrupt, a changed frequency band register or nIRQ stuck low (checked once a second);
  the channel is kept. A re-init blocks for about 140 ms, retried every 2 s while the chip stays dead.
- `src/main.cpp`: `startup_middle_hook` drives SDN low a few ms after reset; it used to float high for
  about 319 ms at every power-up, with the PA on (see below).

| Verification (remote board, 05 T8 and T9) | Result |
| --- | --- |
| Stock driver, chip reset during a transmit | stalled |
| Fixed, chip reset during a transmit / while receiving / configuration lost / nIRQ left low | 20/20, 15/15, 15/15, 10/10 recovered |
| Power cycles at room temperature | 61/61 and 20/20 OK on the first attempt |
| Power cycles under heat, 30 to 99 C (old code failed above about 82 C on this module) | 127/127 OK on the first attempt |
| SDN at power-up | first high is the 10 ms init pulse, float gone |

The robot board was not run with the fix; it has the same circuit and the same `Radio.h` and driver,
and `env:teensy41` builds.

## Hardware findings (not fixed by firmware)

- **In shutdown the PA is on**: TX_ON sits at 0.72 V and the board draws 154 mA more (05 T6b). Firmware
  must never park the radio in shutdown. Since the SDN hook, the PA-on time per boot is about 45 ms
  (init pulse plus the chip's own power-on resets) instead of about 384 ms.
- The PA is on without RF drive for about 390 us around every packet (T1), and TX_ON/RX_ON levels are
  weak during transmit (0.61 / 2.19 V).
- SDN is undefined whenever the Teensy does not drive it (bootloader: low, boot before the hook: high).
- PCB recommendations in [04](04-pcb-revision-recommendations.md), with the SDN pull-up corrected to a pull-down.

## Open

- Deploying: merge into `mavlink`, flash the robot board(s) and the remotes.
- From 01/02, not done: right joystick Y on pin 13 (SPI SCK; the PCB uses A13/D27 according to 01),
  robot continuing with unconfigured motion control after a failed radio init, watchdog.
- Not measured: the remote's boost converter under transmit current (brown-out as the trigger for chip
  resets in the field is still a hypothesis).

## Build and flash

| Environment | Board | Use |
| --- | --- | --- |
| `teensy41` (default) | robot | normal firmware |
| `remote` | remote | normal firmware |
| `remote_diag` | remote | bench: radio temperature and init details on USB serial, fault injection over serial (`k`, `i`, `f`, `n`, see 05 T8/T9) |
| `remote_diag_stock` | remote | bench: as `remote_diag` with the framework's unmodified RadioHead, shows the stall |

`pio run -e <env> -t upload`. On the remote the VUSB/VIN pad is cut: it must be powered from its cell
(or the 5 V rail) while flashing over USB.

## Documents

| Document | Contents |
| --- | --- |
| [01 - Issues and root causes](01-issues-and-root-causes.md) | first analysis from the code and datasheets (status note at the top) |
| [02 - Firmware hardening](02-firmware-hardening.md) | the `radio-hardening` branch (status note: not to be flashed as is) |
| [03 - Diagnostics to run](03-diagnostics-to-run.md) | the measurement plan, mapped to the tests in 05 |
| [04 - PCB revision recommendations](04-pcb-revision-recommendations.md) | hardware changes for the next revision (status note: SDN correction) |
| [05 - Bench test plan and results](05-bench-test-plan.md) | bench setup, every test with its measured tables, T0 to T9 |

Bench scripts are in `bench/` (instrument drivers in `bench/lib/instruments`, test firmware in
`bench/firmware`); raw captures were kept locally only (`bench/data/`, not in git).
