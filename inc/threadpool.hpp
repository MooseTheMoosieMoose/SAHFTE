#pragma once

#include <thread>
#include <mutex>
#include <queue>
#include <vector>
#include <functional>
#include <atomic>
#include <condition_variable>
#include <concepts>

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
    Threadpool ();

    /**
     * @brief the base destructor handles signaling to child threads that the process should end
     */
    ~Threadpool();

    Threadpool (const Threadpool& other) = delete; //We do not want our threadpool copied
    Threadpool& operator=(const Threadpool& other) = delete; //We do not want copy assignment either
    Threadpool (Threadpool&& other) = delete; //Moving is also naughty
    Threadpool& operator=(Threadpool&& other) = delete; //Move assignment is still bad

    /**
     * @brief spins up the threads that the pool will use, this should only be called once!
     */
    void initilize_threads(std::size_t thread_count);

    /**
     * @brief a template type-eraser that allows you to place any  `void` returning callable into queue 
     */
    template <typename Func, typename... Args>
    void queue_task(Func&& callable, Args&&... args) {
        queued_jobs_count++;
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
        queue_cv.notify_one();
    }

    /**
     * @brief given some container that supports `begin()`, `end()`, map some task across the elements inside `container`,
     * split as `jobs` tasks 
     * @note necessarily blocking to prevent pointer invalidation
     */
    template <typename T, template <typename, typename...> class Container, typename Func, typename... Args>
    requires requires (Container<T> container ){ { container.end() - container.begin() } -> std::convertible_to<std::ptrdiff_t>; }
    void queue_and_map_task(Container<T>& container, std::size_t jobs, Func&& callable, Args&&... args) {
        //Get the number of elements in the container
        const std::size_t element_count = static_cast<std::size_t>(container.end() - container.begin());

        //If there are more jobs than elements, then we would be wasting jobs, clip it down for a 1-1 matching
        if (jobs > element_count) {
            jobs = element_count;
        }

        //Calculate the number of elements per_job
        const std::size_t elem_per_job = element_count / jobs;

        //Create each job
        auto lambda_iter_begin = container.begin();

        for (std::size_t i = 0; i < jobs; i++) {
            //Increment the job counter and calculate the end point of this job
            queued_jobs_count++;
            auto lambda_iter_end = lambda_iter_begin + elem_per_job;
            if (i == jobs - 1 || lambda_iter_end > container.end()) {
                lambda_iter_end = container.end();
            }

            //Enforce a seperate scope for the lock on the task queue
            {
                std::lock_guard<std::mutex> lock(queue_mtx);
                task_queue.push([&, begin = lambda_iter_begin, end = lambda_iter_end]() mutable {
                    auto self_iter = begin;
                    while (self_iter != end) {
                        std::invoke(callable, *self_iter, args...);
                        self_iter++;
                    }
                });
            }

            //Advance the iterator
            lambda_iter_begin = lambda_iter_end;
        }

        //We must wait to prevent pointer invalidation
        join_and_process();
    }

    /**
     * @brief attaches the calling thread as a thread running the tasks queued in the pool, is blocking
     * until the number of jobs in the pool is reduced to 0
     * @note sort of analogous to `join()` in other libraries, but instead of sitting around, the joining 
     * thread helps out in the pool
     */
    void join_and_process();

    /**
     * @brief gets the number of queued elements, which is internally stored as an `atomic<std::size_t>`,
     * this number is understandably volatile, especially if being read while enqueing tasks
     */
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

    //Wait mutex for the calling thread to pend on
    std::mutex wait_mtx {};
    
    //Wait CV so we dont burn cycles waiting for the TP to finish
    std::condition_variable wait_cv {};

    void process_tasks();
};

} // end namespace fusion system