#pragma once

#include <cstdint>
#include <string>

namespace nativesensor {

/// Camera cluster types for XR headset sensors
enum class CameraClusterType : int32_t {
    Unknown = 0,
    Passthrough = 1,    // High-res RGB cameras for video passthrough
    Avatar = 2,       // Low-res cameras for SLAM/positional tracking
    EyeTracking = 3,    // IR cameras for eye/gaze tracking
    Depth = 4           // ToF or structured light depth sensor
};

/// Lens facing direction
enum class CameraFacing : int32_t {
    Unknown = -1,
    Front = 0,
    Back = 1,
    External = 2
};

/// Camera metadata for enumeration and display
struct CameraInfo {
    std::string id;
    CameraFacing facing = CameraFacing::Unknown;
    CameraClusterType clusterType = CameraClusterType::Unknown;
    int32_t width = 0;
    int32_t height = 0;
    int32_t maxFps = 0;
    bool isPhysicalCamera = false;
    std::string physicalCameraIds;  // Comma-separated for logical cameras
};

/// Camera frame statistics
struct CameraStats {
    float frameRateHz = 0.0f;
    float latencyMs = 0.0f;
    int64_t frameCount = 0;
    int64_t droppedFrames = 0;
};

/// Frame metadata passed with each captured frame
struct [[maybe_unused]] FrameMetadata {
    [[maybe_unused]] int64_t timestampNs = 0;
    int32_t width = 0;
    int32_t height = 0;
    [[maybe_unused]] int32_t format = 0;
    [[maybe_unused]] int64_t frameNumber = 0;
};

}  // namespace nativesensor
