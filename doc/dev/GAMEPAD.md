# Gamepad support

OpenLieroX can drive human players with a gamepad. There are two halves to it:

- **Digital controls** — buttons, the d-pad and the triggers — are bound through
  the normal key-mapping system (fire / jump / rope / weapon select /
  previous-next weapon), exactly like keyboard keys.
- **Analog sticks** add a **twin-stick** aiming/movement scheme on top: left
  stick walks, right stick aims.

Each human player uses their own pad — player 1 → pad 0, player 2 → pad 1, and so
on. This applies to the human worms you control on your own machine, so it works
the same in single-player, split-screen and network games (your own player gets
gamepad control on your client).

The key idea for the rest of this document: **the gamepad is added on top of the
existing controls, it never replaces them.** Keyboard, mouse and d-pad behave
exactly as they always did; the analog sticks are an extra input layer that only
does something while a stick is actually pushed.

## Controls at a glance

| Input | What it does |
|-------|--------------|
| **Left stick** | Walks left/right. It *never turns the worm* — the worm keeps facing/aiming where the right stick points, i.e. strafing is always on. If the right stick is idle, the left stick also aims (so a one-stick player aims where they walk). |
| **Right stick** | Aims. The stick's direction *is* the aim: left/right picks the facing side, up/down sets the aim angle. Wins over the left stick when both are pushed. |
| **Buttons / triggers / d-pad** | Fire / jump / rope / weapon select / previous-next weapon — the normal, configurable gamepad bindings. |

## Twin-stick controls

This section covers the analog-stick half. A few behaviours worth knowing:

- **Aim speed is capped, and there's a helper cursor.** The worm's aim does not
  jump instantly to the stick; it *rotates toward* it at the same maximum speed a
  keyboard player has, so a gamepad player can't out-aim the keyboard. To keep
  this from feeling disconnected, two cursors are drawn: a **green cursor** jumps
  instantly to where the stick points (your intended aim), and the normal cursor
  shows the worm's actual aim catching up to it.
- **Walking always digs.** Pushing the left stick into a wall tunnels straight
  through it, in the walk direction, no matter where you're aiming.
- **Aim holds when released.** The aim only updates while a stick is outside its
  deadzone; let go of both sticks and the worm keeps its last aim.
- **Weapon select.** On the weapon-selection screen the left stick moves the
  cursor up/down (one row per push, then auto-repeats if held), alongside the
  usual keyboard/d-pad keys.

### Design rules (how the layering works)

These three rules explain every code change below:

1. **Additive, never a replacement.** The keyboard/mouse/d-pad aim and movement
   code runs unconditionally, unchanged. The stick code runs *after* it and only
   takes effect while a stick is pushed. With no stick touched, behaviour is
   identical to before this feature.
2. **The aim stick owns the facing.** While a stick is aiming, walking (with the
   keys, d-pad, *or* the left stick) only moves the worm — it does not turn it.
   So the right stick decides which way you face, overriding any walk input.
3. **Aim speed is always capped** to the keyboard aim max speed. There is no
   option to turn this off.

### Engine note (important)

Twin-stick runs on the **gusanos engine** (the default mods), not the classic
LX56 path. Aiming, digging and shooting go through the gus code
(`getPointingAngle()`, `CWorm::dig()`), not the LX56 crosshair/carve code — so
several changes below only make sense because the gus engine is active.

### Which worms get it

It is decided per worm, at the top of `CWormHumanInputHandler::getInput()`:

```cpp
const int  twinPad   = localHumanWormIndex(m_worm); // 0 = first human, 1 = second, ...
const bool twinStick = (twinPad >= 0) && isPadPresent(twinPad);
```

`localHumanWormIndex()` returns the worm's position among the human worms
controlled on this machine (or −1) — in a network game that's your own player(s).
That index doubles as its gamepad slot, so each human player you control gets
twin-stick on the pad with the matching index; bots, remote players and humans
without a pad are untouched. Extra controllers are opened into their slots
automatically on SDL hotplug (`CInput::OnControllerAdded`).

## Binding model

The digital controls (buttons, d-pad, triggers) live in the gamepad binding
table (`Joysticks[]` in `src/client/CInput.cpp`) and remain bindable via the
`j<N>_<suffix>` config form. The analog sticks are **not** in that table — the
four stick-axis entries were removed, so the sticks can't be mapped to actions in
config. The twin-stick code reads them directly off the controller instead (see
the readers below). The default gamepad bindings never used the sticks, so no
defaults change.

## Implementation map

### `include/CInput.h` / `src/client/CInput.cpp` — raw stick readers
The twin-stick code reads the physical controller directly so it doesn't depend
on any key bindings:
- `isPadPresent(padIndex)`
- `getPadLeftStickX/Y(padIndex)`, `getPadRightStickX/Y(padIndex)` — raw axis
  values, −32768..32767 (0 if the pad is absent).
- `getPadStickDeadzone()` — the shared stick deadzone, so stick code and the
  classic binding code agree on one value.

These are thin wrappers over `SDL_GameControllerGetAxis`. Dedicated/headless
builds (`DEDICATED_ONLY`) get stubs returning 0/false. The deadzone is set by
`JOY_DEADZONE_PERCENT` (default **60%** of full deflection) — one knob to tune,
or to expose in a UI later. This file also removes the stick axes from
`Joysticks[]` (see [Binding model](#binding-model)).

### `src/common/CWormHuman.cpp` (`getInput()`) — aiming & walking
Resolves the active aim stick *before* the aim code into `stickAiming` +
`(aimAx, aimAy)`: the right stick if it's past its deadzone, otherwise the left
stick. While a stick is active it sets the facing side instantly, and computes
the absolute target angle the stick points at
(`targetAngle = CLAMP(atan2f(aimAy, |aimAx|) * 180/PI, -90, maxAngle)`).

- **Aim (rule 1 + 3).** The keyboard/mouse aim block (`cUp`/`cDown`, i.e. the
  d-pad) runs unchanged. Then, when a stick is aiming, `fAngle` *rotates toward*
  `targetAngle` capped per frame to `aimMaxSpeed * dt` (`fAimMaxSpeed`, or 100°/s
  under `FT_ForceLX56Aim`) — it never snaps. It also stores `fAimCursorAngle` and
  sets `bAimCursorActive` for the cursor (drawn in `CWorm::draw`, below).
- **Walk (rule 1 + 2).** The original `cLeft`/`cRight` movement block runs
  unchanged. Its two `iFaceDirectionSide = ...` writes are gated by
  `!stickAiming`, so while a stick aims, keyboard/d-pad walking only sets the
  move direction and never turns the worm. A separate `if(twinStick)` block then
  adds the left stick the same way (move direction only, never facing).
- **Walk-to-dig.** In the left-stick walk branch, `ws->bCarve` is pulsed every
  `carveDelay` (0.2s) while moving (`bCarve` is reset each frame, so the throttle
  makes it pulse); `OlxInputToGusEvents()` then fires `baseActionStart(DIG)` on
  each rising edge → `CWorm::dig()`.

### `src/gusanos/base_worm.cpp` — aim vs. dig direction
- **`getPointingAngle()`** now follows `iFaceDirectionSide` (the aim/right stick)
  instead of `iMoveDirectionSide` (walk/left stick). In normal play face == move,
  so nothing changes; while strafing it keeps the aim line, firecone and shots
  pointing where the right stick aims.
- **`getDiggingAngle()`** (new) is a purely horizontal angle along the *movement*
  direction (`DIR_RIGHT → 90°`, `DIR_LEFT → 270°`). `CWorm::dig()` uses it
  instead of `getPointingAngle()`, so walking into a wall always tunnels through
  it regardless of aim.

### `src/game/CWorm.h` / `src/common/CWorm.cpp` — the aim cursors
- **`fAimCursorAngle` / `bAimCursorActive`** (new, render-only, **not**
  networked). Set in `getInput()` whenever a stick is aiming; `bAimCursorActive`
  is reset to false at the top of `getInput()` each frame. `fAimCursorAngle` uses
  the same convention as `fAngle`.
- **`CWorm::draw()`** draws the two cursors via the `drawAimCrosshair()` helper
  when `bAimCursorActive && drawExtras && bLocal`: first the **green** cursor at
  `fAimCursorAngle` (the stick's intended direction), then the **normal** cursor
  at the worm's actual aim (`fAngle`) on top. They have **no** `gusEngineUsed()`
  gate (unlike the classic crosshair), so they show on the gus engine where the
  feature runs.

## Angle & coordinate conventions (gotchas)

- **`fAngle`** ranges −90..60 (or 90 with `FT_FullAimAngle`); negative = up,
  positive = down. The left/right sign is **not** in `fAngle` — it's in
  `iFaceDirectionSide`. Rendering/laser flip with `a = 180 - a` when facing left.
- **Gus `Angle`**: `getAimAngle() == Angle(fAngle + 90)`. Aim right → 90°, up →
  0°, down → 180°, left → 270°. `Vec(angle, len)` builds a direction from it.
- **SDL sticks**: left/up = negative, right/down = positive. Deadzone is
  `getPadStickDeadzone()`, derived from `JOY_DEADZONE_PERCENT` (60% → 19660).

## Trying it out

Build the client (see the top-level build docs), plug in a controller per human
player (player 1 → first pad, player 2 → second pad, ...), start a game with
those players, and the gamepad controls apply to each of them on their matching
pad.
