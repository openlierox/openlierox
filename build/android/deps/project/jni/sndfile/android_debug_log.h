
#include <stdio.h>
#include <android/log.h>

#define puts(...) __android_log_print(ANDROID_LOG_INFO, "libsndfile", "%s", __VA_ARGS__)
#define printf(...) __android_log_print(ANDROID_LOG_INFO, "libsndfile", __VA_ARGS__)

