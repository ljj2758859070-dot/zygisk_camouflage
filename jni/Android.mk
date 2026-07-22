LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE := zygisk_hook
LOCAL_SRC_FILES := main.cpp
LOCAL_CPPFLAGS += -std=c++17
LOCAL_LDLIBS += -llog

include $(BUILD_SHARED_LIBRARY)
