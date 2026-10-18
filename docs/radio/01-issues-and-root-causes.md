# 01 - Issues and root causes

## Symptoms

- Both boards (robot and remote) sometimes fail radio initialization at power-up.
  Sometimes it succeeds after a few retries; sometimes it keeps failing, and only several power cycles
  or several minutes unpowered bring it back.
- Both boards sometimes hang. It happens under any load. While the robot firmware is hung, the servo
  timer interrupts keep pulsing the motors at the last commanded velocity.

## Hardware involved

Same radio circuit on both boards (schematic references: robot `RT1`/`SH1`/`M1`, remote `R0`/`SH1`/`M0`):

| Signal | Teensy pin | Path to the RFM23BP |
| --- | --- | --- |
| SDN (shutdown) | D8 | TXB0108 A7 -> B7 |
| nIRQ | D9 | TXB0108 B6 -> A6 |
| nSEL (chip select) | D10 | TXB0108 A5 -> B5 |
| SDI / SDO / SCK | D11 / D12 / D13 | TXB0108 A3/A2/A4 <-> B3/B2/B4 |
| TX_ON | - | wired directly to the module's GPIO_1 |
| RX_ON | - | wired directly to the module's GPIO_0 |

- RFM23BP supply: the 5 V `VCC` rail, shared with the Teensy `Vin` and the TXB0108 B side (VCCB).
  On the remote it comes from a Pololu U1V10F5 boost converter running off one Li-ion cell.
  About 20 uF and 220 uF of capacitance near the 5 V input.
- The RFM23BP needs 5 V logic (VIH = VDD - 0.6 V), hence the TXB0108 level shifter.
- TXB0108 OE is tied directly to 3V3 on both boards.
- Transmit power is the maximum, +30 dBm. RadioHead documents about 550 mA supply current at that setting.

## Root cause 1: firmware hang waiting for a lost interrupt (confirmed in code)

RadioHead's `RH_RF22::send()` starts with `waitPacketSent()`, which is:

```cpp
while (_mode == RHModeTx) YIELD;   // no timeout
```

`_mode` only leaves TX when the interrupt handler sees the "packet sent" flag. If that interrupt is
lost once, the next `send()` spins forever. On the robot this is in `sendData()`, called after every
received frame. On the remote, `updateTemperature()` -> `adcRead()` had a second unbounded loop.

Ways the interrupt can be lost:

- **The radio resets during a transmission** (supply brown-out at 30 dBm, see root cause 2). The
  chip comes back with default registers and never reports "packet sent".
- **nIRQ is already low when the interrupt is attached.** The interrupt is edge triggered (FALLING).
  RadioHead enables the radio's interrupt sources before calling `attachInterrupt()`, and the Teensy 4
  `attachInterrupt()` clears any pending edge (`gpio[ISR_INDEX] = mask` in `cores/teensy4/interrupt.c`).
  If a flag is set at that moment, no falling edge ever comes again.
- **Any other reason nIRQ stays low** (noise, a long interrupts-off period). An edge-triggered line
  never recovers by itself.

Note: the Teensy GPIO dispatcher clears the pending flag before calling the handler, so an edge that
arrives while the handler runs is not lost. That path is safe.

## Root cause 2: the RFM23BP power amplifier is enabled when it should not be (confirmed from datasheet and schematic; effect not yet measured)

The RFM23BP datasheet (pin descriptions, p. 60) gives TX_ON and RX_ON as **active low**:
TX state = TX_ON 0 / RX_ON 1, RX state = RX_ON 0 / TX_ON 1.

The boards wire GPIO_1 -> TX_ON and GPIO_0 -> RX_ON, as RadioHead recommends, and RadioHead
configures GPIO_0 = "TX state" and GPIO_1 = "RX state". Each of those outputs is high only in its own state:

| Radio state | GPIO_0 -> RX_ON | GPIO_1 -> TX_ON | Result |
| --- | --- | --- | --- |
| TX | 1 | 0 | PA on (correct) |
| RX | 0 | 1 | LNA on (correct) |
| Idle / ready | 0 | 0 | **PA on without RF drive** |
| GPIO power-on defaults | POR / inverted POR | | polarity not documented, possibly PA on |
| Shutdown (SDN high), unpowered | floating | floating | undefined |

The original firmware comment "after init, the radio is not in Rx Mode, we need to set this mode
immediately else we'll overheat" and the local RadioHead edit that returns to RX after "packet sent"
both point at this. The chip is idle, and the PA enabled:

- after every received packet (the chip leaves RX by itself) until the main loop calls `recv()`,
- during initialization, between configuring the GPIOs and entering RX,
- possibly from every power-up / reset until the GPIOs are configured, depending on the POR output polarity.
  With the original code that was more than 100 ms per init attempt (`RHSPIDriver::init()` has a blind
  `delay(100)`), so up to about 12 s over the robot's 100 retries.

A PA running without drive draws a lot of current and heats up. That fits the "needs minutes powered off"
failure (cool-down) and the supply sag that can reset the chip (root cause 1).

## Contributing factors

- **TXB0108 not held off at power-up.** The Teensy pins are undriven for about 300 ms after reset (the
  core waits for USB before `setup()`), and the radio constructor ran even later. With OE tied to 3V3
  the shifter passes the floating inputs to SDN, nSEL, SCK and MOSI. TI recommends OE pulled down during
  power-up. The TXB0108 also cannot drive against pull resistors under 50 kOhm.
- **Stale "chip ready" flag race in init** (confirmed in code). After a reset, RadioHead polls the chip
  ready flag without clearing it first. A flag left over from the previous reset can end the wait while
  the software reset is still in progress, so the following register writes are lost.
- **30 dBm supply current** on a 5 V rail shared with the Teensy, and on the remote with a small boost converter.
- **Robot continued with unconfigured motion control** when radio init failed (`initialize()` returned
  early but the update loop still ran). Looked like a hang from the outside.
- **Remote right joystick Y was set to pin 13**, which is SPI SCK. `pinMode(13, INPUT)` switched SCK
  away from SPI until the display re-initialized SPI. The PCB has the joystick on D27/A13.
- **Local edits to RadioHead lived in `~/.platformio`**, outside version control (chip ready timeout,
  return to RX after packet sent, `RH_RF22_MAX_MESSAGE_LEN` = 50). A framework update or another machine
  would silently build different firmware.

## Confidence

| Item | Status | How it was established |
| --- | --- | --- |
| `send()` can block forever | Confirmed | RadioHead source |
| `attachInterrupt()` drops a pending edge | Confirmed | Teensy core source |
| Stale chip ready flag race | Confirmed | RadioHead source |
| PA enabled while the chip idles | Confirmed on paper | RFM23BP datasheet p. 60 + schematic + RadioHead GPIO config |
| PA enabled at GPIO power-on defaults | Unknown | POR output polarity not documented, see diagnostics D2 |
| PA state while in shutdown | Unknown | depends on module internals, see D2 |
| Overheating causes the persistent init failures | Hypothesis | fits symptoms, see D1 |
| Brown-out during 30 dBm transmit resets the chip | Hypothesis | see D5 |
| Rail not reaching 0 V on quick power cycles (no clean POR) | Hypothesis | see D3 |
