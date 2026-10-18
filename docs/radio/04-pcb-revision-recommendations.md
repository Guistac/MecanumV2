# 04 - PCB revision recommendations

Hardware changes for the next revision of both boards, in priority order.
Confirm with diagnostics D1 to D3 ([03](03-diagnostics-to-run.md)) before committing to the details.

## 1. PA control that is off by default

Today TX_ON/RX_ON are active low and driven directly by GPIO_1/GPIO_0, so the PA is on whenever the
chip idles, possibly at the GPIO power-on defaults, and undefined while the chip is in shutdown.

- **Minimum:** pull-ups to the module VCC on TX_ON and RX_ON (10-100 kOhm; these nets do not go through
  the TXB0108). This keeps the PA off while the GPIOs are not driven (shutdown, unpowered chip).
- **Better:** invert the state outputs so idle means off: TX_ON = NOT(GPIO "TX state"),
  RX_ON = NOT(GPIO "RX state"), for example with a dual inverter powered from the module VCC.
  Then idle, reset and power-on all give TX_ON = RX_ON = 1. The firmware parking keeps working with it
  (it would then need to drive the pins low instead of high, a one-line change).
- **Alternative:** drive TX_ON/RX_ON from two Teensy pins through the level shifter, under full firmware control.

## 2. Radio power switch and bleed

- A high-side P-MOSFET (or load switch) on the RFM23BP supply, controlled by a Teensy pin, so the
  firmware can fully power-cycle the module when it gets stuck instead of needing a manual off/on.
- A bleed resistor on the radio supply so it discharges to 0 V quickly when switched off
  (clean power-on reset, see D3).
- Default off (gate pulled to the off state) until the firmware enables it.

## 3. Level shifting

- **TXB0108 OE:** pull-down resistor, driven high by a Teensy pin after boot (TI datasheet: OE should be
  tied to GND through a pull-down during power-up and power-down).
- **SDN:** pull-up (at least 50 kOhm because of the TXB0108, for example 100 kOhm) on the module side so the
  radio stays in shutdown until the firmware takes over.
- **nSEL:** pull-up (at least 50 kOhm) on the module side.
- **Consider replacing the TXB0108** with unidirectional parts: a 74AHCT125 (already used for the servo
  outputs) for SCK, MOSI, nSEL and SDN, and a 3.3 V-tolerant buffer or divider for MISO and nIRQ.
  The TXB0108's weak (about 4 kOhm) auto-direction outputs cannot hold lines against pull resistors
  and are sensitive to trace capacitance.

## 4. Supply for the RFM23BP

- Size the 5 V source for about 0.55 A peak radio current at 30 dBm plus the Teensy and the rest,
  with margin. On the remote, check the U1V10F5 boost converter's output current rating at minimum battery
  voltage against that.
- Consider a dedicated regulator for the radio so TX current peaks do not disturb the Teensy.
- Keep the bulk capacitance (220 uF) and add ceramic decoupling right at the module VCC pin.

## 5. Thermal

Copper pour and thermal vias under the RFM23BP, and no enclosed hot spots, so the PA can shed heat
when transmitting at full power.

## 6. Test points

TX_ON, RX_ON, SDN, nIRQ, nSEL, MISO (module side), radio VCC, and a current-sense shunt or jumper in the
radio supply. These make D1, D2, D5 and D6 quick to repeat.

## 7. Robot servo outputs at boot

The SN74AHCT125 inputs for the servo enable and pulse lines float for about 300 ms after every reset
(power-up and watchdog reset). Add pull-downs on the enable inputs (and the pulse inputs) so the drives
stay disabled until the firmware runs.

## 8. Lower transmit power, if range allows

If D7 shows enough range at a lower setting, plan the supply and thermal design for that instead of 30 dBm.
