#include <stdarg.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <jni.h>

#include "lzma.h"

#include "jniwrapperstuff.h"

JNIEXPORT jlong JNICALL
JAVA_EXPORT_NAME(XZInputStream_nativeInit) (JNIEnv* env, jobject thiz)
{
	lzma_stream * stream = (lzma_stream *) malloc(sizeof(lzma_stream));
	lzma_stream tmp = LZMA_STREAM_INIT;
	*stream = tmp;
	int ret = lzma_stream_decoder(stream, UINT64_MAX, LZMA_CONCATENATED);
	if (ret != LZMA_OK)
	{
		free(stream);
		return 0;
	}
	return (jlong) (intptr_t) stream;
}

JNIEXPORT void JNICALL
JAVA_EXPORT_NAME(XZInputStream_nativeClose) (JNIEnv* env, jobject thiz, jlong nativeData)
{
	lzma_stream * stream = (lzma_stream *) (intptr_t) nativeData;
	lzma_end(stream);
}

JNIEXPORT jint JNICALL
JAVA_EXPORT_NAME(XZInputStream_nativeRead) (JNIEnv* env, jobject thiz, jlong nativeData,
											jobject inBuf, jint inAvailable, jobject outBuf,
											jint outCount, jobject offsets)
{
	lzma_stream * stream = (lzma_stream *) (intptr_t) nativeData;
	uint8_t *inBufNative = (uint8_t *) (*env)->GetPrimitiveArrayCritical(env, inBuf, NULL);
	uint8_t *outBufNative = (uint8_t *) (*env)->GetPrimitiveArrayCritical(env, outBuf, NULL);
	jint *offsetsNative =  (jint *) (*env)->GetPrimitiveArrayCritical(env, offsets, NULL);
	jint inOffset = offsetsNative[0];
	jint outOffset = offsetsNative[1];
	int ret;

	stream->avail_in = inAvailable - inOffset;
	stream->next_in = inBufNative + inOffset;
	stream->avail_out = outCount;
	stream->next_out = outBufNative + outOffset;
	ret = lzma_code(stream, inAvailable == -1 ? LZMA_FINISH : LZMA_RUN);

	(*env)->ReleasePrimitiveArrayCritical(env, offsets, offsetsNative, 0);
	(*env)->ReleasePrimitiveArrayCritical(env, outBuf, outBufNative, 0);
	(*env)->ReleasePrimitiveArrayCritical(env, inBuf, inBufNative, 0);

	inOffset = inAvailable - stream->avail_in;
	outOffset += outCount - stream->avail_out;

	offsetsNative[0] = inOffset;
	offsetsNative[1] = outOffset;

	return ret;
}
