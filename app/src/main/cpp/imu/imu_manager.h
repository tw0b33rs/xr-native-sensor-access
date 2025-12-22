#pragma once

#include <android/sensor.h>
#include <android/looper.h>
#include <functional>
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include <vector>
#include <string>

#include "imu_data.h"
#include "ring_buffer.h"
#include "sensor_types.h"

namespace nativesensor {

/// Callback type for IMU data - called from sensor thread
using ImuCallback = std::function<void(const ImuSample&)>;

/// High-frequency, low-latency IMU sensor manager.
/// Uses ASensorManager with callback-based event queue.
class ImuManager {
public:
    ImuManager();
    ~ImuManager();

    ImuManager(const ImuManager&) = delete;
    ImuManager& operator=(const ImuManager&) = delete;

    /// Start IMU subscription at maximum hardware rate
    void start(ImuCallback callback);

    /// Stop IMU subscription and release resources
    void stop();

    /// Switch to specific sensors by handle
    void switchSensors(int32_t accelHandle, int32_t gyroHandle);

    /// Check if sensors are running
    [[nodiscard]]
    bool isRunning() const noexcept { return running_.load(std::memory_order_acquire); }

    /// Get the latest accelerometer sample (thread-safe)
    [[nodiscard]]
    ImuSample getLatestAccel() const;

    /// Get the latest gyroscope sample (thread-safe)
    [[nodiscard]]
    ImuSample getLatestGyro() const;

    /// Get sensor statistics (resets counters)
    ImuStats getStats();

    /// Get current sensor metadata
    [[nodiscard]]
    ImuSensorMetadata getMetadata() const;

    /// Enumerate all available IMU sensors
    std::vector<SensorInfo> enumerateSensors();

private:
    void sensorThreadLoop();
    void drainEvents();
    static int64_t getBootTimeNs() noexcept;

    std::atomic<bool> running_{false};
    std::thread sensorThread_;
    ImuCallback callback_;

    std::atomic<int32_t> targetAccelHandle_{-1};
    std::atomic<int32_t> targetGyroHandle_{-1};
    std::atomic<bool> needsSensorSwitch_{false};

    ASensorManager* sensorManager_ = nullptr;
    ALooper* looper_ = nullptr;
    ASensorEventQueue* eventQueue_ = nullptr;
    const ASensor* currentAccel_ = nullptr;
    const ASensor* currentGyro_ = nullptr;

    mutable std::mutex dataMutex_;
    ImuSample latestAccel_{};
    ImuSample latestGyro_{};

    mutable std::mutex statsMutex_;
    int64_t statsWindowStart_ = 0;
    int32_t accelCount_ = 0;
    int32_t gyroCount_ = 0;
    int64_t accelLatencyTotal_ = 0;
    int64_t gyroLatencyTotal_ = 0;

    std::atomic<int32_t> accelMinDelay_{0};
    std::atomic<int32_t> accelFifo_{0};
    std::atomic<int32_t> gyroMinDelay_{0};
    std::atomic<int32_t> gyroFifo_{0};

    static constexpr const char* kPackageName = "com.tw0b33rs.nativesensoraccess";
};

}  // namespace nativesensor

