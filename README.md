# Android XR Native Sensor Access

Raw, low-latency sensor data access on Android XR devices via NDK/C++. Built for developers building sensor fusion, computer vision, or custom XR applications on Samsung Galaxy XR.

## Features

- **High-frequency IMU access** — Accelerometer and gyroscope at maximum hardware rates (`SENSOR_DELAY_FASTEST`)
- **Zero-copy camera streams (currently pass-through cameras only)** — Direct `AImageReader` access with `ANativeWindow` output
- **Avatar "selfie camera" stream**

![sensor-access](./doc/native-sensor-access_overview.gif)

## Architecture

```
┌─────────────────────────────────────────────────────────┐
│              Kotlin/Compose UI (Display Only)           │
│  MainActivity.kt → Raw values visualization             │
├─────────────────────────────────────────────────────────┤
│                   JNI Bridge Layer                      │
│  Minimal crossing: batch callbacks, direct buffers      │
├─────────────────────────────────────────────────────────┤
│           C++ Native Sensor Layer (Modular)             │
│  imu/ → ASensorManager subscriptions                    │
│  camera/ → ACameraManager + AImageReader (zero-copy)    │
│  common/ → Threading, callbacks, buffer management      │
└─────────────────────────────────────────────────────────┘
```

## Prerequisites

- Android Studio Otter or later
- Android SDK 36
- NDK 26.1.10909125 or later
- Android XR device (only tested with Samsung Galaxy XR)

## Getting Started

### 1. Clone the repository

```bash
git clone https://github.com/your-username/xr-native-sensor-access.git
cd xr-native-sensor-access
```

### 2. Build and run

```bash
./gradlew assembleDebug
./gradlew installDebug
```

### 3. Debug sensor access

```bash
# Check sensor availability
adb shell dumpsys sensorservice

# Filter native logs
adb logcat -s NativeSensor
```

## Project Structure

```
app/src/main/
├── cpp/                              # Native sensor layer
│   ├── common/
│   │   ├── callback_handler.h        # Thread-safe callback dispatch
│   │   ├── sensor_types.h            # Shared data structs
│   │   └── ring_buffer.h             # Lock-free SPSC buffer
│   ├── imu/
│   │   ├── imu_manager.h/cpp         # ASensorManager wrapper
│   │   └── imu_data.h                # Raw IMU struct {x,y,z,timestamp_ns}
│   ├── camera/
│   │   ├── camera_manager.h/cpp      # ACameraManager lifecycle
│   │   ├── camera_stream.h/cpp       # AImageReader zero-copy capture
│   │   └── camera_data.h             # Frame metadata
│   └── jni/
│       ├── jni_bridge.cpp            # JNI exports
│       └── jni_helpers.h             # JNIEnv utilities
├── java/.../nativesensoraccess/
│   ├── MainActivity.kt               # XR spatial/2D mode switching
│   └── sensor/
│       ├── NativeSensorBridge.kt     # JNI bindings
│       ├── CameraBridge.kt           # Camera JNI bindings
│       ├── SensorData.kt             # Kotlin data classes
│       └── SensorViewModel.kt        # UI state holder
└── res/
```

## Usage

### IMU Access

The native layer provides high-frequency IMU data via `ASensorManager`:

```kotlin
// Initialize and start IMU at maximum rate
NativeSensorBridge.init()

// Get latest samples
val accel = NativeSensorBridge.getAccelData()  // m/s²
val gyro = NativeSensorBridge.getGyroData()    // rad/s

// Get statistics
val stats = NativeSensorBridge.getStats()
println("Accel: ${stats.accelFrequencyHz} Hz, ${stats.accelLatencyMs} ms")

// Clean up
NativeSensorBridge.stop()
```

### Camera Streams

Zero-copy camera preview using native window surfaces:

```kotlin
// Enumerate available cameras
val cameras = CameraBridge.enumerateCameras()

// Start preview to a surface
CameraBridge.startPreview(cameraId, surface)

// Stop and release
CameraBridge.stopPreview()
```

### XR Compose Integration

The app automatically switches between spatial and 2D modes:

```kotlin
if (LocalSpatialCapabilities.current.isSpatialUiEnabled) {
    Subspace {
        SpatialPanel(SubspaceModifier.width(1200.dp).height(800.dp).resizable().movable()) {
            SensorDataDisplay(viewModel)
        }
    }
} else {
    My2DContent { SensorDataDisplay(viewModel) }
}
```

## Required Permissions

Add to `AndroidManifest.xml`:

```xml
<uses-permission android:name="android.permission.CAMERA" />
<uses-permission android:name="android.permission.HIGH_SAMPLING_RATE_SENSORS" />

<uses-feature android:name="android.hardware.sensor.accelerometer" android:required="true" />
<uses-feature android:name="android.hardware.sensor.gyroscope" android:required="true" />
<uses-feature android:name="android.hardware.camera" android:required="true" />
```

## Design Principles

| Principle | Implementation |
|-----------|----------------|
| Zero-copy where possible | `AHardwareBuffer`, direct `AImage` access |
| Subscribe, never poll | Callback/listener patterns for all sensors |
| Maximum rate | `SENSOR_DELAY_FASTEST` (0μs) for IMU |
| Raw data first | Unprocessed sensor values, hardware timestamps |
| RAII throughout | Smart pointers, custom deleters for NDK handles |

## Upcoming Features

- [ ] **Eye tracking** — Gaze direction and fixation points via XR runtime APIs
- [ ] **Hand tracking** — Skeletal hand pose and gesture recognition
- [ ] **Microphone access** — Low-latency audio capture for voice input
- [ ] **Depth sensing** — ToF/stereo depth maps from XR cameras
- [ ] **Spatial anchors** — Persistent world-locked reference points
- [ ] **Controller input** — Raw input from XR controllers

## Resources

- [Android NDK Sensors](https://developer.android.com/ndk/guides/sensors)
- [Camera2 NDK API](https://developer.android.com/ndk/reference/group/camera)
- [Android XR Compose](https://developer.android.com/develop/xr)

> [!NOTE]
> This project is optimized for Samsung Galaxy XR. Other Android XR devices may have different sensor configurations.

