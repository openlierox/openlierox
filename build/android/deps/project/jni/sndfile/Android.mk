LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := sndfile

LOCAL_C_INCLUDES := $(LOCAL_PATH)/include $(LOCAL_PATH)/src $(LOCAL_PATH)/../ogg/include $(LOCAL_PATH)/../vorbis/include $(LOCAL_PATH)/../flac/include
LOCAL_CFLAGS := -DHAVE_CONFIG_H -include $(LOCAL_PATH)/android_debug_log.h


LOCAL_CPP_EXTENSION := .cpp

APP_SUBDIRS := $(patsubst $(LOCAL_PATH)/%, %, $(shell find $(LOCAL_PATH)/src -type d))
LOCAL_SRC_FILES := $(foreach F, $(APP_SUBDIRS), $(addprefix $(F)/,$(notdir $(wildcard $(LOCAL_PATH)/$(F)/*.cpp))))
LOCAL_SRC_FILES += $(foreach F, $(APP_SUBDIRS), $(addprefix $(F)/,$(notdir $(wildcard $(LOCAL_PATH)/$(F)/*.c))))

LOCAL_STATIC_LIBRARIES := flac vorbis ogg

LOCAL_SHARED_LIBRARIES := 

LOCAL_LDLIBS := -llog

include $(BUILD_SHARED_LIBRARY)
