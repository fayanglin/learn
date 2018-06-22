LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= ClientW.cpp  ../ISharedBuffer.cpp ../audio_queue.c

LOCAL_C_INCLUDES := ../ISharedBuffer.h 
	
	
#LOCAL_STATIC_LIBRARIES :=
#LOCAL_SHARED_LIBRARIES :=	

#LOCAL_CFLAGS:= -DLOG_TAG=\"HidSendDataService\"

LOCAL_CFLAGS += -fvisibility=hidden

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
	libandroid_runtime

LOCAL_MODULE:= ClientW

include $(BUILD_EXECUTABLE)

