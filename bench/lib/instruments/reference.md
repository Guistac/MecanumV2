# Bench instruments reference

## OWON ODP3063

Source: OWON "ODP Series DC Power Supply Programming Manual" (ODP3033 / ODP3063 / ODP6033,
PDF modified 2020-05-06), mirror:
https://technica-m.ru/upload/support_ext/owon_odp_series_power_supply_programming_manual.pdf
OWON download center (SCPI zip, user manuals): https://www.owon.com.hk/download/index.asp?S=32

Unit: `*IDN?` -> `OWON,ODP3063,23450067,FV:V3.6.0`. Only TCP port 3000 is open (full nmap scan).

| Command | Meaning | Tested |
| --- | --- | --- |
| `*IDN?` | identity | yes |
| `INST:NSEL {1\|2\|3}` / `INST {CH1\|CH2\|CH3}` | select channel; `INST?` -> `CH1` | yes |
| `VOLT <v>` / `VOLT?` | voltage setpoint, selected channel | yes |
| `CURR <a>` / `CURR?` | current setpoint, selected channel | yes |
| `APP:VOLT v1,v2,v3` / `APP:VOLT?` | all voltage setpoints, selection unchanged | yes |
| `APP:CURR a1,a2,a3` / `APP:CURR?` | all current setpoints | yes |
| `VOLT:LIM <v>` / `VOLT:LIM:ALL v1,v2,v3` | OVP, selected / all | yes (write and read) |
| `CURR:LIM <a>` / `CURR:LIM:ALL a1,a2,a3` | OCP, selected / all | yes (write and read) |
| `CURR:LIM:ALL? MAX` | hardware maximum OCP: 6.1, 6.1, 3.1 | yes |
| `CHAN:OUTP:ALL a,b,c` / `CHAN:OUTP:ALL?` | per-channel output on/off | yes, only the chosen channel comes on |
| `CHAN:OUTP {ON\|OFF}` | output of the selected channel | yes |
| `OUTP {ON\|OFF}` / `OUTP?` | ALL channels / "any channel on" | yes (dangerous, never use ON) |
| `OUTP:TRAC`, `OUTP:SER`, `OUTP:PAR` | CH1/CH2 track, series, parallel | not tested, leave OFF |
| `MEAS:VOLT?` `MEAS:CURR?` `MEAS:POW?` | measured, selected channel | yes |
| `MEAS:VOLT:ALL?` `MEAS:CURR:ALL?` `MEAS:ALL?` | measured, all channels | yes |
| `SYST:REM` / `SYST:LOC` | remote mode (locks the panel except KeyLock) / local | LOC only; never needed |
| `*RST` | factory defaults | not tested, do not use |

Not supported (no reply): `SYST:ERR?`, `SYST:VERS?`, `APPL?`, `VOLT:PROT:STAT?`, `CURR:PROT:STAT?`.
`VOLT:PROT?` / `CURR:PROT?` answer but just echo the setpoints.

### Test log 2026-10-26 (nothing connected to the terminals)

- `OUTP ON` with CH3 selected switched CH1 (24 V) and CH2 (24.5 V) on too.
- `CHAN:OUTP:ALL 0,0,1`: CH3 4.997 V after 1.0 s, CH1/CH2 stayed 0 V (setpoints 24/24.5 V).
- Polling `MEAS` at about 30/s showed 0 V on an output that was on; at 2 Hz the readings were correct.
- CH3 OVP 5.5 V: `VOLT 6.0` rejected, setpoint stayed 5.0 V. CH3 OCP 0.7 A: `CURR 0.9` rejected.
  OCP lowered to 0.4 A with setpoint 0.6 A: setpoint became 0.4 A.
- Panel changes (OCP set on the panel) show up immediately over LAN; the panel stays usable while
  a client is connected.

## Rigol DHO914S

Unit: `*IDN?` -> `RIGOL TECHNOLOGIES,DHO914S,DHO9S254603418,00.01.01` (firmware 00.01.01),
static IP 10.0.0.11/24, MAC C6:E1:8F:4C:2E:3F, VISA `TCPIP::10.0.0.11::INSTR`.
Open ports: 80, 8080 (web), 111 (VXI-11 portmapper), 5555 (raw SCPI). `:SYST:VERS?` -> 3.0 (SCPI).
Reference: Rigol DHO800/DHO900 programming guide (rigol.com); commands below were checked on the unit.

| Command | Meaning | Tested |
| --- | --- | --- |
| `*IDN?`, `:SYST:ERR?` | identity, error queue (`0,"No error"`) | yes |
| `:ACQ:SRAT?`, `:ACQ:MDEP?` | sample rate (312.5 MSa/s), memory depth (10 kpts) | yes |
| `:TIM:MAIN:SCAL?` | timebase s/div | yes (read) |
| `:TRIG:STAT?` | AUTO / WAIT / STOP / ... | yes |
| `:TRIG:MODE?`, `:TRIG:SWE?` | EDGE, AUTO | yes (read) |
| `:CHAN<n>:DISP? / :SCAL? / :PROB?` | channel shown, V/div, probe factor | yes (read) |
| `:SING`, `:TFOR`, `:STOP`, `:RUN` | single, force trigger, stop, run | yes |
| `:WAV:SOUR CHAN<n>`, `:WAV:MODE NORM\|RAW`, `:WAV:FORM BYTE` | waveform transfer setup | yes |
| `:WAV:PRE?` | format,type,points,count,xinc,xorig,xref,yinc,yorig,yref | yes |
| `:WAV:STAR`, `:WAV:STOP`, `:WAV:DATA?` | block `#9...`, volts = (byte - yorig - yref) * yinc | yes (10 kpts) |
| `:DISP:DATA? PNG` | screenshot block | yes |
| `:CHAN<n>:DISP/PROB/COUP/SCAL/OFFS` writes | channel setup | yes; hidden channel ignores SCAL/OFFS silently; set PROB before SCAL |
| `:TIM:MAIN:SCAL/OFFS` writes | timebase | yes |
| `:TRIG:EDGE:SOUR/SLOP/LEV`, `:TRIG:SWE NORM\|AUTO` writes | edge trigger | yes |
| `:ACQ:MDEP 10k\|1M` | memory depth | yes; `1.0000E+04` (its own readback format) is refused |
| `:MEAS:ITEM? VMAX\|VMIN\|VPP\|VAVG\|FREQ,CHAN<n>` | automatic measurement | yes; 9.9E+37 = no valid value |
| `:SOUR:FUNC SIN\|SQU\|DC\|...`, `:SOUR:FREQ`, `:SOUR:VOLT:AMPL`, `:SOUR:VOLT:OFFS` | generator setup | yes; `PULS` refused |
| `:SOUR:OUTP:STAT 0\|1` | generator output | yes |

Refused on this firmware (each one pops up "remote Cmd Error" on the scope screen):
`:SOUR:OUTP?`, `:SOUR:VOLT?`, `:SOUR:OUTP:IMP?`, `:SOUR:OUTP:LOAD?`, `:SOUR:FUNC PULS`.
Generator defaults found: SIN, 1 kHz, 5 Vpp, 0 V offset, output off.

### Test log 2026-10-26 (nothing connected to the inputs)

- Screen read: 1000 points in 27 ms (xinc 20 ns at 2 us/div). Screenshot 100 kB in 289 ms.
- `:SING` with a flat CH1 stayed in WAIT for 5.4 s (correct); `:STOP` then RAW read gave 0 bytes.
- `:SING`, `:TFOR`: STOP after 0.3 s; RAW read 10000 points in 26 ms (xinc 3.2 ns).
- `:WAV:MODE` restored to NORM and `:RUN` back to AUTO afterwards, error queue empty.

### Loopback 2026-10-26 (generator output cabled straight to CH1)

Generator SQU 1 kHz, 3.3 Vpp, +1.65 V offset; CH1 1x, 1 V/div, -1.65 V; 200 us/div; trigger CH1
rising 1.65 V, NORM. All helper setup methods passed write + readback, error queue empty throughout.

| Read | Points | Time | Result |
| --- | --- | --- | --- |
| `:MEAS:ITEM?` | - | 0.1 s | VMAX 3.241, VMIN -0.117, VPP 3.356, FREQ 1000.0 |
| screen (NORM) | 1000 | 0.11 s | 1000.0 Hz |
| RAW, 10k depth | 10 000 | 0.26 s | dt 0.4 us, 4 ms, 1000.000 Hz, min -0.137 max 3.209 |
| RAW, 1M depth, 100k chunks | 1 000 000 | 0.87 s | dt 3.2 ns, 3.2 ms, 1000.000 Hz, min -1.195 max 4.369 |
| RAW, 1M depth, one 1M read | 1 000 000 | 0.42 s | same |

Findings:
- **`:WAV:PRE?` points = the current `:WAV:STAR/STOP` window, which persists between reads**, not
  the capture size. The first 1M read returned only the first 10k points. `waveform()` now takes
  the size from `:ACQ:MDEP?` and sets the window.
- **Decimated reads hide short events**: at 10k the edges look clean (max 3.21 V); at 1M the
  ringing of the unterminated cable shows (-1.2 V / +4.4 V). For supply dips and glitches use
  deep memory (or peak-detect acquisition, untested) and trigger on the event.
- The generator drives 3.3 Vpp into the 1 MOhm input as set (no 50 Ohm doubling).

## Supply CH1 -> scope CH2, 2026-10-26 (direct cable, no other load)

Setup: CH1 5 V / 0.1 A, OVP 6 V, OCP 0.2 A; scope CH2 1x, 1 V/div, -2.5 V, 1M points.

- Timebase: 200, 100 and 50 ms/div put the scope in roll mode (offset forced to -5 div, no trigger,
  `:ACQ:MDEP` writes ignored without an error). 20 ms/div triggers normally; 1M points then cover
  -0.12..0.28 s at 0.4 us per sample with the offset at 0.08 s.
- Rise (trigger 0.5 V rising): seen 0.22 s after `CHAN:OUTP:ALL 1,0,0` (incl. polling);
  10 to 90 % (0.5 to 4.5 V) in 50 ms; settles at 4.86 to 4.93 V on the scope; max 4.915 V, no overshoot.
- Fall (trigger 4.5 V falling): 4.90 V to 1.11 V at the end of the 0.28 s window; below 0.1 V
  about 1 s after off (scope VAVG log).
- Setpoint change 5 V to 3 V with the output on: scope 4.93 V to 2.95 V within 1 s.
- **Supply readback frozen**: `MEAS:VOLT:ALL?` gave CH1 23.997 V for all 12 s while on (5 V, then
  3 V); 0.000 when off. `MEAS:CURR` 0.000 A (load only 1 MOhm, so no information).
  The first status of the session also showed CH2/CH3 at 0.001 V while on at 12 V / 3.3 V.
  Same session earlier in the day: CH3 readings did follow 0 -> 5 V -> 0 V. Not yet explained.
- **After a mains power cycle of the supply** (same day, same setup): readback followed correctly,
  CH1 0 -> 4.991 (single `MEAS:VOLT?` at 0.4 s) -> 5.000 V, then 3.000 V within 0.7 s of the
  setpoint change, 0 after off. So the freeze was a stuck supply state, cleared by the power cycle.
  Suspected trigger: the about 30 queries/s polling test earlier that day (unconfirmed).
- Scope vs supply after the power cycle: 4.917 V at 5.000 V and 2.919 V at 3.000 V, i.e. a constant
  about -0.08 V offset rather than a gain error (CH2 offset setting -2.5 V). Run the scope's
  self-calibration or check with a DMM before trusting absolute values to better than 0.1 V.
## CH1 shorted with a thick cable, 2026-10-26 (after the power cycle)

Setpoint 1 V, OVP 2 V, OCP 1.1 A; current stepped with `CURR` while on; single-channel `MEAS:VOLT?` /
`MEAS:CURR?` with 0.25 s between queries (readings steady, no freeze).

| CURR set | Read | Voltage | R |
| --- | --- | --- | --- |
| 0.100 A | 0.101 A | 4 mV | 40 mOhm |
| 0.250 A | 0.250 A | 9 mV | 36 mOhm |
| 0.500 A | 0.500 A | 19 mV | 38 mOhm |
| 0.750 A | 0.750 A | 28 mV | 37 mOhm |
| 1.000 A | 1.000 A | 37 mV | 37 mOhm |

OCP trip: `CURR 0.5`, `CURR:LIM 0.5`, output on into the short. `CHAN:OUTP:ALL?` was `1, 0, 0` at
1.4 s and `0, 0, 0` from 2.5 s on (latched off, `OUTP?` 0). No other trip indication over LAN.

- Scope reads 1.3 to 1.5 % below the setpoint (4.93 V at 5.000 V, 2.95 V at 3.000 V): within the
  scope's DC gain accuracy, so the supply is not proven off either; use a DMM to calibrate if needed.
