/* Minimal <AL/alut.h> shim for the OpenLieroX Android port.
 *
 * The real freealut library doesn't build cleanly on modern CMake;
 * this header pairs with build/android/app/src/main/jni/alut_stub.c,
 * which implements the four entry points OpenLieroX actually calls.
 * alutCreateBufferFromFile returns 0 (no buffer) so sample loading
 * fails gracefully. */

#ifndef OLX_ALUT_H
#define OLX_ALUT_H

#include <AL/al.h>
#include <AL/alc.h>

#ifdef __cplusplus
extern "C" {
#endif

ALboolean   alutInit(int* argcp, char** argv);
ALboolean   alutExit(void);
ALenum      alutGetError(void);
const char* alutGetErrorString(ALenum err);
ALuint      alutCreateBufferFromFile(const char* filename);

#ifdef __cplusplus
}
#endif

#endif
