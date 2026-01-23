/**                                                      
 *  ,---.    ,---.  ,--.  ,--.,------.,--------.,------. 
 * '   .-'  /  O  \ |  '--'  ||  .---''--.  .--'|  .---' 
 * `.  `-. |  .-.  ||  .--.  ||  `--,    |  |   |  `--,  
 * .-'    ||  | |  ||  |  |  ||  |`      |  |   |  `---. 
 * `-----' `--' `--'`--'  `--'`--'       `--'   `------'                                                      
 * 
 * SAHFTE (Spatial Algorithmic Hashing Fusion Time-sliced Engine)
 * @file mtx_ds.hpp
 * @author Moose Abou-Harb
 * @brief this file  contains the templates for some mutex protected data structures, they are simply STL
 * implementations protected with lock_guards
 * @copyright `26, Lisenced under whatever Paccar Inc.'s requirements are
 */

#pragma once

#include <mutex>    //Mutexes to provide locking for the structures
#include <vector>   //Vector for TSVector
#include <queue>    //Queue fro TSQueue
#include <optional> //Optional to provide safe popping off a queue

namespace FusionSystem {

/*=====================================================================================================
                                     Thread Safe Queue (TSQueue)
=====================================================================================================*/

/**
 * @brief a basic thread safe queue that is protected by a mutex, provides most of the operations
 * that a queue can, but with automatic mutex locking
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
     * @brief retreives the first element in a queue, protected by an `std::optional` wrapped return to
     * protect against popping when the queue is empty
     * @returns the first element in a FIFO manner, protected inside an `std::optional`
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

    /**
     * @brief adds an element into the queue in a FIFO manner
     * @param elem the element to push
     * @note this is the L-value specialization of this function
     */
    void push(T&& elem) {
        std::lock_guard<std::mutex> lock(mtx);
        storage.push(std::move(elem));
    }

    /**
     * @brief adds an element into the queue in a FIFO manner
     * @param elem the element to push
     * @note this is the R-value specialization of this function
     */
    void push(const T& elem) {
        std::lock_guard<std::mutex> lock(mtx);
        storage.push(elem);
    }

    /**
     * @brief gets the size in a moment in time of the queue,
     * if this is activly being manipulated this value will be invalidated
     * as soon as the queue is changed
     * @return the current size of the queue at the moment of this being called
     */
    std::size_t size() const noexcept {
        std::lock_guard<std::mutex> lock(mtx);
        return storage.size();
    }

    /**
     * @brief gets a reference to the underlying storage element, this operation is
     * NOT threadsafe!
     * @returns a reference to the underlying `std::queue` that this object manages
     * @warning NOT THREAD SAFE! It is reccomended to operate on this in the minimum
     * needed scope so that this reference is dropped ASAP
     */
    std::queue<T>& storage_ref() noexcept {
        return storage;
    }


private:
    //The underlying storage element for the queue
    std::queue<T> storage {};

    //The mutex which protects the queue
    std::mutex mtx {};
};

/*=====================================================================================================
                                     Thread Safe Vector (TSVector)
=====================================================================================================*/

/**
 * @brief a type safe vector, essentially just a vector that is protected by a mutex, and provides
 * most of the basic operations you would expect of a vector
 */
template <typename T>
class TSVector {
public:
    TSVector () = default;
    ~TSVector() = default;
    TSVector (const TSVector& other) = delete;
    TSVector& operator=(const TSVector& other) = delete;
    TSVector (TSVector&& other) = delete;
    TSVector& operator=(TSVector&& other) = delete;

    /**
     * @brief inserts an element into the underlying vector
     * @param elem the item to insert at the end
     * @note this is the R-Value specialization of this function
     */
    void push_back(T&& elem) {
        std::lock_guard<std::mutex> lock(mtx);
        storage.push_back(std::move(elem));
    }

    /**
     * @brief inserts an element into the underlying vector
     * @param elem the item to insert at the end
     * @note this is the L-Value specialization of this function
     */
    void push_back(const T& elem) {
        std::lock_guard<std::mutex> lock(mtx);
        storage.push_back(elem);
    }

    /**
     * @brief calls `.reserve()` on the underlying vector, can help
     * improve performance if you know ahead of times how many elements
     * you will be inserting
     * @param count the number of elements to reserve
     */
    void reserve(std::size_t count) {
        std::lock_guard<std::mutex> lock(mtx);
        storage.reserve(count);
    }

    /**
     * @brief gets the current size of the underlying vector at the point of call
     * @return an `std::size_t` with the number of elements in the vector
     * @note if the vector is being pushed, this may be invalid right after call,
     * this value only makes sense when the vector is in a read-only state
     */
    std::size_t size() {
        std::lock_guard<std::mutex> lock(mtx);
        return storage.size();
    }

    /**
     * @brief gets a reference to the underlying vector directly
     * @returns a reference to the underlying `std::vector<T>` used in this object
     * @warning THIS IS NOT THREADSAFE! Only call this when this object is garunteed
     * to be in a read-only state! It is reccomended to operate on this in the minimum
     * needed scope so that this reference is dropped ASAP
     */
    std::vector<T>& storage_ref() {
        return storage;
    }


private:
    //The underlying storage component where items are put into
    std::vector<T> storage {};

    //The mutex which guards everything
    std::mutex mtx {};
};

} //End namespace fusion system