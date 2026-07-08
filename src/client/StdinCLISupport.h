#ifndef OLX_STDINCLISUPPORT_H
#define OLX_STDINCLISUPPORT_H

#include "util/Result.h"

Result initStdinCLISupport();
void quitStdinCLISupport();
void activateStdinCLIHistory(); // this must wait until we have the filesystem inited
bool stdinCLIActive();

void stdinCLIRestoreTerminal(); // restore cooked mode at shutdown, latch raw off

// Use this whereever you want to print on stdout.
// It does nothing if stdin CLI support is not available.
struct StdinCLI_StdoutScope {
	StdinCLI_StdoutScope();
	~StdinCLI_StdoutScope();
};

#endif // OLX_STDINCLISUPPORT_H
