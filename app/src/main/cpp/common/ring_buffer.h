#pragma once

#include <array>
#include <atomic>
#include <cstddef>

namespace nativesensor {

/// Lock-free single-producer single-consumer ring buffer for high-frequency sensor data.
/// Uses power-of-two size for efficient modulo via bitwise AND.
template<typename T, size_t Capacity>
class [[maybe_unused]] RingBuffer {
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be power of 2");
    static_assert(Capacity > 0, "Capacity must be positive");

public:
    RingBuffer() noexcept : head_(0), tail_(0) {}

    /// Push a new element (producer side). Returns false if buffer is full.
    [[maybe_unused]]
    bool push(const T& item) noexcept {
        const size_t currentHead = head_.load(std::memory_order_relaxed);
        const size_t nextHead = (currentHead + 1) & kMask;

        if (nextHead == tail_.load(std::memory_order_acquire)) {
            return false;
        }

        buffer_[currentHead] = item;
        head_.store(nextHead, std::memory_order_release);
        return true;
    }

    /// Push with overwrite (drops oldest if full)
    [[maybe_unused]]
    void pushOverwrite(const T& item) noexcept {
        const size_t currentHead = head_.load(std::memory_order_relaxed);
        const size_t nextHead = (currentHead + 1) & kMask;

        if (nextHead == tail_.load(std::memory_order_acquire)) {
            tail_.store((tail_.load(std::memory_order_relaxed) + 1) & kMask,
                       std::memory_order_release);
        }

        buffer_[currentHead] = item;
        head_.store(nextHead, std::memory_order_release);
    }

    /// Pop an element (consumer side). Returns false if buffer is empty.
    [[maybe_unused]]
    bool pop(T& item) noexcept {
        const size_t currentTail = tail_.load(std::memory_order_relaxed);

        if (currentTail == head_.load(std::memory_order_acquire)) {
            return false;
        }

        item = buffer_[currentTail];
        tail_.store((currentTail + 1) & kMask, std::memory_order_release);
        return true;
    }

    [[nodiscard]] [[maybe_unused]]
    bool empty() const noexcept {
        return head_.load(std::memory_order_acquire) ==
               tail_.load(std::memory_order_acquire);
    }

    [[nodiscard]] [[maybe_unused]]
    size_t size() const noexcept {
        const size_t h = head_.load(std::memory_order_acquire);
        const size_t t = tail_.load(std::memory_order_acquire);
        return (h - t) & kMask;
    }

    [[maybe_unused]]
    void clear() noexcept {
        tail_.store(head_.load(std::memory_order_relaxed), std::memory_order_release);
    }

    [[nodiscard]] [[maybe_unused]]
    static constexpr size_t capacity() noexcept { return Capacity; }

private:
    static constexpr size_t kMask = Capacity - 1;

    std::array<T, Capacity> buffer_;
    std::atomic<size_t> head_;
    std::atomic<size_t> tail_;
};

}  // namespace nativesensor

