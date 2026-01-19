#pragma once

#include <thread>
#include <mutex>
#include <queue>
#include <vector>
#include <functional>
#include <atomic>
#include <condition_variable>

namespace FusionSystem {

/**
 * @brief a basic threadpool for tasks that require args, but are void return typed
 * @note if `std::thread::hardware_concurancy` < `thread_count`, then the threadpool will only
 * use `std::thread::hardware_concurancy` threads
 * @note due to STD limitations on std::functional, this pool cannot manage tasks that required move-only
 * parameters
 */
class Threadpool {
public:
    /**
     * @brief normal constructor for a threadpool
     */
    Threadpool (std::size_t thread_count);

    template <typename Func, typename... Args>
    void queue_task(Func&& callable, Args&&... args) {
        //Create a scope to minimize the effect of locking
        {
            //Lock the queue
            std::lock_guard<std::mutex> lock(queue_mtx);

            //Add the new task to the queue
            task_queue.push([=]() mutable {
                std::invoke(callable, args...); 
            });
        }

        //Flag that a new task is queued
        queued_jobs_count++;
        queue_cv.notify_one();
    }

    /**
     * @brief the base destructor handles signaling to child threads that the process should end
     */
    ~Threadpool();

    Threadpool (const Threadpool& other) = delete; //We do not want our threadpool copied
    Threadpool& operator=(const Threadpool& other) = delete; //We do not want copy assignment either
    Threadpool (Threadpool&& other) = delete; //Moving should be fine
    Threadpool& operator=(Threadpool&& other) = delete; //Move assignment is also chill

    /**
     * @brief attaches the calling thread as a thread running the tasks queued in the pool, is blocking
     * until the number of jobs in the pool is reduced to 0
     * @note sort of analogous to join, but instead of sitting around, the joining thread helps out in the pool
     */
    void attach_to_process_context();

    std::size_t get_queued_count() const noexcept;

private:
    std::size_t max_threads;

    //Threads holds all the threads managed by the pool
    std::vector<std::thread> threads {};

    //The task queue which the threadpool will pop off
    std::queue<std::function<void()>> task_queue {};

    //Count the number of tasks queued
    std::atomic<std::size_t> queued_jobs_count;

    //The mutex which protects the task_queue
    std::mutex queue_mtx {};

    //The cv that waits the threads for tasks to be queued
    std::condition_variable queue_cv {};

    //Exit flag is set true when the child threads are ready to rejoin
    std::atomic<bool> exit_flag;

    void process_tasks();
};

} // end namespace fusion system