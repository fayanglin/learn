LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)
LOCAL_C_INCLUDES:= external/tinyalsa/include
LOCAL_SRC_FILES:= mixer.c pcm.c
LOCAL_MODULE := libtinyalsa
LOCAL_SHARED_LIBRARIES:= liblog libcutils libutils
LOCAL_MODULE_TAGS := optional

include $(BUILD_SHARED_LIBRARY)

#ifeq ($(HOST_OS), linux)
#include $(CLEAR_VARS)
#LOCAL_C_INCLUDES:= external/tinyalsa/include
#LOCAL_SRC_FILES:= mixer.c pcm.c
#LOCAL_MODULE := libtinyalsa
#LOCAL_SHARED_LIBRARIES:= libcutils libutils
#include $(BUILD_HOST_STATIC_LIBRARY)
#endif

include $(CLEAR_VARS)
LOCAL_C_INCLUDES:= external/tinyalsa/include
LOCAL_SRC_FILES:= tinyplay.c
LOCAL_MODULE := tinyplay
LOCAL_SHARED_LIBRARIES:= libcutils libutils libtinyalsa liblog
LOCAL_MODULE_TAGS := optional

include $(BUILD_EXECUTABLE)

#ifeq ($(HOST_OS), linux)
#include $(CLEAR_VARS)
#LOCAL_C_INCLUDES:= external/tinyalsa/include
#LOCAL_SRC_FILES:= tinyplay.c
#LOCAL_MODULE := tinyplay
#LOCAL_SHARED_LIBRARIES:= libcutils libutils libtinyalsa liblog
#LOCAL_MODULE_TAGS := optional
#include $(BUILD_HOST_EXECUTABLE)
#endif

include $(CLEAR_VARS)
LOCAL_C_INCLUDES:= external/tinyalsa/include
LOCAL_SRC_FILES:= tinycap.c
LOCAL_MODULE := tinycap
LOCAL_SHARED_LIBRARIES:= libcutils libutils libtinyalsa liblog
LOCAL_MODULE_TAGS := optional

include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
LOCAL_C_INCLUDES:= external/tinyalsa/include
LOCAL_SRC_FILES:= tinymix.c
LOCAL_MODULE := tinymix
LOCAL_SHARED_LIBRARIES:= libcutils libutils libtinyalsa liblog
LOCAL_MODULE_TAGS := optional
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
LOCAL_C_INCLUDES:= external/tinyalsa/include
LOCAL_SRC_FILES:= tinypcminfo.c
LOCAL_MODULE := tinypcminfo
LOCAL_SHARED_LIBRARIES:= libcutils libutils libtinyalsa liblog
LOCAL_MODULE_TAGS := optional

include $(BUILD_EXECUTABLE)
include $(CLEAR_VARS)
LOCAL_C_INCLUDES:= external/tinyalsa/include
LOCAL_SRC_FILES:= audiocap.c
LOCAL_MODULE := audiocap
LOCAL_SHARED_LIBRARIES:= libcutils libutils libtinyalsa liblog
LOCAL_MODULE_TAGS := optional
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
LOCAL_C_INCLUDES:= external/tinyalsa/include \
	$(call include-path-for, audio-utils) \
	$(call include-path-for, audio-route) \
	$(call include-path-for, speex)
LOCAL_SRC_FILES:= resample.c
LOCAL_MODULE := resample
LOCAL_SHARED_LIBRARIES := liblog libcutils libtinyalsa libaudioutils libaudioroute libhardware_legacy  libspeexresampler
LOCAL_STATIC_LIBRARIES := libspeex
LOCAL_MODULE_TAGS := optional
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
LOCAL_C_INCLUDES:= external/tinyalsa/include \
	$(call include-path-for, audio-utils) \
	$(call include-path-for, audio-route) \
	$(call include-path-for, speex)
LOCAL_SRC_FILES:= oneC.c
LOCAL_MODULE := oneC
LOCAL_SHARED_LIBRARIES := liblog libcutils libtinyalsa libaudioutils libaudioroute libhardware_legacy  libspeexresampler
LOCAL_STATIC_LIBRARIES := libspeex
LOCAL_MODULE_TAGS := optional
include $(BUILD_EXECUTABLE)