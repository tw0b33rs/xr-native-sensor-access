# Android XR Native Sensor Access - Copilot Instructions

## Project Purpose

**Developer-focused sample** demonstrating raw, low-latency sensor data access on Android XR (Samsung Galaxy XR) via NDK/C++. Provides clean access to IMU and camera streams at maximum hardware rates—designed as a foundation for sensor fusion, computer vision, SLAM, or custom XR applications.

## Design Principles

- **Zero-copy where possible**: Use `AHardwareBuffer`, direct `AImage` access, avoid unnecessary memcpy
- **Subscribe, never poll**: All sensors use callback/listener patterns for lowest latency
- **Maximum rate**: Request `SENSOR_DELAY_FASTEST` (0μs) for IMU, highest FPS for cameras
- **Modular by sensor type**: Each sensor category in its own namespace/folder for independent use
- **Raw data first**: Expose unprocessed sensor values; let developers handle fusion/filtering

## Architecture Overview

```
┌─────────────────────────────────────────────────────────┐
│              Kotlin/Compose UI (Display Only)            │
│  MainActivity.kt → Raw values visualization             │
├─────────────────────────────────────────────────────────┤
│                   JNI Bridge Layer                       │
│  Minimal crossing: batch callbacks, direct buffers      │
├─────────────────────────────────────────────────────────┤
│           C++ Native Sensor Layer (Modular)             │
│  imu/ → ASensorManager subscriptions                    │
│  camera/ → ACameraManager + AImageReader (zero-copy)    │
│  common/ → Threading, callbacks, buffer management      │
└─────────────────────────────────────────────────────────┘
```

## File Organization (Modular Structure)

```
app/src/main/
├── cpp/                              # Native sensor layer
│   ├── CMakeLists.txt
│   ├── common/
│   │   ├── callback_handler.h        # Thread-safe callback dispatch
│   │   ├── sensor_types.h            # Shared data structs (timestamps, etc.)
│   │   └── ring_buffer.h             # Lock-free buffer for high-freq data
│   ├── imu/
│   │   ├── imu_manager.h/cpp         # ASensorManager wrapper
│   │   ├── accelerometer.h/cpp       # TYPE_ACCELEROMETER subscription
│   │   ├── gyroscope.h/cpp           # TYPE_GYROSCOPE subscription
│   │   └── imu_data.h                # Raw IMU struct {x,y,z,timestamp_ns}
│   ├── camera/
│   │   ├── camera_manager.h/cpp      # ACameraManager lifecycle
│   │   ├── camera_stream.h/cpp       # AImageReader zero-copy capture
│   │   └── camera_data.h             # Frame metadata, buffer handles
│   └── jni/
│       ├── jni_bridge.cpp            # JNI exports (RegisterNatives)
│       └── jni_helpers.h             # JNIEnv utilities, direct buffers
├── java/.../nativesensoraccess/
│   ├── MainActivity.kt               # XR spatial/2D mode switching
│   ├── ui/theme/                     # Material3 theming
│   └── sensor/                       # (create) Kotlin bridge layer
│       ├── NativeSensorBridge.kt     # JNI bindings, native method declarations
│       ├── ImuDataFlow.kt            # SharedFlow for IMU callbacks
│       ├── CameraFrameFlow.kt        # SharedFlow for camera frames
│       └── SensorViewModel.kt        # UI state holder
└── res/
```

## NDK Integration (Required Setup)

Add to `app/build.gradle.kts`:
```kotlin
android {
    defaultConfig {
        ndk { abiFilters += listOf("arm64-v8a") }  // XR devices are ARM64
    }
    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
            version = "3.22.1"
        }
    }
    ndkVersion = "26.1.10909125"
}
```

## Sensor Subscription Patterns (NOT Polling)

### IMU: ASensorManager with Callback
```cpp
// imu/imu_manager.cpp - Subscribe at maximum rate
void ImuManager::startAccelerometer(SensorCallback callback) {
    ASensor const* sensor = ASensorManager_getDefaultSensor(
        sensorManager_, ASENSOR_TYPE_ACCELEROMETER);
    
    // Create queue with callback (NOT polling with ALooper_pollOnce)
    eventQueue_ = ASensorManager_createEventQueue(
        sensorManager_, looper_, ALOOPER_POLL_CALLBACK,
        [](int fd, int events, void* data) -> int {
            auto* self = static_cast<ImuManager*>(data);
            self->drainEvents();  // Process all pending events
            return 1;  // Continue receiving
        }, this);
    
    ASensorEventQueue_enableSensor(eventQueue_, sensor);
    ASensorEventQueue_setEventRate(eventQueue_, sensor, 0);  // FASTEST = 0μs
}
```

### Camera: AImageReader with Listener (Zero-Copy)
```cpp
// camera/camera_stream.cpp - Zero-copy frame access
void CameraStream::setupImageReader(int32_t width, int32_t height) {
    AImageReader_new(width, height, AIMAGE_FORMAT_YUV_420_888, 
                     maxImages_, &imageReader_);
    
    AImageReader_ImageListener listener{
        .context = this,
        .onImageAvailable = [](void* ctx, AImageReader* reader) {
            auto* self = static_cast<CameraStream*>(ctx);
            AImage* image = nullptr;
            if (AImageReader_acquireLatestImage(reader, &image) == AMEDIA_OK) {
                self->onFrame(image);  // Zero-copy: direct AImage access
                AImage_delete(image);
            }
        }
    };
    AImageReader_setImageListener(imageReader_, &listener);
}

// Access raw plane data without copy
void CameraStream::onFrame(AImage* image) {
    uint8_t* yPlane; int yLen;
    AImage_getPlaneData(image, 0, &yPlane, &yLen);  // Direct pointer
    int64_t timestamp;
    AImage_getTimestamp(image, &timestamp);  // Hardware timestamp (ns)
    // Pass to callback—developer handles CV/fusion from here
}
```

## JNI Bridge Pattern (Minimal Crossing)

```kotlin
// sensor/NativeSensorBridge.kt
object NativeSensorBridge {
    init { System.loadLibrary("nativesensor") }
    
    external fun startImu(callback: ImuCallback)
    external fun stopImu()
    external fun startCamera(cameraId: String, callback: CameraCallback)
    external fun stopCamera()
    
    // Use DirectByteBuffer for zero-copy frame data
    interface CameraCallback {
        fun onFrame(buffer: ByteBuffer, width: Int, height: Int, timestampNs: Long)
    }
}
```

## Required Permissions (AndroidManifest.xml)
```xml
<uses-permission android:name="android.permission.CAMERA" />
<uses-permission android:name="android.permission.HIGH_SAMPLING_RATE_SENSORS" />
<uses-feature android:name="android.hardware.sensor.accelerometer" android:required="true" />
<uses-feature android:name="android.hardware.sensor.gyroscope" android:required="true" />
<uses-feature android:name="android.hardware.camera" android:required="true" />
```

## XR Compose UI Pattern

```kotlin
// MainActivity.kt - existing pattern for spatial/2D mode
if (LocalSpatialCapabilities.current.isSpatialUiEnabled) {
    Subspace { SpatialPanel(...) { SensorDataDisplay(viewModel) } }
} else {
    My2DContent { SensorDataDisplay(viewModel) }
}
```

## Build & Run

```bash
./gradlew assembleDebug    # Build with native code
./gradlew installDebug     # Deploy to device
adb shell dumpsys sensorservice  # Debug sensor availability
adb logcat -s NativeSensor       # Filter native logs
```

**Requirements**: Android SDK 36, NDK 26+, Java 11, ARM64 device (Samsung Galaxy XR)

## Code Conventions

- **C++17**: RAII wrappers for NDK handles, `std::unique_ptr` with custom deleters
- **No raw new/delete**: Use smart pointers and RAII throughout
- **Lock-free buffers**: Ring buffers for high-frequency IMU data
- **Kotlin coroutines**: `SharedFlow` for sensor callbacks, `StateFlow` for UI state
- **JNI**: Batch events before crossing, use `DirectByteBuffer` for frames
- **Timestamps**: Always preserve hardware `timestamp_ns` for fusion accuracy
