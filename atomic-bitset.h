#pragma once

#include <stddef.h>
#include <atomic>

#define ATOMIC_BITSET_TRACE(...) // printf("%s:%d ", __FILE__, __LINE__), printf(__VA_ARGS__), printf("\n")

namespace lockfree
{
#if defined(__GNUC__) || defined(__clang__)
#define ATOMIC_BITSET_LIKELY(x)   (__builtin_expect((x), 1))
#define ATOMIC_BITSET_UNLIKELY(x) (__builtin_expect((x), 0))
#define ATOMIC_BITSET_ALIGNED(x)  __attribute__((aligned(x)))
#else
#define ATOMIC_BITSET_LIKELY(x)   (x)
#define ATOMIC_BITSET_UNLIKELY(x) (x)
#define ATOMIC_BITSET_ALIGNED(x)
#endif

enum class status { success, exceed, yes, no };

template <size_t Capacity, size_t BucketWidth = 16, typename BucketWord = unsigned long, size_t MaxTries = 32,
          typename Index = size_t>
class atomic_bitset {
  public:
    atomic_bitset()
    {
        for (size_t i = 0; i < Capacity; i++) {
            elements[i].store(nullptr, std::memory_order_relaxed);
        }
    }

    status set(Index pos)
    {
        size_t tries = 0;
        size_t cardinality, bucket, index, offset;
        cardinality = relocate(pos, bucket, index, offset);

        BitsetBucket *newelt = nullptr;
        while (tries < MaxTries) {
            // ATOMIC_BITSET_TRACE("pos = %zu, bucket = %zu, index = %zu, offset = %zu", pos, bucket, index, offset);

            auto elt = elements[bucket].load(std::memory_order_relaxed);
            if (ATOMIC_BITSET_UNLIKELY(elt == nullptr)) {
                if (newelt == nullptr) {
                    newelt = new BitsetBucket(cardinality, index, offset);
                } else {
                    newelt->set(index, offset);
                }
                if (elements[bucket].compare_exchange_weak(elt, newelt, std::memory_order_release,
                                                           std::memory_order_relaxed)) {
                    return status::success;
                } else {
                    if (elt->cardinality == cardinality) {
                        elt->reset(index, offset);
                        return status::success;
                    } else {
                        tries++;
                        ATOMIC_BITSET_TRACE("tries = %zu", tries);
                        newelt->reset(index, offset);
                        relocate(bucket << BucketWidth, bucket, index, offset);
                    }
                }
            } else if (ATOMIC_BITSET_LIKELY(elt->cardinality == cardinality)) {
                elt->set(index, offset);
                return status::success;
            } else {
                tries++;
                ATOMIC_BITSET_TRACE("tries = %zu", tries);
                relocate(bucket << BucketWidth, bucket, index, offset);
            }
        }
        if (newelt != nullptr) {
            delete newelt;
        }

        return status::exceed;
    }

    status reset()
    {
        for (size_t i = 0; i < Capacity; i++) {
            auto element = elements[i].load(std::memory_order_relaxed);
            if (element != nullptr) {
                for (size_t i = 0; i < (1 << BucketWidth) / (8 * sizeof(BucketWord)); i++) {
                    element->words[i].store(0, std::memory_order_relaxed);
                }
            }
        }

        return status::success;
    }

    status reset(Index pos)
    {
        size_t tries = 0;
        size_t cardinality, bucket, index, offset;
        cardinality = relocate(pos, bucket, index, offset);

        while (tries < MaxTries) {
            ATOMIC_BITSET_TRACE("pos = %zu, bucket = %zu, index = %zu, offset = %zu", pos, bucket, index, offset);
            auto elt = elements[bucket].load(std::memory_order_relaxed);
            if (ATOMIC_BITSET_UNLIKELY(elt == nullptr)) {
                return status::success;
            } else if (ATOMIC_BITSET_LIKELY(elt->cardinality == cardinality)) {
                elt->reset(index, offset);
                return status::success;
            } else {
                tries++;
                ATOMIC_BITSET_TRACE("tries = %zu", tries);
                relocate(bucket << BucketWidth, bucket, index, offset);
            }
        }

        return status::exceed;
    }

    status test(Index pos) const
    {
        size_t tries = 0;
        size_t cardinality, bucket, index, offset;
        cardinality = relocate(pos, bucket, index, offset);

        while (tries < MaxTries) {
            ATOMIC_BITSET_TRACE("pos = %zu, bucket = %zu, index = %zu, offset = %zu", pos, bucket, index, offset);
            auto elt = elements[bucket].load(std::memory_order_relaxed);
            if (ATOMIC_BITSET_UNLIKELY(elt == nullptr)) {
                return status::no;
            } else if (ATOMIC_BITSET_LIKELY(elt->cardinality == cardinality)) {
                return elt->test(index, offset) ? status::yes : status::no;
            } else {
                tries++;
                ATOMIC_BITSET_TRACE("tries = %zu", tries);
                relocate(bucket << BucketWidth, bucket, index, offset);
            }
        }
        return status::exceed;
    }

  private:
    struct BitsetBucket {
        size_t cardinality;
        std::atomic<BucketWord> words[(1 << BucketWidth) / (8 * sizeof(BucketWord))];
        BitsetBucket(size_t cardinality = 0, size_t index = 0, size_t offset = ~0) : cardinality(cardinality)
        {
            for (size_t i = 0; i < (1 << BucketWidth) / (8 * sizeof(BucketWord)); i++) {
                words[i].store(0, std::memory_order_relaxed);
            }
            if (offset <= 8 * sizeof(BucketWord) - 1) {
                words[index].fetch_or(static_cast<BucketWord>(1) << offset, std::memory_order_relaxed);
            }
        }

        inline void set(size_t index, size_t offset)
        {
            words[index].fetch_or(static_cast<BucketWord>(1) << offset, std::memory_order_relaxed);
        }

        inline void reset(size_t index, size_t offset)
        {
            words[index].fetch_and(~(static_cast<BucketWord>(1) << offset), std::memory_order_relaxed);
        }

        inline bool test(size_t index, size_t offset) const
        {
            return words[index].load() & (static_cast<BucketWord>(1) << offset);
        }
    };
    std::atomic<BitsetBucket *> ATOMIC_BITSET_ALIGNED(64) elements[Capacity];

    inline size_t relocate(Index pos, size_t &bucket, size_t &index, size_t &offset) const
    {
        size_t cardinality = pos >> BucketWidth;
        size_t location = pos & ((1 << BucketWidth) - 1);
        bucket = cardinality % Capacity;
        index = location / (8 * sizeof(BucketWord));
        offset = location & (8 * sizeof(BucketWord) - 1);

        return cardinality;
    }
};
} // namespace lockfree
