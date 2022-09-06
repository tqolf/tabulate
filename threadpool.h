/**
 * Copyright 2022 Kiran Nowak(kiran.nowak@gmail.com)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <queue>              // std::queue
#include <mutex>              // std::mutex, std::unique_lock
#include <atomic>             // std::atomic
#include <chrono>             // std::chrono
#include <future>             // std::future, std::promise
#include <functional>         // std::function
#include <memory>             // std::shared_ptr, std::unique_ptr
#include <thread>             // std::this_thread, std::thread
#include <utility>            // std::move, std::swap
#include <type_traits>        // std::decay_t, std::enable_if_t, std::is_void_v, std::invoke_result_t
#include <condition_variable> // std::condition_variable

#ifndef THREADPOOL_TRACE
#include <math.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>

#define THREADPOOL_TRACE(fmt, ...)                                                             \
    do {                                                                                       \
        char buff[32];                                                                         \
        struct tm tm;                                                                          \
        struct timeval tv;                                                                     \
        gettimeofday(&tv, NULL);                                                               \
        localtime_r(&tv.tv_sec, &tm);                                                          \
        auto len = strftime(buff, sizeof(buff), "%H:%M:%S", &tm);                              \
        snprintf(buff + len, sizeof(buff) - len, ".%03u", (unsigned int)(tv.tv_usec / 1000));  \
        printf("\033[2;3m%s\033[0m %s:%d " fmt "\n", buff, __FILE__, __LINE__, ##__VA_ARGS__); \
    } while (0)
#endif

#define THREADPOOL_VERSION "v1.2.0 (2022-08-31)"

namespace multiprocessing
{
/**
 * @brief A C++17 thread pool class. The user submits tasks to be executed into a queue. Whenever a thread becomes
 * available, it pops a task from the queue and executes it. Each task is automatically assigned a future, which can be
 * used to wait for the task to finish executing and/or obtain its eventual return value.
 *
 * if your threads in the thread pool are constantly fed with tasks and you need fast response time, then yield is what
 * you want, but yield will burn cpu cycles no matter what the waiting thread is doing. if not, you can use the
 * conditional approach, threads will sleep until a task is ready (note though, a conditional can wake a thread, even if
 * no ready signal was sent), the response time might be slower, but you will not burn cpu cycles.
 *
 * I would recommend the conditional approach, and if the reaction time is too slow, switch to yield.
 */

class threadpool {
  public:
    threadpool(size_t concurrency = std::thread::hardware_concurrency())
        : m_paused(false),
          m_stopped(false),
          m_running(0),
          m_submitted(0),
          m_processed(0),
          m_unfinished(0),
          m_concurrency(concurrency)
    {
        for (size_t i = 0; i < m_concurrency; i++) {
            m_workers.emplace_back(&threadpool::__worker, this, i);
        }
    }
    ~threadpool()
    {
        shutdown();
    }

    void pause()
    {
        m_paused = true;
    }

    void resume()
    {
        m_paused = false;
        m_cond_queued.notify_all();
    }

    void wait()
    {
        THREADPOOL_TRACE("Idle, Wait for running or queued tasks to be finished");
        std::unique_lock<std::mutex> latch(m_queue_lock);
        m_cond_finished.wait(latch, [this] {
            return (!m_paused && m_unfinished.load() == 0) || (m_paused && m_running.load() == 0);
        });
    }

    void shutdown()
    {
        bool stopped = false;
        if (m_stopped.compare_exchange_strong(stopped, true)) {
            m_cond_queued.notify_all();
            for (auto &worker : m_workers) {
                worker.join();
            }
            m_workers.clear();
        }
    }

    void reset(size_t concurrency = std::thread::hardware_concurrency())
    {
        shutdown();
        m_stopped = false;
        m_concurrency = concurrency;
        for (size_t i = 0; i < m_concurrency; i++) {
            m_workers.emplace_back(&threadpool::__worker, this, i);
        }
    }

    bool is_alive()
    {
        return !m_stopped;
    }

    bool is_active()
    {
        return !m_stopped && !m_paused;
    }

    size_t concurrency() const
    {
        return m_concurrency;
    }

    size_t unfinished_size() const
    {
        return m_unfinished;
    }

    size_t running_size() const
    {
        return m_running;
    }

    size_t queued_size() const
    {
        return m_unfinished - m_running;
    }

    template <typename Task>
    void push(Task &&task)
    {
        m_unfinished++;
        THREADPOOL_TRACE("unfinished: %u, running: %u", m_unfinished.load(), m_running.load());
        {
            std::unique_lock<std::mutex> latch(m_queue_lock);
            m_queued_tasks.emplace(std::forward<Task>(task));
            m_submitted++;
        }
        m_cond_queued.notify_one();
    }

    template <typename Task, typename... Args>
    void push(const Task &task, const Args &...args)
    {
        push([task, args...] {
            task(args...);
        });
    }

    template <typename T, typename... Args, void (T::*Task)(Args...)>
    void push(typename T::Task *task, const Args &...args)
    {
        push([task, args...] {
            task(args...);
        });
    }

    /**
     * @brief Submit a function with zero or more arguments and no return value into the task queue, and get an
     * std::future<bool> that will be set to true upon completion of the task.
     *
     * @tparam Task The type of the function.
     * @tparam Args The types of the zero or more arguments to pass to the function.
     * @param task The function to submit.
     * @param args The zero or more arguments to pass to the function.
     * @return A future to be used later to check if the function has finished its execution.
     */
    template <
        typename Task, typename... Args,
        typename = std::enable_if_t<std::is_void_v<std::invoke_result_t<std::decay_t<Task>, std::decay_t<Args>...>>>>
    std::future<bool> submit(const Task &task, const Args &...args)
    {
        std::shared_ptr<std::promise<bool>> promise(new std::promise<bool>);
        std::future<bool> future = promise->get_future();
        push([task, args..., promise] {
            try {
                task(args...);
                promise->set_value(true);
            } catch (...) {
                try {
                    promise->set_exception(std::current_exception());
                } catch (...) {
                }
            }
        });
        return future;
    }

    /**
     * @brief Submit a function with zero or more arguments and a return value into the task queue, and get a future for
     * its eventual returned value.
     *
     * @tparam Task The type of the function.
     * @tparam Args The types of the zero or more arguments to pass to the function.
     * @tparam Result The return type of the function.
     * @param task The function to submit.
     * @param args The zero or more arguments to pass to the function.
     * @return A future to be used later to obtain the function's returned value, waiting for it to finish its execution
     * if needed.
     */
    template <typename Task, typename... Args,
              typename Result = std::invoke_result_t<std::decay_t<Task>, std::decay_t<Args>...>,
              typename = std::enable_if_t<!std::is_void_v<Result>>>
    std::future<Result> submit(const Task &task, const Args &...args)
    {
        std::shared_ptr<std::promise<Result>> promise(new std::promise<Result>);
        std::future<Result> future = promise->get_future();
        push([task, &args..., promise] {
            try {
                promise->set_value(task(args...));
            } catch (...) {
                try {
                    promise->set_exception(std::current_exception());
                } catch (...) {
                }
            }
        });
        return future;
    }

  public:
    void __worker([[maybe_unused]] size_t id)
    {
        while (true) {
            THREADPOOL_TRACE("WORKER[%02zu]: WAIT TASK", id);
            std::unique_lock<std::mutex> latch(m_queue_lock);
            m_cond_queued.wait(latch, [this] {
                return m_stopped || (!m_paused && !m_queued_tasks.empty());
            });
            if (!m_queued_tasks.empty()) {
                auto task = m_queued_tasks.front();
                m_queued_tasks.pop();
                latch.unlock();

                [[maybe_unused]] auto taskid = m_processed.load();
                THREADPOOL_TRACE("WORKER[%02zu]: TASK[%u] POPPED", id, taskid);
                m_running++, task(), m_running--, m_unfinished--, m_processed++;
                THREADPOOL_TRACE("WORKER[%02zu]: TASK[%u] FINISHED", id, taskid);

                latch.lock();
                m_cond_finished.notify_one();
                latch.unlock();

                THREADPOOL_TRACE("WORKER[%02zu]: Unfinished=%u, Running=%u", id, m_unfinished.load(), m_running.load());
            } else if (m_stopped) {
                THREADPOOL_TRACE("WORKER[%02zu]: EXIT", id);
                return;
            }
        }
    }

    /**
     * An atomic variable indicating to the workers to pause.
     *
     * When set to true, the workers temporarily stop popping new tasks out of the queue,
     * although any tasks already executed will keep running until they are done.
     *
     * Set to false again to resume popping tasks.
     */
    std::atomic<bool> m_paused;

    /**
     * An atomic variable indicating to the workers to keep running.
     *
     * When set to true, the workers permanently stop working.
     */
    std::atomic<bool> m_stopped;

    // statistics
    std::atomic_uint m_running;    //!> number of running tasks
    std::atomic_uint m_submitted;  //!> number of tasks that has been submitted
    std::atomic_uint m_processed;  //!> number of tasks that has been processed
    std::atomic_uint m_unfinished; //!> number of tasks that not finished, queued or running

    // worker pool
    size_t m_concurrency;               //!> number of threads
    std::vector<std::thread> m_workers; //!> list of worker threads

    // scheduler queue
    std::mutex m_queue_lock;                 //!> mutex to synchronize access to the task queue by different threads.
    std::condition_variable m_cond_queued;   //!> condition variable to notify after a task been pushed to task queue
    std::condition_variable m_cond_finished; //!> condition variable to notify after a task has run over
    std::queue<std::function<void()>> m_queued_tasks; //!> queue of tasks to be executed by the threads
};
} // namespace multiprocessing
