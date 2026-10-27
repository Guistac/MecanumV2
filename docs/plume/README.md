# Plume - design

> **STATUS 2026-10-27**: implemented on `remote-tier-a` [D-157] as the arcade's seventh game:
> `lib/RemoteGames/src/PlumeGame.{h,cpp}`, tests in `test/test_game_plume/`. The arcade adaptations are listed in
> "In the arcade" below; where this design and the code differ, the code is the truth. Every number is still a
> starting value to tune on the device.

A one-stick game for the handheld remote. You are a small fire extinguisher canister drifting in a space shaft,
propelled by its own CO2 plume. Gravity pulls you down, the plume pushes you anywhere, and the plume also blasts
away the rock it hits. Climb as high as you can. Relaxed: nothing chases you, and fuel is infinite.

## Frame and screen

- The remote is held sideways. On the 128x32 OLED, **up in the game is screen right**, gravity points
  **screen left**. The shaft is 32 px wide and endless.
- Scale: 10 px = 1 m. One screen covers 12.8 m of altitude.
- During play the only overlay is the HUD in the top right corner: the hull (four pips), the altitude and the points
  (see "Collectibles and the altitude/points display" below). Text uses the 5x7 font (remote OLED rules), drawn
  sideways to read in the sideways grip.

```
 bottom (fall out = game over)                         top: new terrain appears here
 |                                                                       ▪▪ ▪▪ ▪▪ ▪▪ <- hull
 |              ▓▓▓                    ███                   ▓▓▓▓
 |   ~~~####.  ▓▓▓▓▓       ███         ███        ▓▓▓       ▓▓▓▓▓
 |      #####   ▓▓▓        ███                   ▓▓▓▓▓       ▓▓▓
 |   ~~~####.                                     ▓▓▓
 |<-- 40 % -->|
               camera keeps the ship at most here
```

## Controls

- **The stick aims the plume** (owner decision 2026-10-27): the spray leaves the canister in the stick's direction
  and the canister is pushed the opposite way. Thrust magnitude proportional to deflection after a deadzone of 0.12
  (on a -1 to 1 stick range).
- **Both sticks do the same thing**: each frame the stick with the larger deflection is used. The two are never
  added, so two hands never give double thrust.
- Stick push: not used by the game (the arcade starts it with a stick click; the right pushbutton pauses).
- Axis signs to check on the device: stick right must spray towards screen right (the canister pushed towards
  screen left, down the shaft), stick up must spray towards screen top (row 0).
- History: the first version read "thrust direction = stick direction, the plume leaves the canister on the opposite
  side". The owner played it on the device and found it inverted ("plume controls are inverted"): one aims the
  extinguisher's spray, not the canister. Changed on 2026-10-27; the demo autopilot writes the stick the same way.

## Physics

Fixed step, 50 Hz (the arcade's tick; every value below is per second, stepped with 0.02 s). Starting values, to be
tuned on the device:

| Quantity | Value |
| :-- | :-- |
| Gravity | 40 px/s2 towards screen left |
| Full thrust | 88 px/s2 (2.2 times gravity; about half stick hovers) |
| Drag | linear, `v -= v * 0.6 * dt` |
| Speed cap | 90 px/s |

The canister keeps its inertia: letting go of the stick does not stop it.

## Camera, score and game over

- The camera only moves up. `cameraX = max(cameraX, shipX - 51)`: the ship is kept at most 40 % of the screen
  width (51 px) from the bottom edge. When the ship falls back, the view stays put.
- **Game over** when either:
  - the ship leaves the bottom of the screen (`shipX < cameraX`), whatever hull is left; or
  - the hull reaches zero.
- **Score**: first the highest altitude reached in metres, which is `(cameraX + 51) / 10` once the camera has moved;
  since the collectibles (below), **points = 1 per metre of altitude reached + 25 per gem**. The best scores are the
  arcade's top 3 (kept in EEPROM, slot 6), ranked by points and shown as `1450`.

## Hull

- 4 points, shown as four 2x2 pips in the top-right corner, drawn XOR so they stay visible over rock. They run down
  the right edge (columns 125 and 126, rows 1 to 11), so they read as a row in the sideways grip.
- Touching rock costs 1 point. The ship is pushed out of the rock and bounces: the velocity component along the
  contact normal is reversed and scaled by 0.4. The normal is estimated from which pixels around the ship are
  rock.
- After a hit, 1 s of invulnerability; the ship blinks. Scraping along a wall therefore costs one point, not one
  per frame.
- A lost pip blinks twice before it disappears.
- Repair: +1 point (up to 4) for every 50 m of new altitude climbed without a hit, and +1 for each heart collected.
- The long screen edges (rows 0 and 31) are the shaft walls: touching them counts as a hit, like rock.

## Plume

- Pool of 48 particles, one pixel each. Emission rate proportional to thrust, up to 120 particles/s at full stick.
- A particle starts at the nozzle side opposite the thrust, with the ship's velocity plus 70 px/s along the plume
  direction, spread by up to 12 degrees either side; it lives 0.3 s.
- **Erosion**: a particle that enters a rock pixel clears that pixel and dies. Particles never hurt the ship.
- Digging therefore means pointing the plume at the rock, which pushes the ship away from it: to dig a plug above
  you, you must thrust down, against your own climb, and gravity helps.

## Terrain

### Storage

- One `uint32_t` per world column, bit `y` set = rock at row `y`. Set, clear and test are single bit operations.
- A ring buffer of 256 columns (1 KB) holds the screen plus what is generated ahead. Columns that fall below the
  camera are never needed again (falling out is game over), so they are overwritten.
- Generation runs ahead of the camera: every column up to `cameraX + 128 + 64` exists.

### Generation, three layers per column

1. **Blobs**: 2D smooth gradient noise (Perlin-style) sampled at every pixel, `rock = noise(x / size, y / size) >
   threshold`. Gives organic caverns and floating lumps. Float is fine (Cortex-M7 FPU); 32 samples per new column.
2. **Boxes**: axis-aligned rectangles stamped on top of the blobs at random positions. Hard-edged contrast to the
   blobs, from small crates to long slabs.
3. **Plugs**: from time to time a wall across all 32 rows, with a noisy edge. It has to be cut through with the
   plume. Everything is erodible, so the shaft is always passable.

The first 20 m are empty (a calm area to learn the controls). The generator is seeded per run, so every run is
different.

### Difficulty

Every knob follows `d = min(altitude_m / 500, 1)` and interpolates linearly from the start value to the cap value,
so the game gets harder smoothly and stops getting harder at 500 m.

| Knob | Start (d = 0) | Cap (d = 1) |
| :-- | :-- | :-- |
| Blob coverage (via the noise threshold) | about 8 % of the shaft | about 35 % |
| Blob feature size | 24 px | 10 px |
| Boxes per 100 m | 2 | 8 |
| Box size | 3x3 to 6x6 | 3x3 to 12x10 |
| Plug spacing | 150 m | 60 m |
| Plug thickness | 2 px | 6 px |

## Graphics

- Rock: solid pixels.
- Ship: a 5x3 canister, nozzle towards the top of the game (screen right). It never rotates; the plume shows
  where it pushes.

  ```
  ####.
  #####
  ####.
  ```

- Plume particles: single pixels.
- Full frame redrawn each step: terrain columns from `cameraX`, then the free items, particles, ship (blinking
  while invulnerable), then the HUD (hull pips, altitude, points; XOR). 512 bytes over SPI at 7 MHz is about 0.6 ms per frame.

## Screens

The arcade owns the title (the picker), pause (the help card) and game over screens; Plume draws only play: the
shaft, the items, the canister, the plume and the HUD (hull pips, altitude, points).

## In the arcade [D-157]

- Entered and left like every arcade game (docs/arcade/README.md): a stick click in the picker starts it, the right
  pushbutton pauses, the left one goes back to the picker. Plume's own title, pause and game over screens and its
  stick-push pause are dropped.
- 50 Hz instead of 60 Hz, with the per second values as designed.
- Score: the points (1 per metre of the highest altitude above the start, 25 per gem). The canister starts 1.2 m up
  the shaft, so a run that never climbs scores 0 and does not enter the top 3. The game over screen shows
  `PTS 1450 412M` (the points and the altitude) through the arcade's `Game::overText`.
- Falling out: the game ends once the whole canister has left the bottom of the screen. The hull at zero: the
  canister bursts into particles for half a second, then the game is over.
- Hull contact with the long screen edges: a canister pixel in row 0 or row 31.
- The first plug comes at 50 m (not after the first 150 m), so digging is learnt early; later plugs follow the
  spacing of the difficulty table, with 15 % of random play.
- The music tempo rises in steps of 5 % with the difficulty, to 125 % at 500 m.
- A demo player for the picker: it climbs along the most open rows, closes in on what blocks the way and blasts it
  in short bursts of down thrust, the stick aiming the plume up (falling back between them), and now and then lets
  go of the stick for a while, so it sometimes falls out. It goes for two out of three items it sees in open space
  ahead (it never digs for one).
- Sound (docs/arcade/README.md section 6): a slow, sparse ambient arpeggio of four chords on the back layer
  (triangle melody, triangle bass, a sine bell now and then; 16 s, looping; written two octaves up, E5 to E7,
  so the disc plays it, see docs/arcade/README.md section 3); soft high noise grains while thrusting,
  denser with more thrust; a tiny crackle when the plume blasts rock (at most every 80 ms); a clunk on a hull hit;
  two rising notes on a repair; a ping every 100 m; a long falling whistle when it falls out; a noise burst when the
  hull is gone; a chime for a gem, an arpeggio for a heart, a click when a buried item is freed. All synth notes, no
  timer squares.

## Collectibles and the altitude/points display (owner, 2026-10-27)

Added after the first implementation, agreed in chat with Leo.

### Scale

10 px of altitude = 1 m (one screen width, 128 px, is 12.8 m). Altitude = how far the canister has climbed above the
start.

### Items

| Item | Look | Effect | Rate |
| :-- | :-- | :-- | :-- |
| Gem | 3x3 diamond | +25 points | about one every 30 to 60 m |
| Heart | small heart, 5x5 or smaller | +1 hull (at most 4) | rare, about one every 150 m |

- About 40 % of the gems and most hearts are **buried**: placed inside rock (blobs, boxes, near or inside plugs). The
  share and the depth of burial grow with the difficulty `d`.
- A buried item is drawn as a **dark cut-out in the rock** (its outline in rock, the item dark), so the player sees it
  and can choose to dig.
- The plume **never destroys an item**: particles that reach an item die there without clearing it. The rock around
  it is eroded as usual. An item is collected when the canister touches it (in open space, or once dug out).
- None in the first 20 m (the calm area). Items are placed by the seeded generator: a run is the same for the same
  seed.

### Score

- **Points = 1 per metre of altitude reached + 25 per gem.** The arcade's top 3 ranks by points.
- `scoreText` shows the points (e.g. `1450`); the game over screen shows both points and altitude (e.g. `PTS 1450` and
  `412M`).

### Display while playing

- Next to the hull pips: the altitude in metres and the points, small digits, drawn XOR so they read over rock.
- The remote is held sideways for Plume (game up = screen right): draw these numbers rotated a quarter turn so they
  read upright in that grip. Keep them compact (they must not hide the terrain ahead more than needed); nothing
  touches (1 px gaps).

### Sounds

- Gem collected: a bright two-note chime.
- Heart collected: a warm rising arpeggio.
- An item freed (its last surrounding rock pixel cleared): a soft click.

### As implemented [D-157]

- **Generator**: the items have their own random generator, seeded from the run's seed, and are placed while the
  shaft's columns are made, so a seed always gives the same items whatever the player does. The first gem comes
  between 24 m and 49 m (never in the calm 20 m), then one every 30 to 60 m; the first heart between 79 m and 139 m,
  then one every 120 to 180 m. Six item slots, reused once an item is collected or has
  gone below the camera.
- **Burial**: a gem is buried with a chance from 30 % (`d = 0`) to 50 % (`d = 1`), about 40 % over a run; a heart
  from 65 % to 90 %. An item placed near or inside a plug is always buried (an open item's clear ring would cut a
  way through the plug). A buried item gets a square lump of rock around it, `depth` px thick, with its own shape cut
  out: `depth` = 1 to 3 with `d`, plus 0 or 1 at random. Its row is the rockiest of four tries in the blob noise; an
  item in open space takes the most open of four tries and has its shape and a 1 px ring around it cleared of rock.
  Items sit in rows 2 to 29, so the canister can always reach them.
- **Free and buried**: an item is buried while any rock touches it (its 1 px ring), drawn as the dark cut-out; once
  the plume has cleared its last touching rock pixel it is free, drawn lit, and the click sounds. Particles die on an
  item pixel, buried or not.
- **Collecting**: a canister pixel on or next to an item pixel (nothing between them), checked at every half-pixel
  step of the canister. A buried item can be collected as soon as a tunnel reaches it, before it is fully free. A
  heart at full hull is still taken (no effect beyond its sound).
- **HUD**: the hull pips moved to run down the right edge (columns 125 and 126, rows 1 to 11: a row in the sideways
  grip); the altitude (`412M`) with its letters' tops at column 123 and the points (`1450`) at column 115, both from
  row 1 down, XOR over the scene, 1 px apart. Five characters fit across the screen (99999 points, 9999M). The text
  is `GameDraw::textSideways`, shared with Blocks.
- **Game over**: the arcade shows the game's `overText` in its pill (`PTS 1450 412M`, then the rank if it entered
  the top 3); its dark box grows when the pill is wider than 92 px.
- **Sounds**: gem: E6 then B6, 25 % pulse; heart: C5 E5 G5 C6, triangle; freed: a 7 ms noise tick at 1500 Hz, 66 %.

## Sideways text in Blocks and Plume (owner, 2026-10-27)

Blocks and Plume are both played with the remote held sideways, the same grip for both: game up = screen right
(Blocks' pieces fall leftward, Plume's gravity points left), so the remote is turned a quarter turn
counter-clockwise. Every number or word either game draws **during play** (Plume: altitude, points; Blocks: its
score, lines, level, next piece label or whatever it shows) is drawn rotated to read upright in that grip: text runs
from the screen's top edge towards its bottom edge, the tops of the letters towards the screen's right edge. One
shared helper for both (in the games' drawing helpers, `GameDraw`, e.g. `textSideways`), same 5 x 7 font and rules
(nothing touches, XOR or a dark margin over the play field). Check the direction on a rendered sheet: turn the
picture a quarter turn counter-clockwise and the text must read normally.

The arcade's own screens (picker, cards, pause, game over, name entry) stay landscape: they are used with the remote
held normally.

As implemented: `GameDraw::textSideways(out, topX, y, text)` (and `textSidewaysCentered`, and `textSidewaysXor` with
a callback that says which screen pixels are lit, since the display cannot be read back) in
`lib/RemoteGames/src/Game.{h,cpp}`: `topX` is the screen column of the letters' tops, the text covers `topX - 6` to
`topX` and runs down from row `y` for `textWidth(text)` px. Blocks draws its score centred across the screen at the
right edge (columns 121 to 127; `123K` from 100000 on, so it fits) and its level (`L3`) just right of the entry line
(columns 88 to 94); the next piece between them is drawn as it will enter. Blocks shows no other text during play.

## Open items

- Tuning: every number above is a starting value for the device.
- Later, not now: indestructible hard rock as a second material.
