# Contributing to OpenLieroX

## Workflow

- Build and run the game first: see [README](README.md) (dependencies in [`DEPS`](DEPS)); the build is CMake-based.
- Report bugs and request features as GitHub issues.
- Contribute via a branch and a pull request against `master`; another developer reviews before merge.
- Discuss large or risky changes before you start.
- Code must compile and pass the tests before you push.

## Commits

- One logical change per commit.
- Subject: imperative mood, <= 50 chars, capitalized, no trailing period.
- Then a blank line and a body wrapped at ~72, explaining what and why.
- Keep commits small and focused — it makes review and later bisecting easy.
- Don't force-push a published commit; a new change is a new commit.
  Only rewrite/force-push your own not-yet-merged commits (or when asked to).

## C++ style

- Standard C++ and the STL (`std::string`, `std::vector`, ...); no C-isms (`strcpy`, C strings, `int` for `bool`).
- Clean, robust, general code — no hacks or workarounds; fix the real cause instead.
- Small functions, few locals.
- Prefer type/logic checks over error-prone patterns; avoid broad catch-all error handling.
- Indent with tabs; put spaces between tokens; two spaces before `{` after `if`/`for`/`while`.
- Comment only non-obvious code; mark incomplete or temporary code with `TODO`.
- No leftover debug/test code, and no new option for every tweak — discuss first.
- Match the surrounding file's conventions.

## Comments, doc strings, prose and commit messages

Break lines at clause boundaries — sentence end, comma, conjunction, end of a phrase —
not at a fixed column.
Each line should read as one logical unit;
lengths vary, so the right edge is ragged, not straight.
This applies to `//` and `/* */` comments, multi-line string literals,
Markdown/READMEs and commit messages alike.

- Keep comments tight: compress to the essential signal.
  A comment longer than the code it explains is usually too long.
- A trailing inline comment is fine only while the whole line stays within the column limit;
  otherwise move it to its own line(s) above.

## Tests

- Headless test suite: [`tests/headless/`](tests/headless/) (see its README); runs in CI via `./tests/headless/run.sh`.
- Add or adjust tests together with your change.
- A known critical bug should fail loudly — a plain failing test, not `xfail`.

## Code map

- `src/main.cpp` — entry point and game loop.
- `src/client/*` — menu and client; `src/server/*` — server and the game itself.
- `src/common/*` — core structures (worm, map, ...); `include/*` — headers.
- Key types: `CMap`, `CWorm`, `CServer`, `CClient`, `CGameScript`, `LieroX`.
- Physics: `Physics*`, `CProjectile`, `CNinjaRope`; net: `Networking`, `CBytestream`, `CChannel`; files/cache: `FindFile`, `Cache`.
