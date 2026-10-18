# RadioHead_RFM23BP

Minimal fork of RadioHead 1.112 (the copy bundled with framework-arduinoteensy 1.155),
containing only what the RF22 driver needs. `platformio.ini` sets `lib_ignore = RadioHead`
so the framework copy is never linked.

Changes in `RH_RF22.cpp/.h` (search for "Mecanum fork" / "RFM23BP hardening"):

- Antenna control: on the RFM23BP TX_ON/RX_ON are active low and driven by GPIO_1/GPIO_0.
  In IDLE (and possibly at the GPIO power-on defaults) the PA is enabled without drive.
  `parkAntennaControl()` drives both pins high; init parks them right after reset and only hands
  them to the TX/RX state functions once the chip is in RX. The chip is put back into RX after
  every received and every sent packet, so it never idles.
- `send()` waits for the previous packet with a timeout instead of forever (firmware hang).
- `adcRead()` (temperature) has a timeout.
- `init()` reports why it failed (`lastInitError()`, `deviceType()`), polls the chip ready flag
  instead of the 100ms blind delay in `RHSPIDriver::init()`, checks register readback,
  clears stale interrupt flags before attaching the interrupt, and detaches the interrupt on re-init.
- A chip power-on reset while running (IPOR, brown-out) parks the antenna pins in the ISR and is
  reported with `consumeChipResetFlag()`.
- `serviceInterrupt()` recovers from a stuck low nIRQ line.
- RX has its own buffers; payload and headers of a packet are captured together.
- Local edits that were made in the framework copy are kept: `RH_RF22_MAX_MESSAGE_LEN` = 50,
  return to RX after packet sent, chip ready timeout.
