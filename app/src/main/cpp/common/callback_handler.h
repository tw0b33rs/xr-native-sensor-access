#pragma once

#include <jni.h>
#include <functional>
#include <mutex>
#include <atomic>

namespace nativesensor {

/// Thread-safe callback dispatcher for sensor events.
/// Stores JNI callback references and dispatches to Kotlin/Java.
/// Reserved for future JNI callback integration.
class [[maybe_unused]] CallbackHandler {
public:
    CallbackHandler() = default;
    ~CallbackHandler() { reset(); }

    CallbackHandler(const CallbackHandler&) = delete;
    CallbackHandler& operator=(const CallbackHandler&) = delete;
    CallbackHandler(CallbackHandler&&) = delete;
    CallbackHandler& operator=(CallbackHandler&&) = delete;

    /// Store a global reference to a Kotlin callback object
    [[maybe_unused]]
    void setCallback(JNIEnv* env, jobject callback) {
        std::lock_guard<std::mutex> lock(mutex_);
        reset_internal(env);
        if (callback) {
            callback_ = env->NewGlobalRef(callback);
        }
    }

    /// Check if callback is registered
    [[nodiscard]] [[maybe_unused]]
    bool hasCallback() const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        return callback_ != nullptr;
    }

    /// Get the callback object (caller must hold lock or use invokeCallback)
    [[nodiscard]] [[maybe_unused]]
    jobject getCallback() const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        return callback_;
    }

    /// Thread-safe callback invocation
    template<typename Func>
    [[maybe_unused]]
    void invokeCallback(JNIEnv* env, Func&& func) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (callback_) {
            func(env, callback_);
        }
    }

    /// Release the callback reference
    void reset(JNIEnv* env = nullptr) noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        reset_internal(env);
    }

private:
    void reset_internal(JNIEnv* env) {
        if (callback_ && env) {
            env->DeleteGlobalRef(callback_);
            callback_ = nullptr;
        }
    }

    mutable std::mutex mutex_;
    jobject callback_ = nullptr;
};

}  // namespace nativesensor

