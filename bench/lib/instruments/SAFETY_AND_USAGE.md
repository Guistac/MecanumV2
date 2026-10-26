# Bench instruments

Copy of the bench-instruments skill (`~/.claude/skills/bench-instruments`, 2026-10-26) so the bench
scripts in `bench/` run from this repo. Usage, safety rules and the test log; command tables in
`reference.md`.

Instruments on the 10.0.0.0/24 bench network, reachable from the Mac (a NIC is configured).

| Instrument | Address | Interface | Helper | Status |
| --- | --- | --- | --- | --- |
| OWON ODP3063, 3 outputs (CH1/CH2 30 V 6 A, CH3 6 V 3 A) | 10.0.0.10:3000 | raw SCPI over TCP, `\n` terminated | `owon.py` | tested 2026-10-26 |
| Rigol DHO914S, 4 ch, 125 MHz, AWG | 10.0.0.11:5555 | raw SCPI over TCP (also VXI-11 on 111, web on 80/8080) | `rigol.py` | tested 2026-10-26 (acquisition and readout; setup writes not yet) |

## Safety rules (always)

1. **Ask before switching power to hardware** that is connected, unless the owner gave
   control for that session. With nothing connected the owner gave full control (2026-10-26).
2. **Never send `OUTP ON`**: it switches all three outputs, so a 24 V channel can land on a 5 V
   board. Switch per channel with `CHAN:OUTP:ALL a,b,c` (`owon.py` refuses `OUTP ON`).
3. **Limits first, then setpoints, then read back.** OVP (`VOLT:LIM`) and OCP (`CURR:LIM`) are
   ceilings: a setpoint above the limit is silently rejected; lowering a limit below the
   setpoint pulls the setpoint down. There is no error reply, only the readback tells.
4. **Check caps before every run and every cycle** (`check_caps`): the front panel stays usable
   while a script runs, so someone can change a value under it.
5. **Poll measurements at 2 Hz or slower.** Back-to-back queries freeze the readings, which looks
   like a dead output.
6. **Leave the supply as found**: `snapshot()` at the start, `restore()` at the end. The owner's
   presets were CH1 24 V/2 A, CH2 24.5 V/0.2 A, CH3 5 V/1 A, OVP 31/31/6.6 V, OCP 6.1/6.1/3.1 A
   (they change; trust the snapshot).

7. **Switch the device's supply output off when a test is done** (owner request), unless a run
   deliberately holds a reproduced failure for measurement; then say it is still powered.

## Using the OWON helper

Run scripts with `python3 -I` and add this folder to the path, or copy `owon.py` next to the
harness (it is standard library only).

```python
import sys; sys.path.insert(0, "bench/lib/instruments")
from owon import ODP3063

with ODP3063() as psu:
    saved = psu.snapshot()
    try:
        psu.all_off()
        psu.configure(3, volt=5.0, curr=1.0, ovp=5.5, ocp=1.0)
        psu.check_caps({3: (5.5, 1.0)})
        psu.channel_on(3)
        volts, amps = psu.measure()        # rate limited, about 1 s until the output is up
    finally:
        psu.restore(saved)
```

Command line: `python3 -I owon.py status` (read only), `python3 -I owon.py off` (all outputs off).

## OWON behaviour worth knowing

- **The supply's readback can get stuck until a mains power cycle.** On 2026-10-26 CH1 read a
  frozen 23.997 V while the scope showed 5 V and then 3 V (0 when off). After a power cycle it
  followed again (5.000 / 3.000 / 0 V, about 0.7 s lag; single-channel `MEAS:VOLT?` updates faster
  than `:ALL`). Suspected trigger: polling at about 30 queries/s earlier that day; never poll fast.
  **Cross-check with the scope**: after settling, a readback that does not move with a setpoint
  change means stuck, so stop the run and ask for a supply power cycle. The scope is the reference.
- Real output timing (scope, 1 MOhm load only): output starts within 0.2 s of `CHAN:OUTP:ALL`,
  0.5 to 4.5 V in 50 ms, no overshoot. Off: 4.9 V to 1.1 V in 0.28 s (internal bleed); a real load
  discharges faster. The "about 1 s" seen in readback earlier was readback lag, not the output.
- Unsupported queries get no reply at all (the helper raises after the timeout); `SYST:ERR?` is
  not supported.
- `OUTP?` means "any channel on"; per-channel state is `CHAN:OUTP:ALL?`.
- **Current setpoint = the fast limit, OCP = a slow backup.** Into a short the supply holds the
  current setpoint at once (constant current); OCP switched the output off only 1.4 to 2.5 s after
  the current reached it. Set the current to what the device may draw, OCP a little above it.
- An OCP trip latches the output off and shows over LAN only as `CHAN:OUTP:ALL?` turning 0 (no trip
  flag): call `expect_outputs()` every cycle. OVP trip not tested (needs an external voltage).
- The panel then shows "channel output value reaches limit, it is forced to shut down, press any
  key to exit warning". Setpoint/limit writes over LAN still work while it is shown; switching an
  output back on before someone dismisses it is untested. **After a trip, stop the run and ask the
  owner** to check the bench and dismiss the warning.
- Current readback is accurate (0.101 / 0.250 / 0.500 / 0.750 / 1.000 A at those setpoints).

## Using the Rigol helper

```python
from rigol import DHO914S                  # same sys.path line as above

with DHO914S() as scope:
    scope.single()                         # arm BEFORE switching power, trigger on the event
    psu.channel_on(3)
    if scope.wait_stopped(timeout=5):      # False: nothing crossed the trigger level
        t0, dt, volts = scope.waveform(1, raw=True)   # capture memory, scope is stopped
        scope.screenshot("cycle_001.png")
    scope.run()
```

Command line: `python3 -I rigol.py status` (read only), `python3 -I rigol.py shot out.png`.

## Rigol behaviour worth knowing

- `:SYST:ERR?` works (`0,"No error"`): `check()` after setup writes.
- `:SING` sets `:TRIG:STAT?` to `WAIT` and `STOP` once triggered. A flat signal never triggers;
  `:TFOR` forces one. Reading RAW data without a capture returns 0 bytes.
- Screen read = 1000 points (`:WAV:MODE NORM`); RAW = full memory depth (10 kpts tested, 26 ms).
  Screenshot `:DISP:DATA? PNG` about 100 kB in 0.3 s.
- Static IP, mDNS is off on the scope: address it by IP. Without a link it shows "disconnected";
  a reboot after changing network settings was needed once.
- Check probe attenuation (`:CHAN<n>:PROB?`, CH1 was 1x) against the probe switch before trusting volts.
- **Every refused command pops up "remote Cmd Error" on the scope screen**, visible to whoever is
  at the bench. Only send commands listed in reference.md; try new ones once, with the owner told.
- **Say before switching the generator output on**, even with nothing connected (owner request).
  Tell the owner what will visibly change on the scope (channels, timebase, outputs) before a test.
- A hidden channel silently ignores scale/offset writes; set probe factor before V/div
  (`setup_channel` does both in the right order). Memory depth is written as `10k` / `1M`.
- `measure()` returns None when the scope answers 9.9E+37 (no valid value).
- **Roll mode from 50 ms/div**: no triggering, the trigger offset reads back -5 div and memory depth
  cannot change. Triggered captures are limited to 20 ms/div (about 0.4 s of 1M memory). Restore the
  timebase before the memory depth.
- **Short events need deep memory**: a 10k capture decimates and hid the edge ringing a 1M capture
  showed. For rail dips, use 1M (reads in about 0.4 s) and trigger on the dip itself.
- Loopback self-test (generator cabled to CH1, see reference.md) checks the whole chain in seconds.

Full command list and test log: [reference.md](reference.md).
