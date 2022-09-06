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

#include <fstream>
#include <iomanip>
#include <random>
#include <string>
#include <vector>
#include <iostream>
#include <set>
#include <map>
#include <mutex>

class debuger {
  public:
    debuger(std::ostream &_os = std::cout) : os(_os){};

    template <typename T, typename... Args>
    void print(const T &first, const Args &...args)
    {
        const std::scoped_lock __(lock);
        os << first;
        if (sizeof...(Args) > 0) {
            print(" ", args...);
        }
    }

    template <typename... T>
    void println(const T &...items)
    {
        print(items..., '\n');
    }

  private:
    std::ostream &os;
    mutable std::mutex lock;
};

debuger dbg;
// #define THREADPOOL_TRACE dbg.println

#include "threadpool.h"
using namespace multiprocessing;

/**
 * @brief A helper class to synchronize printing to an output stream by different threads.
 */
class synced_stream {
  public:
    /**
     * @brief Construct a new synced stream.
     *
     * @param _out_stream The output stream to print to. The default value is std::cout.
     */
    synced_stream(std::ostream &_out_stream = std::cout) : out_stream(_out_stream){};

    /**
     * @brief Print any number of items into the output stream. Ensures that no other threads print to this stream
     * simultaneously, as long as they all exclusively use this synced_stream object to print.
     *
     * @tparam T The types of the items
     * @param items The items to print.
     */
    template <typename... T>
    void print(const T &...items)
    {
        const std::scoped_lock lock(stream_mutex);
        (out_stream << ... << items);
    }

    /**
     * @brief Print any number of items into the output stream, followed by a newline character. Ensures that no other
     * threads print to this stream simultaneously, as long as they all exclusively use this synced_stream object to
     * print.
     *
     * @tparam T The types of the items
     * @param items The items to print.
     */
    template <typename... T>
    void println(const T &...items)
    {
        print(items..., '\n');
    }

  private:
    /**
     * @brief A mutex to synchronize printing.
     */
    mutable std::mutex stream_mutex = {};

    /**
     * @brief The output stream to print to.
     */
    std::ostream &out_stream;
};

//                                    End class synced_stream                                    //
// ============================================================================================= //

// ============================================================================================= //
//                                       Begin class timer                                       //

/**
 * @brief A helper class to measure execution time for benchmarking purposes.
 */
class timer {
    typedef std::int_fast64_t i64;

  public:
    /**
     * @brief Start (or restart) measuring time.
     */
    void start()
    {
        start_time = std::chrono::steady_clock::now();
    }

    /**
     * @brief Stop measuring time and store the elapsed time since start().
     */
    void stop()
    {
        elapsed_time = std::chrono::steady_clock::now() - start_time;
    }

    /**
     * @brief Get the number of milliseconds that have elapsed between start() and stop().
     *
     * @return The number of milliseconds.
     */
    i64 ms() const
    {
        return (std::chrono::duration_cast<std::chrono::milliseconds>(elapsed_time)).count();
    }

  private:
    /**
     * @brief The time point when measuring started.
     */
    std::chrono::time_point<std::chrono::steady_clock> start_time = std::chrono::steady_clock::now();

    /**
     * @brief The duration that has elapsed between start() and stop().
     */
    std::chrono::duration<double> elapsed_time = std::chrono::duration<double>::zero();
};

// Define short names for commonly-used integer types.
typedef std::int_fast32_t i32;
typedef std::uint_fast32_t ui32;
typedef std::int_fast64_t i64;
typedef std::uint_fast64_t ui64;

// Define two global synced_streams objects: one prints to std::cout and the other to a file.
synced_stream sync_cout(std::cout);
std::ofstream log_file;
synced_stream sync_file(log_file);

// A global thread pool object.
threadpool pool;

// A global random_device object used to seed some random number generators.
std::random_device rd;

// Global variables to measure how many checks succeeded and how many failed.
ui32 tests_succeeded = 0;
ui32 tests_failed = 0;

/**
 * @brief Print any number of items into both std::cout and the log file, syncing both independently.
 *
 * @tparam T The types of the items.
 * @param items The items to print.
 */
template <typename... T>
void dual_print(const T &...items)
{
    sync_cout.print(items...);
    sync_file.print(items...);
}

/**
 * @brief Print any number of items into both std::cout and the log file, followed by a newline character, syncing both
 * independently.
 *
 * @tparam T The types of the items.
 * @param items The items to print.
 */
template <typename... T>
void dual_println(const T &...items)
{
    dual_print(items..., '\n');
}

/**
 * @brief Print a stylized header.
 *
 * @param text The text of the header. Will appear between two lines.
 * @param symbol The symbol to use for the lines. Default is '='.
 */
void print_header(const std::string &text, const char &symbol = '=')
{
    dual_println();
    dual_println(std::string(text.length(), symbol));
    dual_println(text);
    dual_println(std::string(text.length(), symbol));
}

/**
 * @brief Get a string representing the current time.
 *
 * @return The string.
 */
std::string get_time()
{
    const std::time_t t = std::time(nullptr);
    char time_string[32];
    std::strftime(time_string, sizeof(time_string), "%Y-%m-%d_%H.%M.%S", std::localtime(&t));
    return std::string(time_string);
}

/**
 * @brief Check if a condition is met, report the result, and count the number of successes and failures.
 *
 * @param condition The condition to check.
 */
void check(const bool condition)
{
    if (condition) {
        dual_println("-> PASSED!");
        tests_succeeded++;
    } else {
        dual_println("-> FAILED!");
        tests_failed++;
    }
}

/**
 * @brief Count the number of unique threads in the thread pool to ensure that the correct number of individual threads
 * was created. Pushes a number of tasks equal to four times the thread count into the thread pool, and count the number
 * of unique thread IDs returned by the tasks.
 */
ui32 count_unique_threads()
{
    std::mutex lock;
    std::atomic<size_t> excutes = 0;
    std::map<std::thread::id, size_t> thread_IDs;

    for (size_t i = 0; i < pool.concurrency() * 4; i++) {
        pool.push([&]() {
            lock.lock();
            thread_IDs[std::this_thread::get_id()]++;
            lock.unlock();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            excutes++;
        });
    }
    pool.wait();

    return thread_IDs.size();
}

/**
 * @brief Check that the constructor works.
 */
void check_constructor()
{
    dual_println("Checking that the thread pool reports a number of threads equal to the hardware concurrency...");
    check(pool.concurrency() == std::thread::hardware_concurrency());
    dual_println(
        "Checking that the manually counted number of unique thread IDs is equal to the reported number of threads...");
    auto unique_threads = count_unique_threads();
    dual_println("unfinished: ", pool.unfinished_size(), ", running: ", pool.running_size(),
                 ", queued: ", pool.queued_size(), ", thread-count: ", pool.concurrency(),
                 ", unique-threds: ", unique_threads);
    check(pool.concurrency() == unique_threads);
}

/**
 * @brief Check that reset() works.
 */
void check_reset()
{
    pool.reset(std::thread::hardware_concurrency() / 2);
    dual_println("Checking that after reset() the thread pool reports a number of threads equal to half the hardware "
                 "concurrency...");
    check(pool.concurrency() == std::thread::hardware_concurrency() / 2);
    dual_println("Checking that after reset() the manually counted number of unique thread IDs is equal to the "
                 "reported number of threads...");
    check(pool.concurrency() == count_unique_threads());
    dual_println("before reset");
    pool.reset(std::thread::hardware_concurrency());
    dual_println("Checking that after a second reset() the thread pool reports a number of threads equal to the "
                 "hardware concurrency...");
    check(pool.concurrency() == std::thread::hardware_concurrency());
    dual_println("Checking that after a second reset() the manually counted number of unique thread IDs is equal to "
                 "the reported number of threads...");
    check(pool.concurrency() == count_unique_threads());
}

/**
 * @brief Check that push() works.
 */
void check_push_task()
{
    dual_println("Checking that push() works for a function with no arguments or return value...");
    {
        bool flag = false;
        pool.push([&flag] {
            flag = true;
        });
        pool.wait();
        check(flag);
    }
    dual_println("Checking that push() works for a function with one argument and no return value...");
    {
        bool flag = false;
        pool.push(
            [](bool *flag) {
                *flag = true;
            },
            &flag);
        pool.wait();
        check(flag);
    }
    dual_println("Checking that push() works for a function with two arguments and no return value...");
    {
        bool flag1 = false;
        bool flag2 = false;
        pool.push(
            [](bool *flag1, bool *flag2) {
                *flag1 = *flag2 = true;
            },
            &flag1, &flag2);
        pool.wait();
        check(flag1 && flag2);
    }
}

/**
 * @brief Check that submit() works.
 */
void check_submit()
{
    dual_println("Checking that submit() works for a function with no arguments or return value...");
    {
        bool flag = false;
        auto my_future = pool.submit([&flag] {
            flag = true;
        });
        check(my_future.get() && flag);
    }
    dual_println("Checking that submit() works for a function with one argument and no return value...");
    {
        bool flag = false;
        auto my_future = pool.submit(
            [](bool *flag) {
                *flag = true;
            },
            &flag);
        check(my_future.get() && flag);
    }
    dual_println("Checking that submit() works for a function with two arguments and no return value...");
    {
        bool flag1 = false;
        bool flag2 = false;
        auto my_future = pool.submit(
            [](bool *flag1, bool *flag2) {
                *flag1 = *flag2 = true;
            },
            &flag1, &flag2);
        check(my_future.get() && flag1 && flag2);
    }
    dual_println("Checking that submit() works for a function with no arguments and a return value...");
    {
        bool flag = false;
        auto my_future = pool.submit([&flag] {
            flag = true;
            return 42;
        });
        check(my_future.get() == 42 && flag);
    }
    dual_println("Checking that submit() works for a function with one argument and a return value...");
    {
        bool flag = false;
        auto my_future = pool.submit(
            [](bool *flag) {
                *flag = true;
                return 42;
            },
            &flag);
        check(my_future.get() == 42 && flag);
    }
    dual_println("Checking that submit() works for a function with two arguments and a return value...");
    {
        bool flag1 = false;
        bool flag2 = false;
        auto my_future = pool.submit(
            [](bool *flag1, bool *flag2) {
                *flag1 = *flag2 = true;
                return 42;
            },
            &flag1, &flag2);
        check(my_future.get() == 42 && flag1 && flag2);
    }
}

/**
 * @brief Check that wait() works.
 */
void check_wait_for_tasks()
{
    ui32 n = pool.concurrency() * 10;
    std::vector<std::atomic<bool>> flags(n);
    for (ui32 i = 0; i < n; i++)
        pool.push([&flags, i] {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            flags[i] = true;
        });
    pool.wait();
    bool all_flags = true;
    for (ui32 i = 0; i < n; i++) all_flags = all_flags && flags[i];
    check(all_flags);
}

/**
 * @brief Check that task monitoring works.
 */
void check_task_monitoring()
{
    ui32 n = std::min<ui32>(std::thread::hardware_concurrency(), 4);
    dual_println("Resetting pool to ", n, " threads.");
    pool.reset(n);
    dual_println("Submitting ", n * 3, " tasks.");
    std::vector<std::atomic<bool>> release(n * 3);
    for (ui32 i = 0; i < n * 3; i++) {
        pool.push([&release, i] {
            while (!release[i]) std::this_thread::yield();
            dual_println("Task ", i, " released.");
        });
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    dual_println("After submission, should have: ", n * 3, " tasks total, ", n, " tasks running, ", n * 2,
                 " tasks queued...");
    check(pool.unfinished_size() == n * 3 && pool.running_size() == n && pool.queued_size() == n * 2);
    for (ui32 i = 0; i < n; i++) release[i] = true;
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    dual_println("After releasing ", n, " tasks, should have: ", n * 2, " tasks total, ", n, " tasks running, ", n,
                 " tasks queued...");
    for (ui32 i = n; i < n * 2; i++) release[i] = true;
    check(pool.unfinished_size() == n * 2 && pool.running_size() == n && pool.queued_size() == n);
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    dual_println("After releasing ", n, " more tasks, should have: ", n, " tasks total, ", n, " tasks running, ", 0,
                 " tasks queued...");
    check(pool.unfinished_size() == n && pool.running_size() == n && pool.queued_size() == 0);
    for (ui32 i = n * 2; i < n * 3; i++) release[i] = true;
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    dual_println("After releasing the final ", n, " tasks, should have: ", 0, " tasks total, ", 0, " tasks running, ",
                 0, " tasks queued...");
    check(pool.unfinished_size() == 0 && pool.running_size() == 0 && pool.queued_size() == 0);
    dual_println("Resetting pool to ", std::thread::hardware_concurrency(), " threads.");
    pool.reset(std::thread::hardware_concurrency());
}

/**
 * @brief Check that pausing works.
 */
void check_pausing()
{
    ui32 n = std::min<ui32>(std::thread::hardware_concurrency(), 4);
    dual_println("Resetting pool to ", n, " threads.");
    pool.reset(n);
    dual_println("Pausing pool.");
    pool.pause();
    dual_println("Submitting ", n * 3, " tasks, each one waiting for 200ms.");
    for (ui32 i = 0; i < n * 3; i++) {
        pool.push([i] {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            dual_println("Task ", i, " done.");
        });
    }

    dual_println("Immediately after submission, should have: ", n * 3, " tasks total, ", 0, " tasks running, ", n * 3,
                 " tasks queued...");
    check(pool.unfinished_size() == n * 3 && pool.running_size() == 0 && pool.queued_size() == n * 3);
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    dual_println("300ms later, should still have: ", n * 3, " tasks total, ", 0, " tasks running, ", n * 3,
                 " tasks queued...");
    check(pool.unfinished_size() == n * 3 && pool.running_size() == 0 && pool.queued_size() == n * 3);
    dual_println("Unpausing pool.");
    pool.resume();
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    dual_println("300ms later, should have: ", n * 2, " tasks total, ", n, " tasks running, ", n, " tasks queued...");
    check(pool.unfinished_size() == n * 2 && pool.running_size() == n && pool.queued_size() == n);
    dual_println("Pausing pool and using wait() to wait for the running tasks.");
    pool.pause();
    pool.wait();
    dual_println("After waiting, should have: ", n, " tasks total, ", 0, " tasks running, ", n, " tasks queued...");
    check(pool.unfinished_size() == n && pool.running_size() == 0 && pool.queued_size() == n);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    dual_println("200ms later, should still have: ", n, " tasks total, ", 0, " tasks running, ", n, " tasks queued...");
    check(pool.unfinished_size() == n && pool.running_size() == 0 && pool.queued_size() == n);
    dual_println("Unpausing pool and using wait() to wait for all tasks.");
    pool.resume();
    pool.wait();
    dual_println("After waiting, should have: ", 0, " tasks total, ", 0, " tasks running, ", 0, " tasks queued...");
    check(pool.unfinished_size() == 0 && pool.running_size() == 0 && pool.queued_size() == 0);
    dual_println("Resetting pool to ", std::thread::hardware_concurrency(), " threads.");
    pool.reset(std::thread::hardware_concurrency());
}

/**
 * @brief Check that exception handling work.
 */
void check_exceptions()
{
    bool caught = false;
    auto my_future = pool.submit([] {
        throw std::runtime_error("Exception thrown!");
    });
    try {
        my_future.get();
    } catch (const std::exception &e) {
        if (e.what() == std::string("Exception thrown!")) caught = true;
    }
    check(caught);
}

/**
 * @brief Print the timing of a specific test.
 *
 * @param num_tasks The number of tasks.
 * @param mean_sd std::pair containing the mean as the first member and standard deviation as the second member.
 */
void print_timing(const ui32 &num_tasks, const std::pair<double, double> &mean_sd)
{
    if (num_tasks == 1)
        dual_print("With   1  task");
    else
        dual_print("With ", std::setw(3), num_tasks, " tasks");
    dual_println(", mean execution time was ", std::setw(6), mean_sd.first, " ms with standard deviation ",
                 std::setw(4), mean_sd.second, " ms.");
}

/**
 * @brief Calculate and print the speedup obtained by multithreading.
 *
 * @param timings A vector of the timings corresponding to different numbers of tasks.
 * @return The maximum speedup obtained.
 */
double print_speedup(const std::vector<double> &timings)
{
    const auto [min_time, max_time] = std::minmax_element(std::begin(timings), std::end(timings));
    double max_speedup = *max_time / *min_time;
    dual_println("Maximum speedup obtained: ", max_speedup, "x.");
    return max_speedup;
}

/**
 * @brief Calculate the mean and standard deviation of a set of integers.
 *
 * @param timings The integers.
 * @return std::pair containing the mean as the first member and standard deviation as the second member.
 */
std::pair<double, double> analyze(const std::vector<i64> &timings)
{
    double mean = 0;
    for (size_t i = 0; i < timings.size(); i++) mean += (double)timings[i] / (double)timings.size();
    double variance = 0;
    for (size_t i = 0; i < timings.size(); i++)
        variance += ((double)timings[i] - mean) * ((double)timings[i] - mean) / (double)timings.size();
    double sd = std::sqrt(variance);
    return std::pair(mean, sd);
}

int main()
{
    std::string log_filename = "threadpool_test-" + get_time() + ".log";
    log_file.open(log_filename);

    dual_println("A C++17 Thread Pool for High-Performance Scientific Computing");
    dual_println("GitHub: https://github.com/vxfury/devkit\n");

    dual_println("Thread pool library version is ", THREADPOOL_VERSION, ".");
    dual_println("Hardware concurrency is ", std::thread::hardware_concurrency(), ".");
    dual_println("Generating log file: ", log_filename, ".\n");

    dual_println("Important: Please do not run any other applications, especially multithreaded applications, in "
                 "parallel with this test!");

    print_header("Checking that the constructor works:");
    check_constructor();

    print_header("Checking that reset() works:");
    check_reset();

    print_header("Checking that push() works:");
    check_push_task();

    print_header("Checking that submit() works:");
    check_submit();

    print_header("Checking that wait() works...");
    check_wait_for_tasks();

    print_header("Checking that task monitoring works:");
    check_task_monitoring();

    print_header("Checking that pausing works:");
    check_pausing();

    print_header("Checking that exception handling works:");
    check_exceptions();

    if (tests_failed == 0) {
        print_header("SUCCESS: Passed all " + std::to_string(tests_succeeded) + " checks!", '+');
    } else {
        print_header("FAILURE: Passed " + std::to_string(tests_succeeded) + " checks, but failed "
                         + std::to_string(tests_failed) + "!",
                     '+');
        dual_println("\nPlease submit a bug report at https://github.com/vxfury/devkit/issues including the "
                     "exact specifications of your system (OS, CPU, compiler, etc.) and the generated log file.");
    }

    return 0;
}
