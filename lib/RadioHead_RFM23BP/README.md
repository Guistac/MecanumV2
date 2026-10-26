# RadioHead_RFM23BP

Copy of RadioHead 1.112 as bundled with framework-arduinoteensy 1.155, reduced to what the RF22
driver needs. `platformio.ini` sets `lib_ignore = RadioHead` so the framework copy is never linked
and the build no longer depends on edits made in a machine's framework folder.

The files were copied unchanged from the framework folder of the bench Mac (2026-10-26), which
already carried earlier local edits: `RH_RF22_MAX_MESSAGE_LEN` = 50, return to RX after packet
sent, a 200 ms chip ready timeout and the interrupt handler layout in `RH_RF22.cpp`.

Changes on top of that (search for "RFM23BP fork"), see docs/radio/05-bench-test-plan.md:

- `init()`: one software reset, then 2 ms wait and a single read of the chip ready flag, instead of
  back to back polls that can clear the flag unseen (T6e, T7a).
- `send()`: waits for the previous packet with a timeout instead of forever; a timeout is counted
  in `txTimeouts()` and the caller re-initializes the radio.
- `adcRead()` (radio temperature) gives up after 5 ms and returns 0 instead of waiting forever for
  a chip that does not answer.
- The interrupt handler counts chip power-on resets (brown-out, SDN glitch) in `chipResets()`; the
  caller re-initializes.
