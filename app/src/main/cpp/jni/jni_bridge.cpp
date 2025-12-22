#include <jni.h>
#include <string>
#include <sstream>
#include <memory>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <android/log.h>
#include <android/native_window_jni.h>

#include "imu_manager.h"
#include "camera_manager.h"
#include "camera_stream.h"
#include "jni_helpers.h"

namespace {

constexpr const char* kLogTag = "NativeSensor.JNI";

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, kLogTag, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, kLogTag, __VA_ARGS__)

constexpr double kNsToMs = 1'000'000.0;

// IMU manager singleton
std::unique_ptr<nativesensor::ImuManager> g_imuManager;
std::mutex g_imuMutex;

// Camera manager singleton and per-camera streams
std::unique_ptr<nativesensor::CameraManager> g_cameraManager;
std::unordered_map<std::string, std::unique_ptr<nativesensor::CameraStream>> g_cameraStreams;
std::mutex g_cameraMutex;

nativesensor::ImuManager* getImuManager() {
    std::lock_guard<std::mutex> lock(g_imuMutex);
    if (!g_imuManager) {
        g_imuManager = std::make_unique<nativesensor::ImuManager>();
    }
    return g_imuManager.get();
}

nativesensor::CameraManager* getCameraManager() {
    std::lock_guard<std::mutex> lock(g_cameraMutex);
    if (!g_cameraManager) {
        g_cameraManager = std::make_unique<nativesensor::CameraManager>();
    }
    return g_cameraManager.get();
}

nativesensor::CameraStream* getOrCreateCameraStream(const std::string& cameraId) {
    // Get manager first (uses the same mutex)
    auto* manager = getCameraManager();

    std::lock_guard<std::mutex> lock(g_cameraMutex);
    auto it = g_cameraStreams.find(cameraId);
    if (it == g_cameraStreams.end()) {
        auto stream = std::make_unique<nativesensor::CameraStream>(*manager);
        auto* ptr = stream.get();
        g_cameraStreams[cameraId] = std::move(stream);
        return ptr;
    }
    return it->second.get();
}

void stopCameraStream(const std::string& cameraId) {
    std::lock_guard<std::mutex> lock(g_cameraMutex);
    auto it = g_cameraStreams.find(cameraId);
    if (it != g_cameraStreams.end()) {
        it->second->stopPreview();
        g_cameraStreams.erase(it);
    }
}

void stopAllCameraStreams() {
    std::lock_guard<std::mutex> lock(g_cameraMutex);
    for (auto& [id, stream] : g_cameraStreams) {
        stream->stopPreview();
    }
    g_cameraStreams.clear();
}

// No-op JNI_OnLoad, but required for JNI linkage
JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
    JNIEnv* env;
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
        return JNI_ERR;
    }
    LOGI("Native sensor library loaded successfully");
    return JNI_VERSION_1_6;
}

}  // namespace

extern "C" {

// Package: com.tw0b33rs.nativesensoraccess.sensor
// Class: NativeSensorBridge

JNIEXPORT void JNICALL
Java_com_tw0b33rs_nativesensoraccess_sensor_NativeSensorBridge_nativeInit(
    JNIEnv* /* env */,
    jobject /* thiz */) {
    LOGI("NativeSensorBridge.nativeInit()");
    auto* manager = getImuManager();
    manager->start([](const nativesensor::ImuSample&) {});
}

JNIEXPORT void JNICALL
Java_com_tw0b33rs_nativesensoraccess_sensor_NativeSensorBridge_nativeStop(
    JNIEnv* /* env */,
    jobject /* thiz */) {
    LOGI("NativeSensorBridge.nativeStop()");
    if (g_imuManager) {
        g_imuManager->stop();
    }
}

JNIEXPORT jfloatArray JNICALL
Java_com_tw0b33rs_nativesensoraccess_sensor_NativeSensorBridge_nativeGetAccelData(
    JNIEnv* env,
    jobject /* thiz */) {
    auto* manager = getImuManager();
    auto sample = manager->getLatestAccel();

    jfloatArray result = env->NewFloatArray(4);
    float data[4] = {sample.x, sample.y, sample.z,
                     static_cast<float>(static_cast<double>(sample.timestampNs) / kNsToMs)};
    env->SetFloatArrayRegion(result, 0, 4, data);
    return result;
}

JNIEXPORT jfloatArray JNICALL
Java_com_tw0b33rs_nativesensoraccess_sensor_NativeSensorBridge_nativeGetGyroData(
    JNIEnv* env,
    jobject /* thiz */) {
    auto* manager = getImuManager();
    auto sample = manager->getLatestGyro();

    jfloatArray result = env->NewFloatArray(4);
    float data[4] = {sample.x, sample.y, sample.z,
                     static_cast<float>(static_cast<double>(sample.timestampNs) / kNsToMs)};
    env->SetFloatArrayRegion(result, 0, 4, data);
    return result;
}

JNIEXPORT jfloatArray JNICALL
Java_com_tw0b33rs_nativesensoraccess_sensor_NativeSensorBridge_nativeGetStats(
    JNIEnv* env,
    jobject /* thiz */) {
    auto* manager = getImuManager();
    auto stats = manager->getStats();

    jfloatArray result = env->NewFloatArray(4);
    float data[4] = {
        stats.accelFrequencyHz,
        stats.accelLatencyMs,
        stats.gyroFrequencyHz,
        stats.gyroLatencyMs
    };
    env->SetFloatArrayRegion(result, 0, 4, data);
    return result;
}

JNIEXPORT jintArray JNICALL
Java_com_tw0b33rs_nativesensoraccess_sensor_NativeSensorBridge_nativeGetMetadata(
    JNIEnv* env,
    jobject /* thiz */) {
    auto* manager = getImuManager();
    auto meta = manager->getMetadata();

    jintArray result = env->NewIntArray(4);
    int data[4] = {
        meta.accelMinDelayUs,
        meta.accelFifoReserved,
        meta.gyroMinDelayUs,
        meta.gyroFifoReserved
    };
    env->SetIntArrayRegion(result, 0, 4, data);
    return result;
}

JNIEXPORT jstring JNICALL
Java_com_tw0b33rs_nativesensoraccess_sensor_NativeSensorBridge_nativeEnumerateSensors(
    JNIEnv* env,
    jobject /* thiz */) {
    auto* manager = getImuManager();
    auto sensors = manager->enumerateSensors();

    std::ostringstream ss;
    for (const auto& sensor : sensors) {
        ss << sensor.handle << "|"
           << static_cast<int>(sensor.type) << "|"
           << (sensor.name ? sensor.name : "Unknown") << "|"
           << (sensor.vendor ? sensor.vendor : "Unknown") << "|"
           << sensor.minDelayUs << "|"
           << sensor.maxFrequencyHz << "|"
           << sensor.fifoReserved << "\n";
    }

    return env->NewStringUTF(ss.str().c_str());
}

JNIEXPORT void JNICALL
Java_com_tw0b33rs_nativesensoraccess_sensor_NativeSensorBridge_nativeSwitchSensors(
    JNIEnv* /* env */,
    jobject /* thiz */,
    jint accelHandle,
    jint gyroHandle) {
    LOGI("Switching sensors - Accel: %d, Gyro: %d", accelHandle, gyroHandle);
    auto* manager = getImuManager();
    manager->switchSensors(accelHandle, gyroHandle);
}

JNIEXPORT jboolean JNICALL
Java_com_tw0b33rs_nativesensoraccess_sensor_NativeSensorBridge_nativeIsRunning(
    JNIEnv* /* env */,
    jobject /* thiz */) {
    if (!g_imuManager) return JNI_FALSE;
    return g_imuManager->isRunning() ? JNI_TRUE : JNI_FALSE;
}

// =============================================================================
// Camera JNI Functions (CameraBridge)
// =============================================================================

JNIEXPORT jstring JNICALL
Java_com_tw0b33rs_nativesensoraccess_sensor_CameraBridge_nativeEnumerateCameras(
    JNIEnv* env,
    jobject /* thiz */) {
    LOGI("CameraBridge.nativeEnumerateCameras()");
    auto* manager = getCameraManager();
    auto cameras = manager->enumerateCameras();

    std::ostringstream ss;
    for (const auto& cam : cameras) {
        ss << cam.id << "|"
           << static_cast<int>(cam.facing) << "|"
           << static_cast<int>(cam.clusterType) << "|"
           << cam.width << "|"
           << cam.height << "|"
           << cam.maxFps << "|"
           << (cam.isPhysicalCamera ? 1 : 0) << "|"
           << cam.physicalCameraIds << "\n";
    }

    return env->NewStringUTF(ss.str().c_str());
}

JNIEXPORT jboolean JNICALL
Java_com_tw0b33rs_nativesensoraccess_sensor_CameraBridge_nativeStartPreview(
    JNIEnv* env,
    jobject /* thiz */,
    jstring cameraId,
    jobject surface) {
    const char* idStr = env->GetStringUTFChars(cameraId, nullptr);
    std::string id(idStr);
    env->ReleaseStringUTFChars(cameraId, idStr);

    LOGI("CameraBridge.nativeStartPreview(%s)", id.c_str());

    if (!surface) {
        LOGE("Cannot start preview: null surface");
        return JNI_FALSE;
    }

    ANativeWindow* window = ANativeWindow_fromSurface(env, surface);
    if (!window) {
        LOGE("Cannot start preview: failed to get ANativeWindow from surface");
        return JNI_FALSE;
    }

    auto* stream = getOrCreateCameraStream(id);
    bool success = stream->startPreview(id, window, nullptr);
    ANativeWindow_release(window);

    return success ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL
Java_com_tw0b33rs_nativesensoraccess_sensor_CameraBridge_nativeStopPreview(
    JNIEnv* /* env */,
    jobject /* thiz */) {
    LOGI("CameraBridge.nativeStopPreview() - stopping all cameras");
    stopAllCameraStreams();
}

JNIEXPORT void JNICALL
Java_com_tw0b33rs_nativesensoraccess_sensor_CameraBridge_nativeStopCameraPreview(
    JNIEnv* env,
    jobject /* thiz */,
    jstring cameraId) {
    const char* idStr = env->GetStringUTFChars(cameraId, nullptr);
    std::string id(idStr);
    env->ReleaseStringUTFChars(cameraId, idStr);

    LOGI("CameraBridge.nativeStopCameraPreview(%s)", id.c_str());
    stopCameraStream(id);
}

JNIEXPORT jfloatArray JNICALL
Java_com_tw0b33rs_nativesensoraccess_sensor_CameraBridge_nativeGetCameraStats(
    JNIEnv* env,
    jobject /* thiz */) {
    // Return combined stats from all streams (for backward compatibility)
    std::lock_guard<std::mutex> lock(g_cameraMutex);

    float avgFrameRate = 0.0f;
    float maxLatency = 0.0f;
    float totalFrameCount = 0.0f;
    float totalDroppedFrames = 0.0f;
    int activeStreamCount = 0;

    for (const auto& [id, stream] : g_cameraStreams) {
        if (stream && stream->isStreaming()) {
            auto stats = stream->getStats();
            avgFrameRate += stats.frameRateHz;
            maxLatency = std::max(maxLatency, stats.latencyMs);
            totalFrameCount += static_cast<float>(stats.frameCount);
            totalDroppedFrames += static_cast<float>(stats.droppedFrames);
            activeStreamCount++;
        }
    }

    // Average frame rate across all streams instead of sum
    // (individual camera FPS is meaningful, summed FPS is not)
    if (activeStreamCount > 0) {
        avgFrameRate /= static_cast<float>(activeStreamCount);
    }

    jfloatArray result = env->NewFloatArray(4);
    float data[4] = {avgFrameRate, maxLatency, totalFrameCount, totalDroppedFrames};
    env->SetFloatArrayRegion(result, 0, 4, data);
    return result;
}

JNIEXPORT jfloatArray JNICALL
Java_com_tw0b33rs_nativesensoraccess_sensor_CameraBridge_nativeGetCameraStatsById(
    JNIEnv* env,
    jobject /* thiz */,
    jstring cameraId) {
    const char* idStr = env->GetStringUTFChars(cameraId, nullptr);
    std::string id(idStr);
    env->ReleaseStringUTFChars(cameraId, idStr);

    std::lock_guard<std::mutex> lock(g_cameraMutex);
    auto it = g_cameraStreams.find(id);

    nativesensor::CameraStats stats{};
    if (it != g_cameraStreams.end() && it->second) {
        stats = it->second->getStats();
    }

    jfloatArray result = env->NewFloatArray(4);
    float data[4] = {
        stats.frameRateHz,
        stats.latencyMs,
        static_cast<float>(stats.frameCount),
        static_cast<float>(stats.droppedFrames)
    };
    env->SetFloatArrayRegion(result, 0, 4, data);
    return result;
}

JNIEXPORT jboolean JNICALL
Java_com_tw0b33rs_nativesensoraccess_sensor_CameraBridge_nativeIsStreaming(
    JNIEnv* /* env */,
    jobject /* thiz */) {
    // Returns true if any camera is streaming (backward compatibility)
    std::lock_guard<std::mutex> lock(g_cameraMutex);
    for (const auto& [id, stream] : g_cameraStreams) {
        if (stream && stream->isStreaming()) {
            return JNI_TRUE;
        }
    }
    return JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_com_tw0b33rs_nativesensoraccess_sensor_CameraBridge_nativeIsCameraStreaming(
    JNIEnv* env,
    jobject /* thiz */,
    jstring cameraId) {
    const char* idStr = env->GetStringUTFChars(cameraId, nullptr);
    std::string id(idStr);
    env->ReleaseStringUTFChars(cameraId, idStr);

    std::lock_guard<std::mutex> lock(g_cameraMutex);
    auto it = g_cameraStreams.find(id);
    if (it != g_cameraStreams.end() && it->second) {
        return it->second->isStreaming() ? JNI_TRUE : JNI_FALSE;
    }
    return JNI_FALSE;
}

JNIEXPORT jstring JNICALL
Java_com_tw0b33rs_nativesensoraccess_sensor_CameraBridge_nativeGetCurrentCameraId(
    JNIEnv* env,
    jobject /* thiz */) {
    // Returns comma-separated list of all streaming camera IDs
    std::lock_guard<std::mutex> lock(g_cameraMutex);
    std::ostringstream ss;
    bool first = true;
    for (const auto& [id, stream] : g_cameraStreams) {
        if (stream && stream->isStreaming()) {
            if (!first) ss << ",";
            ss << id;
            first = false;
        }
    }
    return env->NewStringUTF(ss.str().c_str());
}

JNIEXPORT jint JNICALL
Java_com_tw0b33rs_nativesensoraccess_sensor_CameraBridge_nativeGetActiveStreamCount(
    JNIEnv* /* env */,
    jobject /* thiz */) {
    std::lock_guard<std::mutex> lock(g_cameraMutex);
    int count = 0;
    for (const auto& [id, stream] : g_cameraStreams) {
        if (stream && stream->isStreaming()) {
            count++;
        }
    }
    return count;
}

}  // extern "C"