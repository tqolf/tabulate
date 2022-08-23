#pragma once

#include <stddef.h>
#include <atomic>

#define TRACE(...) // printf("%s:%d ", __FILE__, __LINE__), printf(__VA_ARGS__), printf("\n")

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

enum class status { success, failed, not_found, yes, no };

template <size_t Capacity, size_t BitsetWidth = 16, typename BitsetWord = uint32_t, size_t MaxTries = 32,
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

        BitSetElement *newelt = nullptr;
        while (tries < MaxTries) {
            TRACE("pos = %zu, bucket = %zu, index = %zu, offset = %zu", pos, bucket, index, offset);

            auto elt = elements[bucket].load(std::memory_order_relaxed);
            if (ATOMIC_BITSET_UNLIKELY(elt == nullptr)) {
                if (newelt == nullptr) {
                    newelt = new BitSetElement(cardinality, index, offset);
                } else {
                    newelt->set(index, offset);
                }
                if (elements[bucket].compare_exchange_weak(elt, newelt, std::memory_order_release,
                                                           std::memory_order_relaxed)) {
                    return status::success;
                } else {
                    if (elt->cardinality == cardinality) {
                        elt->words[index].fetch_or(1 << offset);
                        return status::success;
                    } else {
                        newelt->reset(index, offset);
                        tries++;
                        printf("tries = %zu\n", tries);
                        relocate(bucket << BitsetWidth, bucket, index, offset);
                    }
                }
            } else if (ATOMIC_BITSET_LIKELY(elt->cardinality == cardinality)) {
                elt->set(index, offset);
                return status::success;
            } else {
                tries++;
                printf("tries = %zu\n", tries);
                relocate(bucket << BitsetWidth, bucket, index, offset);
            }
        }
        if (newelt != nullptr) {
            delete newelt;
        }

        return status::failed;
    }

    status reset()
    {
        for (size_t i = 0; i < Capacity; i++) {
            auto element = elements[i].load(std::memory_order_relaxed);
            if (element != nullptr) {
                for (size_t i = 0; i < (1 << BitsetWidth) / (8 * sizeof(BitsetWord)); i++) {
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
            TRACE("pos = %zu, bucket = %zu, index = %zu, offset = %zu", pos, bucket, index, offset);

            auto val = elements[bucket].load(std::memory_order_relaxed);
            if (ATOMIC_BITSET_UNLIKELY(val == nullptr)) {
                return status::success;
            } else if (ATOMIC_BITSET_LIKELY(val->cardinality == cardinality)) {
                val->reset(index, offset);
                return status::success;
            } else {
                tries++;
                printf("tries = %zu\n", tries);
                relocate(bucket << BitsetWidth, bucket, index, offset);
            }
        }

        return status::not_found;
    }

    status test(Index pos) const
    {
        size_t tries = 0;
        size_t cardinality, bucket, index, offset;
        cardinality = relocate(pos, bucket, index, offset);

        while (tries < MaxTries) {
            TRACE("pos = %zu, bucket = %zu, index = %zu, offset = %zu", pos, bucket, index, offset);

            auto val = elements[bucket].load(std::memory_order_relaxed);
            if (ATOMIC_BITSET_UNLIKELY(val == nullptr)) {
                return status::no;
            } else if (ATOMIC_BITSET_LIKELY(val->cardinality == cardinality)) {
                return val->test(index, offset) ? status::yes : status::no;
            } else {
                tries++;
                printf("tries = %zu\n", tries);
                relocate(bucket << BitsetWidth, bucket, index, offset);
            }
        }
        return status::not_found;
    }

  private:
    struct BitSetElement {
        size_t cardinality;
        std::atomic<BitsetWord> words[(1 << BitsetWidth) / (8 * sizeof(BitsetWord))];
        BitSetElement(size_t cardinality = 0, size_t index = 0, size_t offset = ~0) : cardinality(cardinality)
        {
            for (size_t i = 0; i < (1 << BitsetWidth) / (8 * sizeof(BitsetWord)); i++) {
                words[i].store(0, std::memory_order_relaxed);
            }
            if (offset <= 8 * sizeof(BitsetWord) - 1) {
                words[index].fetch_or(1 << offset, std::memory_order_relaxed);
            }
        }

        void set(size_t index, size_t offset)
        {
            words[index].fetch_or(1 << offset, std::memory_order_relaxed);
        }

        void reset(size_t index, size_t offset)
        {
            words[index].fetch_and(~(1 << offset), std::memory_order_relaxed);
        }

        bool test(size_t index, size_t offset) const
        {
            return words[index].load() & (1 << offset);
        }
    };
    std::atomic<BitSetElement *> ATOMIC_BITSET_ALIGNED(64) elements[Capacity];

    inline size_t relocate(Index pos, size_t &bucket, size_t &index, size_t &offset) const
    {
        bucket = pos >> BitsetWidth;
        size_t sub = pos & ((1 << BitsetWidth) - 1);
        index = sub / (8 * sizeof(BitsetWord));
        offset = sub & (8 * sizeof(BitsetWord) - 1);

        return bucket;
    }
};
} // namespace lockfree
