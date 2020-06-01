#===============================================
# This Makefile is responsible for building HIDL server and HAL implementation
# separately into binary and shared library respectively
#===============================================

#===============================================
# Makefile for compliling service binary
#===============================================
LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE_RELATIVE_PATH := hw
LOCAL_PROPRIETARY_MODULE := true

LOCAL_MODULE := android.hardware.hvuleds@2.0-service
LOCAL_INIT_RC := android.hardware.hvuleds@2.0-service.rc
LOCAL_SRC_FILES := \
    service.cpp \

LOCAL_SHARED_LIBRARIES := \
    libcutils \
    libdl \
    libbase \
    libutils \
    libhardware \
    libhidlbase \
    libhidltransport \
    android.hardware.hvuleds@2.0 \
    android.hardware.hvuleds@2.0-impl \

include $(BUILD_EXECUTABLE)


#=================================================
# Makefile for compiling implement shared library
#=================================================
include $(CLEAR_VARS)
LOCAL_MODULE := android.hardware.hvuleds@2.0-impl
LOCAL_SRC_FILES := Hvuleds.cpp

LOCAL_SHARED_LIBRARIES := \
    libbase \
    liblog \
    libhidlbase \
    libhidltransport \
    libhardware \
    libutils \
    android.hardware.hvuleds@2.0 \

LOCAL_MODULE_TAGS := optional

include $(BUILD_SHARED_LIBRARY)
