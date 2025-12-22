package com.tw0b33rs.nativesensoraccess.logging

import android.util.Log

/**
 * Centralized logging utility for sensor-related operations.
 * Provides consistent formatting and log levels across all sensor types.
 *
 * Usage:
 * ```
 * SensorLogger.imu.info("Accelerometer started")
 * SensorLogger.camera.debug("Frame captured", mapOf("width" to 1920, "height" to 1080))
 * SensorLogger.imu.logSensorDiscovery(sensorList)
 * ```
 */
object SensorLogger {

    /** Logger for IMU sensors (accelerometer, gyroscope) */
    val imu = Logger("NativeSensor.IMU")

    /** Logger for camera sensors */
    @Suppress("unused")
    val camera = Logger("NativeSensor.Camera")

    /** Logger for general sensor operations */
    val general = Logger("NativeSensor")

    /** Logger for JNI bridge operations */
    val jni = Logger("NativeSensor.JNI")

    /** Logger for performance metrics */
    val perf = Logger("NativeSensor.Perf")

    /**
     * Individual logger instance with consistent formatting
     */
    class Logger(private val tag: String) {

        //noinspection unused - Part of public logging API for verbose output
        fun verbose(message: String, details: Map<String, Any?>? = null) {
            Log.v(tag, formatMessage(message, details))
        }

        fun debug(message: String, details: Map<String, Any?>? = null) {
            Log.d(tag, formatMessage(message, details))
        }

        fun info(message: String, details: Map<String, Any?>? = null) {
            Log.i(tag, formatMessage(message, details))
        }

        fun warn(message: String, details: Map<String, Any?>? = null, throwable: Throwable? = null) {
            if (throwable != null) {
                Log.w(tag, formatMessage(message, details), throwable)
            } else {
                Log.w(tag, formatMessage(message, details))
            }
        }

        fun error(message: String, details: Map<String, Any?>? = null, throwable: Throwable? = null) {
            if (throwable != null) {
                Log.e(tag, formatMessage(message, details), throwable)
            } else {
                Log.e(tag, formatMessage(message, details))
            }
        }

        /**
         * Log a section header for better readability in logcat
         */
        fun section(title: String) {
            Log.i(tag, "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━")
            Log.i(tag, "  $title")
            Log.i(tag, "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━")
        }

        /**
         * Log a table of key-value pairs
         */
        fun table(title: String, rows: List<Pair<String, Any?>>) {
            Log.i(tag, "┌─ $title")
            rows.forEach { (key, value) ->
                Log.i(tag, "│  $key: $value")
            }
            Log.i(tag, "└─────────────────────────────────────────────────")
        }

        private fun formatMessage(message: String, details: Map<String, Any?>?): String {
            return if (details.isNullOrEmpty()) {
                message
            } else {
                val detailStr = details.entries.joinToString(", ") { "${it.key}=${it.value}" }
                "$message [$detailStr]"
            }
        }
    }
}

/**
 * Extension functions for logging sensor-specific data
 */
object SensorLogExtensions {

    /**
     * Log discovered IMU sensors in a formatted table
     */
    fun SensorLogger.Logger.logSensorDiscovery(
        sensorType: String,
        sensors: List<SensorLogInfo>
    ) {
        if (sensors.isEmpty()) {
            warn("No $sensorType sensors found")
            return
        }

        section("$sensorType Sensors Discovered: ${sensors.size}")

        sensors.forEachIndexed { index, sensor ->
            table("Sensor #${index + 1}", listOf(
                "Handle" to sensor.handle,
                "Name" to sensor.name,
                "Vendor" to sensor.vendor,
                "Type" to sensor.type,
                "Min Delay" to "${sensor.minDelayUs} μs",
                "Max Frequency" to "${"%.1f".format(sensor.maxFrequencyHz)} Hz",
                "FIFO Reserved" to "${sensor.fifoReserved} events",
                "Uncalibrated" to sensor.isUncalibrated
            ))
        }
    }

    /**
     * Log sensor activation
     */
    @Suppress("unused")  // Part of public logging API
    fun SensorLogger.Logger.logSensorActivated(
        sensorType: String,
        name: String,
        requestedRateHz: Float,
        minDelayUs: Int
    ) {
        info("$sensorType activated", mapOf(
            "name" to name,
            "requestedRate" to "${requestedRateHz.toInt()} Hz",
            "minDelay" to "$minDelayUs μs",
            "maxPossibleRate" to "${if (minDelayUs > 0) 1_000_000 / minDelayUs else 0} Hz"
        ))
    }

    /**
     * Log performance statistics
     */
    fun SensorLogger.Logger.logPerformanceStats(
        sensorType: String,
        frequencyHz: Float,
        latencyMs: Float
    ) {
        debug("$sensorType performance", mapOf(
            "frequency" to "${"%.1f".format(frequencyHz)} Hz",
            "latency" to "${"%.2f".format(latencyMs)} ms"
        ))
    }
}

/**
 * Data class for sensor information logging
 */
data class SensorLogInfo(
    val handle: Int,
    val name: String,
    val vendor: String,
    val type: String,
    val minDelayUs: Int,
    val maxFrequencyHz: Float,
    val fifoReserved: Int,
    val isUncalibrated: Boolean = false
)

