LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := tremor

LOCAL_C_INCLUDES := $(LOCAL_PATH) $(LOCAL_PATH)/../ogg/include/ogg $(LOCAL_PATH)/../ogg/include $(LOCAL_PATH)/include/tremor
LOCAL_CFLAGS := -O3 -DHAVE_ALLOCA_H

LOCAL_CPP_EXTENSION := .cpp

LOCAL_SRC_FILES := $(notdir $(wildcard $(LOCAL_PATH)/*.c))

LOCAL_STATIC_LIBRARIES := ogg

include $(BUILD_STATIC_LIBRARY)

