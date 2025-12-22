cmake_minimum_required(VERSION 3.22.1)

project("nativesensor" VERSION 1.0.0 LANGUAGES CXX)

# C++17 standard for modern features (smart pointers, structured bindings, etc.)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Optimization flags for low-latency
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -ffast-math")

# 16KB page size alignment for Android 15+ compatibility (Google Play requirement)
# See: https://developer.android.com/guide/practices/page-sizes
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,-z,max-page-size=16384")

add_library(${PROJECT_NAME} SHARED
    # Common utilities
    common/sensor_types.h
    common/callback_handler.h
    common/ring_buffer.h

    # IMU module
    imu/imu_data.h
    imu/imu_manager.h
    imu/imu_manager.cpp

    # Camera module
    camera/camera_data.h
    camera/camera_manager.h
    camera/camera_manager.cpp
    camera/camera_stream.h
    camera/camera_stream.cpp

    # JNI bridge
    jni/jni_helpers.h
    jni/jni_bridge.cpp
)

# Find required Android libraries
find_library(log-lib log)
find_library(android-lib android)
find_library(camera2ndk-lib camera2ndk)
find_library(mediandk-lib mediandk)
find_library(nativewindow-lib nativewindow)

# Link against Android NDK libraries
target_link_libraries(${PROJECT_NAME}
    ${log-lib}
    ${android-lib}
    ${camera2ndk-lib}
    ${mediandk-lib}
    ${nativewindow-lib}
)

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/common
    ${CMAKE_CURRENT_SOURCE_DIR}/imu
    ${CMAKE_CURRENT_SOURCE_DIR}/camera
    ${CMAKE_CURRENT_SOURCE_DIR}/jni
)

