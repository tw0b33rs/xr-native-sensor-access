#pragma once

#include <camera/NdkCameraDevice.h>
#include <camera/NdkCameraCaptureSession.h>
#include <camera/NdkCaptureRequest.h>
#include <media/NdkImageReader.h>
#include <android/native_window.h>
#include <functional>
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include <string>

#include "camera_data.h"
#include "camera_manager.h"

namespace nativesensor {

/// Callback for frame statistics updates
using CameraStatsCallback = std::function<void(const CameraStats&)>;

/// Zero-copy camera stream using AImageReader with ANativeWindow output
class CameraStream {
public:
    explicit CameraStream(CameraManager& manager);
    ~CameraStream();

    CameraStream(const CameraStream&) = delete;
    CameraStream& operator=(const CameraStream&) = delete;

    /// Start streaming to a native window surface (zero-copy)
    /// @param cameraId Camera to open
    /// @param surface Native window from SurfaceView/SpatialExternalSurface
    /// @param statsCallback Optional callback for frame statistics
    /// @return true if streaming started successfully
    bool startPreview(const std::string& cameraId, 
                      ANativeWindow* surface,
                      CameraStatsCallback statsCallback = nullptr);

    /// Stop streaming and release resources
    void stopPreview();

    /// Check if currently streaming
    [[nodiscard]]
    bool isStreaming() const { return streaming_.load(std::memory_order_acquire); }

    /// Get current camera statistics
    [[nodiscard]]
    CameraStats getStats() const;

    /// Get the currently active camera ID
    [[nodiscard]] [[maybe_unused]]
    std::string getCurrentCameraId() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return currentCameraId_; 
    }

private:
    // Camera device callbacks
    static void onDeviceDisconnected(void* context, ACameraDevice* device);
    static void onDeviceError(void* context, ACameraDevice* device, int error);

    // Capture session callbacks
    static void onSessionClosed(void* context, ACameraCaptureSession* session);
    static void onSessionReady(void* context, ACameraCaptureSession* session);
    static void onSessionActive(void* context, ACameraCaptureSession* session);

    // Capture callbacks for statistics
    static void onCaptureStarted(void* context, ACameraCaptureSession* session,
                                  const ACaptureRequest* request, int64_t timestamp);
    static void onCaptureCompleted(void* context, ACameraCaptureSession* session,
                                    ACaptureRequest* request, const ACameraMetadata* result);

    void cleanup();
    void updateStats(int64_t timestampNs);

    CameraManager& manager_;
    mutable std::mutex mutex_;

    std::atomic<bool> streaming_{false};
    std::string currentCameraId_;

    // NDK handles (RAII cleanup in destructor/cleanup)
    ACameraDevice* cameraDevice_ = nullptr;
    ACameraCaptureSession* captureSession_ = nullptr;
    ACaptureSessionOutputContainer* outputContainer_ = nullptr;
    ACaptureSessionOutput* sessionOutput_ = nullptr;
    ACameraOutputTarget* outputTarget_ = nullptr;
    ACaptureRequest* captureRequest_ = nullptr;
    ANativeWindow* surface_ = nullptr;

    // Statistics tracking
    CameraStatsCallback statsCallback_;
    std::atomic<int64_t> frameCount_{0};
    std::atomic<int64_t> droppedFrames_{0};

    // Direct frequency/latency calculation (instant values)
    mutable std::mutex statsMutex_;
    int64_t prevFrameTimestampNs_{0};   // Previous frame's hardware timestamp for frequency calc
    float lastFrameRateHz_{0.0f};       // Frequency = 1 / (currentTs - prevTs)
    float lastLatencyMs_{0.0f};         // Latency = now - eventTimestamp
    int64_t lastCallbackTimeNs_{0};     // For periodic callback throttling

    // Callback structs (must persist for lifetime of camera session)
    ACameraDevice_StateCallbacks deviceCallbacks_{};
    ACameraCaptureSession_stateCallbacks sessionCallbacks_{};
    ACameraCaptureSession_captureCallbacks captureCallbacks_{};
};

}  // namespace nativesensor
