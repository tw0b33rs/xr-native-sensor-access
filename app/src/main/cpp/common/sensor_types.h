#pragma once

#include <cstdint>

namespace nativesensor {

/// Hardware timestamp in nanoseconds since boot (CLOCK_BOOTTIME)
using TimestampNs = int64_t;

/// Sensor type identifiers matching Android NDK
enum class SensorType : int32_t {
    Accelerometer = 1,
    Gyroscope = 4,
    AccelerometerUncalibrated [[maybe_unused]] = 35,  // Reserved for uncalibrated sensor support
    GyroscopeUncalibrated [[maybe_unused]] = 16       // Reserved for uncalibrated sensor support
};

/// Sensor metadata for enumeration
struct SensorInfo {
    int32_t handle;
    SensorType type;
    const char* name;
    const char* vendor;
    int32_t minDelayUs;
    float maxFrequencyHz;
    int32_t fifoReserved;
};

}  // namespace nativesensor

