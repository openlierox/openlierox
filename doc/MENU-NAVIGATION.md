# Menu navigation with keyboard and gamepad

The menus can be used without a mouse.
The keyboard moves a focus between widgets and activates them,
and a gamepad is mirrored as the same keys,
so there is only one navigation code path.

This restores the keyboard navigation of the 0.58 branch
and adds the gamepad on top of it.

## Controls

| Key | Gamepad | Effect |
|-----|---------|--------|
| Arrow keys | D-pad, left stick | Move the focus to the previous/next widget (up/left, down/right) |
| Return, Enter | A | Activate the focused widget (click a button, toggle a checkbox, open a combobox, ...) |
| Escape | B, Start | Go back: clicks the Back/Quit/Cancel/No button of the page, or OK if that is all there is |
| PageUp, PageDown | Left, right shoulder | Move by ten items in lists and dropdowns; switch the Controls sub-tab in the options |

In sliders, left and right change the value.
In text boxes the arrow keys move the text cursor, and do not move the focus.
In the last line of a chat box, down moves the focus on to the next widget.

Left Alt, Left Ctrl, Left Shift, `X` and `Z` also activate the focused widget.
They are the keys of the default Liero controls,
and are kept for compatibility with 0.58.
See `CWidget::isActivationKey`.

Moving the focus plays the click sound and moves the mouse cursor to the widget.
Any real mouse movement leaves keyboard mode again.

## How it works

### Focus order

`CGuiLayout::Process` handles the arrow keys.
`CGuiLayout::keyboardNavigationTarget` picks the next widget by position:
`x + y * 0x10000 + order * 0x10000000`,
so widgets are visited line by line, left to right.
Labels and widgets of type `wid_None` are skipped.

`CWidget::setKeyboardNavigationOrder(1)` puts a widget after all the ones with order 0,
which is how the Back/OK buttons end up last and the main content comes first.
A widget that needs the arrow keys itself overrides `CWidget::handlesArrowKeys`.

### Cursor follows the focus

`Menu_WarpMouse(x, y)` moves the system cursor to menu coordinates.
It adds the offset of the centered menu and converts to window coordinates
with `SDL_RenderLogicalToWindow`, because the render size is scaled.
The motion event caused by the warp is told apart from real motion
in `Menu_ProcessMouseMotion`, which switches keyboard mode off for real motion.
`Menu_IsKeyboardNavigationUsed()` tells whether keyboard mode is on.
List views and popup menus use it to keep the cursor on their selected row.

### Gamepad

`InputEvents.cpp` mirrors the gamepad as key events with `InjectSyntheticKey`.
The left stick repeats like a held key,
after 400 ms and then every 100 ms.

It is only done while a menu reads the keyboard:
in the main menus and the lobby, in the in-game menu, and in the in-game options.
During the match the same buttons control the worm.
A release is only sent for a press that was sent,
so a stray Escape release cannot open the in-game menu.

It is also off while a key is being bound (`CInput::IsWaitingForInput`),
because `CInput::Wait` looks at the keyboard before the gamepad,
and would bind the mirrored key instead of the button.

## Making a page navigable

Everything in a `CGuiLayout` can be focused,
so a widget that is drawn and hit-tested by hand cannot be reached.
Such parts were turned into widgets:

- The netplay tab buttons (`Menu_Net_AddTabBarButtons`),
  the options page buttons and Back button, and the in-game options OK button
  are added to the layout of every page, with the button of the current page highlighted
  (`CButton::setSelected`).
- The Controls sub-tab headers (`CControlsTabButton`)
  and "Reset defaults" (`CResetDefaultsButton`) are text button widgets.
- The Touchscreen page keeps drawing its choices and tiles by hand,
  but has an invisible `CTouchHotspot` over each one that takes the focus and the key.
  The drawing reads the focus to show a ring.
- The weapon restrictions are a list view, and Return or a click cycles the state of the row.

Control ids of such shared buttons start at 1000, clear of the ids of the page widgets.

When the keyboard is used,
a widget returns its event from `KeyDown` (or `KeyUp` for key binding boxes,
so the key reader it opens does not see the same key press),
and the page handles it like the mouse click of the same widget.

## Popup menus

`CMenu` returns `MNU_SELECTION_CHANGED` when the selection moved.
A page that handles the events of a popup must ignore it,
only values from `MNU_USER` on mean an item was chosen.
The player popup of the host lobby and of the in-game menu takes the focus when it opens
and is removed when it loses it.

## Not covered

- Android specific parts of 0.58 (touch text input hints).
- Clicking an already selected list row does not activate it, as it did in 0.58.
  Return does.

## Testing

The headless suite cannot reach GUI code, so this is tested by hand:
walk through the main menu, Netplay (every tab), Options (every page and sub-tab),
Local Play with its game settings and weapon restrictions,
the host lobby player popup and the in-game menu,
once with the keyboard and once with a gamepad.
