#pragma once

#include <mutex>
#include <vector>
#include <atomic>
#include <queue>
#include <optional>
#include <set>

namespace FusionSystem {

/**
 * @brief a basic thread safe queue that is protected by a mutex
 */
template <typename T>
class TSQueue {
public:
    TSQueue() = default;
    ~TSQueue() = default;
    TSQueue (const TSQueue& other) = delete;
    TSQueue& operator=(const TSQueue& other) = delete;
    TSQueue (TSQueue&& other) = delete;
    TSQueue& operator=(TSQueue&& other) = delete;

    /**
     * @brief retreives the first element in a queue
     */
    std::optional<T> pop() {
        std::lock_guard<std::mutex> lock(mtx);
        if (storage.empty()) {
            return std::nullopt;
        } else {
            T item = storage.front();
            storage.pop();
            return item;
        }
        
    }


    void push(T&& elem) {
        std::lock_guard<std::mutex> lock(mtx);
        storage.push(std::move(elem));
    }

    void push(const T& elem) {
        std::lock_guard<std::mutex> lock(mtx);
        storage.push(elem);
    }

    std::size_t size() {
        std::lock_guard<std::mutex> lock(mtx);
        return storage.size();
    }


private:
    std::queue<T> storage {};
    std::mutex mtx {};
};

template <typename T>
class TSVector {
public:
    TSVector () = default;
    ~TSVector() = default;
    TSVector (const TSVector& other) = delete;
    TSVector& operator=(const TSVector& other) = delete;
    TSVector (TSVector&& other) = delete;
    TSVector& operator=(TSVector&& other) = delete;

    void push_back(T&& elem) {
        std::lock_guard<std::mutex> lock(mtx);
        storage.push_back(std::move(elem));
    }

    void push_back(const T& elem) {
        std::lock_guard<std::mutex> lock(mtx);
        storage.push_back(elem);
    }

    std::size_t size() {
        std::lock_guard<std::mutex> lock(mtx);
        return storage.size();
    }


private:
    std::vector<T> storage {};
    std::mutex mtx {};
};

template <typename T>
class TSMultiset {
public:
    TSMultiset  () = default;
    ~TSMultiset () = default;
    TSMultiset  (const TSMultiset & other) = delete;
    TSMultiset & operator=(const TSMultiset & other) = delete;
    TSMultiset  (TSMultiset && other) = delete;
    TSMultiset & operator=(TSMultiset && other) = delete;

    void push(T&& elem) {
        std::lock_guard<std::mutex> lock(mtx);
        storage.insert(std::move(elem));
    }

    void push(const T& elem) {
        std::lock_guard<std::mutex> lock(mtx);
        storage.insert(elem);
    }

    std::size_t size() {
        std::lock_guard<std::mutex> lock(mtx);
        return storage.count();
    }


private:
    std::multiset<T> storage {};
    std::mutex mtx {};
};

} //End namespace fusion system