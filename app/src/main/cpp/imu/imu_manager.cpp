#include "imu_manager.h"

#include <android/log.h>
#include <ctime>
#include <sstream>

namespace {
constexpr const char* kLogTag = "NativeSensor.IMU";
}

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, kLogTag, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, kLogTag, __VA_ARGS__)

#ifndef ASENSOR_TYPE_ACCELEROMETER_UNCALIBRATED
#define ASENSOR_TYPE_ACCELEROMETER_UNCALIBRATED 35
#endif
#ifndef ASENSOR_TYPE_GYROSCOPE_UNCALIBRATED
#define ASENSOR_TYPE_GYROSCOPE_UNCALIBRATED 16
#endif

namespace nativesensor {

namespace {

constexpr int kLooperId = 1;
constexpr int kPollTimeoutMs = 10;
constexpr int64_t kNsPerSecond = 1'000'000'000LL;
constexpr double kNsToMs = 1'000'000.0;
constexpr int kMicrosPerSecond = 1'000'000;

}  // namespace

ImuManager::ImuManager() {
    sensorManager_ = ASensorManager_getInstanceForPackage(kPackageName);
    if (!sensorManager_) {
        LOGE("Failed to get ASensorManager instance");
    }
}

ImuManager::~ImuManager() {
    stop();
}

int64_t ImuManager::getBootTimeNs() noexcept {
    struct timespec t{};
    clock_gettime(CLOCK_BOOTTIME, &t);
    return static_cast<int64_t>(t.tv_sec) * kNsPerSecond + t.tv_nsec;
}

void ImuManager::start(ImuCallback callback) {
    if (running_.load(std::memory_order_acquire)) {
        LOGI("ImuManager already running");
        return;
    }

    if (!sensorManager_) {
        LOGE("Cannot start: no sensor manager");
        return;
    }

    callback_ = std::move(callback);
    running_.store(true, std::memory_order_release);

    // Reset stats
    {
        std::lock_guard<std::mutex> lock(statsMutex_);
        statsWindowStart_ = getBootTimeNs();
        accelCount_ = 0;
        gyroCount_ = 0;
        accelLatencyTotal_ = 0;
        gyroLatencyTotal_ = 0;
    }

    sensorThread_ = std::thread(&ImuManager::sensorThreadLoop, this);
    LOGI("ImuManager started");
}

void ImuManager::stop() {
    if (!running_.load(std::memory_order_acquire)) {
        return;
    }

    running_.store(false, std::memory_order_release);

    // Wake up the looper to exit
    if (looper_) {
        ALooper_wake(looper_);
    }

    if (sensorThread_.joinable()) {
        sensorThread_.join();
    }

    LOGI("ImuManager stopped");
}

void ImuManager::switchSensors(int32_t accelHandle, int32_t gyroHandle) {
    LOGI("Switching sensors - Accel: %d, Gyro: %d", accelHandle, gyroHandle);

    targetAccelHandle_.store(accelHandle, std::memory_order_release);
    targetGyroHandle_.store(gyroHandle, std::memory_order_release);
    needsSensorSwitch_.store(true, std::memory_order_release);

    // If running, restart to apply new sensors
    if (running_.load(std::memory_order_acquire)) {
        auto cb = callback_;
        stop();
        start(cb);
    }
}

void ImuManager::sensorThreadLoop() {
    // Create looper for this thread
    looper_ = ALooper_prepare(ALOOPER_PREPARE_ALLOW_NON_CALLBACKS);
    if (!looper_) {
        LOGE("Failed to prepare ALooper");
        return;
    }

    // Create event queue - poll directly without callback
    eventQueue_ = ASensorManager_createEventQueue(
        sensorManager_,
        looper_,
        kLooperId,
        nullptr,
        nullptr
    );

    if (!eventQueue_) {
        LOGE("Failed to create sensor event queue");
        return;
    }

    // Get sensor list
    ASensorList sensorList;
    int sensorCount = ASensorManager_getSensorList(sensorManager_, &sensorList);

    // Select accelerometer
    int32_t accelHandle = targetAccelHandle_.load(std::memory_order_acquire);
    if (accelHandle >= 0 && accelHandle < sensorCount) {
        currentAccel_ = sensorList[accelHandle];
    } else {
        currentAccel_ = ASensorManager_getDefaultSensor(sensorManager_, ASENSOR_TYPE_ACCELEROMETER);
    }

    // Select gyroscope
    int32_t gyroHandle = targetGyroHandle_.load(std::memory_order_acquire);
    if (gyroHandle >= 0 && gyroHandle < sensorCount) {
        currentGyro_ = sensorList[gyroHandle];
    } else {
        currentGyro_ = ASensorManager_getDefaultSensor(sensorManager_, ASENSOR_TYPE_GYROSCOPE);
    }

    // Register sensors at MAXIMUM rate using minDelay for fastest hardware rate
    if (currentAccel_) {
        int minDelay = ASensor_getMinDelay(currentAccel_);
        accelMinDelay_.store(minDelay, std::memory_order_release);
        accelFifo_.store(ASensor_getFifoReservedEventCount(currentAccel_), std::memory_order_release);

        // Use registerSensor with minDelay for maximum hardware rate
        // maxBatchReportLatencyUs = 0 means no batching, deliver immediately
        ASensorEventQueue_registerSensor(eventQueue_, currentAccel_, minDelay, 0);

        LOGI("Registered accelerometer: %s (minDelay=%dμs, fifo=%d)",
             ASensor_getName(currentAccel_),
             accelMinDelay_.load(),
             accelFifo_.load());
    } else {
        LOGE("No accelerometer found");
        accelMinDelay_.store(0, std::memory_order_release);
        accelFifo_.store(0, std::memory_order_release);
    }

    if (currentGyro_) {
        int minDelay = ASensor_getMinDelay(currentGyro_);
        gyroMinDelay_.store(minDelay, std::memory_order_release);
        gyroFifo_.store(ASensor_getFifoReservedEventCount(currentGyro_), std::memory_order_release);

        // Use registerSensor with minDelay for maximum hardware rate
        ASensorEventQueue_registerSensor(eventQueue_, currentGyro_, minDelay, 0);

        LOGI("Registered gyroscope: %s (minDelay=%dμs, fifo=%d)",
             ASensor_getName(currentGyro_),
             gyroMinDelay_.load(),
             gyroFifo_.load());
    } else {
        LOGE("No gyroscope found");
        gyroMinDelay_.store(0, std::memory_order_release);
        gyroFifo_.store(0, std::memory_order_release);
    }

    // Main event loop
    while (running_.load(std::memory_order_acquire)) {
        int ident = ALooper_pollOnce(kPollTimeoutMs, nullptr, nullptr, nullptr);
        if (ident == kLooperId) {
            drainEvents();
        }
    }

    // Cleanup
    if (currentAccel_) {
        ASensorEventQueue_disableSensor(eventQueue_, currentAccel_);
    }
    if (currentGyro_) {
        ASensorEventQueue_disableSensor(eventQueue_, currentGyro_);
    }

    ASensorManager_destroyEventQueue(sensorManager_, eventQueue_);
    eventQueue_ = nullptr;
    looper_ = nullptr;
    currentAccel_ = nullptr;
    currentGyro_ = nullptr;

    LOGI("Sensor thread exited");
}

void ImuManager::drainEvents() {
    ASensorEvent event;
    const int64_t now = getBootTimeNs();

    // Process ALL pending events in the queue
    while (ASensorEventQueue_getEvents(eventQueue_, &event, 1) > 0) {
        ImuSample sample{};
        sample.timestampNs = event.timestamp;

        bool isAccel = currentAccel_ && event.type == ASensor_getType(currentAccel_);
        bool isGyro = currentGyro_ && event.type == ASensor_getType(currentGyro_);

        if (isAccel) {
            sample.x = event.acceleration.x;
            sample.y = event.acceleration.y;
            sample.z = event.acceleration.z;
            sample.sensorType = SensorType::Accelerometer;

            // Update latest value
            {
                std::lock_guard<std::mutex> lock(dataMutex_);
                latestAccel_ = sample;
            }

            // Update stats
            {
                std::lock_guard<std::mutex> lock(statsMutex_);
                accelCount_++;
                accelLatencyTotal_ += (now - event.timestamp);
            }

        } else if (isGyro) {
            sample.x = event.vector.x;
            sample.y = event.vector.y;
            sample.z = event.vector.z;
            sample.sensorType = SensorType::Gyroscope;

            // Update latest value
            {
                std::lock_guard<std::mutex> lock(dataMutex_);
                latestGyro_ = sample;
            }

            // Update stats
            {
                std::lock_guard<std::mutex> lock(statsMutex_);
                gyroCount_++;
                gyroLatencyTotal_ += (now - event.timestamp);
            }
        }

        // Invoke callback for every sample
        if (callback_ && (isAccel || isGyro)) {
            callback_(sample);
        }
    }
}

ImuSample ImuManager::getLatestAccel() const {
    std::lock_guard<std::mutex> lock(dataMutex_);
    return latestAccel_;
}

ImuSample ImuManager::getLatestGyro() const {
    std::lock_guard<std::mutex> lock(dataMutex_);
    return latestGyro_;
}

ImuStats ImuManager::getStats() {
    std::lock_guard<std::mutex> lock(statsMutex_);

    const int64_t now = getBootTimeNs();
    const double dtSeconds = static_cast<double>(now - statsWindowStart_) / kNsPerSecond;

    ImuStats stats{};

    if (dtSeconds > 0.0) {
        stats.accelFrequencyHz = static_cast<float>(accelCount_ / dtSeconds);
        stats.gyroFrequencyHz = static_cast<float>(gyroCount_ / dtSeconds);
    }

    if (accelCount_ > 0) {
        stats.accelLatencyMs = static_cast<float>(
            static_cast<double>(accelLatencyTotal_) / accelCount_ / kNsToMs);
    }

    if (gyroCount_ > 0) {
        stats.gyroLatencyMs = static_cast<float>(
            static_cast<double>(gyroLatencyTotal_) / gyroCount_ / kNsToMs);
    }

    // Reset counters
    statsWindowStart_ = now;
    accelCount_ = 0;
    gyroCount_ = 0;
    accelLatencyTotal_ = 0;
    gyroLatencyTotal_ = 0;

    return stats;
}

ImuSensorMetadata ImuManager::getMetadata() const {
    ImuSensorMetadata meta{};
    meta.accelMinDelayUs = accelMinDelay_.load(std::memory_order_acquire);
    meta.accelFifoReserved = accelFifo_.load(std::memory_order_acquire);
    meta.gyroMinDelayUs = gyroMinDelay_.load(std::memory_order_acquire);
    meta.gyroFifoReserved = gyroFifo_.load(std::memory_order_acquire);
    meta.accelName = currentAccel_ ? ASensor_getName(currentAccel_) : "None";
    meta.gyroName = currentGyro_ ? ASensor_getName(currentGyro_) : "None";
    return meta;
}

std::vector<SensorInfo> ImuManager::enumerateSensors() {
    std::vector<SensorInfo> sensors;

    if (!sensorManager_) {
        return sensors;
    }

    ASensorList list;
    int count = ASensorManager_getSensorList(sensorManager_, &list);

    for (int i = 0; i < count; ++i) {
        const ASensor* sensor = list[i];
        int type = ASensor_getType(sensor);

        // Filter for IMU sensors only
        if (type == ASENSOR_TYPE_ACCELEROMETER ||
            type == ASENSOR_TYPE_GYROSCOPE ||
            type == ASENSOR_TYPE_ACCELEROMETER_UNCALIBRATED ||
            type == ASENSOR_TYPE_GYROSCOPE_UNCALIBRATED) {

            SensorInfo info{};
            info.handle = i;
            info.type = static_cast<SensorType>(type);
            info.name = ASensor_getName(sensor);
            info.vendor = ASensor_getVendor(sensor);
            info.minDelayUs = ASensor_getMinDelay(sensor);
            info.maxFrequencyHz = (info.minDelayUs > 0)
                ? (static_cast<float>(kMicrosPerSecond) / static_cast<float>(info.minDelayUs))
                : 0.0f;
            info.fifoReserved = ASensor_getFifoReservedEventCount(sensor);

            sensors.push_back(info);
        }
    }

    LOGI("Enumerated %zu IMU sensors", sensors.size());
    return sensors;
}

}  // namespace nativesensor

