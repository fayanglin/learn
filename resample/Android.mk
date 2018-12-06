LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
#LOCAL_MODULE := hampoo_audio.default
#LOCAL_MODULE_RELATIVE_PATH := hw
LOCAL_MODULE := hampoo_audio
LOCAL_SRC_FILES := \
	hampoo_audio_hw.c \
	AudioQueue.c cae_thread.c

LOCAL_C_INCLUDES += \
	external/tinyalsa/include \
	$(call include-path-for, audio-utils) \
	$(call include-path-for, audio-route) \
	$(call include-path-for, speex)

LOCAL_SHARED_LIBRARIES := liblog libcutils libtinyalsa libaudioutils libaudioroute libhardware_legacy
LOCAL_STATIC_LIBRARIES := libspeex
LOCAL_MODULE_TAGS := optional
include $(BUILD_EXECUTABLE)
#include $(BUILD_SHARED_LIBRARY)


