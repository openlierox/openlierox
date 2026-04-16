/*
    Cocoa helpers used by the OLX C++ side (clipboard + user-attention
    notifications). The legacy src/MacMain.m also defines these, but it
    carries an SDL 1.x main() that the cmake build replaces, so this
    file is the source of truth for the cmake build.
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
