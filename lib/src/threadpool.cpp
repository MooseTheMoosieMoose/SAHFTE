/**                                                      
 *  ,---.    ,---.  ,--.  ,--.,------.,--------.,------. 
 * '   .-'  /  O  \ |  '--'  ||  .---''--.  .--'|  .---' 
 * `.  `-. |  .-.  ||  .--.  ||  `--,    |  |   |  `--,  
 * .-'    ||  | |  ||  |  |  ||  |`      |  |   |  `---. 
 * `-----' `--' `--'`--'  `--'`--'       `--'   `------'                                                      
 * 
 * SAHFTE (Spatial Algorithmic Hashing Fusion Time-sliced Engine)
 * @file threadpool.cpp
 * @author Moose Abou-Harb
 * @brief this file has the function definitions for the threadpool class
 * @copyright `26, Moose Abou-Harb under the 3-Clause BSD Lisence
 */

#include "threadpool.hpp"

namespace FusionSystem {

Threadpool::Threadpool() : queued_jobs_count(0), exit_flag(false) {}

Threadpool::~Threadpool() {
    //Signal to threads that work is done
    exit_flag.store(true);
    queue_cv.notify_all();

    //Join all threads
    for (auto& t : threads) {
        if (t.joinable()) {
            t.join();
        }
    }
}

void Threadpool::initilize_threads(std::size_t thread_count) {
    max_threads = (std::thread::hardware_concurrency() < thread_count) ? std::thread::hardware_concurrency() : thread_count;

    //Create the threads, attaching them to the process tasks job
    for (std::size_t i = 0; i < max_threads; i++) {
        threads.push_back(std::thread([this](){ process_tasks(); }));
    }
}

void Threadpool::join_and_process() {
    //Enter into the process queue and help clean out any jobs not dispatched
    {
        std::unique_lock<std::mutex> lock(queue_mtx, std::defer_lock);
        while (true) {

            lock.lock();
            if (task_queue.empty()) {
                lock.unlock();
                break;
            }

            auto runnable = task_queue.front();
            task_queue.pop();
            lock.unlock();

            runnable();
            queued_jobs_count--;
        }
    }

    //Replace wait CV with atomic wait in V3
    std::size_t current = queued_jobs_count.load(std::memory_order_acquire);
    while (current > 0) {
        queued_jobs_count.wait(current, std::memory_order_acquire);
        current = queued_jobs_count.load(std::memory_order_acquire);
    }
}

std::size_t Threadpool::get_queued_count() const noexcept {
    return queued_jobs_count.load();
}

std::size_t Threadpool::get_max_threads() const noexcept {
    return max_threads;
}

void Threadpool::process_tasks() {
    //Create a lock that the condition variable will capture
    std::unique_lock<std::mutex> lock(queue_mtx);

    //Wait until the exit flag is set by the destructor
    while (true) {

        //Sleep while there are no jobs, and no exit signal
        queue_cv.wait(lock, [this]{ return !task_queue.empty() || exit_flag.load(); });

        //If the flag is set, and the jobs are done, exit
        //TODO on destruction this waits for jobs to finish, maybe discard them on exit?
        if (exit_flag.load() && task_queue.empty()) {
            return;
        }

        //Once a job is ready to go, pop it off
        auto runnable = std::move(task_queue.front());
        task_queue.pop();
        lock.unlock();

        //Then execute the job
        runnable();

        //Flag that the queued jobs count has decreased
        if (queued_jobs_count.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            queued_jobs_count.notify_all();
        }

        lock.lock();
    }
    
}

} //End namespace fusion system