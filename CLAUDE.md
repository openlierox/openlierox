# Port: keyboard/gamepad menu navigation (0.58 → 0.59)

Status: **in progress** — branch `port-menu-navigation-to-0.59` (off `0.59`).
Builds cleanly; runtime-tested by hand: "mostly works, not perfect yet" (see Known issues / Next steps).

## Goal

The `0.58` branch can navigate the menus with keyboard and gamepad; `0.59` could
not. This branch ports that feature. The source is the series of `origin/0.58`
commits below; `0.59` already had the first two foundational ones
(`19b36a988` "Navigate some game menus with arrow keys" and `1cc016c16`
"Auto-select first menu item…"), so only the rest were ported.

## How to build & run

```sh
# configure once (out-of-source build dir)
cmake -S . -B /tmp/olxbuild -DDEBUG=0
make -C /tmp/olxbuild -j6            # ~5 min cold; binary -> /tmp/olxbuild/bin/openlierox

# run: must cd into the game-data dir so it finds fonts/mods (data is in share/gamedir)
cd share/gamedir && DISPLAY=:1 /tmp/olxbuild/bin/openlierox
```

All build deps (cmake, g++, SDL2, libxml2, …) are present in this environment.
Clean `0.59` and this branch both build with 0 errors.

## What was ported (each is one commit on this branch)

Cherry-picked from `origin/0.58` (commit-message order = chronological):

| 0.58 commit | What | Notes |
|---|---|---|
| `8b95730b2` | Netplay tab buttons keyboard nav | tab bar became part of each sub-menu's `CGuiLayout` |
| `e25e82b28` | Down key on last CBrowser line → next widget | |
| `867b27884` | Game options + local-game team selection | **options-menu part dropped** (see below); team-select part retargeted to 0.59 APIs |
| `8caf84c4d` | Weapon-restrictions nav | **CListview part only** (PgUp/PgDn, click-to-enter); file-move dropped |
| `2f19f0f6d` | Listview mouse-follow + CWidget cleanup | tweaks to deleted files dropped |
| `36ab85304` | Game-settings menu nav | CListview part as-is; menu part applied in `Menu_Local.cpp` |
| `0fb2fb3d6` | Server kickban popup nav (+ "Change team" item) | |
| `08fb99fd8` | Popup-menu nav fix (m_nPosY→m_nPosX) | |
| `c63cfa33f` | In-game kickban menu nav | only ESC (not Enter) closes the menu now |
| `09cfe72f5` | Strafe always digs (gamepad digging) | `CWorm_Simulate.cpp`→`CWormHuman.cpp` on 0.59 |
| `84a46db86` | Fix mouse stuck in server list after kb nav | adds `Menu_WarpMouse`/`Menu_ProcessMouseMotion` |
| `b3b4bacd9` | Fix host popup menu mouse nav | |

Plus two **0.59-adaptation commits** I authored (`d6fdfc326`, `a846f4da5`) folding
in the SDL2/API fixes below.

## 0.59 divergences that required adaptation (watch for these in follow-ups)

`0.59` migrated to SDL2 and rewrote several subsystems, so the 0.58 patches did
not apply cleanly. Mechanical translations applied:

- `#include "Sounds.h"` → `#include "sound/SoundsBase.h"` (Sounds.h not on 0.59 include path).
- `SDL_WarpMouse(x,y)` → `SDL_WarpMouseInWindow(NULL, x, y)`.
- `Action::handle()` returns `Result`, not `int`.
- `(DWORD)0` → `(uintptr_t)0` (0.59 dropped the DWORD typedef, commit `2dbc2ced2`).
- Game-state API: `tLXOptions->tGameInfo.gameMode->…` → `gameSettings[FT_GameMode].as<GameModeInfo>()->mode->…` or `game.gameMode()->…`.
- Worm access: `cServer->getWorms()[i]` → `game.wormById(i, false)`; `iMatchWinner*`/`cRemoteWorms[]` → `game.iMatchWinner*` / `game.wormById`.
- Server list: 0.59 has the `ServerList` class; old `Menu_SvrList_*`/`psServerList` globals are gone — don't re-add them.
- Files merged/removed on 0.59: `Menu_GameSettings.cpp` and the weapon-restrictions
  dialog now live in `Menu_Local.cpp` (`Menu_GameSettings()` ~line 965). `GotoJoinLobby()`
  was removed.

## Deliberately NOT ported (design decisions — revisit if needed)

1. **`be7d72091` "Keyboard navigation for in-game floating options" — SKIPPED.**
   It merges the floating options into the *old* options-menu architecture
   (deletes `Menu_FloatingOptions.cpp`, builds on `Menu_OptionsProcessTopButtons`).
   On 0.59 `Menu_FloatingOptions.cpp` still exists and already uses `CGuiLayout`,
   so it inherits keyboard nav from the base infra. Forcing the old structure
   would regress it.

2. **`867b27884` options-menu portion — NOT applied.**
   0.59's latest commit (`e19b977b0`, gamepad config) deliberately **redesigned**
   the options/controls menu (sub-tabs `ct_Player1/2/General`, `TopButtons[3]`,
   custom tab hit-boxes — see `Menu_Options.cpp`). It has its *own* navigation and
   does not use `setKeyboardNavigationOrder`. Applying the 0.58 options nav would
   revert that redesign. Kept 0.59's `Menu_Options.cpp` as-is.

## Known issues / Next steps (where to continue)

- Hand-test reported "mostly works, not perfect." Specific glitches not yet
  characterised — **next step: pin down exactly which menus/transitions misbehave.**
- Likely suspects to investigate first, given the decisions above:
  - **Options/controls menu**: uses 0.59's custom nav, not the ported system —
    keyboard/gamepad behaviour there may be inconsistent with the rest.
  - **In-game floating options** (ESC during a match): relies on inherited
    `CGuiLayout` nav only; the dedicated 0.58 fixes were skipped.
  - Team-select in local game and the host kickban "Change team" item were
    re-implemented against 0.59 APIs by hand — verify they actually mutate
    team/profile state correctly in-game.
- Not yet done: pushing the branch / opening a PR; no automated tests run.

## Git

Branch `port-menu-navigation-to-0.59`, 14 commits on top of `0.59`
(`git log --oneline 0.59..HEAD`). Working tree clean. Each ported commit keeps a
`Cherry-picked-from:`/`(cherry picked from …)` reference to the 0.58 original.
