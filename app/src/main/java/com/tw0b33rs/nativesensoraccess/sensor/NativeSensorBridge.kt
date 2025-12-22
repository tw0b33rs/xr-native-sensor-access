package com.tw0b33rs.nativesensoraccess.sensor

import com.tw0b33rs.nativesensoraccess.logging.SensorLogExtensions.logSensorDiscovery
import com.tw0b33rs.nativesensoraccess.logging.SensorLogInfo
import com.tw0b33rs.nativesensoraccess.logging.SensorLogger

/**
 * JNI bridge to native sensor layer.
 * Provides low-latency, high-frequency IMU access via NDK.
 */
object NativeSensorBridge {

    private val log = SensorLogger.jni

    init {
        try {
            System.loadLibrary("nativesensor")
            log.info("Native sensor library loaded successfully")
        } catch (e: UnsatisfiedLinkError) {
            log.error("Failed to load native sensor library", throwable = e)
        }
    }

    // Native method declarations
    private external fun nativeInit()
    private external fun nativeStop()
    private external fun nativeGetAccelData(): FloatArray
    private external fun nativeGetGyroData(): FloatArray
    private external fun nativeGetStats(): FloatArray
    private external fun nativeGetMetadata(): IntArray
    private external fun nativeEnumerateSensors(): String
    private external fun nativeSwitchSensors(accelHandle: Int, gyroHandle: Int)
    private external fun nativeIsRunning(): Boolean

    /**
     * Initialize and start IMU sensors at maximum hardware rate.
     */
    fun init() {
        SensorLogger.imu.section("IMU Initialization")
        SensorLogger.imu.info("Starting IMU sensors at maximum hardware rate")

        logDiscoveredSensors()
        nativeInit()

        // Note: Metadata will show actual values once sensor thread has initialized.
        // The actual sensor registration happens async in native layer.
        // Logging here is just for initialization confirmation - accurate stats
        // come from the polling loop.
        log.info("ImuManager started")
    }

    /**
     * Stop IMU sensors and release resources.
     */
    fun stop() {
        SensorLogger.imu.info("Stopping IMU sensors")
        nativeStop()
        SensorLogger.imu.info("IMU sensors stopped")
    }

    /**
     * Check if sensors are currently running.
     */
    fun isRunning(): Boolean = nativeIsRunning()

    /**
     * Get latest accelerometer sample.
     * @return ImuSample with x, y, z values in m/s² and timestamp
     */
    fun getAccelData(): ImuSample {
        val data = nativeGetAccelData()
        return ImuSample(
            x = data.getOrElse(0) { 0f },
            y = data.getOrElse(1) { 0f },
            z = data.getOrElse(2) { 0f },
            timestampMs = data.getOrElse(3) { 0f }
        )
    }

    /**
     * Get latest gyroscope sample.
     * @return ImuSample with x, y, z values in rad/s and timestamp
     */
    fun getGyroData(): ImuSample {
        val data = nativeGetGyroData()
        return ImuSample(
            x = data.getOrElse(0) { 0f },
            y = data.getOrElse(1) { 0f },
            z = data.getOrElse(2) { 0f },
            timestampMs = data.getOrElse(3) { 0f }
        )
    }

    /**
     * Get IMU statistics (resets measurement window).
     * @return ImuStats with frequency and latency measurements
     */
    fun getStats(): ImuStats {
        val data = nativeGetStats()
        return ImuStats(
            accelFrequencyHz = data.getOrElse(0) { 0f },
            accelLatencyMs = data.getOrElse(1) { 0f },
            gyroFrequencyHz = data.getOrElse(2) { 0f },
            gyroLatencyMs = data.getOrElse(3) { 0f }
        )
    }

    /**
     * Get current sensor metadata.
     */
    fun getMetadata(): ImuMetadata {
        val data = nativeGetMetadata()
        return ImuMetadata(
            accelMinDelayUs = data.getOrElse(0) { 0 },
            accelFifoReserved = data.getOrElse(1) { 0 },
            gyroMinDelayUs = data.getOrElse(2) { 0 },
            gyroFifoReserved = data.getOrElse(3) { 0 }
        )
    }

    /**
     * Enumerate all available IMU sensors.
     * @return List of available accelerometers and gyroscopes
     */
    fun enumerateSensors(): List<SensorInfo> {
        val rawData = nativeEnumerateSensors()
        if (rawData.isEmpty()) {
            log.warn("No sensor data returned from native layer")
            return emptyList()
        }

        return rawData.trim().split("\n").mapNotNull { line ->
            val parts = line.split("|")
            if (parts.size == 7) {
                try {
                    SensorInfo(
                        handle = parts[0].toInt(),
                        type = parts[1].toInt(),
                        name = parts[2],
                        vendor = parts[3],
                        minDelayUs = parts[4].toInt(),
                        maxFrequencyHz = parts[5].toFloat(),
                        fifoReserved = parts[6].toInt()
                    )
                } catch (e: Exception) {
                    log.warn("Failed to parse sensor info: $line", throwable = e)
                    null
                }
            } else {
                log.debug("Skipping malformed sensor line", mapOf("line" to line, "parts" to parts.size))
                null
            }
        }
    }

    /**
     * Switch to specific sensors by handle.
     * @param accelHandle Accelerometer handle from enumeration (-1 for default)
     * @param gyroHandle Gyroscope handle from enumeration (-1 for default)
     */
    fun switchSensors(accelHandle: Int, gyroHandle: Int) {
        SensorLogger.imu.info("Switching sensors", mapOf(
            "accelHandle" to accelHandle,
            "gyroHandle" to gyroHandle
        ))
        nativeSwitchSensors(accelHandle, gyroHandle)
    }

    /**
     * Get accelerometers from enumerated sensors.
     */
    fun getAccelerometers(): List<SensorInfo> =
        enumerateSensors().filter { it.isAccelerometer }

    /**
     * Get gyroscopes from enumerated sensors.
     */
    fun getGyroscopes(): List<SensorInfo> =
        enumerateSensors().filter { it.isGyroscope }

    /**
     * Log all discovered IMU sensors for debugging.
     */
    private fun logDiscoveredSensors() {
        val allSensors = enumerateSensors()

        // Log accelerometers
        val accelerometers = allSensors.filter { it.isAccelerometer }
        SensorLogger.imu.logSensorDiscovery(
            sensorType = "Accelerometer",
            sensors = accelerometers.map { it.toLogInfo() }
        )

        // Log gyroscopes
        val gyroscopes = allSensors.filter { it.isGyroscope }
        SensorLogger.imu.logSensorDiscovery(
            sensorType = "Gyroscope",
            sensors = gyroscopes.map { it.toLogInfo() }
        )
    }

    /**
     * Convert SensorInfo to SensorLogInfo for logging
     */
    private fun SensorInfo.toLogInfo() = SensorLogInfo(
        handle = handle,
        name = name,
        vendor = vendor,
        type = when (type) {
            SensorInfo.SENSOR_TYPE_ACCELEROMETER -> "Accelerometer"
            SensorInfo.SENSOR_TYPE_GYROSCOPE -> "Gyroscope"
            SensorInfo.SENSOR_TYPE_ACCELEROMETER_UNCALIBRATED -> "Accelerometer (Uncalibrated)"
            SensorInfo.SENSOR_TYPE_GYROSCOPE_UNCALIBRATED -> "Gyroscope (Uncalibrated)"
            else -> "Unknown ($type)"
        },
        minDelayUs = minDelayUs,
        maxFrequencyHz = maxFrequencyHz,
        fifoReserved = fifoReserved,
        isUncalibrated = isUncalibrated
    )
}

