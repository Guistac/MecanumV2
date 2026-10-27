# Remote display and radio status (plan, 2026-10-26)

Plan for the remote's OLED (SSD1305, 128 x 32) and for what travels back over the radio, in three tiers. Tier A is
decided (D-147) and implemented on branch `remote-tier-a`, not yet run on hardware; Tiers B and C are plans. Findings
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

## Tier A: remote only, no protocol change (decided: D-147)

Decided in the interview `docs/architecture/decisions/archive/REVIEW_HANDHELD_DISPLAY.md` (Stacato repo, H1 to H16), transcribed as
D-147 (drafted as D-146, which another decision took first: commits `f487c58` and `a0f6d01` say "D-146 draft"). Implemented on branch `remote-tier-a`.

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
| `lib/RemoteLogic/src` | hardware-free logic, tested on the host: `StickGuard`, `AxisStepper`, `EstopContacts`, `BatteryMonitor`, `RemoteSettings`, `SettingsMenu`, `PageNavigator`, `StatusView` (big line and line 3), `Dimmer`, `BootGuard` (`ResetHistory`), `UplinkFrame`, `LinkMonitor`, `LedPattern`, `StickCalibration` (math, storage, procedure), `Stopwatch` (D-160); `RobotStates.h` and `LiIonBattery.h` moved here |
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
  between them the stopwatch (Stopwatch (D-160) below; the Status page has no page dots), or while they last a
  marker pill or "thank you". A countdown adds its bar across the screen in row 8. During an alarm (a full-width banner leaves no gap above
  9 px pills) the bottom row is the alarm's explanation in plain text.
- The alive sign: a one pixel gap travelling round a dot's edge every few frames (the screen is still being drawn),
  one per screen: the state dot on the main screen, the active page dot on the pages; while sticks are to be released
  the main screen has no state dot, and a single 3 x 3 dot at the right end of the bottom row's centre span carries it
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

Startup (owner's request, 2026-10-27): the BERUPO logo ([berupo_logo.png](berupo_logo.png)) replaces "Becker Poels".
The screen still fills white from the left and clears from the left as before; in between the black letters follow
the motion of the Game Boy Color start screen: each comes in three times its size from below left 75 ms after the one
before, arcs up past its place and drops shrinking, landing closer to the word's centre (0.7 of its distance), slides
onto its place speeding up (210 ms) and hops there twice (0.34 of its height in 270 ms, then 0.1 in 180 ms, ending at
rest), the hops running across the word as a wave; slide and hops measured frame by frame on the recorded start
screen. A half-tone shine crosses the word as the last letter's first hop ends, then it clears. 2.1 s in all, then the
screen reveals itself as before ([startup.gif](startup.gif)). `tools/screens/logo.py <picture>` regenerates
`lib/RemoteUi/src/BerupoLogo.h` (each letter at 4x, 24 px tall, gloss marks filled in); `RemoteSplash` draws it.
Both stick buttons held at power on: the former "Becker Poels" startup instead (0.75 s, unchanged).

### Stopwatch (D-160)

Owner's request (2026-10-27), for rehearsals driven from the handheld alone: a stopwatch and countdown on the main
screen. Display only: nothing changes in what the remote sends (the stick button bits go out as before, Stacato
ignores them). Logic in `lib/RemoteLogic/src/Stopwatch.h` (`Stopwatch`, `StopwatchControl`), host tested; the set time
lives in RAM only (kept until power off).

Controls, only on the Status page showing a state: not on any other page, not with the menu open, in the games or
their loading, in the calibration, before the sticks are zeroed at startup, during an alarm or while sticks are to be
released. A press that happened elsewhere is spent: holding a button while coming back does nothing.

- Right stick click: start / stop (stopped: start continues).
- Left stick click: reset (to 0:00, or to the set countdown) and stop.
- Both together: nothing. A click acts 50 ms after its press (or on its release, if sooner) and only if the other
  button stays up; a press while the other one is held is ignored (except in edit mode, where the right click clears
  the set time). The time is taken at the press, so nothing is lost.
- Edit mode: hold the left stick button with the speed switch at OFF (its press resets first, so the timer is never
  running). While held (owner, after trying it: second-accurate, quick for minutes):
  - right stick up / down: +/- 1 s (first step on the push, repeats after 400 ms every 150 ms, every 80 ms once
    held 1.5 s; always 1 s);
  - right stick right / left: +/- 1 min (first step on the push, repeats after 400 ms every 200 ms);
  - a diagonal push counts as its stronger axis;
  - right stick click: clear to 0:00 (counting up), BUMP;
  - 0:00 to 99:59: a step past either end stays there (BUMP); stepping down to 0:00 also sounds BUMP.

  Letting go (or the switch leaving OFF) ends it: the timer stands at the new time. A stick already out when edit
  mode starts does nothing until it has come back. During the edit hold the
  sticks-at-OFF grind and the OFF blink stay quiet (only then).

The two stick buttons are otherwise unused inputs, and the timer interferes with nothing: its state lives in the
board and is never paused or reset by a page change, the menu, the calibration, the games, the USB/radio handover or an
alarm; it keeps counting, shown on the Status page only. The time-up beeps play wherever the remote is (they may cut
a game effect). Nothing it does changes the frame sent, the stick guard, the zero capture, the arcade gate or the
menu.

Screen, bottom centre between the speed and frame labels, centred on x 63 and kept 1 px clear of both:

- Counting up or idle: plain digits, `m:ss.t` below 10 minutes, `mm:ss` from 10 (`0:00.0`, `2:13.4`, `12:05`).
- Countdown: plain digits (the time left, rounded up to the tenth) and a 1 px bar across the whole width in row 8,
  lit from the left for remaining / set time, so it shrinks from right to left; idle with a set time it is full.
  Row 8 is the only row that keeps 1 px clear of the top row (row 7 empty); the state word's letters start at row 9,
  so the bar leaves a notch over each letter top (the letter's columns and one either side). Nothing touches and
  nothing else moves. The other candidates, rendered and rejected: moving the state word down 1 px (bar in row 8)
  makes its baseline touch the bottom row's pills (SLOW / FAST, WORLD, the edit and overtime pills) in 4 of 5
  example screens; the bar at the bottom edge with the bottom row 1 px up touches every pill along the bar and the
  word's baseline as well. The screen has 32 rows and the full layout uses all of them, so one of the three had to
  give; the notches cost nothing else.
- Past zero it does not stop: negative time counting away from zero (`-0:05.0`, `-1:12.3`) in a pill, the pill and
  plain text taking turns every 0.5 s, the bar full and blinking with it; stopped there, pill and bar steady, the
  time frozen.
- Stopped (otherwise): the digits blink slowly (0.8 s on, 0.4 s off).
- Edit mode: a steady filled pill `SET 2:30` (`SET 0:00` = counting up) with a small stick mark: up and down
  triangles (seconds) and a dot either side (minutes).
- Markers (`REBOOT`, `RST`, `HOT`, `SAFE`) and "thank you" show over it while they last; during an alarm the row is
  the alarm's note as before, the bar is hidden too, and the timer runs on.

Sounds (D-158 families): start A7, stop A6, reset D6, edit entered C8, edit left C7, each one plain square (interface,
setting All); steps the VALUE sound, the ends and clearing BUMP. Time up: three clean rising beeps F#6 A#6 C#7, once
at zero, alarm priority (plays at Alarms only, not at Off; it does not cut a status alarm that is sounding).

### Games (easter egg, D-157)

Six games on the sticks: Pong, Parking (the Mecanum vehicle, driven as in body frame), Cave, Rocks (twin-stick
shooter), Snake and Blocks (Tetris turned sideways, the floor on the left). Code in `lib/RemoteGames` (one class per
game with its rules, drawing and a built-in demo player; the `Arcade` runs them), host tests in `test/test_arcade` and
`test/test_game_<name>` (every game passes `GameContract`: deterministic, the demo ends within 3 minutes, a tick under
2 ms on the Mac). `python3 tools/screens/render_games.py [name|all|arcade]` records them playing themselves as GIFs.

- In: both stick buttons held 2 s on the About page, with the speed switch at OFF, the sticks zeroed and the own
  E-stop clear. A button already held when the page opens has to be released first.
- Picker: the pushbuttons choose a game, which plays itself behind its name; a stick click starts it. Playing: the
  left pushbutton goes back to the picker, the right one pauses. Game over: a click plays again, a pushbutton goes
  back to the picker. The best result of each game is kept until the remote is switched off (RAM only: the EEPROM is
  written only on SAVE).
- Out: both stick buttons held 2 s again, 2 minutes without input, the speed switch leaving OFF, the own E-stop or a
  new alarm (the status screen then shows at once).
- Safety: while a game runs nothing of the sticks is sent (held neutral like in the menu, their buttons masked); after
  leaving, the sticks are sent again only once both have been released. The radio, the watchdog, enable, disable and
  the E-stop are untouched; a game never sees them or the speed switch.

### Sound (D-158)

A 12 mm piezo disc between pins 19 and 18, 220 ohm in series, pressed against the case over a hole. Audible from
100 to 5000 Hz.

- **Engine** (`lib/RemoteAudio`): `Sound.h` (notes, cues, priorities, the `SoundSink` a part plays into, the
  `SoundOutput` the board implements; notes are timer squares, synth voices or two-leg duets), `SoundPlayer.h` (two
  layers: front for interface sounds, alarms and game effects, back for a game's music; a cue never cuts off a higher
  priority one), `Synth.h` (eight voices: square and pulse with smoothed edges, sine, triangle, noise, recorded
  clips; level, attack, decay, start delay). The board's end is `src/RemoteBoard/RemoteSound.h`: timer square waves
  on both legs in opposite phase (loudest), or the synth as a PWM DAC (62.5 kHz carrier, 31.25 kHz samples, both
  legs mirrored).
- **The remote's sounds** (`lib/RemoteLogic/src/RemoteSounds.*`): the table of cues and when they play. Two families
  that cannot be confused: interface sounds are one short square on the C pentatonic (page changes, a note per page;
  menu item and value, a bump at the ends; Save and discard; enable and disable ticks; the speed and direction
  switches; a buzz when enable cannot work); status sounds are several notes off that scale with an echo, or rough
  two-leg buzzes (enabling, enabled, disabling, disabled, E-stop engaged (harsh), E-stop released, link found; one
  warning for link lost, radio fault, E-stop wiring, battery). Status sounds follow the state the receiver reports.
  NO LINK: a falling pair every 2 s with three fading echoes, the LEDs flashing and echoing with it. A stick pushed
  at speed OFF: a continuous grind, and OFF blinks on the Status screen.
- **Startup** (`lib/RemoteUi/src/StartupScore.h`): the owner's recording (`tools/sounds/startup.m4a`) taken apart by
  `tools/sounds/partials.py` into a chord and a bell, played one note at a time as synth squares: an arpeggio with
  the letters, the bell with the shine. Played as samples (`tools/sounds/clip.py`) it was faint and noisy on the
  disc.
- **Sound setting**: All, Alarms only (default), Off (no startup sound either); the games and their way in (the About
  page's loading tune) play unless Off.
- **Games** (next): a game gets a `SoundSink` and plays cues (`soundCue(notes, SoundPriority::GAME)`) or notes made
  at run time (`playNote`), music on the back layer with a loop length.

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
