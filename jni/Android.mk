LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE := zygisk_hook
LOCAL_SRC_FILES := main.cpp plt_hook.cpp
LOCAL_LDLIBS := -llog
LOCAL_CPPFLAGS += -O2

include $(BUILD_SHARED_LIBRARY)
