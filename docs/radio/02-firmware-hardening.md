# 02 - Firmware hardening

Branch `radio-hardening`. Builds without warnings; **not yet tested on hardware** (see [03](03-diagnostics-to-run.md)).

## Overview

| Area | Change | Where |
| --- | --- | --- |
| Driver | RadioHead forked into the repo, framework copy ignored | `lib/RadioHead_RFM23BP/`, `platformio.ini` |
| PA protection | Antenna control pins parked (PA off) outside RX/TX, chip never left idle | `RH_RF22.cpp` |
| Hangs | All waits on radio events bounded | `RH_RF22.cpp` |
| Recovery | Stuck nIRQ, stuck TX, chip reset and register changes detected, radio restarted in background | `src/Hardware/Radio.h` |
| Init | Reports why it failed, retries with growing pauses, radio kept in shutdown between attempts | `RH_RF22.cpp`, `Radio.h` |
| Boot | SDN and CS driven high a few ms after reset | `src/main.cpp` |
| Watchdog | 1.5 s hardware watchdog, robot ramps servos to zero 0.5 s before the reset | `src/Utilities/Watchdog.*`, `RobotBoard.cpp` |
| Robot | Motion control configured even if the radio fails, LED shows radio state | `src/RobotBoard/RobotBoard.cpp` |
| Remote | Init error shown and retried, "No Radio" status, joystick pin fix | `src/RemoteBoard/*` |

## Driver fork

`lib/RadioHead_RFM23BP` is a copy of the RadioHead 1.112 files the RF22 driver needs, taken from
framework-arduinoteensy 1.155 including the earlier local edits. The library is renamed to
`RadioHead_RFM23BP`, and `platformio.ini` has `lib_ignore = RadioHead`, so only the fork is compiled.
Its [README](../../lib/RadioHead_RFM23BP/README.md) lists the changes; in the code they are marked
"Mecanum fork" / "RFM23BP hardening".

## PA protection

- `parkAntennaControl()` sets GPIO_0 and GPIO_1 to direct digital output (function code `0x0A`) with
  both levels high: TX_ON = RX_ON = 1, PA and LNA off.
- `init()` parks the pins right after the software reset, before any other configuration.
  Only at the very end it enters RX and then switches the GPIOs to the TX state / RX state functions.
- After every valid received packet the interrupt handler puts the chip straight back into RX
  (stock RadioHead left it idle until the main loop called `recv()`). After "packet sent" it already went back to RX.
- A chip power-on reset while running (IPOR interrupt, e.g. brown-out) parks the pins inside the
  interrupt handler and is reported to the wrapper.
- On any fault, and between failed init attempts, the wrapper parks the pins (if the chip still responds)
  and puts the chip in shutdown (SDN high).
- The 100 ms blind delay in `RHSPIDriver::init()` was removed; init polls the chip ready flag instead.
  This shortens the time after power-up during which the GPIOs are at their power-on defaults.

## Bounded waits and recovery

- `send()` waits for the previous packet with a timeout (`setTxTimeout()`, 60 ms in `Radio.h`;
  an 8 byte frame takes about 13 ms at 19.2 kbps). On timeout it aborts the transmission, returns to RX,
  and counts a TX timeout.
- `adcRead()` (temperature) gives up after 3 ms.
- `Radio::update()`, called every loop iteration on both boards, faults the radio when:
    - the chip reported a power-on reset,
    - a TX timeout occurred,
    - a transmission did not complete within the TX timeout,
    - the periodic check (every 250 ms) finds the device type or the GPIO configuration registers changed.
- If nIRQ stays low for more than 2 ms, the interrupt handler is run from the main loop to read
  and clear the flags (`serviceInterrupt()`). A healthy line is low only for microseconds.
- Interrupt flags are cleared before the interrupt is attached, the interrupt is detached before a
  re-init, and anything pending after attaching is handled once.
- RX has its own buffers (stock RadioHead shares `_buf` between TX and RX). Payload and headers of a
  packet are captured together in the interrupt and handed out together by `recv()`. This matters
  because the first 4 bytes of each 8 byte frame travel in the header registers.

## Initialization and retries

Each attempt (`Radio::attemptInitialization()`):

1. SDN high 10 ms, then low; wait 20 ms (tPOR is 16 ms typical).
2. Clear stale status flags, software reset, wait up to 100 ms for chip ready.
3. Park the antenna pins, check the device type, read back the GPIO registers.
4. Configure modem (GFSK 19.2 kbps), TX power, channel; enter RX; check registers again.

On failure the radio is shut down. `initialize(maxTries)` blocks for up to `maxTries` attempts
(5 on both boards). After that, and after any runtime fault, `update()` keeps retrying in the background,
the pause doubling from 100 ms up to 2 s. The pause also gives a hot module time to cool.

`Radio::getDiagnostics()` keeps: last init error, last device type value, init attempts and failures,
runtime faults and the last fault reason, nIRQ recoveries, health check failures.

| Init error | Shown as | Meaning |
| --- | --- | --- |
| `INIT_CHIP_NOT_READY` | no response | chip ready flag never set |
| `INIT_BAD_DEVICE_TYPE` | bad device id | device type is not 0x08 (`id:FF` = MISO not driven, `id:00` = MISO stuck low) |
| `INIT_REGISTER_READBACK` | spi readback | a register write did not read back |
| `INIT_NO_INTERRUPT_PIN`, `INIT_TOO_MANY_DEVICES` | bad irq pin, no irq vector | configuration errors, should never occur |

## Boot

`startup_middle_hook()` in `src/main.cpp` runs a few milliseconds after reset, before the core's
300 ms USB wait. It drives CS (D10) and SDN (D8) high (deselected, shutdown), setting the level
before switching the pin to output so it never glitches low. The `Radio` constructor keeps them that way
until `initialize()`.

## Watchdog

`Watchdog::begin(1.5, callback)` starts WDOG1 with a 1.5 s timeout. The main loops call
`Watchdog::feed()`; blocking waits use `Watchdog::delayFed()`.
The optional callback runs from the watchdog interrupt 0.5 s before the reset. On the robot it sets all
servo velocity targets to zero, so the servo interrupts ramp the motors down (about 0.4 s from full speed)
instead of continuing at the last speed. The watchdog pauses while the core is halted, which the
Teensy bootloader does to start an upload.

Note: during the about 300 ms boot after a watchdog reset, the servo control lines float (same as at
every power-up), see [04](04-pcb-revision-recommendations.md).

## Board behavior

- **Robot:** motion control is always configured. If the radio cannot start, the robot stays disabled
  (no remote) and keeps retrying. The remote LED double-blinks while the radio is down.
  Debug messages (`DEBUG_PRINTS` in `RobotConfig.h`) share the USB serial with MAVLink, so they are off by default.
- **Remote:** if init fails, the screen shows "radio init failed", the error and the device id, and the
  attempt counter, and keeps retrying instead of asking for a reboot. While running, the main screen shows
  "No Radio" when the radio is down (versus "Offline" when the robot does not answer).
  Init messages are also printed on the USB serial.
- **Remote joystick:** `REMOTE_JOYSTICKRIGHT_Y_PIN` is now `A13` (D27).

## Settings

| Setting | Value | Where |
| --- | --- | --- |
| TX power | 30 dBm (`RH_RF22_RF23BP_TXPOW_30DBM`) | `Radio.h` `txPower` |
| TX timeout | 60 ms | `Radio.h` `txTimeoutMillis` |
| Health check interval | 250 ms | `Radio.h` |
| Stuck nIRQ threshold | 2 ms | `Radio.h` |
| Retry pause | 100 ms doubling to 2 s | `Radio.h` |
| Blocking init attempts | 5 | `RobotBoard.cpp`, `RemoteBoard.cpp` |
| Watchdog timeout / warning | 1.5 s / 1.0 s | `RobotBoard.cpp`, `RemoteBoard.cpp` |

## Known limits

- The direct-output function code `0x0A` comes from the Si4432 register table; the HopeRF datasheet
  does not list GPIO function codes. Verify with diagnostic D2.
- In shutdown the GPIOs are not driven, so TX_ON/RX_ON float. Whether the module keeps the PA off then
  is unknown (D2). Firmware cannot fix that.
- The PA is still briefly enabled between "packet sent" / "packet valid" and the interrupt handler
  putting the chip back into RX (microseconds, longer if interrupts are blocked).
- Before the first init attempt, the GPIOs sit at their power-on defaults for about 20-30 ms.
- Firmware cannot fix a supply that sags at 30 dBm; lowering the TX power is the only firmware lever.
