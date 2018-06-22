LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
	ISharedBuffer.cpp \
	SharedBufferServer.cpp \
	audio_queue.c \
	ClientW.cpp \
	ClientR.cpp

LOCAL_MODULE_TAGS := optional 

#LOCAL_CFLAGS += -fvisibility=hidden

LOCAL_SHARED_LIBRARIES := \
	libcutils \
	libhardware \
	libhardware_legacy \
	libutils \
	liblog \
	libbinder \
	libui \
	libgui \
	libnativehelper \
	libandroid_runtime \
	libaudioutils libmedia 

LOCAL_MODULE:= libShareHost

include $(BUILD_SHARED_LIBRARY)
#include $(BUILD_STATIC_LIBRARY)

