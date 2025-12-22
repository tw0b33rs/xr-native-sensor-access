#pragma once

#include "sensor_types.h"

namespace nativesensor {

/// Raw IMU data sample with hardware timestamp
struct ImuSample {
    float x;
    float y;
    float z;
    TimestampNs timestampNs;
    [[maybe_unused]] SensorType sensorType;  // Reserved for multi-sensor disambiguation
};

/// IMU statistics for performance monitoring
struct ImuStats {
    float accelFrequencyHz;
    float accelLatencyMs;
    float gyroFrequencyHz;
    float gyroLatencyMs;
};

/// Current IMU sensor metadata
struct ImuSensorMetadata {
    int32_t accelMinDelayUs;
    int32_t accelFifoReserved;
    int32_t gyroMinDelayUs;
    int32_t gyroFifoReserved;
    [[maybe_unused]] const char* accelName;  // Reserved for debugging/logging
    [[maybe_unused]] const char* gyroName;   // Reserved for debugging/logging
};

}  // namespace nativesensor

