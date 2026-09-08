#pragma once

#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <optional>
#include <queue>
#include <utility>

namespace fpv4mac {

template <typename T>
class BoundedQueue {
public:
    explicit BoundedQueue(const std::size_t capacity) : capacity_(capacity) {}

    bool try_push(T value) {
        std::lock_guard lock(mutex_);
        if (closed_ || capacity_ == 0 || queue_.size() >= capacity_) {
            return false;
        }
        queue_.push(std::move(value));
        ready_.notify_one();
        return true;
    }

    std::optional<T> pop() {
        std::unique_lock lock(mutex_);
        ready_.wait(lock, [this] { return closed_ || !queue_.empty(); });
        if (queue_.empty()) {
            return std::nullopt;
        }
        T value = std::move(queue_.front());
        queue_.pop();
        return value;
    }

    void close() {
        std::lock_guard lock(mutex_);
        closed_ = true;
        ready_.notify_all();
    }

private:
    const std::size_t capacity_;
    std::mutex mutex_;
    std::condition_variable ready_;
    std::queue<T> queue_;
    bool closed_{};
};

} // namespace fpv4mac
