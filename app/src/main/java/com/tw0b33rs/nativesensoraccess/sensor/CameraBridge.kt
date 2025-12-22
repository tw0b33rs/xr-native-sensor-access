package com.tw0b33rs.nativesensoraccess.sensor

import android.view.Surface
import com.tw0b33rs.nativesensoraccess.logging.SensorLogger

/**
 * Camera cluster types matching C++ CameraClusterType enum.
 */
enum class CameraClusterType(val value: Int) {
    UNKNOWN(0),
    PASSTHROUGH(1),
    AVATAR(2),
    EYE_TRACKING(3),
    DEPTH(4);

    companion object {
        fun fromValue(value: Int): CameraClusterType =
            entries.find { it.value == value } ?: UNKNOWN
    }
}

/**
 * Camera facing direction matching C++ CameraFacing enum.
 */
enum class CameraFacing(val value: Int) {
    UNKNOWN(-1),
    FRONT(0),
    BACK(1),
    EXTERNAL(2);

    companion object {
        fun fromValue(value: Int): CameraFacing =
            entries.find { it.value == value } ?: UNKNOWN
    }
}

/**
 * Camera metadata for UI display and selection.
 */
data class CameraInfo(
    val id: String,
    val facing: CameraFacing,
    val clusterType: CameraClusterType,
    val width: Int,
    val height: Int,
    val maxFps: Int,
    val isPhysicalCamera: Boolean,
    val physicalCameraIds: String
) {
    val resolution: String
        get() = "${width}x${height}"

    val displayName: String
        get() = buildString {
            append("Camera $id")
            if (width > 0 && height > 0) {
                append(" ($resolution)")
            }
        }

    @get:Suppress("unused")  // Part of public API
    val shortName: String
        get() = "Cam $id"
}

/**
 * Camera streaming statistics.
 */
data class CameraStats(
    val frameRateHz: Float,
    val latencyMs: Float,
    val frameCount: Long,
    val droppedFrames: Long
)

/**
 * JNI bridge to native camera layer.
 * Provides zero-copy camera preview via ANativeWindow/Surface.
 */
object CameraBridge {

    private val log = SensorLogger.camera

    init {
        try {
            System.loadLibrary("nativesensor")
            log.info("Camera native library ready")
        } catch (e: UnsatisfiedLinkError) {
            log.error("Failed to load native library for camera", throwable = e)
        }
    }

    // Native method declarations
    private external fun nativeEnumerateCameras(): String
    private external fun nativeStartPreview(cameraId: String, surface: Surface): Boolean
    private external fun nativeStopPreview()
    private external fun nativeStopCameraPreview(cameraId: String)
    private external fun nativeGetCameraStats(): FloatArray
    private external fun nativeGetCameraStatsById(cameraId: String): FloatArray
    private external fun nativeIsStreaming(): Boolean
    private external fun nativeIsCameraStreaming(cameraId: String): Boolean
    private external fun nativeGetCurrentCameraId(): String
    private external fun nativeGetActiveStreamCount(): Int

    /**
     * Enumerate all available cameras with metadata.
     * @return List of CameraInfo for all detected cameras
     */
    fun enumerateCameras(): List<CameraInfo> {
        val rawData = nativeEnumerateCameras()
        if (rawData.isEmpty()) {
            log.warn("No cameras returned from native layer")
            return emptyList()
        }

        return rawData.trim().split("\n").mapNotNull { line ->
            parseCameraInfo(line)
        }.also { cameras ->
            log.info("Enumerated ${cameras.size} cameras", mapOf(
                "passthrough" to cameras.count { it.clusterType == CameraClusterType.PASSTHROUGH },
                "avatar" to cameras.count { it.clusterType == CameraClusterType.AVATAR },
                "eyeTracking" to cameras.count { it.clusterType == CameraClusterType.EYE_TRACKING },
                "depth" to cameras.count { it.clusterType == CameraClusterType.DEPTH },
                "unknown" to cameras.count { it.clusterType == CameraClusterType.UNKNOWN }
            ))
        }
    }

    /**
     * Start camera preview with zero-copy surface rendering.
     * @param cameraId Camera ID from enumeration
     * @param surface Surface from SurfaceView/SpatialExternalSurface
     * @return true if preview started successfully
     */
    fun startPreview(cameraId: String, surface: Surface): Boolean {
        log.info("Starting camera preview", mapOf("cameraId" to cameraId))
        return nativeStartPreview(cameraId, surface).also { success ->
            if (success) {
                log.info("Camera preview started: $cameraId")
            } else {
                log.error("Failed to start camera preview: $cameraId")
            }
        }
    }

    /**
     * Stop all camera previews and release resources.
     */
    fun stopPreview() {
        log.info("Stopping all camera previews")
        nativeStopPreview()
    }

    /**
     * Stop a specific camera preview and release its resources.
     * @param cameraId Camera ID to stop
     */
    fun stopPreview(cameraId: String) {
        log.info("Stopping camera preview", mapOf("cameraId" to cameraId))
        nativeStopCameraPreview(cameraId)
    }

    /**
     * Check if any camera is currently streaming.
     */
    fun isStreaming(): Boolean = nativeIsStreaming()

    /**
     * Check if a specific camera is currently streaming.
     * @param cameraId Camera ID to check
     */
    fun isStreaming(cameraId: String): Boolean = nativeIsCameraStreaming(cameraId)

    /**
     * Get comma-separated list of currently streaming camera IDs.
     */
    @Suppress("unused")  // Part of public API
    fun getCurrentCameraId(): String = nativeGetCurrentCameraId()

    /**
     * Get the number of active camera streams.
     */
    @Suppress("unused")  // Part of public API
    fun getActiveStreamCount(): Int = nativeGetActiveStreamCount()

    /**
     * Get combined camera streaming statistics (all streams).
     */
    @Suppress("unused")  // Part of public API
    fun getStats(): CameraStats {
        val data = nativeGetCameraStats()
        return CameraStats(
            frameRateHz = data.getOrElse(0) { 0f },
            latencyMs = data.getOrElse(1) { 0f },
            frameCount = data.getOrElse(2) { 0f }.toLong(),
            droppedFrames = data.getOrElse(3) { 0f }.toLong()
        )
    }

    /**
     * Get streaming statistics for a specific camera.
     * @param cameraId Camera ID to get stats for
     */
    @Suppress("unused")  // Part of public API
    fun getStats(cameraId: String): CameraStats {
        val data = nativeGetCameraStatsById(cameraId)
        return CameraStats(
            frameRateHz = data.getOrElse(0) { 0f },
            latencyMs = data.getOrElse(1) { 0f },
            frameCount = data.getOrElse(2) { 0f }.toLong(),
            droppedFrames = data.getOrElse(3) { 0f }.toLong()
        )
    }

    // Extension functions for cluster grouping

    /**
     * Get cameras grouped by cluster type.
     */
    @Suppress("unused")  // Part of public API
    fun getCamerasByCluster(): Map<CameraClusterType, List<CameraInfo>> =
        enumerateCameras().groupBy { it.clusterType }

    /**
     * Get passthrough cameras (high-res RGB for video passthrough).
     */
    @Suppress("unused")  // Part of public API
    fun getPassthroughCameras(): List<CameraInfo> =
        enumerateCameras().filter { it.clusterType == CameraClusterType.PASSTHROUGH }

    /**
     * Get tracking cameras (low-res for SLAM/positional tracking).
     */
    @Suppress("unused")  // Part of public API
    fun getAvatarCameras(): List<CameraInfo> =
        enumerateCameras().filter { it.clusterType == CameraClusterType.AVATAR }

    /**
     * Get eye tracking cameras (IR for gaze tracking).
     */
    @Suppress("unused")  // Part of public API
    fun getEyeTrackingCameras(): List<CameraInfo> =
        enumerateCameras().filter { it.clusterType == CameraClusterType.EYE_TRACKING }

    /**
     * Get depth sensor cameras.
     */
    @Suppress("unused")  // Part of public API
    fun getDepthCameras(): List<CameraInfo> =
        enumerateCameras().filter { it.clusterType == CameraClusterType.DEPTH }

    private fun parseCameraInfo(line: String): CameraInfo? {
        // Format: id|facing|clusterType|width|height|maxFps|isPhysical|physicalIds
        val parts = line.split("|")
        if (parts.size < 7) {
            log.debug("Skipping malformed camera line", mapOf("line" to line, "parts" to parts.size))
            return null
        }

        return try {
            CameraInfo(
                id = parts[0],
                facing = CameraFacing.fromValue(parts[1].toInt()),
                clusterType = CameraClusterType.fromValue(parts[2].toInt()),
                width = parts[3].toInt(),
                height = parts[4].toInt(),
                maxFps = parts[5].toInt(),
                isPhysicalCamera = parts[6] == "1",
                physicalCameraIds = parts.getOrElse(7) { "" }
            )
        } catch (e: Exception) {
            log.warn("Failed to parse camera info: $line", throwable = e)
            null
        }
    }
}
