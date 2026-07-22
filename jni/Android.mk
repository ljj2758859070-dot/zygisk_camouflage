LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE := zygisk_hook
LOCAL_SRC_FILES := main.cpp plt_hook.cpp
LOCAL_CPPFLAGS += -std=c++17 -fPIC
LOCAL_LDFLAGS += -Wl,-Bsymbolic
LOCAL_LDLIBS += -llog -ldl

include $(BUILD_SHARED_LIBRARY)
