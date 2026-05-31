/**
 * @file network/MessageQueue.hpp
 * @brief Thread-safe FIFO queue template for passing messages between threads.
 * @details Responsibility: provide a single-producer, single-consumer queue that can
 *          be written from the network receive thread and read from the main thread
 *          without data races.
 *
 *          Architecture: NetworkClient owns a MessageQueue<ServerMessage> as a private
 *          member. The recv thread calls push() after parsing each line; the main thread
 *          calls tryPop() once per frame inside NetworkClient::poll(). The mutex inside
 *          the queue ensures these two concurrent accesses do not corrupt the internal
 *          std::queue.
 *
 */

#pragma once

#include <mutex>
#include <optional>
#include <queue>

/**
 * @brief Thread-safe FIFO queue wrapping std::queue<T>.
 * @details Lifetime: owned by NetworkClient as a member. Created when NetworkClient is
 *          constructed, destroyed when NetworkClient is destroyed. All methods are safe
 *          to call concurrently from different threads.
 *
 * @tparam T The element type stored in the queue. Must be movable.
 */
template <typename T>
class MessageQueue {
public:
    /**
     * @brief Default constructor. Creates an empty queue.
     */
    MessageQueue() = default;

    MessageQueue(const MessageQueue&)            = delete; ///< Non-copyable: mutex and queue state cannot be duplicated.
    MessageQueue& operator=(const MessageQueue&) = delete; ///< Non-copyable: mutex and queue state cannot be duplicated.
    MessageQueue(MessageQueue&&)                 = delete; ///< Non-movable: no safe way to move a locked mutex.
    MessageQueue& operator=(MessageQueue&&)      = delete; ///< Non-movable: no safe way to move a locked mutex.

    /**
     * @brief Push an item to the back of the queue.
     * @details Thread-safe. Acquires the internal mutex, then move-constructs T into the queue.
     * @param item Value to enqueue. Moved into the queue's internal storage.
     */
    void push(T item)
    {
        std::lock_guard<std::mutex> lock(_mutex);
        _items.push(std::move(item));
    }

    /**
     * @brief Remove and return the front item if the queue is non-empty.
     * @details Thread-safe. Returns std::nullopt when the queue is empty rather than
     *          blocking or throwing. The caller pattern is:
     *          @code
     *          while (auto msg = queue.tryPop()) { handle(*msg); }
     *          @endcode
     *          This drains all available messages without blocking the caller's thread.
     * @return The front element wrapped in std::optional, or std::nullopt if empty.
     */
    std::optional<T> tryPop()
    {
        std::lock_guard<std::mutex> lock(_mutex);
        if (_items.empty()) {
            return std::nullopt;
        }
        // Move the front element out before pop() destroys it.
        T front = std::move(_items.front());
        _items.pop();
        return front;
    }

    /**
     * @brief Return the number of items currently in the queue.
     * @details Thread-safe. Note: by the time the caller acts on the returned size,
     *          other threads may have pushed or popped items. Use for diagnostics only,
     *          not for control flow.
     * @return Number of elements in the queue at the moment the lock was held.
     */
    std::size_t size() const
    {
        std::lock_guard<std::mutex> lock(_mutex);
        return _items.size();
    }

private:
    mutable std::mutex _mutex; ///< Guards all accesses to _items.
    std::queue<T>      _items; ///< Underlying FIFO container.
};
