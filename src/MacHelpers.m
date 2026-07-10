/*
    Cocoa helpers used by the OLX C++ side
    (clipboard + user-attention notifications, OS cursor).
    The legacy src/MacMain.m also defines the clipboard/notification ones,
    but it carries an SDL 1.x main() that the cmake build replaces,
    so this file is the source of truth for the cmake build.
*/

#import <Cocoa/Cocoa.h>

void mac__copy_to_clipboard(const char* s) {
    [[NSPasteboard generalPasteboard]
        declareTypes:[NSArray arrayWithObject:NSPasteboardTypeString]
               owner:nil];
    [[NSPasteboard generalPasteboard]
        setString:[NSString stringWithUTF8String:s]
          forType:NSPasteboardTypeString];
}

const char* mac__copy_from_clipboard(void) {
    NSString *paste = [[NSPasteboard generalPasteboard]
        stringForType:NSPasteboardTypeString];
    return [paste UTF8String];
}

void mac__NotifyUserOnEvent(void) {
    [NSApp requestUserAttention:NSCriticalRequest];
}

void mac__ClearUserNotify(void) {
}

// Force the hardware OS cursor hidden or shown.
// CGDisplayHideCursor keeps a global hide count that, unlike the cursor rects
// SDL_ShowCursor relies on, is not reset when the mouse re-enters the window,
// so the cursor stays hidden until we balance it with CGDisplayShowCursor.
// Keep the calls balanced with our own state and call on the main thread.
void mac__EnforceSystemCursorHidden(int hidden) {
    static int applied = 0;  // whether we currently hold the cursor hidden
    if (hidden && !applied) {
        CGDisplayHideCursor(kCGDirectMainDisplay);
        applied = 1;
    } else if (!hidden && applied) {
        CGDisplayShowCursor(kCGDirectMainDisplay);
        applied = 0;
    }
}
