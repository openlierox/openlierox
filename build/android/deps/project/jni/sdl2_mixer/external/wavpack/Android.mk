# Android build of wavpack for SDL_mixer

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := libwavpack

LOCAL_C_INCLUDES := $(LOCAL_PATH)/include

ASSEMBLY_CFLAGS_ARM7 := -DOPT_ASM_ARM32
ASSEMBLY_SRC_ARM7 := \
	src/unpack_armv7.S

ASSEMBLY_CFLAGS_X86 := -DOPT_ASM_X86
ASSEMBLY_SRC_X86 := \
	src/pack_x86.S \
	src/unpack_x86.S

ASSEMBLY_CFLAGS_X64 := -DOPT_ASM_X64
ASSEMBLY_SRC_X64 := \
	src/pack_x64.S \
	src/unpack_x64.S

WAVPACK_DSD_CFLAGS := -DENABLE_DSD
WAVPACK_DSD_SRC := \
	src/pack_dsd.c \
	src/unpack_dsd.c

# legacy support is disabled by default
WAVPACK_LEGACY_CFLAGS := -DENABLE_LEGACY
WAVPACK_LEGACY_SRC := \
	src/unpack3.c \
	src/unpack3_open.c \
	src/unpack3_seek.c


ifeq ($(TARGET_ARCH_ABI),armeabi)
PLATFORM_CFLAGS := $(ASSEMBLY_CFLAGS_ARM7)
PLATFORM_SRC := $(ASSEMBLY_SRC_ARM7)
endif

ifeq ($(TARGET_ARCH_ABI),armeabi-v7a)
PLATFORM_CFLAGS := $(ASSEMBLY_CFLAGS_ARM7)
PLATFORM_SRC := $(ASSEMBLY_SRC_ARM7)
endif

ifeq ($(TARGET_ARCH_ABI),x86)
PLATFORM_CFLAGS := $(ASSEMBLY_CFLAGS_X86)
PLATFORM_SRC := $(ASSEMBLY_SRC_X86)
endif

ifeq ($(TARGET_ARCH_ABI),arm64-v8a)
PLATFORM_CFLAGS :=
PLATFORM_SRC :=
endif

ifeq ($(TARGET_ARCH_ABI),x86_64)
PLATFORM_CFLAGS := $(ASSEMBLY_CFLAGS_X64)
PLATFORM_SRC := $(ASSEMBLY_SRC_X64)
endif


LOCAL_CFLAGS := $(WAVPACK_DSD_CFLAGS) $(PLATFORM_CFLAGS)
# $(WAVPACK_LEGACY_CFLAGS)

LOCAL_SRC_FILES := \
    src/common_utils.c \
    src/decorr_utils.c \
    src/entropy_utils.c \
    src/extra1.c \
    src/extra2.c \
    src/open_utils.c \
    src/open_filename.c \
    src/open_legacy.c \
    src/open_raw.c \
    src/pack.c \
    src/pack_dns.c \
    src/pack_floats.c \
    src/pack_utils.c \
    src/read_words.c \
    src/tags.c \
    src/tag_utils.c \
    src/unpack.c \
    src/unpack_floats.c \
    src/unpack_seek.c \
    src/unpack_utils.c \
    src/write_words.c \
    $(WAVPACK_DSD_SRC) \
    $(PLATFORM_SRC)
# $(WAVPACK_LEGACY_SRC)

LOCAL_EXPORT_C_INCLUDES += $(LOCAL_C_INCLUDES)

include $(BUILD_STATIC_LIBRARY)
