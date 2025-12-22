#include "camera_manager.h"

#include <android/log.h>
#include <algorithm>
#include <cctype>

namespace {
constexpr const char* kLogTag = "NativeSensor.Camera";
}

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, kLogTag, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, kLogTag, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, kLogTag, __VA_ARGS__)

namespace nativesensor {

namespace {

// Resolution threshold for camera classification heuristics
constexpr int32_t kHighResThreshold = 1920 * 1080;  // > 1080p = likely passthrough

// Helper to convert string to lowercase for matching
std::string toLower(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return result;
}

}  // namespace

CameraManager::CameraManager() {
    cameraManager_ = ACameraManager_create();
    if (!cameraManager_) {
        LOGE("Failed to create ACameraManager");
    } else {
        LOGI("ACameraManager created successfully");
    }
}

CameraManager::~CameraManager() {
    if (cameraManager_) {
        ACameraManager_delete(cameraManager_);
        cameraManager_ = nullptr;
        LOGI("ACameraManager destroyed");
    }
}

std::vector<CameraInfo> CameraManager::enumerateCameras() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<CameraInfo> cameras;

    if (!cameraManager_) {
        LOGE("Cannot enumerate cameras: no camera manager");
        return cameras;
    }

    ACameraIdList* cameraIds = nullptr;
    camera_status_t status = ACameraManager_getCameraIdList(cameraManager_, &cameraIds);

    if (status != ACAMERA_OK || !cameraIds) {
        LOGE("Failed to get camera ID list: %d", status);
        return cameras;
    }

    LOGI("Found %d cameras", cameraIds->numCameras);

    for (int i = 0; i < cameraIds->numCameras; ++i) {
        const char* id = cameraIds->cameraIds[i];
        CameraInfo info;
        info.id = id;

        if (queryCharacteristics(id, info) && info.width > 0 && info.height > 0) {
            info.clusterType = classifyCamera(info, id);
            cameras.push_back(std::move(info));

            LOGI("Camera[%d]: id=%s, %dx%d@%dfps, facing=%d, cluster=%d",
                 i, id, info.width, info.height, info.maxFps,
                 static_cast<int>(info.facing), static_cast<int>(info.clusterType));
        } else {
            LOGW("Skipping invalid camera %s (resolution %dx%d)", id, info.width, info.height);
        }
    }

    ACameraManager_deleteCameraIdList(cameraIds);
    return cameras;
}

bool CameraManager::queryCharacteristics(const char* cameraId, CameraInfo& outInfo) {
    ACameraMetadata* metadata = nullptr;
    camera_status_t status = ACameraManager_getCameraCharacteristics(
        cameraManager_, cameraId, &metadata);

    if (status != ACAMERA_OK || !metadata) {
        LOGE("Failed to get characteristics for camera %s: %d", cameraId, status);
        return false;
    }

    // Query lens facing
    ACameraMetadata_const_entry facingEntry;
    if (ACameraMetadata_getConstEntry(metadata, ACAMERA_LENS_FACING, &facingEntry) == ACAMERA_OK) {
        switch (facingEntry.data.u8[0]) {
            case ACAMERA_LENS_FACING_FRONT:
                outInfo.facing = CameraFacing::Front;
                break;
            case ACAMERA_LENS_FACING_BACK:
                outInfo.facing = CameraFacing::Back;
                break;
            case ACAMERA_LENS_FACING_EXTERNAL:
                outInfo.facing = CameraFacing::External;
                break;
            default:
                outInfo.facing = CameraFacing::Unknown;
                break;
        }
    }

    // Query stream configurations for resolution
    ACameraMetadata_const_entry scmEntry;
    if (ACameraMetadata_getConstEntry(metadata,
            ACAMERA_SCALER_AVAILABLE_STREAM_CONFIGURATIONS, &scmEntry) == ACAMERA_OK) {
        // Format: [format, width, height, input] tuples
        int32_t maxWidth = 0, maxHeight = 0;
        for (uint32_t j = 0; j < scmEntry.count; j += 4) {
            int32_t format = scmEntry.data.i32[j];
            int32_t width = scmEntry.data.i32[j + 1];
            int32_t height = scmEntry.data.i32[j + 2];
            int32_t isInput = scmEntry.data.i32[j + 3];

            // Only output configurations, prefer YUV_420_888 or IMPLEMENTATION_DEFINED
            if (isInput == 0 && (format == 0x23 || format == 0x22)) {  // YUV_420_888 or IMPLEMENTATION_DEFINED
                if (width * height > maxWidth * maxHeight) {
                    maxWidth = width;
                    maxHeight = height;
                }
            }
        }
        outInfo.width = maxWidth;
        outInfo.height = maxHeight;
    }

    // Query FPS ranges for max frame rate
    ACameraMetadata_const_entry fpsEntry;
    if (ACameraMetadata_getConstEntry(metadata,
            ACAMERA_CONTROL_AE_AVAILABLE_TARGET_FPS_RANGES, &fpsEntry) == ACAMERA_OK) {
        int32_t maxFps = 0;
        for (uint32_t j = 0; j < fpsEntry.count; j += 2) {
            int32_t maxRange = fpsEntry.data.i32[j + 1];
            maxFps = std::max(maxFps, maxRange);
        }
        outInfo.maxFps = maxFps;
    }

    // Check for physical camera IDs (logical camera detection)
    ACameraMetadata_const_entry physicalEntry;
    if (ACameraMetadata_getConstEntry(metadata,
            ACAMERA_LOGICAL_MULTI_CAMERA_PHYSICAL_IDS, &physicalEntry) == ACAMERA_OK) {
        outInfo.isPhysicalCamera = false;
        // Parse null-separated physical camera IDs
        std::string ids;
        const char* ptr = reinterpret_cast<const char*>(physicalEntry.data.u8);
        for (uint32_t j = 0; j < physicalEntry.count; ++j) {
            if (ptr[j] == '\0' && j + 1 < physicalEntry.count) {
                if (!ids.empty()) ids += ",";
                ids += std::string(ptr);
                ptr = reinterpret_cast<const char*>(physicalEntry.data.u8) + j + 1;
            }
        }
        if (!ids.empty()) {
            outInfo.physicalCameraIds = ids;
        }
    } else {
        outInfo.isPhysicalCamera = true;  // No physical IDs = this is a physical camera
    }

    ACameraMetadata_free(metadata);
    return outInfo.width > 0 && outInfo.height > 0;
}

CameraClusterType CameraManager::classifyCamera(const CameraInfo& info, const std::string& id) {
    std::string lowerId = toLower(id);

    // Heuristic 1: Check ID for keywords
    if (lowerId.find("eye") != std::string::npos ||
        lowerId.find("gaze") != std::string::npos ||
        lowerId.find("ir") != std::string::npos) {
        return CameraClusterType::EyeTracking;
    }

    if (lowerId.find("depth") != std::string::npos ||
        lowerId.find("tof") != std::string::npos) {
        return CameraClusterType::Depth;
    }

    if (lowerId.find("track") != std::string::npos ||
        lowerId.find("slam") != std::string::npos) {
        return CameraClusterType::Avatar;
    }

    // Heuristic 2: Resolution-based classification
    int32_t resolution = info.width * info.height;

    if (resolution >= kHighResThreshold) {
        // High resolution (>= 1080p) = likely passthrough camera
        return CameraClusterType::Passthrough;
    }

    // Heuristic 3: Front-facing cameras with sub-1080p resolution on XR devices
    // are typically tracking cameras (SLAM, world-facing fisheye sensors)
    if (info.facing == CameraFacing::Front && resolution > 0) {
        return CameraClusterType::Avatar;
    }

    // Heuristic 4: External cameras on XR devices are often tracking cameras
    if (info.facing == CameraFacing::External) {
        return CameraClusterType::Avatar;
    }

    // Heuristic 5: Low/medium resolution back-facing could be tracking
    if (resolution > 0 && resolution < kHighResThreshold) {
        // Anything below 1080p that isn't passthrough is likely tracking
        return CameraClusterType::Avatar;
    }

    // Default to unknown if we can't determine
    return CameraClusterType::Unknown;
}

}  // namespace nativesensor
