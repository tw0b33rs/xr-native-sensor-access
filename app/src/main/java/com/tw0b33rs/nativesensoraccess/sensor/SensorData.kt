package com.tw0b33rs.nativesensoraccess.sensor

/**
 * Sensor metadata for UI display and selection.
 */
data class SensorInfo(
    val handle: Int,
    val type: Int,
    val name: String,
    val vendor: String,
    val minDelayUs: Int,
    val maxFrequencyHz: Float,
    val fifoReserved: Int
) {
    val isAccelerometer: Boolean
        get() = type == SENSOR_TYPE_ACCELEROMETER || type == SENSOR_TYPE_ACCELEROMETER_UNCALIBRATED

    val isGyroscope: Boolean
        get() = type == SENSOR_TYPE_GYROSCOPE || type == SENSOR_TYPE_GYROSCOPE_UNCALIBRATED

    val isUncalibrated: Boolean
        get() = type == SENSOR_TYPE_ACCELEROMETER_UNCALIBRATED || type == SENSOR_TYPE_GYROSCOPE_UNCALIBRATED

    companion object {
        const val SENSOR_TYPE_ACCELEROMETER = 1
        const val SENSOR_TYPE_GYROSCOPE = 4
        const val SENSOR_TYPE_ACCELEROMETER_UNCALIBRATED = 35
        const val SENSOR_TYPE_GYROSCOPE_UNCALIBRATED = 16
    }
}

/**
 * IMU sample data with timestamp.
 */
data class ImuSample(
    val x: Float,
    val y: Float,
    val z: Float,
    val timestampMs: Float
)

/**
 * IMU performance statistics.
 */
data class ImuStats(
    val accelFrequencyHz: Float,
    val accelLatencyMs: Float,
    val gyroFrequencyHz: Float,
    val gyroLatencyMs: Float
)

/**
 * Current sensor metadata.
 */
data class ImuMetadata(
    val accelMinDelayUs: Int,
    val accelFifoReserved: Int,
    val gyroMinDelayUs: Int,
    val gyroFifoReserved: Int
)

