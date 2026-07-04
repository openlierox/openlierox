# Split-screen (local multiplayer)

OpenLieroX supports up to **four** local human players sharing one screen in a
**Local play** game. Each local player gets their own viewport (their own slice
of the screen, following their own worm) and their own input device.

The player cap is a single constant, `MAX_LOCAL_PLAYERS` in
[`include/Consts.h`](../../include/Consts.h):

```cpp
static constexpr int MAX_LOCAL_PLAYERS = 4;   // bump this to allow more
static constexpr int NUM_VIEWPORTS     = MAX_LOCAL_PLAYERS; // one viewport each
```

`NUM_VIEWPORTS` is derived from it and sizes the `CViewport cViewports[…]` array
on `CClient`. The gusanos engine's own local-input slot count
(`GusGame::MAX_LOCAL_PLAYERS`) is aliased to the same constant so the two never
drift apart. To raise the cap you also need to add a screen layout for the new
player count in `splitScreenViewportRect()` (see below) — everything else already
loops over `NUM_VIEWPORTS` / `MAX_LOCAL_PLAYERS`.

## Input: who controls which player

| Player | Keyboard | Gamepad |
|--------|----------|---------|
| 1 | `Ply1Controls` defaults (arrows + …) | pad 1 (`j1_*`) |
| 2 | `Ply2Controls` defaults (numpad + …) | pad 2 (`j2_*`) |
| 3 | *(none)* | pad 3 (`j3_*`) |
| 4 | *(none)* | pad 4 (`j4_*`) |

Players 1 and 2 keep their existing keyboard **and** gamepad bindings. Players 3
and 4 are **gamepad-only**: a single keyboard has neither enough keys nor the
ergonomics for four players, so they have no keyboard defaults and are not shown
in the controls options UI. Their gamepad bindings mirror players 1 and 2, just
on pads 3 and 4. Analog **twin-stick** aiming/movement (see
[GAMEPAD.md](GAMEPAD.md)) works for every local player, because it is driven by
each human worm's index (`localHumanWormIndex()` → pad N), which was already
generic.

The per-player control sets live in `GameOptions::sPlayerControls` (one
`PlyControls` per local player). It is sized to `MAX_LOCAL_PLAYERS` **once** in
the `GameOptions` constructor, before the controls are registered as scriptable
vars — `RegisterVars` stores pointers into those elements, so the vector must not
be resized afterwards. See `Options.cpp` (`Ply1Controls` … `Ply4Controls`
registration and the `j3_`/`j4_` gamepad defaults for the extra players).

Gamepad binding strings use the generic `j<N>_<suffix>` form (`CInput::Setup`),
which already accepts any pad number, so `j3_*` / `j4_*` needed no parser change.
`getControllerPlayerSlot()` maps a physical pad to a 0-based player slot and is
bounds-checked up to `MAX_LOCAL_PLAYERS`.

## Screen layout

`CClient::SetupViewports(const std::vector<CWorm*>& worms, int type)` lays out one
viewport per local player. The no-argument `SetupViewports()` collects the local
**human** worms (capped at `MAX_LOCAL_PLAYERS`) and calls it. The actual
rectangles come from the helper `splitScreenViewportRect()`:

```
 1 player      2 players       3 players        4 players
+---------+   +----+----+     +----+----+      +----+----+
|         |   |    |    |     | 0  | 1  |      | 0  | 1  |
|    0    |   | 0  | 1  |     +----+----+      +----+----+
|         |   |    |    |     | 2  |radar|     | 2  | 3  |
+---------+   +----+----+     +----+----+      +----+----+
```

**1 player** is the odd one out: there's a single full-screen view, and the
**radar map** is just a plain corner overlay in the top-right (nobody to share it
with). This is the single-viewport default, identical in local and network play.

**2, 3 and 4 players are all the same case — split screen.** The screen is
divided into equal-sized viewports, one per player, and the **radar map** is
placed in space that is shared *equally* between the players — never inside a
single player's view — so it always costs everyone the same screen estate. Only
the concrete geometry changes with the player count:

- **2**: two equal full-height columns. Shared space = the top of the seam
  between them, where the radar is centered.
- **3**: a 2×2 quarter grid filled by three players. Shared space = the vacant
  4th quarter, which holds the radar (so nobody gets a larger, unfair view — the
  cells are the same size as in the 4-player grid).
- **4**: a 2×2 quarter grid. Shared space = the four viewports' intersection at
  the middle of the screen, where the radar is centered.

A 4 px `gap` separates neighbouring viewports. The gaps lie *outside* every
viewport rect, so effects that draw past a viewport's clip (e.g. blood) can leave
artifacts in them; `CClient::Draw` repaints the seams every frame with
`tLX->clViewportSplit`. The seam-fill in `CClient_Draw.cpp` just paints whichever
seams the active grid has — the vertical column seam (2+), the horizontal row
seam (3+), and the second column seam in the bottom row (4) — plus, for 3
players, the vacant 4th quarter behind the radar.

> The older two-argument `SetupViewports(CWorm* w1, CWorm* w2, …)` overload is
> **kept** for the spectator viewport manager and the `setviewport` command,
> which drive exactly two viewports; only the local-play path uses the new
> vector overload.

## Game setup

The **Local play** menu (`Menu_Local.cpp`) already lets you add many players to
the "playing" list (its limit is `MAX_PLAYERS` = 32, counting AI worms too). The
real split-screen limit is applied where the human input handlers and viewports
are wired up:

- `CClient::SetupGameInputs()` assigns a control set to each local human worm,
  up to `MAX_LOCAL_PLAYERS` (previously hard-capped at 2).
- `CClient::SetupViewports()` builds at most `MAX_LOCAL_PLAYERS` viewports.

So to actually play 3–4 handed, add 3–4 **human** profiles to the Local play
list (plus any AI), give each human player their own gamepad, and start the game.

### Default profiles

To make 3–4 player games playable out of the box, two extra human profiles ship
by default: **"The Third"** and **"The Fourth"**. They live in the bundled
profile database `share/gamedir/cfg/players.dat`, right after the stock
**"OpenLieroXor"** and **"The Second"** human profiles (the CPU profiles follow).
That binary file is what the game loads on a fresh install — `AddDefaultPlayers()`
in `ProfileSystem.cpp` only runs as a fallback when no `players.dat` is found on
any search path. They are ordinary human profiles you assign to the 3rd and 4th
slots; being gamepad-controlled, they use pad 3 and pad 4.

## Implementation map

| File | Change |
|------|--------|
| `include/Consts.h` | `MAX_LOCAL_PLAYERS` (new), `NUM_VIEWPORTS` derived from it |
| `src/gusanos/gusgame.h` | `GusGame::MAX_LOCAL_PLAYERS` aliased to the global constant |
| `src/client/Options.cpp` | `sPlayerControls` sized to `MAX_LOCAL_PLAYERS`; register `Ply3`/`Ply4` gamepad-only controls |
| `include/CClient.h` / `src/client/CClient.cpp` | vector `SetupViewports` overload + `splitScreenViewportRect()` grid layout; `SetupGameInputs` cap raised |
| `src/client/CClient_Draw.cpp` | grid-aware seam fill between viewports |
| `src/client/CInput.cpp` | `getControllerPlayerSlot()` generalized & bounds-checked |
| `src/client/ProfileSystem.cpp` | default "The Third" / "The Fourth" human profiles |

## Trying it out

Build the client, plug in a gamepad per human player (player 1 → first pad, … ,
player 4 → fourth pad), open **Local play**, add 3–4 human players to the list
(e.g. your player plus "The Third" / "The Fourth"), start the game, and each
player gets their own viewport driven by their matching pad.
