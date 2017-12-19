LOCAL_PATH := $(call my-dir)
PROJECT_ROOT:= $(call my-dir)/../../../..

include $(CLEAR_VARS)


OPENCV_CAMERA_MODULES:=on
OPENCV_INSTALL_MODULES:=on
OPENCV_LIB_TYPE:=SHARED

OPENCVROOT:= $(PROJECT_ROOT)/../OpenCV-android-sdk
include $(OPENCVROOT)/sdk/native/jni/OpenCV.mk


LOCAL_MODULE    := nativeStillCapture
LOCAL_CFLAGS    := -Werror -Wno-write-strings -std=c++11
LOCAL_SRC_FILES := native-lib.cpp \
                   CV_Main.cpp \
                   Native_Camera.cpp \
                   Image_Reader.cpp

LOCAL_LDLIBS    := -llog -landroid -lcamera2ndk -lmediandk
LOCAL_LDFLAGS += -v
include $(BUILD_SHARED_LIBRARY)
