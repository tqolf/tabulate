#pragma once

#include <array>
#include <atomic>
#include <stdexcept>

namespace lockfree
{
#if defined(__GNUC__) || defined(__clang__)
#define ATOMIC_HASHMAP_LIKELY(x)   (__builtin_expect((x), 1))
#define ATOMIC_HASHMAP_UNLIKELY(x) (__builtin_expect((x), 0))
#else
#define ATOMIC_HASHMAP_LIKELY(x)   (x)
#define ATOMIC_HASHMAP_UNLIKELY(x) (x)
#endif

// https://stackoverflow.com/questions/664014/what-integer-hash-function-are-good-that-accepts-an-integer-hash-key
struct DefaultRehasher {
    size_t operator()(size_t n) const
    {
        if (sizeof(size_t) == 32) {
            n = ((n >> 16) ^ n) * 0x45d9f3b;
            n = ((n >> 16) ^ n) * 0x45d9f3b;
            n = (n >> 16) ^ n;
            return n;
        } else if (sizeof(size_t) == 64) {
            n = (n ^ (n >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
            n = (n ^ (n >> 27)) * UINT64_C(0x94d049bb133111eb);
            n = n ^ (n >> 31);
            return n;
        }

        return n;
    }
};

template <typename Container>
struct iterator {
    using value_type = typename Container::value_type;
    static constexpr auto size = Container::size;
    size_t bucket;
    typename Container::HashMapElement *value;
    Container &self;

    iterator(Container &s, size_t b) : bucket(b), value(nullptr), self(s)
    {
        increment();
    }

    typename Container::HashMapElement &operator*() const
    {
        return *value;
    }

    typename Container::HashMapElement *operator->() const
    {
        return value;
    }

    iterator &operator++()
    {
        if (bucket < size) {
            ++bucket;
            increment();
        }
        return *this;
    }

    template <typename IteratorType>
    bool operator==(const IteratorType &a) const
    {
        return bucket == a.bucket;
    }

    template <typename IteratorType>
    bool operator!=(const IteratorType &a) const
    {
        return bucket != a.bucket;
    }

  private:
    void increment()
    {
        while (bucket < size) {
            value = self.elements[bucket].load(std::memory_order_relaxed);
            if (value != nullptr) {
                break;
            }
            ++bucket;
        }
    }
};

template <typename Container>
struct const_iterator {
    using value_type = typename Container::value_type;
    static constexpr auto size = Container::size;
    size_t bucket;
    const typename Container::HashMapElement *value;
    const Container &self;

    const_iterator(const Container &s, size_t b) : bucket(b), value(nullptr), self(s)
    {
        increment();
    }

    const typename Container::HashMapElement &operator*() const
    {
        return *value;
    }
    const typename Container::HashMapElement *operator->() const
    {
        return value;
    }
    const_iterator &operator++()
    {
        if (bucket < size) {
            ++bucket;
            increment();
        }
        return *this;
    }

    template <typename IteratorType>
    bool operator==(const IteratorType &a) const
    {
        return bucket == a.bucket;
    }

    template <typename IteratorType>
    bool operator!=(const IteratorType &a) const
    {
        return bucket != a.bucket;
    }

  private:
    void increment()
    {
        while (bucket < size) {
            value = self.elements[bucket].load(std::memory_order_relaxed);
            if (value != nullptr) {
                break;
            }
            ++bucket;
        }
    }
};

template <size_t Capacity, typename Key, typename Value, typename Hasher = std::hash<Key>,
          typename Rehasher = DefaultRehasher, size_t MaxTries = 32>
class atomic_hashmap {
  public:
    atomic_hashmap() = default;
    atomic_hashmap(const atomic_hashmap &another) = delete;
    atomic_hashmap(atomic_hashmap &&another) = delete;
    atomic_hashmap &operator=(const atomic_hashmap &another) = delete;
    atomic_hashmap &operator=(atomic_hashmap &&another) = delete;

  public:
    using key_type = Key;
    using value_type = Value;
    static constexpr auto size = Capacity;
    using iterator_type = iterator<atomic_hashmap>;
    using const_iterator_type = const_iterator<atomic_hashmap>;
    friend iterator_type;
    friend const_iterator_type;

    ~atomic_hashmap()
    {
        for (size_t i = 0; i < Capacity; ++i) {
            HashMapElement *elt = elements[i].load(std::memory_order_relaxed);

            if (elt != nullptr) {
                delete elt;
            }
        }
    }

    Value *get(const Key &key) const
    {
        const auto hasher = Hasher();
        const auto rehasher = Rehasher();

        size_t tries = 0, bucket;
        size_t hash = hasher(key), rehash = hash;
        while (tries < MaxTries) {
            bucket = rehash % Capacity;
            HashMapElement *elt = elements[bucket].load(std::memory_order_relaxed);

            if (elt == nullptr) {
                return nullptr;
            } else if (elt->hash == hash) {
                return &elt->val;
            } else if (elt->hash != 0) {
                rehash = rehasher(rehash);
            }
            ++tries;
        }

        return nullptr;
    }

    template <typename... Args>
    std::pair<iterator_type, bool> get_or_emplace(const Key &key, Args &&...args)
    {
        const auto hasher = Hasher();
        const auto rehasher = Rehasher();

        size_t tries = 0, bucket;
        size_t hash = hasher(key), rehash = hash;
        while (tries < MaxTries) {
            bucket = rehash % Capacity;
            HashMapElement *elt = elements[bucket].load(std::memory_order_relaxed);

            if (elt == nullptr) {
                HashMapElement *newelt = new HashMapElement(hash, key, std::forward<Args>(args)...);

                if (elements[bucket].compare_exchange_weak(elt, newelt, std::memory_order_release,
                                                           std::memory_order_relaxed)) {
                    return {iterator_type(*this, bucket), true};
                } else if (elt->hash == hash) {
                    delete newelt;
                    return {iterator_type(*this, bucket), false};
                } else if (elt->hash != 0) {
                    delete newelt;
                    rehash = rehasher(rehash);
                }
            } else if (elt->hash == hash) {
                return {iterator_type(*this, bucket), false};
            } else if (elt->hash != 0) {
                rehash = rehasher(rehash);
            }
            ++tries;
        }

        return {end(), false};
    }

    iterator_type begin()
    {
        return iterator_type(*this, 0);
    }

    iterator_type end()
    {
        return iterator_type(*this, Capacity);
    }

    const_iterator_type begin() const
    {
        return const_iterator_type(*this, 0);
    }

    const_iterator_type end() const
    {
        return const_iterator_type(*this, Capacity);
    }

  private:
    struct HashMapElement {
        const size_t hash;
        const Key key;
        Value val;

        template <typename... Args>
        HashMapElement(size_t hash, const Key &key, Args &&...args)
            : hash(hash), key(key), val(std::forward<Args>(args)...)
        {
        }
    };
    std::array<std::atomic<HashMapElement *>, Capacity> elements;
};
} // namespace lockfree
