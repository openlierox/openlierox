# Support for Android-based projects that use ndk-build instead of CMake
#
# (Don't forget to set APP_STL=...)

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := libgme

LOCAL_CPP_FEATURES := exceptions
#LOCAL_SANITIZE := undefined

LOCAL_C_INCLUDES := $(LOCAL_PATH)/gme

# YM2612 emulator to use:
# VGM_YM2612_NUKED: LGPLv2.1+
# VGM_YM2612_MAME: GPLv2+
# VGM_YM2612_GENS: LGPLv2.1+
GME_YM2612_EMU=VGM_YM2612_NUKED

# For zlib compressed formats:
GME_ZLIB=N

LOCAL_CFLAGS := -O2 -Wall \
	-DBLARGG_LITTLE_ENDIAN=1 \
	-DBLARGG_BUILD_DLL \
	-DLIBGME_VISIBILITY \
	-fwrapv \
	-fvisibility=hidden \
	-D$(GME_YM2612_EMU)

ifeq ($(GME_ZLIB),Y)
LOCAL_CFLAGS += -DHAVE_ZLIB_H
endif

LOCAL_CPPFLAGS := -std=c++11 \
	-fvisibility-inlines-hidden

LOCAL_LDFLAGS := -Wl,-no-undefined
ifeq ($(GME_ZLIB),Y)
LOCAL_LDFLAGS += -lz
endif

LOCAL_SRC_FILES := \
	gme/Ay_Apu.cpp \
	gme/Ay_Cpu.cpp \
	gme/Ay_Emu.cpp \
	gme/Blip_Buffer.cpp \
	gme/Classic_Emu.cpp \
	gme/Data_Reader.cpp \
	gme/Dual_Resampler.cpp \
	gme/Effects_Buffer.cpp \
	gme/Fir_Resampler.cpp \
	gme/Gb_Apu.cpp \
	gme/Gb_Cpu.cpp \
	gme/Gb_Oscs.cpp \
	gme/Gbs_Emu.cpp \
	gme/Gme_File.cpp \
	gme/Gym_Emu.cpp \
	gme/Hes_Apu.cpp \
	gme/Hes_Cpu.cpp \
	gme/Hes_Emu.cpp \
	gme/Kss_Cpu.cpp \
	gme/Kss_Emu.cpp \
	gme/Kss_Scc_Apu.cpp \
	gme/M3u_Playlist.cpp \
	gme/Multi_Buffer.cpp \
	gme/Music_Emu.cpp \
	gme/Nes_Apu.cpp \
	gme/Nes_Cpu.cpp \
	gme/Nes_Fme7_Apu.cpp \
	gme/Nes_Namco_Apu.cpp \
	gme/Nes_Oscs.cpp \
	gme/Nes_Vrc6_Apu.cpp \
	gme/Nsf_Emu.cpp \
	gme/Nsfe_Emu.cpp \
	gme/Sap_Apu.cpp \
	gme/Sap_Cpu.cpp \
	gme/Sap_Emu.cpp \
	gme/Sms_Apu.cpp \
	gme/Snes_Spc.cpp \
	gme/Spc_Cpu.cpp \
	gme/Spc_Dsp.cpp \
	gme/Spc_Emu.cpp \
	gme/Spc_Filter.cpp \
	gme/Vgm_Emu.cpp \
	gme/Vgm_Emu_Impl.cpp \
	gme/Ym2413_Emu.cpp \
	gme/Ym2612_Nuked.cpp \
	gme/Ym2612_GENS.cpp \
	gme/Ym2612_MAME.cpp \
	gme/gme.cpp

LOCAL_EXPORT_C_INCLUDES = $(LOCAL_PATH)

include $(BUILD_SHARED_LIBRARY)
