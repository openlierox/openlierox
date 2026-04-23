LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := miniupnpc

LOCAL_C_INCLUDES := 
LOCAL_CFLAGS := -O2 -DMINIUPNPC_SET_SOCKET_TIMEOUT -D_BSD_SOURCE -D_POSIX_C_SOURCE=200112L 

LOCAL_CPP_EXTENSION := .cpp

LOCAL_SRC_FILES := $(foreach F, ., $(addprefix $(F)/,$(notdir $(wildcard $(LOCAL_PATH)/$(F)/*.cpp))))
LOCAL_SRC_FILES += $(foreach F, ., $(addprefix $(F)/,$(notdir $(wildcard $(LOCAL_PATH)/$(F)/*.c))))

LOCAL_SHARED_LIBRARIES :=

LOCAL_STATIC_LIBRARIES :=

LOCAL_LDLIBS :=

include $(BUILD_SHARED_LIBRARY)
