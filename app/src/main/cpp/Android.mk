LOCAL_PATH := $(call my-dir)
PROJECT_ROOT:= $(call my-dir)/../../../..

include $(CLEAR_VARS)

OPENCV_INSTALL_MODULES:=on

include D:/51.java_proj/OpenCV-android-sdk/sdk/native/jni/OpenCV.mk

LOCAL_MODULE    := libopencv_ndk
LOCAL_CFLAGS    := -Werror -Wno-write-strings -std=c++11
LOCAL_SRC_FILES := native-lib.cpp \
                   CV_Main.cpp \
                   Native_Camera.cpp \
                   Image_Reader.cpp \
                   native-camera-jni.cpp
LOCAL_LDLIBS    := -llog -landroid -lcamera2ndk -lmediandk
include $(BUILD_SHARED_LIBRARY)
