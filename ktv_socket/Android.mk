LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)
LOCAL_C_INCLUDES:= external/tinyalsa/include
LOCAL_SRC_FILES:= ktv_audio_client.c
LOCAL_MODULE := clientktv
LOCAL_SHARED_LIBRARIES:= libcutils libutils libtinyalsa liblog
LOCAL_MODULE_TAGS := optional
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
LOCAL_C_INCLUDES:= external/tinyalsa/include
LOCAL_SRC_FILES:= ktv_audio_server.c
LOCAL_MODULE := serverktv
LOCAL_SHARED_LIBRARIES:= libcutils libutils libtinyalsa liblog
LOCAL_MODULE_TAGS := optional
include $(BUILD_EXECUTABLE)