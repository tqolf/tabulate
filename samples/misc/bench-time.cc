#include <time.h>
#include <unistd.h>
#include <sys/time.h>
#include <chrono>
#include <atomic>
#include <algorithm>
#include <string>
#include <set>
#include <bitset>
#include <unordered_set>
#include <mutex>
#include <shared_mutex>
#include <roaring/roaring.h>

#include "profiler.h"
#include "sampling.h"
#include "atomic-bitset.h"

int main(int argc, char **argv)
{
    size_t repeat = 0;
    if (argc >= 2) {
        repeat = atoi(argv[1]);
    }
    size_t N = 5, NP = 6;

    profiler::SetTitle("Benchmark of Time APIs");

    profiler::Add(
        "time",
        []() {
            profiler::DoNotOptimize(time(nullptr));
            return true;
        },
        repeat, N);

    profiler::AddMultiThread(
        "time(threading)",
        []() {
            profiler::DoNotOptimize(time(nullptr));
            return true;
        },
        repeat, N, NP);

    profiler::Add(
        "clock_gettime",
        []() {
            struct timespec ts;
            profiler::DoNotOptimize(clock_gettime(CLOCK_MONOTONIC, &ts));
            return true;
        },
        repeat, N);

    profiler::AddMultiThread(
        "clock_gettime(threading)",
        []() {
            struct timespec ts;
            profiler::DoNotOptimize(clock_gettime(CLOCK_MONOTONIC, &ts));
            return true;
        },
        repeat, N, NP);

    profiler::Add(
        "clock_gettime-seconds",
        []() {
            profiler::DoNotOptimize([]() {
                struct timespec ts;
                clock_gettime(CLOCK_MONOTONIC, &ts);
                return ts.tv_sec;
            });
            return true;
        },
        repeat, N);

    profiler::AddMultiThread(
        "clock_gettime-seconds(threading)",
        []() {
            profiler::DoNotOptimize([]() {
                struct timespec ts;
                clock_gettime(CLOCK_MONOTONIC, &ts);
                return ts.tv_sec;
            });
            return true;
        },
        repeat, N, NP);

    profiler::Add(
        "gettimeofday",
        []() {
            struct timeval tv;
            profiler::DoNotOptimize(gettimeofday(&tv, nullptr));
            return true;
        },
        repeat, N);

    profiler::AddMultiThread(
        "gettimeofday(threading)",
        []() {
            struct timeval tv;
            profiler::DoNotOptimize(gettimeofday(&tv, nullptr));
            return true;
        },
        repeat, N, NP);

    profiler::Add(
        "gettimeofday-seconds",
        []() {
            profiler::DoNotOptimize([]() {
                struct timeval tv;
                gettimeofday(&tv, nullptr);
                return tv.tv_sec;
            });
            return true;
        },
        repeat, N);

    profiler::AddMultiThread(
        "gettimeofday-seconds(threading)",
        []() {
            profiler::DoNotOptimize([]() {
                struct timeval tv;
                gettimeofday(&tv, nullptr);
                return tv.tv_sec;
            });
            return true;
        },
        repeat, N, NP);

    profiler::Add(
        "localtime",
        []() {
            time_t t = time(nullptr);
            profiler::DoNotOptimize(localtime(&t));
            return true;
        },
        repeat, N);

    profiler::AddMultiThread(
        "localtime(threading)",
        []() {
            time_t t = time(nullptr);
            profiler::DoNotOptimize(localtime(&t));
            return true;
        },
        repeat, N, NP);

    profiler::Add(
        "localtime_r",
        []() {
            time_t t = time(nullptr);
            struct tm tm;
            profiler::DoNotOptimize(localtime_r(&t, &tm));
            return true;
        },
        repeat, N);

    profiler::AddMultiThread(
        "localtime_r(threading)",
        []() {
            time_t t = time(nullptr);
            struct tm tm;
            profiler::DoNotOptimize(localtime_r(&t, &tm));
            return true;
        },
        repeat, N, NP);

    profiler::Add(
        "chrono::high_resolution_clock",
        []() {
            profiler::DoNotOptimize(std::chrono::high_resolution_clock::now());
            return true;
        },
        repeat, N);

    profiler::AddMultiThread(
        "chrono::high_resolution_clock(threading)",
        []() {
            profiler::DoNotOptimize(std::chrono::high_resolution_clock::now());
            return true;
        },
        repeat, N, NP);

    profiler::Add(
        "SAMPLING_HIT_FREQEUENCY",
        []() {
            profiler::DoNotOptimize(SAMPLING_HIT_FREQEUENCY(10, 10000, 100));
            return true;
        },
        repeat, N);

    profiler::AddMultiThread(
        "SAMPLING_HIT_FREQEUENCY(threading)",
        []() {
            profiler::DoNotOptimize(SAMPLING_HIT_FREQEUENCY(10, 10000, 100));
            return true;
        },
        repeat, 5, 6);

    profiler::Add(
        "SAMPLING_HIT_FREQEUENCY_BY_KEY",
        [&]() {
            const std::string key = "key";
            profiler::DoNotOptimize(SAMPLING_HIT_FREQEUENCY_BY_KEY(key, 10, 10000, 100));
            return true;
        },
        repeat, N);

    profiler::AddMultiThread(
        "SAMPLING_HIT_FREQEUENCY_BY_KEY(threading)",
        [&]() {
            const std::string key = "key";
            profiler::DoNotOptimize(SAMPLING_HIT_FREQEUENCY_BY_KEY(key, 10, 10000, 100));
            return true;
        },
        repeat, 5, 6);

    {
        profiler::Add("nothing", []() {
            return true;
        });
        profiler::AddMultiThread("nothing(threading)", []() {
            return true;
        });

        std::bitset<1 << 24> set;
        profiler::Add("std::bitset::set", [&]() {
            for (size_t i = 0; i < 100; i++) {
                set.set(i);
            }
            for (size_t i = 10000; i < 10200; i++) {
                set.set(i);
            }

            return true;
        });

        profiler::Add("std::bitset::test", [&]() {
            size_t n = 0;
            for (size_t i = 0; i < 10200; i++) {
                n += set.test(i);
            }

            return n == 300;
        });
    }

    {
        std::set<size_t> set;
        profiler::Add("std::set::insert", [&]() {
            for (size_t i = 0; i < 100; i++) {
                set.insert(i);
            }
            for (size_t i = 10000; i < 10200; i++) {
                set.insert(i);
            }

            return true;
        });

        profiler::Add("std::set::count", [&]() {
            size_t n = 0;
            for (size_t i = 0; i < 10200; i++) {
                n += set.count(i);
            }

            return n == 300;
        });

        std::shared_mutex mutex;
        profiler::AddMultiThread("std::set::insert(threading)", [&]() {
            std::unique_lock<std::shared_mutex> lock(mutex);
            for (size_t i = 0; i < 100; i++) {
                set.insert(i);
            }
            for (size_t i = 10000; i < 10200; i++) {
                set.insert(i);
            }

            return true;
        });

        profiler::AddMultiThread("std::set::count(threading)", [&]() {
            std::shared_lock<std::shared_mutex> lock(mutex);

            size_t n = 0;
            for (size_t i = 0; i < 10200; i++) {
                n += set.count(i);
            }

            return n == 300;
        });
    }

    {
        std::unordered_set<size_t> set;
        profiler::Add("std::unordered_set::insert", [&]() {
            for (size_t i = 0; i < 100; i++) {
                set.insert(i);
            }
            for (size_t i = 10000; i < 10200; i++) {
                set.insert(i);
            }

            return true;
        });

        profiler::Add("std::unordered_set::count", [&]() {
            size_t n = 0;
            for (size_t i = 0; i < 10200; i++) {
                n += set.count(i);
            }

            return n == 300;
        });
    }

    {
        roaring_bitmap_t *r = roaring_bitmap_create();
        profiler::Add("roaring::bitmap::add", [&]() {
            for (size_t i = 0; i < 100; i++) {
                roaring_bitmap_add(r, i);
            }
            for (size_t i = 10000; i < 10200; i++) {
                roaring_bitmap_add(r, i);
            }

            return true;
        });
        profiler::Add("roaring::bitmap::contains", [&]() {
            size_t n = 0;
            for (size_t i = 0; i < 10200; i++) {
                n += roaring_bitmap_contains(r, i);
            }
            return n == 30;
        });
        roaring_bitmap_free(r);
    }

    {
        lockfree::atomic_bitset<1024> set;
        profiler::Add(
            "lockfree::atomic_bitset::set",
            [&]() {
                size_t n = 0;
                for (size_t i = 0; i < 100; i++) {
                    n += set.set(i) == lockfree::status::success;
                }
                for (size_t i = 10000; i < 10200; i++) {
                    n += set.set(i) == lockfree::status::success;
                }
                return n == 300;
            },
            repeat, 5);

        profiler::Add(
            "lockfree::atomic_bitset::test",
            [&]() {
                size_t n = 0;
                for (size_t i = 0; i < 10200; i++) {
                    n += set.test(i) == lockfree::status::yes;
                }
                if (n != 300) {
                    std::cout << "n = " << n << std::endl;
                }

                return n == 300;
            },
            repeat, 5);

        set.reset();
        profiler::AddMultiThread(
            "lockfree::atomic_bitset::set(threading)",
            [&]() {
                for (size_t i = 0; i < 100; i++) {
                    set.set(i);
                }
                for (size_t i = 10000; i < 10200; i++) {
                    set.set(i);
                }
                return true;
            },
            repeat, 5);

        profiler::AddMultiThread(
            "lockfree::atomic_bitset::test(threading)",
            [&]() {
                size_t n = 0;
                for (size_t i = 0; i < 10200; i++) {
                    n += set.test(i) == lockfree::status::yes;
                }

                return n == 300;
            },
            repeat, 5);
    }

    return 0;
}
