LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := NoxxaRewind
LOCAL_CPP_EXTENSION := .cpp .cc

LOCAL_SRC_FILES := \
    src/main.cpp \
    src/WorldState.cpp \
    src/RewindCore.cpp \
    src/RewindAudio.cpp \
    src/RewindUI.cpp \
    src/TemporalFX.cpp \
    deps/AndroidModLoader/mod/logger.cpp \
    deps/AndroidModLoader/mod/config.cpp

LOCAL_C_INCLUDES += \
    $(LOCAL_PATH) \
    $(LOCAL_PATH)/deps/AndroidModLoader \
    $(LOCAL_PATH)/deps/aml-psdk

LOCAL_CXXFLAGS += -O3 -DNDEBUG -std=c++17 -fexceptions -ffunction-sections -fdata-sections -include math.h -include float.h
LOCAL_LDFLAGS += -Wl,--gc-sections
LOCAL_LDLIBS += -llog -lOpenSLES -ldl -landroid

include $(BUILD_SHARED_LIBRARY)
