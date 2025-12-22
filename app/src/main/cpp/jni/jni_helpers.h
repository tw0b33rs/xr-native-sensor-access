#pragma once

#include <jni.h>

namespace nativesensor {

/// Get JNIEnv for current thread. Returns nullptr if not attached to JVM.
[[maybe_unused]]
inline JNIEnv* getEnvForCurrentThread(JavaVM* jvm) noexcept {
    JNIEnv* env = nullptr;
    if (jvm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) == JNI_OK) {
        return env;
    }
    return nullptr;
}

/// RAII wrapper for attaching/detaching current thread to JVM
class [[maybe_unused]] JniThreadAttachment {
public:
    explicit JniThreadAttachment(JavaVM* jvm) : jvm_(jvm), env_(nullptr), attached_(false) {
        if (!jvm_) return;

        int status = jvm_->GetEnv(reinterpret_cast<void**>(&env_), JNI_VERSION_1_6);
        if (status == JNI_EDETACHED) {
            if (jvm_->AttachCurrentThread(&env_, nullptr) == JNI_OK) {
                attached_ = true;
            }
        }
    }

    ~JniThreadAttachment() {
        if (attached_ && jvm_) {
            jvm_->DetachCurrentThread();
        }
    }

    JniThreadAttachment(const JniThreadAttachment&) = delete;
    JniThreadAttachment& operator=(const JniThreadAttachment&) = delete;

    [[nodiscard]] [[maybe_unused]] JNIEnv* env() const noexcept { return env_; }
    [[nodiscard]] [[maybe_unused]] bool isAttached() const noexcept { return env_ != nullptr; }

private:
    JavaVM* jvm_;
    JNIEnv* env_;
    bool attached_;
};

/// Scoped local reference that auto-deletes
template<typename T>
class [[maybe_unused]] ScopedLocalRef {
public:
    [[maybe_unused]]
    ScopedLocalRef(JNIEnv* env, T ref) noexcept : env_(env), ref_(ref) {}

    ~ScopedLocalRef() {
        if (ref_) {
            env_->DeleteLocalRef(ref_);
        }
    }

    ScopedLocalRef(const ScopedLocalRef&) = delete;
    ScopedLocalRef& operator=(const ScopedLocalRef&) = delete;

    [[nodiscard]] T get() const noexcept { return ref_; }

    [[maybe_unused]]
    T release() noexcept {
        T tmp = ref_;
        ref_ = nullptr;
        return tmp;
    }

private:
    JNIEnv* env_;
    T ref_;
};

}  // namespace nativesensor

