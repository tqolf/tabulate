#pragma once

#include <time.h>
#include <sys/time.h>
#include "atomic-hashmap.h"

template <size_t n, size_t N, typename Timer, decltype(Timer()()) T, typename Key, size_t Capacity, size_t XE = 5>
class Sampling {
  public:
    Sampling &Instance()
    {
        static Sampling instance;
        return instance;
    }
    bool Hit(const Key &key)
    {
        decltype(Timer()()) timenow = Timer()();
        const auto &[it, inserted] = samples.get_or_emplace(key);
        if (ATOMIC_HASHMAP_LIKELY(it != samples.end())) {
            if (it->val.reset_time < timenow - T) {
                it->val.counter = 0;
                it->val.reset_time = timenow;
            }
            return it->val.counter++ % modular == 0;
        } else {
            if (onerror.reset_time < timenow - T) {
                onerror.counter = 0;
                onerror.reset_time = timenow;
            }
            return (XE * onerror.counter++) / modular == 0;
        }
    }

    Sampling() = default;

  private:
    struct sampling_status {
        std::atomic<size_t> counter;
        std::atomic<decltype(Timer()())> reset_time;
        sampling_status() : counter(0), reset_time(0) {}
    };
    static auto constexpr modular = N > n ? (N / n) : 1;

    sampling_status onerror;
    lockfree::atomic_hashmap<Capacity, Key, sampling_status> samples;
};

template <size_t n, size_t N, typename Timer, decltype(Timer()()) T>
class Sampling<n, N, Timer, T, void, 0> {
  public:
    Sampling &Instance()
    {
        static Sampling instance;
        return instance;
    }
    bool Hit()
    {
        auto timenow = Timer()();
        if (reset_time < timenow - T) {
            counter = 0;
            reset_time = timenow;
        }
        return counter++ / modular == 0;
    }

    Sampling() = default;

  private:
    std::atomic<size_t> counter;
    std::atomic<decltype(Timer()())> reset_time;

    static auto constexpr modular = N > n ? (N / n) : 1;
};

struct Timer_seconds {
    auto operator()()
    {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        return tv.tv_sec;
    }
};

struct Timer_millseconds {
    auto operator()()
    {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        return (unsigned long long)(tv.tv_sec * 1000) + tv.tv_usec / 1000;
    }
};

#define SAMPLING_HIT_FREQEUENCY(n, N, T)                               \
    ATOMIC_HASHMAP_UNLIKELY(({                                         \
        static Sampling<n, N, Timer_millseconds, T, void, 0> instance; \
        instance.Hit();                                                \
    }))

#define SAMPLING_HIT_FREQEUENCY_BY_KEY(key, n, N, T)                                                 \
    ATOMIC_HASHMAP_UNLIKELY(({                                                                       \
        static Sampling<n, N, Timer_millseconds, T, std::decay<decltype(key)>::type, 1024> instance; \
        instance.Hit(key);                                                                           \
    }))
