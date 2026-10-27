# Arcade: sound, music, help, scores and Plume - design

> **STATUS 2026-10-27**: implemented on `remote-tier-a` (decision D-157 for the games, D-158 for the sound, in
> Stacato's `DECISIONS.md`), flashed and played on the remote. This was the brief of the multi-agent run; later owner
> changes (louder music, the picker's beat, the mute hint, Plume's aiming, gems and hearts, sideways text) are in the
> code, `docs/plume/README.md` and the remote-display doc. Where they differ, the code is the truth.

This document is the brief for every agent of the run. Read it whole before touching code. Owner rules that apply
everywhere: the remote OLED rules (5 x 7 font for small text, negative text only in a filled pill with 1 px of fill
around it, nothing touches, 1 px gaps), the existing code style (comments like the surrounding code, `b_` booleans,
no allocation in the loop, no `Serial` printing), and no Stacato wording such as master/slave.

## 1. What is being added

1. **Plume**, a seventh game (design: `docs/plume/README.md`, adapted in section 8 here).
2. **Sound and music in every game** and in the arcade's own screens (sections 3 to 6).
3. **Sound modes** chosen in the arcade: SOUND ON, MUSIC OFF, FX OFF, MUTE (section 5).
4. **A help card** per game, and the pause screen showing it (section 5).
5. **Best scores kept in EEPROM**: a top 3 per game with a 4-letter name, entered at game over (section 7).

## 2. Who does what (lanes)

| Lane | Owns (only these files may change) |
| :-- | :-- |
| Foundation (first, alone) | `lib/RemoteAudio/*`, `lib/RemoteGames/src/Game.{h,cpp}`, `lib/RemoteGames/src/Arcade.{h,cpp}`, new arcade helper files in `lib/RemoteGames/src/`, `lib/RemoteLogic/src/RemoteSounds.*` (only if needed), `src/RemoteBoard/*` (sink, store, held pushbuttons), `test/test_arcade/`, `test/test_remote_logic/` (only if engine tests need it), `tools/sounds/` (WAV renderer) |
| Plume | new `lib/RemoteGames/src/PlumeGame.{h,cpp}`, new `test/test_game_plume/`, the game list in `Arcade.{h,cpp}` (add Plume as game 7, nothing else), `tools/screens/render_games.*` (only to include Plume) |
| Blocks + Snake | `BlocksGame.*`, `SnakeGame.*`, their test folders |
| Rocks + Pong | `RocksGame.*`, `PongGame.*`, their test folders |
| Parking + Cave | `ParkingGame.*`, `CaveGame.*`, their test folders |
| Integrate (last) | merges the lanes, docs (`docs/remote-display/README.md` Games and Sound sections, this file's status) |

A game lane never edits the engine, the Arcade or another game. If it needs something from them, it says so in its
report instead.

## 3. Sound engine changes (foundation)

Current engine: `lib/RemoteAudio/src/Sound.h`, `SoundPlayer.h`, `Synth.h`. Changes:

1. **Three voices on the back layer.** `SoundNote::voice` is 0, 1 or 2 within a layer. Front keeps synth voices 2
   and 3 (a front note with voice 2 uses its voice 1); back uses synth voices 0, 1 and 4. Music is melody (0), bass
   (1) and drums (2).
2. **More notes per cue**: `SoundCue::noteCount` becomes `uint16_t` (a looping tune can exceed 255 notes).
3. **Tempo per layer**: `SoundSink::setTempo(SoundLayer, uint16_t percent)`, 100 = as written. The player advances a
   layer's cue time by `elapsed * percent / 100` and shortens note lengths and decays by the same factor. Changing
   it while playing keeps the position (no jump).
4. **`SoundSink::isPlaying(SoundLayer)`** in the interface (the player already has it), so a game can avoid cutting
   its own one-shot effect with a continuous grain.
5. **Sound kit** `lib/RemoteAudio/src/SoundKit.h`: small helpers every lane uses, so the games sound like one set:
   - `SoundKit::hertz(midiNote)` (A4 = 69 = 440 Hz) and note-name constants or a table.
   - Drum notes for voice 2 of the back layer, all synth, written for the disc (amended 2026-10-27 after the
     owner played the flashed build): **kick** (a 10 ms noise thump, then a square sweeping fast from 500 Hz to
     150 Hz, six notes in about 50 ms; the first version, a square from 180 Hz to 50 Hz, was not heard), **snare**
     (90 ms of bright NOISE at 7000 Hz, fast decay), **hat** (20 ms of NOISE at 10000 Hz, quieter).
   - A **sweep** (a note gliding from one pitch to another as short steps) for zaps, whistles and risers.
   - A **noise burst** (crash, explosion), its brightness from the NOISE pitch.
   - A **grain** helper for continuous sounds (rotor, motor whine, plume hiss), like `RemoteSounds::grindGrain`: a
     short note every period, made at run time with `playNote`.
   - A **tracker-style builder** that fills a `SoundNote` array from a compact list (step, voice, midi note, length
     in steps, level) and a step length in ms, so a tune is written as data, not as hand-computed milliseconds.
6. **Host WAV renderer** `tools/sounds/render_cues.cpp` (+ a small Python or shell runner): runs a cue or a game's
   sound calls through the real `SoundPlayer` and `Synth` on the Mac and writes a WAV (timer squares rendered as
   ideal squares). The owner listens to the music before flashing; lanes use it to check lengths and loops.

Rules for every sound (all lanes):

- **A timer square (`Kind::SQUARE` / `DUET`) silences every synth voice while it sounds**, the music included. In a
  game that has music, effects are synth notes; squares only for a crash, a death or a game over, where cutting
  the music is the point.
- **Continuous sounds are grains on the front layer** (one short note per period from `update()`), never a held
  note, and skip a grain while a one-shot effect is playing (`isPlaying(FRONT)`), unless the effect is less
  important.
- **The back layer is music** (what MUSIC OFF silences). Rocks' pulse is music. Ambience such as the motor whine is
  front-layer grains (FX).
- **Sound never touches game state**: a game's sound uses its own small random generator, never the game's
  `GameRandom`, so a game and its demo stay deterministic.
- Games call sound only through `Game::sound()` (section 4); in the picker's demo it is silent.
- Priority: everything a game plays is `SoundPriority::GAME`; the remote's alarms (status, warnings) still cut in.
- **Write for the disc** (owner, 2026-10-27: the first music was far too quiet on the device). The 12 mm piezo is
  loud only from about 1 kHz to 5 kHz and nearly silent below about 400 Hz, and the synth sums its voices and clips
  at 1. Melodies sit in 900 Hz to 3 kHz on bright waves (SQUARE, PULSE25, PULSE12) at full level; basses are short
  (staccato) and no lower than about 300 Hz, quieter than the melody; voices sounding together are staggered or
  short so their sum does not clip hard. `tools/sounds/render_cues.py` prints each render's **disc loudness**
  (the RMS after a model of the disc, a 900 Hz high-pass and a 6 kHz low-pass, over the 50 ms windows where
  something sounds): a game's music should come within about 3 to 4 dB of its effects.
- Levels: the piezo is quiet below about 60 % synth level; squares are the loudest. Keep effects short (most under
  150 ms) so they never smear into each other at 50 ticks a second.
- Large note arrays: `static const` with `PROGMEM` (defined empty on the host).

## 4. Games and the Arcade (foundation)

- `Game` gets `void setSound(SoundSink* sink)` and a protected `SoundSink& sound()` that returns a silent sink when
  none is set. The Arcade sets a silent sink for the picker's demo and its own filtering sink (section 5) while a
  game is played. Tests set a recording sink.
- `virtual void playMusic() {}`: the Arcade calls it when a game starts, on resume and when music is switched back
  on. A game with music starts its tune there (back layer, looping, `setTempo` for the current level). It changes
  the tempo itself as its level rises.
- `virtual void helpLines(const char* lines[3]) const`: up to 3 lines of at most 21 characters (5 x 7 font, 6 px
  pitch), upper case, how to play this game. Unused lines nullptr. Abbreviate (`L STICK`, `CLICK`, `UP/DN`).
- A **recording sink** for tests in `lib/RemoteAudio/src/RecordingSink.h` (header only): keeps the last N cues and
  notes played per layer, the tempo, stop calls. Every game test checks its key sounds fire and its demo is silent.
- `virtual void overText(char* out, size_t size, uint32_t value) const`: the game over pill's text, `scoreText` by
  default; a game may say more (Plume: `PTS 1450 412M`). The dark box behind the pill grows to fit it.
- `GameDraw::textSideways` (and `textSidewaysCentered`, `textSidewaysXor`): the 5 x 7 font a quarter turn for the
  games held sideways (Blocks, Plume): the text runs down the screen, the letters' tops towards the screen's right
  edge. The arcade's own screens stay landscape.
- Game over: the game plays its own death or crash sound; the Arcade stops the back layer and plays its game-over
  phrase on the **back** layer with its first note at 400 ms, so it never cuts the game's crash sound.

## 5. Arcade screens and controls (foundation)

Inputs the arcade has: the two pushbuttons (pressed edges and held state; the board passes both), the sticks and
their buttons. A **flick** is one stick (either) passing 0.6 along one axis from below 0.3, the dominant axis
deciding the direction; it re-arms once back under 0.3.

**Picker** (as today: pushbuttons choose, a stick click starts, the game plays itself behind its name):
- **Flick up**: next sound mode, SOUND ON, MUSIC OFF, FX OFF, MUTE, round. The bottom pill shows the new mode
  for 1.5 s as `SOUND: ON`, `SOUND: MUSIC OFF`, `SOUND: FX OFF`, `SOUND: MUTE`. A blip when the mode allows effects. The mode is saved in EEPROM (section 7).
- **Flick down**: the game's **top 3 card**, full screen, 4 text lines: the game's name, then `1 LEO   12450`
  lines (rank, name, score via `scoreText`); empty ranks as `-`.
- **Flick right**: the game's **help card**, full screen: its 3 help lines, then the arcade line
  `<> GAME ^SOUND vBEST`.
- A card closes with any stick click, a pushbutton (back to the picker on the same game) or the opposite flick, or
  after 8 s.
- **Both pushbuttons held 3 s**: `CLEAR TOP 3?` pill; a stick click within 3 s clears this game's table (and plays
  a falling phrase), anything else cancels.
- The bottom pill shows the record holder: `LEO 12450`, or `CLICK TO PLAY` when the table is empty. Each time the
  arcade opens it first shows `FLICK UP: SOUND` for 2 s (owner, 2026-10-27: muting was not discoverable).
- A small sound-mode icon (7 x 7 or smaller, bottom-left corner, not touching the pill): a speaker, a speaker with a
  note crossed (MUSIC OFF), a note only (FX OFF), a crossed speaker (MUTE). Draw them as bitmaps. A 5 x 7 up arrow
  one column to its right (x 8 to 12) says a flick up changes it; x 13 stays dark, the widest pill starts at x 15.
- **A drum beat** behind the picker and its cards (`ArcadeSounds::pickerBeat`, back layer, looping: kick, snare,
  hats and a bass blip on the root, 120 a minute, two bars), started when the arcade opens and on every return to
  the picker, stopped when a game starts (its `playMusic` takes the layer), for the game over phrase and the name
  entry, and by `leave()`. It is music: MUSIC OFF and MUTE silence it, switching music back on in the picker starts
  it again.

**Playing**: as today (left pushbutton: back to the picker; right: pause).

**Paused**: the pause screen is the help card: line 1 `PAUSED` with `^v` and the sound mode right-aligned, lines 2 to 4
the game's help lines. Flick up or down changes the sound mode. The music stops on pause and restarts from its
beginning on resume (`playMusic`).

**Sound modes** apply through the Arcade's filtering sink: MUSIC OFF drops the back layer, FX OFF the front layer,
MUTE both; switching music back on calls `playMusic()` while playing. The remote's Sound setting at Off still
silences everything (the board passes no sink then).

**Arcade sounds** (all front layer unless said; squares are fine here: one cuts the picker's beat for its 40 ms,
the beat goes on):

| Event | Sound |
| :-- | :-- |
| Pushbutton to another game | a short blip, its pitch one step per game on the C pentatonic |
| A game starts | a quick three-note rising jingle |
| Pause / resume | two falling / two rising notes |
| Back to the picker | two falling notes |
| Card opens / closes | a soft tick |
| Sound mode changed | a blip (not when the new mode silences effects) |
| Game over | a short falling phrase (back layer, from 400 ms); a fanfare instead when the score enters the top 3 |
| Name entry letter change / cursor move / saved | tick / softer tick / the fanfare's last bar |
| Table cleared | a falling phrase |
| Arcade closes | the opening jingle reversed (played by `Arcade::leave()` if a sink is set) |

## 6. Per game: sounds and music

Lanes design the exact notes; the list below is the agreed content. Music is the back layer; everything else front.

| Game | Music | Effects |
| :-- | :-- | :-- |
| Pong | none (the classic) | paddle hit: a high blip whose pitch rises with ball speed; wall: a lower blip; point: a longer low tone; win and lose jingles at the end |
| Parking | none | motor whine: grains whose pitch follows the vehicle's speed (quiet at rest); wall slide: scrape grains (softer than the remote's grind); impact (+2 s): a crunch of noise and a low buzz; parked in the bay: a chime; final stage done: a fanfare |
| Cave | none | rotor chop: noise ticks whose rate follows thrust; a ping every 100 m; crash: a noise burst and a falling whistle |
| Rocks | the tension pulse: two low alternating notes, faster as rocks thin out (`setTempo`) | shot: a short falling zap; rock destroyed: a noise burst, lower for bigger rocks; saucer: a warble while on screen (grains); mine: ticking (grains); power-up: a rising arpeggio; ship lost: a long falling noise; wave clear and boss intro: jingles |
| Snake | an original bouncy loop with melody, bass and drums, faster each level | eat: a blip climbing a pentatonic scale as the snake grows; diamond appears: a twinkle; diamond eaten: an arpeggio; mouse eaten: a squeak; new level: a jingle; death: falling notes |
| Blocks | **Korobeiniki** (melody + bass, the drums join from level 3), faster each level | move: a tiny tick; rotate: a blip; piece lands: a low thud; column cleared: a rising sweep, a bigger fanfare for four at once; level up: a jingle; game over: the music stops, a falling phrase |
| Plume | a slow, sparse, quiet ambient arpeggio (sine or triangle) | plume hiss: soft high noise grains, denser with more thrust; rock blasted: a tiny crackle, rate-limited; hull hit: a clunk; repair: two rising notes; every 100 m: a ping; fell out: a long falling whistle |

**Korobeiniki rights**: the melody is a 19th-century Russian folk song, public domain. The Game Boy "Type A"
arrangement (its bass line, harmony, counter-melody and variations) is copyrighted: do not transcribe it. Write the
folk melody (both of its parts) with our own simple bass (roots and fifths, octave jumps) and our own drum pattern.
Never use the word Tetris anywhere.

Snake's tune and Plume's arpeggio are original compositions; do not imitate a known game tune.

## 7. Scores in EEPROM (foundation)

- The remote keeps bytes 0 to 255 for itself (settings 0 to 3, reset history 4 to 16, stick calibration 20 to 44,
  room to grow). **The arcade block starts at 256**:

  | Offset | Size | Content |
  | :-- | :-- | :-- |
  | 0 | 1 | marker 0xA7 |
  | 1 | 1 | layout version 1 |
  | 2 | 1 | sound mode (0..3) |
  | 3 | 4 | last name entered |
  | 8 | 8 x 24 | 8 game slots (7 games, 1 spare), each a top 3 of entries |

  An entry is 8 bytes: score `uint32_t` little endian, then 4 name characters. A slot is the game's fixed id
  (Pong 0, Parking 1, Cave 2, Rocks 3, Snake 4, Blocks 5, Plume 6), not its place in the picker. An entry is empty
  when its name is not 4 valid characters (blank EEPROM 0xFF reads as empty). Wrong marker or version: all empty,
  defaults (sound mode SOUND ON, last name `AAAA`), written on the first change.
- **`ArcadeStore`** interface in `lib/RemoteGames` (read and write a byte at an offset in the block); the board
  implements it on `EEPROM` at 256 + offset, writing only bytes that change; tests use a RAM array.
- Writes only at a new top-3 entry, a sound mode change and a table clear: an exception to D-147's "EEPROM only on
  Save", decided by the owner for the arcade.
- Ranking: higher is better unless `lowerScoreIsBetter()`; a score must be strictly better to pass an entry; a
  score of 0 never enters (unless lower is better). Replaces today's session-only `best[]`.
- **Name entry** (a new Arcade mode after game over, when the score enters the top 3): `NEW BEST!` or `TOP 3`, the
  rank and the score on line 1; the four letters, the edited one in a pill; line 4 `UP/DN LETTER CLICK OK` (21 characters). Stick
  up/down changes the letter (A to Z, 0 to 9, space; held: repeats every 150 ms after 400 ms), left/right moves
  the cursor (flicks), a stick click saves, a pushbutton saves with the last name unchanged. The last name entered
  is the default and is saved too. The idle exit still applies (the entry is then saved as it stands).
- The game-over screen shows the rank when the score entered (`2ND`), as today's `BEST!` marker.

## 8. Plume in the arcade (Plume lane)

Follow `docs/plume/README.md` with these changes, decided by the owner:
- The Arcade owns the title, pause and game-over screens: Plume draws only play. The stick click starts it from the
  picker; the right pushbutton pauses. Plume's own title/pause/game-over screens and its stick-push pause are dropped.
- The best score is the arcade's top 3 (section 7), not Plume's own EEPROM.
- The arcade ticks at 50 Hz (`Game::tickSeconds`), not 60: keep every value per second as written (gravity,
  thrust, drag, speeds, emission rate, lifetimes) and step with 0.02 s.
- `name()` is `PLUME`; `score()` is the points (1 per metre of altitude, 25 per gem, added 2026-10-27, see the
  Plume doc's Collectibles); `scoreText` like `1450`.
- A `demo()` autopilot that climbs, digs through plugs and sometimes falls, so the picker shows the game.
- The entry, exit and safety open item is settled by the arcade (D-157); delete it from the Plume doc's open items
  and set its status banner to "implemented".
- Tests in `test/test_game_plume/` like the other games (physics step, erosion, hull and invulnerability, camera
  only up, game over both ways, determinism per seed, demo survives some distance, sounds fire).

## 9. Process rules for every agent

- Firmware worktree only (the lane's own folder given in its prompt). Never commit, push, stash or switch branches.
  Never flash, never touch the bench instruments.
- Host tests: `~/.platformio/penv/bin/pio test -e native` (all) or `-f test_game_<name>`; the firmware build:
  `~/.platformio/penv/bin/pio run -e remote`. Both must pass before reporting.
- Keep comment density and style of the surrounding code; every new file starts with a comment saying what it is
  and `[D-157]` (games) or `[D-158]` (sound engine).
- Report: files changed, what was built, test counts, anything left undone or any need from another lane.
