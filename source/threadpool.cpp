
#include "threadpool.hpp"

namespace FusionSystem {

Threadpool::Threadpool (std::size_t thread_count) {
    max_threads = (std::thread::hardware_concurrency() < thread_count) ? std::thread::hardware_concurrency() : thread_count;
    queued_jobs_count.store(0);
    exit_flag.store(false);

    //Create the threads, attaching them to the process tasks job
    for (std::size_t i = 0; i < max_threads; i++) {
        threads.push_back(std::thread([this](){ process_tasks(); }));
    }
}

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

void Threadpool::attach_to_process_context() {
    std::unique_lock<std::mutex> lock(queue_mtx, std::defer_lock);
    while (true) {

        lock.lock();
        if (task_queue.empty()) {
            lock.unlock();
            return;
        }

        auto runnable = task_queue.front();
        task_queue.pop();
        queued_jobs_count--;
        lock.unlock();

        runnable();
    }
}

std::size_t Threadpool::get_queued_count() const noexcept {
    return queued_jobs_count.load();
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
        queued_jobs_count--;
        lock.unlock();

        //Then execute the job
        runnable();
        lock.lock();
    }
    
}

} //End namespace fusion system