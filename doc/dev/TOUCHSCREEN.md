# Touchscreen controls

On-screen controls for finger-driven devices (Android phones,
tablets, and any other SDL2 target with a direct touch device). The
controls coexist with the keyboard and gamepad paths — a player can
switch input devices mid-game without restarting or changing options
— and respect split-screen so player 2 isn't dragged around by
player 1's touches. Verified on a Fairphone 4, Samsung Galaxy S25, and
a Lenovo Tab M10.

Enabled via `Game.TouchscreenControls` in `options.cfg`; default is
`true` on Android (`#ifdef __ANDROID__`) and `false` everywhere else.

---

## Features

- **Two button layouts** that swap automatically based on game state:
  an invisible left-side virtual joystick + right-edge action buttons
  during gameplay, and a discrete arrow-pad + Random/DONE picker
  during the per-round weapon selection.
- **Action-based event channel** ([include/TouchScreenInput.h](include/TouchScreenInput.h)) —
  touches push named actions (`Fire`, `Jump`, `PreviousWeapon`, …)
  into a queue with a player number and `Down/Up` state. The game
  logic reads from this channel directly, **not** through synthetic
  SDL keyboard events. Rebinding an action to the gamepad therefore
  doesn't silence the touch button.
- **Split-screen aware** — touch only drives the first local human
  worm. Player 2's worm follows the keyboard/gamepad only.
- **Auto-hide** — the on-screen UI hides itself the moment the player
  uses another input device (keyboard, gamepad, real mouse) and
  reappears on the next screen touch. Hiding releases held touch
  actions so the worm doesn't keep moving or firing.
- **Player-1 only gate** for auto-hide — split-screen player 2's
  keyboard or gamepad activity doesn't dismiss player 1's touch UI.
- **MENU button** synthesises an Escape press to open the in-game
  menu the same way the keyboard `Esc` does.
- **Minimap relocation** — while the touch UI is visible the minimap
  shifts left so the right-edge action-button column doesn't cover
  it; it returns to the screen edge when the UI auto-hides.

---

## Layouts

### `LM_PLAYING` — gameplay

- Most of the screen (`x < kVJoyAreaRight`, currently 525 logical px)
  is an **invisible virtual joystick**. The first finger landing in
  that zone sets a fixed anchor; dragging from the anchor presses
  `Left`/`Right` past the horizontal threshold and `Up`/`Down` past
  the vertical threshold. Both thresholds default to ≈ 2× worm
  width so a thumb held slightly off-center doesn't pace the worm or
  creep the aim. While engaged, an arrow is drawn from the anchor to
  the finger tip — yellow for aim-only, red once the horizontal
  component crosses the walk threshold.
- Right-edge action-button column:
  - **ROPE** (top), **FIRE** (middle), **JUMP** (bottom).
  - **PREV `<<`** + **NEXT `>>`** weapon-cycle buttons share a row in
    the gap between ROPE and FIRE.
  - **MENU** in the top-right corner.

### `LM_WEAPON_SELECT` — per-round weapon picker

- The continuous joystick is off; discrete arrow buttons drive the
  menu instead.
- Visible **↑ ↓ ← →** arrow buttons on the left.
- **Random** and **DONE** buttons stacked on the right edge —
  triggered directly without first navigating to the corresponding
  row in the picker (mirroring the keyboard "Fire on Random / Done
  row" path).
- Layout follows the **first local human worm**'s `bWeaponsReady`
  state: as soon as the touch player presses DONE, the UI flips back
  to `LM_PLAYING` even if a split-screen player 2 is still picking on
  the keyboard.

Layout transitions release every held finger so an arrow tapped in
the picker can't leak into in-game movement.

---

## Action event channel

Touch buttons do **not** inject synthetic SDL keyboard events. They
push high-level action events into a small queue:

```c
struct Event {
    int          playerNumber;   // 1-based; touch currently only drives player 1
    Action       action;
    StateChange  stateChange;    // Down or Up
};
```

`Action` is binding-independent:
`Left / Right / Up / Down / Fire / Jump / Rope / SelectWeapon /
Strafe / PreviousWeapon / NextWeapon / RandomizeWeapons /
ConfirmWeapons / OpenMenu`.

`TouchControls::Update` calls `TouchScreenInput::processFrame()` once
per gameloop tick. That drains the queue, updates a per-player /
per-action held-state array, and marks any leading-edge taps. The
worm input handler then reads:

```c
TouchScreenInput::isActionDown(1, Action::Fire);         // continuous
TouchScreenInput::wasActionTapped(1, Action::OpenMenu);  // edge
```

Because the consumer side never goes through `CInput`'s
type-specific keyboard / mouse / joystick path, **rebinding an action
to the gamepad does not silence the touch button** — touch + gamepad
coexist freely.

---

## Split-screen

Touch only drives the **first local human worm**. The check is made
at read time inside `getInput()` and `doWeaponSelectionFrame()` via
`isFirstLocalHumanWorm(m_worm)` (which iterates `game.localWorms()`
each frame), so the gate works even before `SetupGameInputs` has
assigned slot numbers. A tap on the touch screen never moves player
2's worm.

`CWormHumanInputHandler` also keeps an `m_localPlayerSlot` field set
by `setupInputs(controls, slot)` for any future callers that need a
stable identification, but the touch path itself is independent of
that field.

---

## Auto-hide

The touch UI hides itself the moment the player switches to another
input device, and reappears on the next screen touch:

- `TouchControls::NotifyExternalInput()` flips an internal
  `gUIActive` flag off and calls `releaseAll()`, which pushes `Up`
  events for any held touch actions so the worm stops moving / firing
  immediately.
- The next `SDL_FINGERDOWN` flips `gUIActive` back on; the same
  touch falls through to the normal button / vjoy logic so the
  player's tap also registers as a press.
- `IsActive()` returns `false` while hidden, so the renderer skips
  the buttons and the vjoy arrow, the auto-carve fall-back is
  disabled, and the minimap shifts back to its default position.

Auto-hide is gated on **player 1 only**, so split-screen player 2's
keys / controller don't dismiss player 1's touch UI:

- Keyboard: `EvHndl_KeyDownUp` calls `NotifyExternalInput` only when
  `isPlayer1KeyBinding(sym)` matches one of `Ply1Controls`.
- Gamepad: button / axis-past-deadzone events fire only when
  `getControllerPlayerSlot(instanceID) == 0`.
- Mouse clicks do not hide the touch UI at all (mouse isn't a
  player action binding); SDL's touch→mouse synthesis
  (`which == SDL_TOUCH_MOUSEID`) is filtered out.

---

## HUD compatibility

While the touch UI is visible the minimap renders at
`x = kVJoyAreaRight - 4 - MiniMapW` instead of flush against the
right edge, so the right-edge action-button column doesn't cover it.
When the UI auto-hides, the minimap returns to its normal position.
The main gameplay viewport keeps its full width.

---

## Implementation

Three modules:

| File | Role |
|------|------|
| [src/client/TouchControls.cpp](src/client/TouchControls.cpp) | Button layout, hit-testing, virtual joystick, finger→action mapping, rendering, auto-hide. |
| [src/client/TouchScreenInput.cpp](src/client/TouchScreenInput.cpp) | Action event queue + per-action held / tap state. |
| [include/TouchScreenInput.h](include/TouchScreenInput.h) | Public `Action` / `Event` types and the producer / consumer API. |

Game-side consumers — `CWormHumanInputHandler::getInput`,
`doWeaponSelectionFrame`, and the in-game ESC handler in
`CClient::Draw` — read `TouchScreenInput::isActionDown(...)` and
`wasActionTapped(...)` and OR them into the existing `CInput` checks.

Event flow:

```
SDL_FINGERDOWN/UP/MOTION
   └─► EvHndl_FingerEvent (gameloop thread)
         └─► TouchControls::HandleFingerEvent
               ├─ vjoy anchoring / direction update
               └─ button hit-test → setButtonPressed
                     └─► TouchScreenInput::pushEvent(player, action, Down/Up)
                            └─ queued

TouchControls::Update (end of ProcessEvents)
   └─► TouchScreenInput::processFrame
         └─ drain queue → update gIsDown / gPendingTaps

worm logic, weapon picker, ESC handler (later in the same frame)
   └─► TouchScreenInput::isActionDown / wasActionTapped
         └─ OR'd into the matching CInput.isDown() / .isDownOnce() check
```

All of this runs on the gameloop thread — no cross-thread sync.

---

## Tuning

All knobs live at the top of
[src/client/TouchControls.cpp](src/client/TouchControls.cpp):

- `kVJoyAreaRight` (525) — right edge of the invisible joystick zone
  in logical px. Increase to claim more screen for the joystick;
  decrease to give the action buttons more room.
- `walk_sensitivity` (40 px ≈ 2× worm width) — horizontal drag from
  the anchor before `Left`/`Right` (movement) fires.
- `aim_sensitivity` (40 px) — vertical drag from the anchor before
  `Up`/`Down` (aim) fires.

Each button is one entry in `gPlayingButtons[]` or
`gWeaponSelectButtons[]` with a logical-640×480 rect, a `SIN_*` index
(for `BA_HOLD`), and a `ButtonAction`. `sinToAction()` maps the
`SIN_*` to a `TouchScreenInput::Action`.

---

## Adding a new action button

1. (Only if it's a brand-new action) Add an entry to
   `TouchScreenInput::Action` in
   [include/TouchScreenInput.h](include/TouchScreenInput.h) and
   either teach the worm-input handler to read it (continuous — add
   to the touch-state captures at the top of `getInput`) or read it
   via `wasActionTapped` at a single callsite (tap).
2. Append an entry to `gPlayingButtons[]` or `gWeaponSelectButtons[]`
   in [src/client/TouchControls.cpp](src/client/TouchControls.cpp):
   logical-640×480 rect, the `SIN_*` index (for `BA_HOLD` actions)
   or `0` (for `BA_TAP_*` actions), the `ButtonAction`, and a short
   label. Keep `x >= kVJoyAreaRight` so it doesn't overlap the
   invisible joystick in the gameplay layout.
3. For new `BA_TAP_*` actions, add a `case` in the tap-action `switch`
   inside `setButtonPressed` that maps it to your
   `TouchScreenInput::Action`.

Multi-touch, render, queue drain and game-side consumption all key
off the existing tables and the action enum, so no further plumbing
is needed.

---

## Cross-platform notes

The code uses only SDL2 finger events (`SDL_FINGERDOWN` /
`SDL_FINGERUP` / `SDL_FINGERMOTION`) and standard rendering
primitives. It compiles and runs on any SDL2 target that has a touch
device. `SDL_GetNumTouchDevices()` + `SDL_GetTouchDeviceType()` can
be used to flip the `bTouchscreenControls` default for non-Android
targets that expose `SDL_TOUCH_DEVICE_DIRECT` (e.g. the Steam Deck
screen); the current default keys off `#ifdef __ANDROID__` only.
