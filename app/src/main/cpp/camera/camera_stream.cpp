#include "camera_stream.h"

#include <android/log.h>
#include <ctime>

namespace {
constexpr const char* kLogTag = "NativeSensor";
constexpr int64_t kNsPerSecond = 1'000'000'000LL;
constexpr double kNsToMs = 1'000'000.0;
}

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, kLogTag, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, kLogTag, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, kLogTag, __VA_ARGS__)

namespace nativesensor {

namespace {

int64_t getBootTimeNs() noexcept {
    struct timespec t{};
    clock_gettime(CLOCK_BOOTTIME, &t);
    return static_cast<int64_t>(t.tv_sec) * kNsPerSecond + t.tv_nsec;
}

}  // namespace

CameraStream::CameraStream(CameraManager& manager)
    : manager_(manager) {
    LOGI("CameraStream created");
}

CameraStream::~CameraStream() {
    stopPreview();
    LOGI("CameraStream destroyed");
}

bool CameraStream::startPreview(const std::string& cameraId,
                                 ANativeWindow* surface,
                                 CameraStatsCallback statsCallback) {
    std::lock_guard<std::mutex> lock(mutex_);

    // If already streaming the same camera, skip restart
    if (streaming_.load(std::memory_order_acquire) && currentCameraId_ == cameraId) {
        LOGI("Already streaming camera %s, skipping restart", cameraId.c_str());
        return true;
    }

    if (streaming_.load(std::memory_order_acquire)) {
        LOGI("Switching from camera %s to %s", currentCameraId_.c_str(), cameraId.c_str());
        cleanup();
    }

    if (!manager_.isValid()) {
        LOGE("Cannot start preview: camera manager invalid");
        return false;
    }

    if (!surface) {
        LOGE("Cannot start preview: null surface");
        return false;
    }

    LOGI("Starting camera preview: %s", cameraId.c_str());

    surface_ = surface;
    ANativeWindow_acquire(surface_);
    statsCallback_ = std::move(statsCallback);
    currentCameraId_ = cameraId;

    // Reset statistics
    frameCount_.store(0, std::memory_order_release);
    droppedFrames_.store(0, std::memory_order_release);
    {
        std::lock_guard<std::mutex> statsLock(statsMutex_);
        prevFrameTimestampNs_ = 0;
        lastFrameRateHz_ = 0.0f;
        lastLatencyMs_ = 0.0f;
        lastCallbackTimeNs_ = 0;
    }

    // Setup device callbacks
    deviceCallbacks_.context = this;
    deviceCallbacks_.onDisconnected = onDeviceDisconnected;
    deviceCallbacks_.onError = onDeviceError;

    // Open camera device
    camera_status_t status = ACameraManager_openCamera(
        manager_.getNativeManager(),
        cameraId.c_str(),
        &deviceCallbacks_,
        &cameraDevice_);

    if (status != ACAMERA_OK || !cameraDevice_) {
        LOGE("Failed to open camera %s: %d", cameraId.c_str(), status);
        cleanup();
        return false;
    }

    LOGI("Camera device opened: %s", cameraId.c_str());

    // Create output target from surface
    status = ACameraOutputTarget_create(surface_, &outputTarget_);
    if (status != ACAMERA_OK) {
        LOGE("Failed to create output target: %d", status);
        cleanup();
        return false;
    }

    // Create capture request
    status = ACameraDevice_createCaptureRequest(cameraDevice_, 
        TEMPLATE_PREVIEW, &captureRequest_);
    if (status != ACAMERA_OK) {
        LOGE("Failed to create capture request: %d", status);
        cleanup();
        return false;
    }

    // Add target to request
    status = ACaptureRequest_addTarget(captureRequest_, outputTarget_);
    if (status != ACAMERA_OK) {
        LOGE("Failed to add target to request: %d", status);
        cleanup();
        return false;
    }

    // Create session output container
    status = ACaptureSessionOutputContainer_create(&outputContainer_);
    if (status != ACAMERA_OK) {
        LOGE("Failed to create output container: %d", status);
        cleanup();
        return false;
    }

    // Create session output
    status = ACaptureSessionOutput_create(surface_, &sessionOutput_);
    if (status != ACAMERA_OK) {
        LOGE("Failed to create session output: %d", status);
        cleanup();
        return false;
    }

    // Add output to container
    status = ACaptureSessionOutputContainer_add(outputContainer_, sessionOutput_);
    if (status != ACAMERA_OK) {
        LOGE("Failed to add output to container: %d", status);
        cleanup();
        return false;
    }

    // Setup session callbacks
    sessionCallbacks_.context = this;
    sessionCallbacks_.onClosed = onSessionClosed;
    sessionCallbacks_.onReady = onSessionReady;
    sessionCallbacks_.onActive = onSessionActive;

    // Create capture session
    status = ACameraDevice_createCaptureSession(
        cameraDevice_,
        outputContainer_,
        &sessionCallbacks_,
        &captureSession_);

    if (status != ACAMERA_OK || !captureSession_) {
        LOGE("Failed to create capture session: %d", status);
        cleanup();
        return false;
    }

    LOGI("Capture session created");

    // Setup capture callbacks for statistics
    captureCallbacks_.context = this;
    captureCallbacks_.onCaptureStarted = onCaptureStarted;
    captureCallbacks_.onCaptureCompleted = onCaptureCompleted;
    captureCallbacks_.onCaptureFailed = nullptr;
    captureCallbacks_.onCaptureSequenceCompleted = nullptr;
    captureCallbacks_.onCaptureSequenceAborted = nullptr;
    captureCallbacks_.onCaptureBufferLost = nullptr;

    // Start repeating capture request
    status = ACameraCaptureSession_setRepeatingRequest(
        captureSession_,
        &captureCallbacks_,
        1,
        &captureRequest_,
        nullptr);

    if (status != ACAMERA_OK) {
        LOGE("Failed to set repeating request: %d", status);
        cleanup();
        return false;
    }

    streaming_.store(true, std::memory_order_release);
    LOGI("Camera streaming started: %s", cameraId.c_str());
    return true;
}

void CameraStream::stopPreview() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!streaming_.load(std::memory_order_acquire)) {
        return;
    }

    LOGI("Stopping camera preview");
    cleanup();
}

void CameraStream::cleanup() {
    streaming_.store(false, std::memory_order_release);

    if (captureSession_) {
        ACameraCaptureSession_stopRepeating(captureSession_);
        ACameraCaptureSession_close(captureSession_);
        captureSession_ = nullptr;
    }

    if (cameraDevice_) {
        ACameraDevice_close(cameraDevice_);
        cameraDevice_ = nullptr;
    }

    if (captureRequest_) {
        ACaptureRequest_free(captureRequest_);
        captureRequest_ = nullptr;
    }

    if (outputTarget_) {
        ACameraOutputTarget_free(outputTarget_);
        outputTarget_ = nullptr;
    }

    if (sessionOutput_) {
        ACaptureSessionOutput_free(sessionOutput_);
        sessionOutput_ = nullptr;
    }

    if (outputContainer_) {
        ACaptureSessionOutputContainer_free(outputContainer_);
        outputContainer_ = nullptr;
    }

    if (surface_) {
        ANativeWindow_release(surface_);
        surface_ = nullptr;
    }

    currentCameraId_.clear();
    statsCallback_ = nullptr;

    LOGI("Camera resources cleaned up");
}

CameraStats CameraStream::getStats() const {
    std::lock_guard<std::mutex> lock(statsMutex_);

    CameraStats stats;
    stats.frameRateHz = lastFrameRateHz_;
    stats.latencyMs = lastLatencyMs_;
    stats.frameCount = frameCount_.load(std::memory_order_acquire);
    stats.droppedFrames = droppedFrames_.load(std::memory_order_acquire);

    return stats;
}

void CameraStream::updateStats(int64_t timestampNs) {
    const int64_t now = getBootTimeNs();
    frameCount_.fetch_add(1, std::memory_order_relaxed);

    std::lock_guard<std::mutex> lock(statsMutex_);

    // Calculate frequency from inter-frame interval using hardware timestamps
    // Frequency = 1 / (currentTs - prevTs)
    if (prevFrameTimestampNs_ > 0 && timestampNs > prevFrameTimestampNs_) {
        double intervalSec = static_cast<double>(timestampNs - prevFrameTimestampNs_) / kNsPerSecond;
        lastFrameRateHz_ = static_cast<float>(1.0 / intervalSec);
    }
    prevFrameTimestampNs_ = timestampNs;

    // Calculate latency: time from hardware capture to callback delivery
    // Latency = now - eventTimestamp
    if (timestampNs > 0 && now > timestampNs) {
        lastLatencyMs_ = static_cast<float>(static_cast<double>(now - timestampNs) / kNsToMs);
    }

    // Periodic callback notification (~1 second)
    if (statsCallback_ && (now - lastCallbackTimeNs_ >= kNsPerSecond)) {
        CameraStats stats;
        stats.frameRateHz = lastFrameRateHz_;
        stats.latencyMs = lastLatencyMs_;
        stats.frameCount = frameCount_.load(std::memory_order_acquire);
        stats.droppedFrames = droppedFrames_.load(std::memory_order_acquire);
        statsCallback_(stats);
        lastCallbackTimeNs_ = now;
    }
}

// Static callbacks

void CameraStream::onDeviceDisconnected(void* context, ACameraDevice* /*device*/) {
    auto* self = static_cast<CameraStream*>(context);
    LOGI("Camera device disconnected");
    self->streaming_.store(false, std::memory_order_release);
}

void CameraStream::onDeviceError(void* context, ACameraDevice* /*device*/, int error) {
    auto* self = static_cast<CameraStream*>(context);
    LOGE("Camera device error: %d", error);
    self->streaming_.store(false, std::memory_order_release);
}

void CameraStream::onSessionClosed(void* /*context*/, ACameraCaptureSession* /*session*/) {
    LOGI("Capture session closed");
}

void CameraStream::onSessionReady(void* /*context*/, ACameraCaptureSession* /*session*/) {
    LOGI("Capture session ready");
}

void CameraStream::onSessionActive(void* /*context*/, ACameraCaptureSession* /*session*/) {
    LOGI("Capture session active");
}

void CameraStream::onCaptureStarted(void* context, ACameraCaptureSession* /*session*/,
                                     const ACaptureRequest* /*request*/, int64_t timestamp) {
    auto* self = static_cast<CameraStream*>(context);
    self->updateStats(timestamp);
}

void CameraStream::onCaptureCompleted(void* /*context*/, ACameraCaptureSession* /*session*/,
                                       ACaptureRequest* /*request*/, const ACameraMetadata* /*result*/) {
    // Frame completed - could extract additional metadata here if needed
}

}  // namespace nativesensor
