// Minimal stand-in for the freealut entry points OpenLieroX uses on
// Android. freealut's CMake build is broken on modern CMake, and we
// only need a tiny slice of its API: alutInit/alutExit are bypassed
// by initialising OpenAL Soft directly, and the error helpers return
// neutral values.

#include <AL/al.h>
#include <AL/alc.h>

static ALCdevice*  g_device  = 0;
static ALCcontext* g_context = 0;

ALboolean alutInit(int* argcp, char** argv) {
    (void)argcp; (void)argv;
    if (g_context) return AL_TRUE;
    g_device = alcOpenDevice(0);
    if (!g_device) return AL_FALSE;
    g_context = alcCreateContext(g_device, 0);
    if (!g_context) { alcCloseDevice(g_device); g_device = 0; return AL_FALSE; }
    if (!alcMakeContextCurrent(g_context)) {
        alcDestroyContext(g_context); g_context = 0;
        alcCloseDevice(g_device); g_device = 0;
        return AL_FALSE;
    }
    return AL_TRUE;
}

ALboolean alutExit(void) {
    if (g_context) {
        alcMakeContextCurrent(0);
        alcDestroyContext(g_context);
        g_context = 0;
    }
    if (g_device) {
        alcCloseDevice(g_device);
        g_device = 0;
    }
    return AL_TRUE;
}

ALenum alutGetError(void) { return 0; }

const char* alutGetErrorString(ALenum err) {
    (void)err;
    return "alut not available";
}

ALuint alutCreateBufferFromFile(const char* filename) {
    (void)filename;
    return 0;  // 0 = no buffer (caller must check)
}
