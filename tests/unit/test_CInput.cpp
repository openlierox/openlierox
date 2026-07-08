/*
 *  Unit tests for the CInput control-mapping edge handling (CInput.h).
 */

#include "unittest.h"
#include "CInput.h"
#include "InputEvents.h"

#include <SDL.h>

namespace {

// One tick of the input pipeline, in the order ProcessEvents runs it:
// key events first,
// then the per-frame poll for non-keyboard bindings,
// then the game reads the control and resets it.
void inputTick(CInput& inp, const KeyboardEvent* ev) {
	if(ev) inp.handleKeyEvent(*ev);
	inp.updateUpForNonKeyboard();
	inp.updateDownOnceForNonKeyboard();
}

}

// A keyboard control fires isDownOnce() exactly once per physical press:
// on the key-down frame, and never again on key-up.
// Regression test for #972 ("double key presses, one on key-up and one on key-down"):
// on the wasm port the per-frame poll re-derived a down-once edge
// for keyboard bindings on release,
// so a rope-key tap shot the rope twice.
void test_KeyboardDownOnceFiresOncePerPress() {
	CInput inp;
	inp.Setup("x");
	CHECK(inp.isKeyboard());

	KeyboardEvent down; down.sym = SDLK_x; down.down = true;
	KeyboardEvent up;   up.sym   = SDLK_x; up.down   = false;

	// Frame with the key going down: fires.
	inputTick(inp, &down);
	CHECK(inp.isDownOnce());
	inp.reset();

	// Frame with the key held (no new event): must not fire again.
	inputTick(inp, NULL);
	CHECK(!inp.isDownOnce());
	inp.reset();

	// Frame with the key going up: must not fire.
	inputTick(inp, &up);
	CHECK(!inp.isDownOnce());
	inp.reset();
}
