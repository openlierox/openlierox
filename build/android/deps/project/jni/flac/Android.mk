LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := flac

LOCAL_C_INCLUDES := $(LOCAL_PATH)/include $(LOCAL_PATH)/src/include $(LOCAL_PATH) $(LOCAL_PATH)/../ogg/include
LOCAL_CFLAGS := -O3 -DHAVE_CONFIG_H -DFLAC__LPC_UNROLLED_FILTER_LOOPS

ifeq "$(TARGET_ARCH_ABI)" "armeabi"
LOCAL_CFLAGS += -DFLAC__INTEGER_ONLY_LIBRARY
endif

LOCAL_CPP_EXTENSION := .cpp

LOCAL_SRC_FILES := $(addprefix src/, $(notdir $(wildcard $(LOCAL_PATH)/src/*.c) $(wildcard $(LOCAL_PATH)/src/*.cpp)))

LOCAL_STATIC_LIBRARIES := ogg

LOCAL_SHARED_LIBRARIES := 

LOCAL_LDLIBS :=

include $(BUILD_STATIC_LIBRARY)

