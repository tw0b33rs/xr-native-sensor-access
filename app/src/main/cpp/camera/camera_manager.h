#pragma once

#include <camera/NdkCameraManager.h>
#include <camera/NdkCameraDevice.h>
#include <camera/NdkCameraMetadata.h>
#include <memory>
#include <vector>
#include <mutex>
#include <string>

#include "camera_data.h"

namespace nativesensor {

/// RAII wrapper for ACameraManager
class CameraManager {
public:
    CameraManager();
    ~CameraManager();

    CameraManager(const CameraManager&) = delete;
    CameraManager& operator=(const CameraManager&) = delete;

    /// Enumerate all available cameras with metadata
    [[nodiscard]]
    std::vector<CameraInfo> enumerateCameras();

    /// Get the native camera manager handle (for CameraStream use)
    [[nodiscard]]
    ACameraManager* getNativeManager() const { return cameraManager_; }

    /// Check if camera manager is valid
    [[nodiscard]]
    bool isValid() const { return cameraManager_ != nullptr; }

private:
    /// Classify camera into cluster based on metadata heuristics
    static CameraClusterType classifyCamera(const CameraInfo& info, const std::string& id);

    /// Query camera characteristics
    bool queryCharacteristics(const char* cameraId, CameraInfo& outInfo);

    ACameraManager* cameraManager_ = nullptr;
    std::mutex mutex_;
};

}  // namespace nativesensor

