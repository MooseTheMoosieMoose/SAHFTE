/**                                                      
 *  ,---.    ,---.  ,--.  ,--.,------.,--------.,------. 
 * '   .-'  /  O  \ |  '--'  ||  .---''--.  .--'|  .---' 
 * `.  `-. |  .-.  ||  .--.  ||  `--,    |  |   |  `--,  
 * .-'    ||  | |  ||  |  |  ||  |`      |  |   |  `---. 
 * `-----' `--' `--'`--'  `--'`--'       `--'   `------'                                                      
 * 
 * SAHFTE (Spatial Algorithmic Hashing Fusion Time-sliced Engine)
 * @file threadpool.hpp
 * @author Moose Abou-Harb
 * @brief this file  contains the headers and necessary templates for working with the threadpool system that
 * SAHFTE uses, care should be taken while editing this file as its arguably the most complex part with its templates
 * @copyright `26, Moose Abou-Harb under the 3-Clause BSD Lisence
 */

#pragma once

#include <thread>             //Provides execution threads to manage, (it aint a threadpool cause its a bobbin)
#include <mutex>              //Provides a locking mechanism to protect various objects
#include <queue>              //Provides a FIFO optimized structure to push tasks onto
#include <vector>             //A nice easy container to store the threads we are managing
#include <functional>         //A heavy yet highly generic wrapper around callables
#include <atomic>             //Gets us the `std::atomic` wrapper for atomic objects
#include <condition_variable> //Gets us condition variables to prevent pointless spinning when tasks are processed
#include <concepts>           //Helps us restrict the input to some templates

namespace FusionSystem {

/**
 * @brief a basic threadpool for tasks that require args, but are void return typed. Uses a fork-join model mainly to safely
 * manage tasks that require mapping over containers
 * @note if `std::thread::hardware_concurancy` < `thread_count`, then the threadpool will only
 * use `std::thread::hardware_concurancy` threads
 * @note due to STD limitations on std::functional, this pool cannot manage tasks that required move-only
 * parameters
 */
class Threadpool {
public:
/*=====================================================================================================
                             Constructors, Destructors and the Big 5
=====================================================================================================*/
    /**
     * @brief normal constructor for a threadpool
     */
    Threadpool ();

    /**
     * @brief the base destructor handles signaling to child threads that the process should end
     */
    ~Threadpool();

    /**
     * @brief due to the objects that `Threadpool` manages, this is not safe to be copy constructed
     */
    Threadpool (const Threadpool& other) = delete;

    /**
     * @brief due to the objects that `Threadpool` manages, this is not safe to be copy assignment constructed
     */
    Threadpool& operator=(const Threadpool& other) = delete;

    /**
     * @brief due to the objects that `Threadpool` manages, this is not safe to be move constructed
     */
    Threadpool (Threadpool&& other) = delete;

    /**
     * @brief due to the objects that `Threadpool` manages, this is not safe to be move assignment constructed
     */
    Threadpool& operator=(Threadpool&& other) = delete;

/*=====================================================================================================
                                     Interface Functions and Templates
=====================================================================================================*/

    /**
     * @brief spins up the threads that the pool will use, this should only be called once!
     * @param thread_count the number of auxillary threads to use in the pool. You should note that when using the mapping
     * functions, best practice is to split it along `get_max_threads() + 1` jobs since the caller thread doesnt count as
     * a thread in the pool
     * @note this is necessarily not a part of the constructor due to the wacky lifetimes this object manages, this must be
     * called before other tasks are pushed, otherwise nothing will happen
     */
    void initilize_threads(std::size_t thread_count);

    /**
     * @brief given some container that supports `begin()`, `end()`, map some task across the elements inside `container`,
     * split as `jobs` tasks. Will call `callable` with `callable(T item, std::size_t indx, args...)`
     * @param container some object that satisfies the requirements of `std::ranges::random_access_iterator`
     * @param callable an object that is callable under `args...`, this can be a lambda, a named function, an overload of
     * operator(), anything so long as it is `void` returning
     * @param args a variable sized parameter list that are the requested args for `callable`, this MUST match the args needed
     * for callable, otherwise you will get some *nasty* compiler errors
     * @note necessarily blocking to prevent pointer invalidation
     * @note this specialization is for random access iterators, such as std::vector, std::string, etc
     */
    template <typename Container, typename Func, typename... Args>
    requires std::ranges::random_access_range<Container>
    void queue_and_map_task(Container& container, std::size_t jobs, Func&& callable, Args&&... args) {
        //Get the number of elements in the container
        const std::size_t element_count = container.size();
        //If there are more jobs than elements, then we would be wasting jobs, clip it down for a 1-1 matching
        if (jobs > element_count) {
            jobs = element_count;
        }

        //Calculate the number of elements per_job
        const std::size_t elem_per_job = element_count / jobs;

        //Create each job
        auto lambda_iter_begin = container.begin();

        //Keep track of the index within each task so that it can be passed to the function
        std::size_t indx = 0;

        for (std::size_t i = 0; i < jobs; i++) {
            auto lambda_iter_end = std::next(lambda_iter_begin, elem_per_job);
            if (i == jobs - 1 || lambda_iter_end > container.end()) {
                lambda_iter_end = container.end();
            }

            //Enforce a seperate scope for the lock on the task queue
            {
                std::lock_guard<std::mutex> lock(queue_mtx);
                task_queue.push([&, begin = lambda_iter_begin, end = lambda_iter_end, i = indx]() mutable {
                    auto self_iter = begin;
                    while (self_iter != end) {
                        std::invoke(callable, *self_iter, i, args...);
                        std::advance(self_iter, 1);
                        i++;
                    }
                });
            }

            //Advance the iterator
            indx += std::distance(lambda_iter_begin, lambda_iter_end);
            lambda_iter_begin = lambda_iter_end;
            
        }

        //Increment the count on the queued jobs
        queued_jobs_count += jobs;

        //Wait until all jobs are pushed and awake all
        queue_cv.notify_all();

        //We must wait to prevent pointer invalidation
        join_and_process();
    }

    /**
     * @brief given some container that supports `begin()`, `end()`, map some task across the elements inside `container`,
     * split as `jobs` tasks. Will call `callable` with `callable(T item, args...)`
     * @param container some object that satisfies the requirements of `std::forward_iterator`
     * @param callable an object that is callable under `args...`, this can be a lambda, a named function, an overload of
     * operator(), anything so long as it is `void` returning
     * @param args a variable sized parameter list that are the requested args for `callable`, this MUST match the args needed
     * for callable, otherwise you will get some *nasty* compiler errors
     * @note necessarily blocking to prevent pointer invalidation
     * @note this specialization is for forward_iterators, which is just about as basic as it gets, allowing you to apply this with
     * things like `std::map`, `std::unordered_map`, etc. However, this will probably be slower than the specialization for
     * random access iterators
     * @note this specialization does not give you access to a container index, as objects with this iterator trait are not garunteed
     * to have a meaningful equivalent to an index
     */
    template <typename Container, typename Func, typename... Args>
    requires std::ranges::forward_range<Container>
    void queue_and_map_task(Container& container, std::size_t jobs, Func&& callable, Args&&... args) {
        //Get the number of items, clip them agains the number of jobs if necessary
        const std::size_t element_count = container.size();
        if (jobs > element_count) {
            jobs = element_count;
        }

        //Keep track of an iter protected by a mutex, so that queued jobs can poll this
        auto begin_iter = container.begin();
        auto item_iter = container.begin();
        auto end_iter = container.end();
        std::mutex container_iter_mtx {};

        //Create each job, allow them to lock onto an element and pass it to their callable
        for (std::size_t i = 0; i < jobs; i++) {

            //Add in a task, enforcing smallest scope on lock
            {
                //With V3 we no longer operate on a per item basis and instead read
                //and process chunks of values, hopefully then we have less lock\
                //contention
                std::lock_guard<std::mutex> lock(queue_mtx);
                task_queue.push([&]() mutable {
                    while (true) {
                        auto chunk_start_iter = end_iter;
                        auto chunk_end_iter = end_iter;

                        //Lock and steal a range of items
                        {
                            std::lock_guard<std::mutex> lock(container_iter_mtx);
                            
                            //If we have hit the end exit
                            if (item_iter == end_iter) {
                                break;
                            }

                            //The chunk we will pop will start at the beggining
                            chunk_start_iter = item_iter;

                            //And go to the end, or 16 items ahead
                            const std::size_t chunk_size = 64;
                            for (std::size_t adv = 0; (adv < chunk_size) && (item_iter != end_iter); adv++) {
                                std::advance(item_iter, 1);
                            }
                            chunk_end_iter = item_iter;

                            //Now that we have advnaced a range of items we can process the range
                            for (auto it = chunk_start_iter; it != chunk_end_iter; std::advance(it, 1)) {
                                std::invoke(callable, *it, args...);
                            }
                        }
                    }
                    
                });
            }

            //As of v3 CV flagging is moved to outside the loop

        }

        queued_jobs_count += jobs;

        queue_cv.notify_all();

        //Once all jobs are created, wait for the pool to finish
        join_and_process();
        
    }

    /**
     * @brief attaches the calling thread as a thread running the tasks queued in the pool, is blocking
     * until all queued jobs at the time of calling are finished
     * @note sort of analogous to `join()` in other libraries, but instead of sitting around, the joining 
     * thread helps out in the pool
     * @note this is called for you when using `queue_and_map_tasks`
     */
    void join_and_process();

    /**
     * @brief gets the number of queued elements, which is internally stored as an `atomic<std::size_t>`,
     * this number is understandably volatile, especially if being read while enqueing tasks, or when
     * tasks are being processed
     * @return the number of tasks queued in the pool
     */
    std::size_t get_queued_count() const noexcept;

    /**
     * @brief returns the maximum number of threads that can be in use directly by the pool at once
     * @return the max number of threads, the same number as is passed when calling `initilize_threads()`
     */
    std::size_t get_max_threads() const noexcept;


private:
/*=====================================================================================================
                                            Class Members
=====================================================================================================*/
    static const std::size_t CACHE_LINE_SIZE = 64;

    //The total number of threads in the pool, essentially a cached value for `threads.size()`
    std::size_t max_threads;

    //Threads holds all the threads managed by the pool
    std::vector<std::thread> threads {};

    //The task queue which the threadpool will pop off
    std::queue<std::function<void()>> task_queue {};

    //Count the number of tasks queued
    alignas(CACHE_LINE_SIZE) std::atomic<std::size_t> queued_jobs_count;

    //The mutex which protects the task_queue
    alignas(CACHE_LINE_SIZE) std::mutex queue_mtx {};

    //The cv that waits the threads for tasks to be queued
    std::condition_variable queue_cv {};

    //Exit flag is set true when the child threads are ready to rejoin
    std::atomic<bool> exit_flag;

/*=====================================================================================================
                                    Private Utility Functions
=====================================================================================================*/
    /**
     * @brief the function that is attached to every thread that is managed by the pool. This function handles
     * exit signals, and flagging so that they should sleep when not in use, and wake up when items are queued.
     */
    void process_tasks();
};

} // end namespace fusion system