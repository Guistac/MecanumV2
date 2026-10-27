# Remote display and radio status (plan, 2026-10-26)

Plan for the remote's OLED (SSD1305, 128 x 32) and for what travels back over the radio, in three tiers. Tier A is
decided (D-146) and implemented on branch `remote-tier-a`, not yet run on hardware; Tiers B and C are plans. Findings
analysed on `mavlink` at `7d818fd`.

Constraint for every tier: the radio link has little bandwidth. No packet grows; new information replaces dead fields
or shares one rotating byte.

## Findings (state at `7d818fd`)

What the display shows today:

| Element | Source | Problem |
| --- | --- | --- |
| `Ch#X` | remote EEPROM channel | fine |
| reply spinner | `rxCounter % 4` | fine |
| signal bar | remote `lastRssi`, scaled -120 to +4 dBm | downlink only; reads about 50 to 70 % in a room and never fills; `drawSignalBar` ignores its `y` argument |
| state text (size 2) | `robotProxy.status` | two sources, not distinguishable (see below) |
| `T:x.xxV` | remote cell | voltage only, no level, no low warning |
| `R:xx.xV` | robot pack, 1 byte for 45 to 70 V | fine; no percentage |
| input test pixels | sticks and 9 inputs, 1 pixel each | unreadable as operator information; the speed switch, frame switch and E-stop are one pixel each |
| dot at x = 127 | frame counter | firmware alive indicator, hidden behind the bar bracket for y 0 to 6 |

What the receiver's 8-byte reply carries on `mavlink`:

| Field | Value on `mavlink` |
| --- | --- |
| state, 3 bits | Stacato's `HEARTBEAT.system_status` (D-27 mapping) while Stacato's heartbeat arrives; otherwise the receiver's own leftover motion state machine (`updateRobotLogic`), which shows "Enabling" for 2 s after an enable press and then "Disabled" with nothing connected |
| speed and direction echo, 3 bits | echo of the remote's own switch; direction is never written (always 0) |
| function button, 1 bit | valid |
| motor alarm and enabled, 1 byte | dead: servo inputs are not connected (pull-ups read "no alarm, not enabled") |
| x, y, r velocity, 3 bytes | dead: always 0, the leftover state machine never reaches ENABLED |
| receiver RSSI of the remote | valid (uplink, the direction that carries the E-stop), not displayed |
| robot battery | valid |
| bits 10 and 11 | free |

Known to the remote but not displayed: round-trip time (computed every reply), packet loss (from the tx counter echo),
radio temperature (read at 5 Hz), radio recoveries, transmit timeouts and chip resets, remote battery level, firmware
version.

Known to Stacato but not reaching the remote: whether Stacato is connected at all; why "ESTOP" (POWEROFF, BOOT,
UNINIT, axis fault, server E-stop, handheld E-stop and the disable ramp all show as ESTOP); actual velocity (`ODOMETRY`
is decoded and dropped by the receiver); command results (`COMMAND_ACK` ignored); who is driving; needs neutral (D-65);
battery percentage (computed by the receiver for `BATTERY_STATUS` but not put on the radio).

Other findings:

- Holding both pushbuttons to edit the channel also sends commands: the left pushbutton makes the receiver send
  `DO_FLIGHTTERMINATION` (Stacato: disable, S4), the right one `PREFLIGHT_REBOOT_SHUTDOWN` (Stacato: UNSUPPORTED).
- Both directions use header `0b111` and 8 bytes, and the radio is promiscuous. A second remote on the same channel is
  parsed as robot status; a second receiver's reply is parsed by the other receiver as stick commands, with the E-stop
  number taken from the battery and RSSI bytes.
- `radioInitFailedScreen()` loops forever ("please reboot remote").
- LED unknown-state branch (`RemoteBoard.cpp`, `default:`) sets the left LED twice and never the right one.
- Contrast is never set (`setContrast` commented out in `Display.h`); the layout is static and always on (OLED
  burn-in over a show day).
- Not a problem: the display and the radio share SPI safely; RadioHead registers nIRQ with `SPI.usingInterrupt`, so
  display transfers mask it.

## Airtime

`Radio::send()` puts frame bytes 0 to 3 in RadioHead's header (to, from, id, flags) and bytes 4 to 7 in the payload,
so an 8-byte frame is on air as preamble 4 (8 nibbles), sync 2, header 4, length 1, payload 4, CRC 2: 17 bytes.
19.2 kbit/s GFSK, one reply per remote packet, 20 Hz while connected. Payload bits only, before TX/RX turnaround:

| | Remote to receiver | Receiver to remote | Airtime at 20 Hz |
| --- | --- | --- | --- |
| today | 17 B, 7.08 ms | 17 B, 7.08 ms | 28 % |
| Tier B (reply 8 to 4 bytes, all in the header) | 7.08 ms | 13 B, 5.42 ms | 25 % |

(The first version of this plan counted the header as unused framing and proposed dropping it, option B6: it carries
half the frame, B6 is withdrawn.)

## Tier A: remote only, no protocol change (decided: D-146)

Decided in the interview `docs/architecture/decisions/REVIEW_HANDHELD_DISPLAY.md` (Stacato repo, H1 to H14), drafted as
D-146. Implemented on branch `remote-tier-a`.

What the screens show and how they look: the section Screen design below, and the reference picture
[screens.png](screens.png) (every screen state, rendered from the firmware's own drawing code).

- Main screen message, highest priority first: own E-stop (`E-STOP` pulsing, "on this remote") or its contacts
  disagreeing over 100 ms (`WIRING`, "e-stop contact fault"), radio fault (`RADIO`, "radio fault, retry n", after 30 s
  "restart the remote"), no reply for 500 ms (`NO LINK`, "e-stop not connected"), remote cell under 3.3 V
  (`BATTERY`, "charge the remote"), sticks to release ("Release left stick"), then the reported state: `E-STOP` "not
  from this remote" (Stacato E-stopped while the own button is clear, after 500 ms), `STOPPING` ("e-stop ramp"),
  Disabling, Disabled, Enabling, Enabled.
- A new alarm brings the Status page back and closes the menu.
- Markers in the bottom centre, taking turns every second: `RST` 3 s after a radio recovery, `HOT` over 70 C (hint),
  `SAFE` in safe mode, `REBOOT` for 5 s after a watchdog or crash reboot.

Pages: left pushbutton previous, right next, wrapping; back to Status after 10 s without input. Status, Link (both
RSSIs, loss, round-trip time), Radio (temperature, recoveries, transmit timeouts, chip resets), Battery (resting and
instant cell voltage, state, robot pack), Inputs (sticks, buttons, E-stop contacts), About (firmware hash, uptime,
last reset cause and count), Settings (only while the speed switch is at OFF). The pushbuttons send no command.

Settings menu (speed switch at OFF only, whatever the vehicle state; leaving OFF closes it): opened with a stick to
the right on the Settings page; up/down selects (Channel, Contrast, Dimming, Sticks, Save, Exit), left/right changes; step at
60 % deflection, re-armed under 30 %, repeated after 0.5 s then every 0.2 s; both sticks work, the stronger one counts.
Values are pending until Save is held right for 1 s (only changed bytes written to EEPROM); Exit, leaving OFF, the
pushbuttons or 10 s without input discard them. Contrast is previewed live.

Joystick calibration (menu item Sticks, H15): "release both" (all axes still for 0.7 s, rest positions averaged),
then "circle both" (extremes recorded, shown live as two circles); the right pushbutton saves once every axis covers a
quarter of the full scale on each side, the left one cancels; after saving, a result screen shows each stick's
measured box, its reach (worst side, % of the half scale) and the offset of its rest position, until a pushbutton. While driving the centre is still the boot zero;
the calibrated range scales each side, with the 1 % deadband and 99 % saturation unchanged (bit-identical to before
without a calibration). The calibrated rest position decides when a stick counts as released at boot.

Stick guard: the sticks are sent neutral while the menu is open, after it closes and after every boot until both
sticks have been centred (within 10 %); the joystick zero is captured at that first release, then a 1 s "thank you"
in the bottom row. The boot no longer blocks on the sticks: the E-stop and the link are live from the start.

Dimming Off / 30 s / 2 min (default 2 min): the lowest contrast after that long without input, a new message or a new
warning; full brightness while an alarm shows. Contrast 4 levels, default 0x2F (the display library's own default,
as before).

LEDs: the D-27 patterns; radio fault both blink together at 1 Hz, own E-stop both on, unknown state both off, the
receiver's finder button fast alternating; a pressed button lights itself.

Crash safety: WDOG1 watchdog, 1 s, fed once per loop; the frame is built and sent before drawing; no `delay()` in
the loop. Reset cause from the core's crash report (CRC checked) and `SRC_SRSR`. Boot-loop protection: 3 watchdog or
crash resets within 60 s (RTC seconds, history in EEPROM from byte 4) start in safe mode: default display settings
(the channel and the stick calibration are kept: the E-stop needs the link), Status page only, the `SAFE` marker.

E-stop contacts (found on the remote, 2026-10-27, Inputs page): released reads NO1 NC0, pressed NO1 NC1. The NC
contact works; the NO input never changes, so the E-stop's NO contact is not fitted or not connected (pin 0 to ground
when pressed). The firmware reads NO1 NC1 and NO0 NC1 as pressed and only both closed (NO0 NC0, impossible for the
switch) as a wiring fault; clear is still only NO1 NC0. With the NC contact alone a broken NC wire reads as pressed
(safe), but a short of the NC wire to ground would read as released while pressed: fitting and wiring the NO contact
block is recommended (hardware), then that short shows as `WIRING` on a press.

Radio: one init attempt at boot, then `Radio::update()` retries every 2 s while `RADIO FAULT` shows (both boards); the
channel set before the chip runs is applied when it comes up.

EEPROM: byte 0 channel (as before; blank reads as 15, as before), 1 contrast, 2 dimming, 4 to 16 reset history, 20 to
44 stick calibration (marker byte, then low, centre, high per axis). Every value range checked on load.

### Code

| Where | What |
| --- | --- |
| `lib/RemoteLogic/src` | hardware-free logic, tested on the host: `StickGuard`, `AxisStepper`, `EstopContacts`, `BatteryMonitor`, `RemoteSettings`, `SettingsMenu`, `PageNavigator`, `StatusView` (big line and line 3), `Dimmer`, `BootGuard` (`ResetHistory`), `UplinkFrame`, `LinkMonitor`, `LedPattern`, `StickCalibration` (math, storage, procedure); `RobotStates.h` and `LiIonBattery.h` moved here |
| `test/test_remote_logic` | the host tests: `pio test -e native` |
| `src/RemoteBoard/RemoteBoard.cpp` | the loop: inputs, logic, LEDs, radio, frame, reply, screen |
| `lib/RemoteUi/src` | the screens: `RemoteScreens` draws a `ScreenModel` with Adafruit GFX (fonts, shapes, small icon bitmaps) |
| `src/RemoteBoard/RemoteGui.cpp` | fills the `ScreenModel` every frame, draws it off screen, the transitions, dimming |
| `tools/screens` | the screens on the Mac: `python3 tools/screens/render.py` builds the drawing code with host stand-ins for Arduino, renders every example state as a PNG plus a sheet, and checks that nothing touches |
| `src/Hardware/Watchdog.h` | WDOG1 and the reset cause |
| `src/Hardware/Radio.h` | init state, retries, fault time, counters |
| `scripts/fw_version.py` | `FW_VERSION` from `git describe` |

Before flashing: `pio test -e native`, `python3 tools/screens/render.py` (no gap problems), then `pio run -e remote` and
`pio run -e teensy41`. When the screens change, copy the new `sheet.png` to `docs/remote-display/screens.png` (and the
GIFs named in Screen design) in the same commit. `render.py` also writes `transitions.png` and one GIF per transition
idea, used to choose the transitions.

### Screen design (owner's rules, 2026-10-26/27)

The display is 128 x 32 pixels, read at arm's length. Rules, from the review of the rendered sheets:

- Small text in the classic 5 x 7 pixel font. It is really 5 x 8 for g, p, q, y and the comma (a descender row), so
  text lines every 8 px are its normal spacing. A proportional bold font (FreeSansBold 9 pt, capitals 13 px) only for
  the state word (Enabled) and the alarm word (E-STOP).
- No outline-only shapes. Pills and bars are filled, with negative text.
- 1 px of fill around negative text, also against the screen edge: a pill or bar is 9 px high.
- At least 1 px between any two elements. A descender may cross a gap if it touches nothing.
- 4 lines of text where they fit: pages are a plain title line (icon, title, page dots) and 3 lines. The menu has 3
  rows (a 9 px bar and its gap per row do not fit 4 times).
- Main screen grid: top line rows 0..6 (signal, channel, vehicle voltage, remote battery), middle rows 8..22 (the
  state word with a dot, or an alarm banner with an icon), bottom rows 23..31. Bottom left above the speed switch:
  `OFF` as plain text, `SLOW` / `FAST` in a pill; bottom right above the frame switch: `BODY` plain, `WORLD` in a pill;
  between them the page dots, a marker pill or "thank you". During an alarm (a full-width banner leaves no gap above
  9 px pills) the bottom row is the alarm's explanation in plain text.
- The alive sign: a one pixel gap travelling round a dot's edge every few frames (the screen is still being drawn),
  one per screen: the state dot on the main screen (the page dots stay still there), the active page dot on the pages
  ([alive_enabled.gif](alive_enabled.gif), [alive_page.gif](alive_page.gif)). A hung loop no longer freezes the screen
  for long anyway: the watchdog reboots the remote within a second.
- Signal: a 16 px wedge filled column by column (16 steps) with the weaker of the two RSSIs, the rest a dotted
  baseline; a blinking cross without link; one pixel beside it flips with every reply.

Transitions (owner's choice of the ideas rendered as GIFs, "A and E"): between two pages the header rolls vertically
(the old title up and out, the new one up from below) while the active page dot glides to its new place and the
content slides sideways in the pushbutton's direction; from the main screen its top line rolls into the page header
(160 ms, easing out) ([transition_pages.gif](transition_pages.gif), [transition_from_main.gif](transition_from_main.gif)).
The menu and the calibration slide as a whole; a new alarm wipes in from the left (100 ms). The blending
(`lib/RemoteUi/src/RemoteTransitions`) is applied when the off-screen frame is copied to the display, so the loop and
the controls never wait for it; the Mac renderer uses the same code.

The gap check in `render.py` fails when one connected shape reaches across a layout's gap rows.

### Checked on the bench (to do)

Directions to confirm on the hardware: stick up = previous menu item (`getYValue()` positive), which speed switch
position reads as LOW (state 0). Then the bench cases of H14 (hang, crash, boot loop, menu at OFF, pushbuttons, stick
check at boot, EEPROM, T8/T9 regression) and the visual checklist.

## Tier B: protocol v2, remote and receiver flashed together

B1. Remove the receiver's leftover motion code: `updateRobotLogic`, `MotionControl`, `ServoMotor`, the motion fields of
`RobotConfig`. Stacato does not depend on it (bit 7 comes from `remoteProxy`). Check first that nothing is connected
to the servo connectors of the show receiver.

B2. Separate IDs per direction in the 3-bit header: uplink `0b101`, reply `0b110` (v1: `0b111` both ways). A
mismatched pair does not connect (Stacato sees no handheld, safe); the remote shows `RX FW OLD` when it receives v1
replies. Fixes the cross-parsing finding above. The uplink is otherwise unchanged, E-stop number included.

B3. The reply shrinks to 4 bytes:

| Byte | Contents |
| --- | --- |
| 0 | `[2:0]` ID `0b110`, `[6:3]` tx counter echo, `[7]` receiver function button |
| 1 | `[3:0]` Stacato `MAV_STATE` (0 to 8; `0xF` = no Stacato heartbeat), `[4]` `MAV_MODE_FLAG_SAFETY_ARMED` (no E-stop latched in Stacato), `[5]` the receiver decodes the remote's E-stop as clear, `[7:6]` reserved |
| 2 | detail code (Tier C), 0 until then |
| 3 | slow value, slot = `echo & 7` (each refreshed every 400 ms at 20 Hz): 0 robot battery V (45 to 70 V), 1 battery % (7 bits) and low flag, 2 receiver RSSI of the remote, 3 receiver radio temperature, 4 receiver radio recoveries, 5 receiver firmware hash byte, 6 and 7 spare |

Removed: motor byte, three velocity bytes, speed and direction echo.

B4. The remote maps the state:

| Byte 1 | Shown |
| --- | --- |
| `0xF` | `NO STACATO` |
| BOOT | `BUS START` |
| POWEROFF | `BUS OFF` |
| UNINIT | `VEHICLE OFF` |
| EMERGENCY, not armed | `E-STOP` |
| EMERGENCY, armed | `AXIS FAULT` |
| CRITICAL | `STOPPING` |
| STANDBY, CALIBRATING, ACTIVE | `Disabled`, `Enabling`, `Enabled` |

LED meanings stay as D-27 records them. The remote also gets the full Tier C detail table in this tier, so Tier C
needs no remote flash.

B5. E-stop confirmation from byte 1 bit 5: `E-STOP` with a check mark once the receiver decodes it, dots until then.

B6: withdrawn (the RadioHead header carries frame bytes 0 to 3, see Airtime).

B7 (option). Uplink 8 to 7 bytes by dropping right stick Y (Stacato stores `MANUAL_CONTROL.r` and never uses it).
Moves the E-stop number bytes.

Tests: bench with `tools/mavlink_mock` (Stacato repo): every `MAV_STATE` with and without the armed flag, Stacato
down, USB unplugged, E-stop and its confirmation, mismatched pairs, two remotes on one channel, airtime on the scope.
Then the handheld block A4 of the sprint plan on the vehicle (E-stop, presence), since the safety path's framing
changed.

Size: 1 to 2 days with the checks.

## Tier C: Stacato fills the detail byte

Stacato packs a summary into `HEARTBEAT.custom_mode` (32 bits, 0 today) and sets `MAV_MODE_FLAG_CUSTOM_MODE_ENABLED`;
no new message, no radio change. The receiver copies 8 bits into reply byte 2. Proposed (Stacato O-19):

| Detail bits | Content | From the RT feedback (`OdometryFeedback`) |
| --- | --- | --- |
| `[3:0]` | reason: none, handheld E-stop, server E-stop, local GUI E-stop, STO pendant, axis fault, stream lost or deadman, needs neutral (D-65), enable refused, configuration not accepted | `stopReason`, `stopLatchedSources`, `needsNeutralSources`, `refusedCommandCount`, `configAccepted` |
| `[5:4]` | who drives: none, handheld, server, local GUI or playback | `activeSourceId` |
| `[6]` | playback executing | `playbackExecuting` |
| `[7]` | the handheld's jog runs in high speed | `activeJogHighSpeed` |

Stacato side: a D-nn, a tracker row, a heartbeat packing test with `ctest`, the mock. The MAVLink thread already reads
`feedbackSlot` for `ODOMETRY`; check that this reader is listed in section 3 of the threading contract. A true command
verdict stays with Stacato O-4.

Size: about a day. Flashes: receiver (and Stacato); remote only if Tier B was skipped.

## Order

Tier A before the show (remote only). Tier B before the show only with bench time for the vehicle E-stop re-run.
Tier C after the show, or with B.

## Open decisions

1. B7 (uplink 8 to 7 bytes): include or skip, with Tier B.
2. Tier C detail byte: Stacato O-19.
