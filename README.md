# SORRY

A 2D top-down exploration game with an emotional core: neon over desert dusk,
excited and melancholy at once. Las Vegas as a feeling. The garden is bat
country. Structurally inspired by OMORI — small rooms, moody music, dialogue
that remembers what you said.

All music is **synthesized in code** — a four-voice chiptune generator makes a
deterministic looping track per "mood", and rooms crossfade between moods as
you walk. Missing sprites are generated at runtime as labeled colored squares,
so the game is fully playable before a single asset exists.

## Quick start

```sh
cmake -B build && cmake --build build && ./build/sorry
```

Run it from the repo root (the game looks for `assets/` in the working
directory, its parent, or next to the executable).

## Building on macOS

The target platform is macOS on Apple Silicon (arm64) with Apple Clang.
Three things are worth knowing so they don't get rediscovered later:

### Homebrew SDL2 setup

```sh
brew install cmake pkg-config sdl2 sdl2_ttf sdl2_image sdl2_mixer
```

- Homebrew nowadays ships **sdl2-compat** as the `sdl2` formula — an SDL2
  API layer over SDL3. That's fine: this project uses only the SDL2 API and
  links via `pkg-config`, which sdl2-compat provides a `sdl2.pc` for.
- Headers land under `/opt/homebrew/include`, libs under `/opt/homebrew/lib`.
  `CMakeLists.txt` appends `/opt/homebrew` to `CMAKE_PREFIX_PATH` on Apple so
  pkg-config and the linker find everything without env vars. If CMake still
  can't find a module, check `pkg-config --libs sdl2 SDL2_ttf SDL2_image
  SDL2_mixer` works in your shell; if not, your `PKG_CONFIG_PATH` may be
  missing `/opt/homebrew/lib/pkgconfig`.

### Font path override

Fonts resolve in this order (see `Assets.cpp`, `openAnyFont`):

1. Any `.ttf`/`.otf` dropped into `assets/fonts/` (first one found wins).
2. `/System/Library/Fonts/Supplemental/Andale Mono.ttf` (macOS fallback).
3. `/System/Library/Fonts/Monaco.ttf`, then DejaVu/Liberation Mono paths so
   the game also boots in Linux CI containers.

If none open, startup fails loudly with instructions — a font is the one
truly critical asset (placeholder sprites need it to draw their glyphs).

### Audio format switch

The mixer device is opened **mono, 44100 Hz, signed 16-bit**
(`Mix_OpenAudioDevice(44100, AUDIO_S16SYS, 1, 1024, ...)` in `Audio.cpp`).
This is deliberate, not a default: procedural music is injected through
`Mix_HookMusic`, whose callback writes **raw samples in the device format**.
`ChiptuneSynth::render` produces exactly that — mono `int16_t` at
`ChiptuneSynth::kSampleRate` (44100).

If you ever change the device format (stereo, different rate, float
samples), you must update `ChiptuneSynth::render` and `kSampleRate` to
match, and the `len / 2` sample count in `Audio.cpp`'s `musicHook` — the
hook hands you a byte length, and the synth currently interprets it as
`int16` mono frames. Mismatching these plays garbage or half-speed audio
rather than erroring.

## Controls

| Key | Action |
| --- | --- |
| Arrows / WASD | Move |
| E / Space | Interact, advance or skip dialogue |
| Up/Down | Pick a dialogue choice |
| I | Open / close inventory |
| Esc | Quit (autosaves) |

## Project layout

```
src/
  main.cpp            SDL init (RAII), fatal error reporting
  Game.{h,cpp}        Game loop, state machine, room switching, camera
  Renderer.{h,cpp}    SDL window/renderer wrapper, draw helpers, RAII deleters
  Input.{h,cpp}       Keyboard state, held vs edge-triggered
  Room.{h,cpp}        Tile grid + entities + mood + transitions + doors
  RoomLoader.{h,cpp}  Parses assets/maps/<room>.txt + <room>.meta
  Player.{h,cpp}      Pixel movement, tile collision, facing, walk cycle
  Entity.{h,cpp}      Base for NPCs / items / triggers
  NPC.{h,cpp}         NPC entity (dialogue looked up by id)
  Dialogue.{h,cpp}    Dialogue trees (in code) + typewriter dialogue box
  Inventory.{h,cpp}   Item list + display names + overlay
  SaveGame.{h,cpp}    JSON save/load (hand-rolled, zero deps)
  Audio.{h,cpp}       SDL_mixer device, SFX, music-file override, synth hook
  ChiptuneSynth.{h,cpp} Procedural 4-voice chiptune + mood registry + crossfade
  Assets.{h,cpp}      Sprite/font/sfx cache with placeholder generation
assets/
  maps/     one .txt + .meta per room
  sprites/  drop PNGs here (see "Swapping in real art")
  fonts/    drop a .ttf here to override the system font
  music/    drop <mood>.ogg here to replace a procedural track
```

## Adding a room

1. Create `assets/maps/myroom.txt`. One character per 32px tile:

   ```
   #  wall        .  floor       ~  tall grass
   >  door        N  npc slot    I  item slot
   ```

   Doors get a compass direction from where they sit: top row = north,
   bottom row = south, left column = west, right column = east.

2. Create `assets/maps/myroom.meta`:

   ```ini
   music_mood = dream_lydian
   transitions = west:garden north:casino_floor
   npcs = 0:npc_flamingo 1:npc_elvis
   items = 0:room_key
   # optional: set a story flag the moment the player walks in
   on_enter_flag = reached_myroom
   ```

   `N` and `I` slots are numbered 0,1,2… in reading order (left-to-right,
   top-to-bottom) and bound to ids here. Transitions are `direction:room` —
   the room name is the map filename without extension.

3. Point an existing room at it (`transitions = ... east:myroom`) and add the
   matching door tile `>` on that edge. Arriving players spawn one tile
   inward from the door that faces back the way they came.

4. New NPC? Register a `DialogueTree` under its id in `dialogueRegistry()`
   (`src/Dialogue.cpp`). New item? Add a display name in
   `Inventory::displayName` (unknown ids fall back to de-underscored text).

No C++ changes are needed for the room itself.

## Adding a music mood

Moods live in `moodTable()` in `src/ChiptuneSynth.cpp`:

```cpp
// name              scale (semitones)      root bpm  seed   duty  bright noise
{"neon_waltz",       {0, 2, 3, 5, 7, 9, 10}, 56, 90, 0x1234, 0.25f, 0.9f, 2},
```

- `scale`: semitone offsets from the root; melody, bass and arps only ever
  use these, so the scale *is* the mood's harmonic character.
- `root`: MIDI note (57 = A3). Lower = darker.
- `bpm`: tempo; also nudges melody density (faster moods play busier lines).
- `seed`: any number — patterns are generated deterministically from it, so
  a mood always sounds the same. Change the seed to reroll the tune.
- `duty`: melody square-wave duty cycle. 0.5 = hollow/round, 0.25 = classic
  NES lead, 0.125 = thin and buzzy.
- `brightness`: overall level trim. `noise`: 0–4 percussion busyness
  (kick always plays; ≥2 adds snare; ≥3 adds hats).

Reference the name from any `.meta` file (`music_mood = neon_waltz`) —
unknown names fall back to the first mood in the table, with no crash.

## Swapping in real art (no code changes)

Drop 32×32 PNGs into `assets/sprites/`. Anything missing keeps its generated
placeholder, so you can replace art one file at a time:

| File | Replaces |
| --- | --- |
| `player_down_0.png`, `player_down_1.png` | player facing down, 2-frame walk |
| `player_up_0/1.png`, `player_left_0/1.png`, `player_right_0/1.png` | other facings |
| `player.png` | single fallback used only while directional frames are missing |
| `tile_floor.png`, `tile_wall.png`, `tile_grass.png`, `tile_door.png` | terrain |
| `npc_mother.png`, `npc_dealer.png`, `npc_shadow.png` | the starter NPCs |
| `apology_note.png` | the starter item (also its inventory icon) |

Any new NPC/item id you invent in a `.meta` file automatically looks for
`assets/sprites/<id>.png` the same way.

## Save file

Autosaves on every room transition and on quit. JSON at
`~/Library/Application Support/sorry/save.json` (macOS) or the SDL pref dir
elsewhere: current room, player position, inventory, story flags. Delete the
file to start over. A corrupt or missing file falls back to a fresh start in
`bedroom` — never a crash.

## Next steps for the artist / musician

**Artist** — everything goes in `assets/sprites/`, 32×32 PNG, transparent
background, filenames exactly as in the table above. Start with the ten
player frames + four tiles; that transforms 90% of what's on screen. Fonts:
drop any `.ttf` in `assets/fonts/` (first file found is used, 18pt).

**Musician** — you have two options, both zero-code:

1. *Tweak the synth*: edit the mood rows in `src/ChiptuneSynth.cpp`
   (scale/root/bpm/seed/duty) — it's one line per mood and rebuilds in
   seconds. Rerolling seeds until the melody lands is a legitimate workflow.
2. *Replace with recordings*: drop a loopable track named after the mood into
   `assets/music/` — e.g. `assets/music/melancholy_a_minor.ogg` (also accepts
   `.wav`, `.mp3`, `.flac`). If that file exists, it plays (looped, 1.5s
   fade-in) instead of the synth; delete it and the synth takes over again.
   Note the mixer runs mono at 44.1kHz — stereo files will be downmixed by
   SDL_mixer, so master accordingly.

**SFX** — optional: `assets/sfx/blip.wav`, `pickup.wav`, `confirm.wav`,
`bump.wav` override the procedural square-wave effects.

## Where to extend next

- **Dialogue in files**: `dialogueRegistry()` is the single seam — swap its
  body for a parser over `assets/dialogue/*.txt` and nothing else changes.
- **Battle / minigame scenes**: `Game::State` is the scene switch; add a
  state, route `update`/`render` through it.
- **Entity behaviors**: `Entity::update` is virtual; wandering NPCs or
  animated props only need a subclass and a `RoomLoader` hook.
- **Triggers**: `EntityKind::Trigger` exists but is unused — invisible tiles
  that start dialogue or set flags are a natural next step.
- **Menus**: Esc currently quits; a pause state fits the same pattern as
  the inventory overlay.
