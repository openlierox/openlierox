# Widescreen support

OpenLieroX was historically hardcoded to a 640×480 render surface. This
feature makes the render resolution adapt to the user's display aspect ratio
while keeping gameplay fair and the (640-authored) UI usable.

Branch: `support-widescreen` (off `0.59`).

## Goals & rules

- **Height stays 480.** Only the *width* varies.
- **Width adapts to the desktop aspect ratio** at startup (e.g. 854 on 16:9).
- **Local games use the full width** (the player sees more of the battlefield).
- **Network games are constrained to 640** so that a wider screen gives **no
  gameplay advantage** — everyone sees the same amount of the world.
- **Menus and in-game popups are authored for 640** and are presented
  **centered** on wider screens, with black bars on the sides.

## Core concepts (VideoPostProcessor — `include/AuxLib.h`)

All of the widescreen logic hangs off a few accessors on `VideoPostProcessor`
(`get()` is the singleton):

| Accessor | Meaning |
| --- | --- |
| `screenWidth()` | The real render-surface / window width (from the aspect ratio). `m_screenWidth`, default 640. |
| `screenHeight()` | Always 480. |
| `menuWidth` | The base width the UI is authored for: **640**. The width menus and network games are constrained to. |
| `displayScreenWidth()` | **The single source of truth** for the current frame's layout width: `screenWidth()` for widescreen games, `menuWidth` (640) for menus and non-widescreen games. Set per-frame by the draw code. |

Whether a game uses widescreen is decided by one function,
`Game::useWideScreen()` (`src/game/Game.h`) — currently `return isLocalGame();`
but the intended single place to later also allow widescreen for network games
where every player runs widescreen.
| `displayScreenOffsetX()` | `(screenWidth() − displayScreenWidth()) / 2`, clamped ≥0. The offset that centers the displayed view. 0 for a full-width local game. |
| `popupCenterOffsetX()` | `(displayScreenWidth() − menuWidth) / 2`, clamped ≥0. The offset to center a 640-wide popup *within* the current view. 107 for a full-width local game on 854; 0 for a network game (already centered). |

### Width computation

`VideoPostProcessor::initWindow()` (`src/client/AuxLib.cpp`) calls
`SDL_GetDesktopDisplayMode(0, …)` just before `SDL_CreateWindow` and sets
`m_screenWidth = round(screenHeight() * desktop_w / desktop_h)`, snapped up to
the nearest even number, floored at 320. Falls back to 640 if SDL fails.

Examples: 4:3 → 640, 16:10 → 768, 16:9 → 854. A `desktop mode: WxH -> using
WxH` line is logged so you can confirm the result.

### Centered presentation (letterbox)

`VideoPostProcessor::render()` (`src/client/AuxLib.cpp`) presents the video
surface to the screen. When `displayScreenOffsetX() > 0` (i.e. the content is
narrower than the screen) it copies only the **left `displayScreenWidth`
columns** of the surface into a horizontally-centered destination rect, leaving
black bars on the sides. Otherwise it does a normal full-surface copy.

So menus and network games **render into the left part of the surface exactly
as if the screen were 640** and are simply *presented* centered — none of the
hundreds of 640-hardcoded UI draw sites had to change.

### Input offset

Because the content is presented shifted, input must be shifted back:

- **Mouse** — `HandleMouseState()` (`src/client/InputEvents.cpp`) subtracts
  `displayScreenOffsetX()` from `Mouse.X`.
- **Touch / mouse-synth** — see the Touch section below.

When `displayScreenWidth() == screenWidth()` (local game) the offset is 0, so
nothing changes.

## Who sets `displayScreenWidth()`

It is pushed in per-frame, by whoever owns the screen that frame:

- **Menus** — `Menu_Frame()` (`src/client/DeprecatedGUI/MenuSystem.cpp`) sets it
  to `menuWidth` (640).
- **Gameplay** — `CClient::Draw()` (`src/client/CClient_Draw.cpp`) sets it to
  `screenWidth()` when `game.useWideScreen()`, else `menuWidth`.
- **Game start** — `CClient::SetupViewports()` (`src/client/CClient.cpp`) sets
  it the same way up front, then sizes the viewports from
  `displayScreenWidth()` (single viewport = full width; split-screen = two
  halves separated by a 4px gap).

(Two UI helpers that used to center against the full screen were reverted to
center against `menuWidth` so they fit inside the presented view:
`Menu_DrawSubTitle`/`Menu_DrawSubTitleAdv`, `Menu_MessageBox`, and the server
info popups in `Menu_Net_{Internet,Lan,Favourites}.cpp`.)

## In-game popups

Popups that overlay a running game (the Esc menu, in-game chat, …) are 640-wide
and were left-aligned on a wide local game. They are kept as overlays on the
**unchanged full-width game** and only the popup itself is centered, shifted by
`popupCenterOffsetX()`.

Helpers added for this:

- `CWidget::move(dx, dy)` (`include/DeprecatedGUI/CWidget.h`)
- `CGuiLayout::moveWidgets(dx, dy)` (`src/client/DeprecatedGUI/CGuiLayout.cpp`)
  — shifts every widget in a layout in one call.

Applied at:

- **Esc game menu** — `CClient::InitializeGameMenu()` calls
  `cGameMenuLayout.moveWidgets(popupCenterOffsetX(), 0)` after building it, and
  `CClient::DrawGameMenu()` offsets its background bitmap by the same amount.
- **In-game chat** — `CChatWidget::GlobalProcessAndDraw()` adds
  `popupCenterOffsetX()` to its setup X.

Mouse hit-testing lines up automatically: in a local game the widgets are moved
by +107 and the mouse is in full-screen (854) space; in a network game the
widgets are not moved (offset 0) and the mouse is already shifted into 640
space by `displayScreenOffsetX()`.

> **TODO:** the in-game **Options** panel (`Menu_FloatingOptions`, reached via
> Esc → Options) is not yet centered — it has a near-full-width panel, several
> sub-layouts and many direct draws plus the shared `Menu_DrawSubTitle`, so it
> needs more careful offsetting.

## Touch controls (`src/client/TouchControls.cpp`)

The on-screen touch layout adapts to the displayed view too.

- **Coordinate space = `displayScreenWidth()`** (not `screenWidth()`). So touch
  buttons render into and are hit-tested against the same view the HUD uses:
  full width for a local game, 640 (centered) for a network game, and 640 for
  the options-menu preview. This is what makes the controls appear *inside* the
  centered network-game view instead of in the clipped area.
- **Finger input** — `HandleFingerEvent()` maps the normalized SDL finger
  coords by `screenWidth()` and then subtracts `displayScreenOffsetX()` so the
  touch lands in the displayed view's space.
- **Mouse→touch bridge** — desktop play without a touchscreen synthesizes
  finger events from the mouse (`synthFingerFromMouse`,
  `src/client/InputEvents.cpp`); it normalizes by `screenWidth()`/
  `screenHeight()` (the mouse already arrives in logical space).
- **Minimap override** — the touch layout's minimap position is resolved
  against `displayScreenWidth()` in `CClient::Draw()`.

### Layout YAML: edge-relative coordinates

Touch layouts live in `share/gamedir/touchscreen/*.yaml`. Button (and minimap)
coordinates may be **negative** to anchor to the opposite edge, which is how a
layout stays right/bottom-aligned regardless of screen width:

- `x ≥ 0` — usual left origin (button's left edge at `x`).
- `x < 0` — button's **right** edge sits `|x|` px from the screen's right edge.
  Resolved as `canvasW + x − w` (`resolveButtonX`).
- `y ≥ 0` — top origin; `y < 0` — bottom edge `|y|` px from the bottom.

To convert a coordinate authored for 640 into a right-anchored one with the
same on-screen position: `x_neg = x + w − 640`. (A button whose right edge is
flush at 640 would compute to 0; give the column a small uniform margin
instead — `classic.yaml` uses 5px.)

`right-aligned.yaml` and `classic.yaml` have their action columns and minimaps
converted to negative x.

## Key files

- `include/AuxLib.h` / `src/client/AuxLib.cpp` — `VideoPostProcessor`: width
  computation, `displayScreenWidth`/offsets, `render()` letterbox.
- `src/client/CClient.cpp` `SetupViewports` — viewport sizing.
- `src/client/CClient_Draw.cpp` — gameplay draw sets `displayScreenWidth`;
  game-menu + minimap popup centering.
- `src/client/InputEvents.cpp` — mouse + mouse→touch offsetting.
- `src/client/DeprecatedGUI/MenuSystem.cpp` — menu frame sets
  `displayScreenWidth`; subtitle/message-box centering.
- `src/client/TouchControls.cpp` — touch layout, display-relative mapping.
- `include/DeprecatedGUI/CWidget.h`, `.../CGuiLayout.{h,cpp}` —
  `move`/`moveWidgets`.
- `share/gamedir/touchscreen/*.yaml` — touch layouts.

## Build & run (Linux)

```sh
mkdir -p build/cmake && cd build/cmake
cmake -DCMAKE_BUILD_TYPE=Release ../..
make -j"$(nproc)"
```

Binary: `build/cmake/bin/openlierox`. Run from a dir containing the `data/`
tree, otherwise font loading fails:

```sh
cd share/gamedir && /<repo>/build/cmake/bin/openlierox
```

(`build/CodeBlocks`, `build/msvc*`, `build/Xcode` are project files, not Linux
build outputs — ignore them.)

## Gotchas

- `SetVideoMode` must run on the main thread; from other threads use
  `doSetVideoModeInMainThread`.
- `SDL_RenderSetLogicalSize` is set to `screenWidth() × screenHeight()` (when
  `bKeepAspectRatio` is on), so the renderer scales the logical surface to the
  window; the `render()` letterbox rects are in that logical space.
- `CViewport::Setup(left, top, virtW, virtH, type)` renders the world at 2×
  scale, so `Width = vw/2`, `Height = vh/2`.

## Known remaining 640/480 assumptions (future work)

- In-game **Options** popup centering (see TODO above).
- HUD top/bottom bars and several HUD elements are still 640-wide bitmaps drawn
  left-aligned in a full-width local game.
- `src/common/CWormHuman.cpp` — `SDL_WarpMouseInWindow(NULL, 640/2, 480/2)`.
- `src/common/MapLoader_Teeworlds.cpp` — `IVec size(640,480)` minimum viewport.
