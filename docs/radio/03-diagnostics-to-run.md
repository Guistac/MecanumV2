# 03 - Diagnostics to run

Bench measurements needed to confirm the hypotheses in [01](01-issues-and-root-causes.md) and to
validate the firmware in [02](02-firmware-hardening.md). D1 to D3 decide what the PCB revision must fix.

Safety: run the robot tests with the wheels off the ground.

## D1 - Is the module hot / drawing current when it fails? (highest priority)

When a board is stuck in failed init (before or after the firmware update):

- Touch or measure the RFM23BP temperature (thermocouple or IR).
- Measure the 5 V supply current with a bench supply or a shunt. RX mode is about 25 mA (datasheet);
  hundreds of mA while not transmitting means the PA is on.
- Note how long the board has to stay unpowered before init works again.

Hot module or high current confirms the PA theory.

## D2 - TX_ON / RX_ON levels in every state

Probe TX_ON and RX_ON (module pins, wired to GPIO_1 / GPIO_0) together with SDN:

| Condition | Expected with the new firmware | What it tells us |
| --- | --- | --- |
| Power-up, SDN high (startup hook) | ? | PA state while in shutdown (pins not driven) |
| SDN low, before the first register write (about 20-30 ms) | ? | Polarity of the GPIO power-on defaults |
| During init, after the software reset | both high | `0x0A` direct output parking works |
| RX | RX_ON low, TX_ON high | normal |
| TX (about 13 ms per packet) | TX_ON low, RX_ON high | normal |
| After a forced chip reset (pulse SDN while running) | both high within microseconds | IPOR handling works |

Never both low except for microseconds.

## D3 - Supply rails through a power cycle

Scope 5 V `VCC` and the Teensy 3V3 while switching off and on again:

- How long the 5 V rail takes to drop below about 0.7 V. The Si4432 power-on reset needs VDD to fall
  below its low VDD limit (0.7-1.3 V) to re-arm; a quick power cycle may not give it a clean reset.
- Repeat with USB connected to the Teensy: check whether the radio is back-powered through the TXB0108
  or the I/O pins (5 V rail not dropping to zero with the main supply off).
- Repeat a fast off/on cycle several times and note which ones fail init.

## D4 - Init failure statistics

- Remote: write down the error and id shown on the "radio init failed" screen (`no response`,
  `bad device id id:FF` / `id:00`, `spi readback`) and the attempt number that finally worked.
- Robot: set `DEBUG_PRINTS` to `true` in `src/Utilities/RobotConfig.h` and read the USB serial
  (MAVLink must be disconnected).
- Over 20-30 power-ups, count successes on the first attempt, after retries, and never.

## D5 - Supply sag during transmission

Scope 5 V at the RFM23BP VCC pin during a 30 dBm packet (trigger on TX_ON falling).
Check the droop and whether the Teensy or the radio resets. On the remote, also at low battery,
since the boost converter has the least margin there.

## D6 - SPI signal quality through the TXB0108

Scope SCK and MISO on the module side (B port) and the Teensy side: ringing, slow edges, glitches.
The SPI clock is about 1 MHz (RadioHead default). Check nIRQ edges too.

## D7 - TX power experiment

Temporarily lower `txPower` in `src/Hardware/Radio.h`, check range on stage, and compare
module temperature, supply current (D1) and failure rate (D4).

## D8 - Firmware validation

- **Watchdog reset:** add a temporary `while(1);` in an update loop; the board must reset after about 1.5 s.
  On the robot, with wheels turning, the motors must ramp down before the reset.
- **Upload:** a normal firmware upload must still work while the watchdog runs.
- **Radio recovery:** pulse SDN or briefly short the radio supply while running; the radio must restart
  by itself and the link come back (remote shows "No Radio", then the robot status again).
- **Soak test:** run the link for several hours. The diagnostic counters (`Radio::getDiagnostics()`:
  faults, last fault, nIRQ recoveries, health check failures) are not shown at runtime yet;
  add a temporary print or a remote debug screen to read them.
