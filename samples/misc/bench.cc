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

#include <openssl/evp.h>

int gcm_encrypt(unsigned char *plaintext, int plaintext_len, unsigned char *aad, int aad_len, unsigned char *key,
                unsigned char *iv, int iv_len, unsigned char *ciphertext, unsigned char *tag)
{
    EVP_CIPHER_CTX *ctx;

    int len;

    int ciphertext_len;

    /* Create and initialise the context */
    if (!(ctx = EVP_CIPHER_CTX_new())) return -1;

    /* Initialise the encryption operation. */
    if (1 != EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL)) return -1;

    /*
     * Set IV length if default 12 bytes (96 bits) is not appropriate
     */
    if (1 != EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, iv_len, NULL)) return -1;

    /* Initialise key and IV */
    if (1 != EVP_EncryptInit_ex(ctx, NULL, NULL, key, iv)) return -1;

    /*
     * Provide any AAD data. This can be called zero or more times as
     * required
     */
    if (1 != EVP_EncryptUpdate(ctx, NULL, &len, aad, aad_len)) return -1;

    /*
     * Provide the message to be encrypted, and obtain the encrypted output.
     * EVP_EncryptUpdate can be called multiple times if necessary
     */
    if (1 != EVP_EncryptUpdate(ctx, ciphertext, &len, plaintext, plaintext_len)) return -1;
    ciphertext_len = len;

    /*
     * Finalise the encryption. Normally ciphertext bytes may be written at
     * this stage, but this does not occur in GCM mode
     */
    if (1 != EVP_EncryptFinal_ex(ctx, ciphertext + len, &len)) return -1;
    ciphertext_len += len;

    /* Get the tag */
    if (1 != EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, 16, tag)) return -1;

    /* Clean up */
    EVP_CIPHER_CTX_free(ctx);

    return ciphertext_len;
}

int gcm_decrypt(unsigned char *ciphertext, int ciphertext_len, unsigned char *aad, int aad_len, unsigned char *tag,
                size_t taglen, unsigned char *key, unsigned char *iv, int iv_len, unsigned char *plaintext)
{
    EVP_CIPHER_CTX *ctx;
    int len;
    int plaintext_len;
    int ret;

    /* Create and initialise the context */
    if (!(ctx = EVP_CIPHER_CTX_new())) return -1;

    /* Initialise the decryption operation. */
    if (!EVP_DecryptInit_ex(ctx, EVP_aes_128_gcm(), NULL, NULL, NULL)) return -1;

    /* Set IV length. Not necessary if this is 12 bytes (96 bits) */
    if (!EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, iv_len, NULL)) return -1;

    /* Initialise key and IV */
    if (!EVP_DecryptInit_ex(ctx, NULL, NULL, key, iv)) return -1;

    /* Provide any AAD data. This can be called zero or more times as required */
    if (aad != nullptr && !EVP_DecryptUpdate(ctx, NULL, &len, aad, aad_len)) return -1;

    /**
     * Provide the message to be decrypted, and obtain the plaintext output.
     * EVP_DecryptUpdate can be called multiple times if necessary
     */
    if (!EVP_DecryptUpdate(ctx, plaintext, &len, ciphertext, ciphertext_len)) return -1;
    plaintext_len = len;

    /* Set expected tag value. Works in OpenSSL 1.0.1d and later */
    if (!EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, taglen, tag)) return -1;

    /**
     * Finalise the decryption. A positive return value indicates success,
     * anything else is a failure - the plaintext is not trustworthy.
     */
    ret = EVP_DecryptFinal_ex(ctx, plaintext + len, &len);

    /* Clean up */
    EVP_CIPHER_CTX_free(ctx);

    if (ret > 0) {
        /* Success */
        plaintext_len += len;
        return plaintext_len;
    } else {
        /* Verify failed */
        return -1;
    }
}

static std::string ToHex(const void *addr, size_t size)
{
    std::string hex;
    hex.resize(size * 2);

    const char HEX_CHARS[] = "0123456789ABCDEF";
    for (size_t i = 0; i < size; i++) {
        unsigned char ch = ((const unsigned char *)addr)[i];
        hex[i * 2 + 0] = HEX_CHARS[(ch >> 4) & 0x0F];
        hex[i * 2 + 1] = HEX_CHARS[ch & 0x0F];
    }

    return hex;
}

int main(int argc, char **argv)
{
    size_t repeat = 0;
    if (argc >= 2) {
        repeat = atoi(argv[1]);
    }
    size_t N = 5, NP = 6;

    profiler::SetTitle("Benchmarks");

    {
        profiler::Add("nothing", []() {
            return true;
        });
        profiler::AddMultiThread("nothing(threading)", []() {
            return true;
        });
    }

    {
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
    }

    {
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
    }

    struct range {
        size_t min;
        size_t max;
        range(size_t min, size_t max) : min(min), max(max) {}
        size_t size()
        {
            return max - min;
        }
    };
    std::vector<range> ranges = {range(10, 20), range(100, 110), range(10000, 10200)};

    auto set_all = [&](std::function<void(size_t)> setter) {
        for (auto r : ranges) {
            for (size_t i = r.min; i < r.max; i++) {
                setter(i);
            }
        }

        return true;
    };

    auto test_all = [&](std::function<bool(size_t)> getter) {
        size_t min = ~0, max = 0, n = 0, expected = 0;
        for (auto r : ranges) {
            expected += r.size();
            min = std::min(min, r.min);
            max = std::max(max, r.max);
        }

        for (size_t i = min * 2 / 3; i < max * 3 / 2; i++) {
            n += getter(i);
        }

        if (n != expected) {
            std::cout << "n: " << n << ", expected: " << expected << std::endl;
            for (auto r : ranges) {
                for (size_t i = r.min * 2 / 3; i < r.max * 3 / 2; i++) {
                    if (getter(i)) {
                        if (i < r.min || i >= r.max) {
                            std::cout << "Unexpected(yes): " << i << std::endl;
                        }
                    } else {
                        if (i >= r.min && i < r.max) {
                            std::cout << "Unexpected(no): " << i << std::endl;
                        }
                    }
                }
            }
            return false;
        }

        return true;
    };

    {
        std::bitset<1 << 24> set;
        if (!set_all([&](size_t i) {
                set.set(i);
            })
            || !test_all([&](size_t i) {
                   return set.test(i);
               })) {
            exit(1);
        }

        profiler::Add("std::bitset::set", [&]() {
            set.set(100);
            return true;
        });

        profiler::Add("std::bitset::test", [&]() {
            set.test(100);
            return true;
        });

        std::shared_mutex mutex;
        profiler::AddMultiThread("std::bitset::set(threading)", [&]() {
            std::unique_lock<std::shared_mutex> lock(mutex);
            set.set(100);

            return true;
        });

        profiler::AddMultiThread("std::bitset::test(threading)", [&]() {
            std::shared_lock<std::shared_mutex> lock(mutex);
            set.test(100);

            return true;
        });
    }

    {
        std::set<size_t> set;
        if (!set_all([&](size_t i) {
                set.insert(i);
            })
            || !test_all([&](size_t i) {
                   return set.count(i) != 0;
               })) {
            exit(1);
        }

        profiler::Add("std::set::insert", [&]() {
            set.insert(100);

            return true;
        });

        profiler::Add("std::set::count", [&]() {
            profiler::DoNotOptimize(set.count(100));

            return true;
        });

        std::shared_mutex mutex;
        profiler::AddMultiThread("std::set::insert(threading)", [&]() {
            std::unique_lock<std::shared_mutex> lock(mutex);
            set.insert(100);

            return true;
        });

        profiler::AddMultiThread("std::set::count(threading)", [&]() {
            std::shared_lock<std::shared_mutex> lock(mutex);
            profiler::DoNotOptimize(set.count(100));

            return true;
        });
    }

    {
        std::unordered_set<size_t> set;
        if (!set_all([&](size_t i) {
                set.insert(i);
            })
            || !test_all([&](size_t i) {
                   return set.count(i) != 0;
               })) {
            exit(1);
        }

        profiler::Add("std::unordered_set::insert", [&]() {
            set.insert(100);

            return true;
        });

        profiler::Add("std::unordered_set::count", [&]() {
            profiler::DoNotOptimize(set.count(100));

            return true;
        });

        std::shared_mutex mutex;
        profiler::AddMultiThread("std::unordered_set::insert(threading)", [&]() {
            std::unique_lock<std::shared_mutex> lock(mutex);
            set.insert(100);

            return true;
        });

        profiler::AddMultiThread("std::unordered_set::count(threading)", [&]() {
            std::shared_lock<std::shared_mutex> lock(mutex);
            profiler::DoNotOptimize(set.count(100));

            return true;
        });
    }

    {
        roaring_bitmap_t *bitmap = roaring_bitmap_create();
        if (!set_all([&](size_t i) {
                roaring_bitmap_add(bitmap, i);
            })
            || !test_all([&](size_t i) {
                   return roaring_bitmap_contains(bitmap, i);
               })) {
            exit(1);
        }

        profiler::Add("roaring::bitmap::add", [&]() {
            roaring_bitmap_add(bitmap, 100);

            return true;
        });
        profiler::Add("roaring::bitmap::contains", [&]() {
            profiler::DoNotOptimize(roaring_bitmap_contains(bitmap, 100));

            return true;
        });

        std::shared_mutex mutex;
        profiler::AddMultiThread("roaring::bitmap::add(threading)", [&]() {
            std::unique_lock<std::shared_mutex> lock(mutex);
            roaring_bitmap_add(bitmap, 100);

            return true;
        });
        profiler::AddMultiThread("roaring::bitmap::contains(threading)", [&]() {
            std::shared_lock<std::shared_mutex> lock(mutex);
            profiler::DoNotOptimize(roaring_bitmap_contains(bitmap, 100));

            return true;
        });
        roaring_bitmap_free(bitmap);
    }

    {
        lockfree::atomic_bitset<1024> set;

        // coverage
        if (0) {
            std::vector<std::thread> threads;
            for (size_t i = 0; i < std::thread::hardware_concurrency(); i++) {
                threads.emplace_back([&]() {
                    set.test(100);
                    set.test(111111111111);
                    set.set(100);
                    set.set(101);
                    set.set(111111111111);
                    for (size_t i = 0; i < 20; i++) {
                        for (size_t j = i * 100000; j < i * 2000000; j++) {
                            set.set(j);
                        }
                    }
                    set.reset(100);
                    set.reset(111111111111);
                    set.reset();
                });
            }
            for (auto &thr : threads) {
                thr.join();
            }
        }

        if (!set_all([&](size_t i) {
                set.set(i);
            })
            || !test_all([&](size_t i) {
                   return set.test(i) == lockfree::status::yes;
               })) {
            exit(1);
        }

        profiler::Add(
            "lockfree::atomic_bitset::set",
            [&]() {
                set.set(100);

                return true;
            },
            repeat, 5);

        profiler::Add(
            "lockfree::atomic_bitset::test",
            [&]() {
                profiler::DoNotOptimize(set.test(100));

                return true;
            },
            repeat, 5);

        // set.reset();
        profiler::AddMultiThread(
            "lockfree::atomic_bitset::set(threading)",
            [&]() {
                set.set(100);

                return true;
            },
            repeat, 5);

        profiler::AddMultiThread(
            "lockfree::atomic_bitset::test(threading)",
            [&]() {
                profiler::DoNotOptimize(set.test(100));

                return true;
            },
            repeat, 5);
    }

    {
        unsigned char encrypted[] = {
            0x00, 0xEC, 0xEE, 0x3D, 0xE6, 0x17, 0xFA, 0x46, 0x8F, 0x55, 0x0B, 0x1C, 0xE4, 0xCE, 0x46, 0x37, 0x47, 0xD0,
            0x92, 0xB7, 0x61, 0xE4, 0xE4, 0x29, 0x52, 0x74, 0xC9, 0x19, 0xE6, 0x23, 0x64, 0x58, 0xA6, 0xC8, 0xBB, 0xE3,
            0x5F, 0x83, 0x94, 0x7D, 0xF8, 0xCA, 0x51, 0x95, 0x43, 0x58, 0x08, 0x66, 0xB8, 0xAD, 0xE8, 0x3B, 0x94, 0x88,
            0xD7, 0xDA, 0x0D, 0x82, 0x72, 0x69, 0x3D, 0x10, 0x98, 0x62, 0x63, 0x8C, 0x90, 0xF2, 0xD6, 0x25, 0xDB, 0xA4,
            0x26, 0xC5, 0x2F, 0x8A, 0xA3, 0x88, 0x98, 0x96, 0x13, 0xDC, 0x59, 0x12, 0x65, 0xC9, 0xA2, 0x9A, 0x6D, 0xA0,
            0x74, 0x26, 0x5B, 0x0F, 0xF5, 0x65, 0x27, 0x93, 0x85, 0x73, 0x4E, 0x5B, 0x0A, 0x77, 0x98, 0x98, 0xF7, 0xA5,
            0x33, 0x06, 0x6A, 0x4B, 0x68, 0xCB, 0x55, 0xF1, 0xE6, 0xAC, 0x24, 0xF1, 0xE4, 0x03, 0xAF, 0x1C, 0xB1, 0x1C,
            0x06, 0x5F, 0xE6, 0x1C, 0xAB, 0xF3, 0x10, 0x98, 0xB5, 0x1A, 0xED, 0x5B, 0xA9, 0xAA, 0xDC, 0xC6, 0x09, 0xEA,
            0x15, 0x7F, 0x8C, 0xB9, 0xCF, 0x67, 0x94, 0x62, 0xF9, 0x93, 0xE2, 0xEE, 0x67, 0xD5, 0xF7, 0x69, 0x95, 0x82,
            0x18, 0x0B, 0x15, 0xC9, 0x50, 0x24, 0xBE, 0x07, 0x0F, 0x29, 0xB7, 0x8D, 0x51, 0x58, 0x58, 0x94, 0x5C, 0x6E,
            0x20, 0x76, 0x6C, 0x2C, 0x96, 0xC7, 0x9C, 0x8E, 0xEC, 0x24, 0x5C, 0xB1, 0x6F, 0xF8, 0xD9, 0x9A, 0x30, 0xA9,
            0x3D, 0xD0, 0x04, 0x5E, 0x14, 0x10, 0x33, 0x29, 0xA2, 0xA7, 0x9B, 0xEA, 0x33, 0x98, 0x48, 0x23, 0x3B, 0x59,
            0xE1, 0x7D, 0xC2, 0x8D, 0xE0, 0x85, 0x26, 0x6F, 0x24, 0xD2, 0x0B, 0xB0, 0xE0, 0x4F, 0x0B, 0x47, 0x98, 0xB0,
            0xDF, 0xCC, 0x8E, 0x00, 0x63, 0xE4, 0x19, 0x56, 0xBB, 0x40, 0x66, 0xB5, 0x23, 0xD9, 0x42, 0x4D, 0xDB, 0x5A,
            0x01, 0x7D, 0x88, 0xE4, 0x54, 0x72, 0x4E, 0x56, 0xAC, 0x7F, 0xEE, 0x4E, 0xA4, 0x23, 0x05, 0x53, 0xD7, 0xB5,
            0xDC, 0xB6, 0x99, 0x1F, 0x50, 0xB1, 0x15, 0xE0, 0x40, 0x18, 0x78, 0x6C, 0x36, 0x87, 0xDD, 0x1F, 0xF7, 0x75,
            0x02, 0x38, 0x3F, 0x86, 0x89, 0x7A, 0xF8, 0x87, 0x41, 0x4D, 0xDD,
        };
        unsigned char tag[] = {
            0x06, 0x17, 0x56, 0x28, 0xE2, 0x77, 0x05, 0xC5, 0xF3, 0xFC, 0xD0, 0x24, 0x01, 0xED, 0x5A, 0x57,
        };
        unsigned char key[] = {
            0x44, 0x32, 0x33, 0x39, 0x44, 0x37, 0x31, 0x30, 0x44, 0x33, 0x33, 0x41, 0x44, 0x36, 0x31, 0x36,
        };
        unsigned char iv[] = {
            0xF3, 0x0D, 0x86, 0xD5, 0xFA, 0xE9, 0x76, 0xC7, 0x8E, 0xE4, 0x85, 0xFB, 0x5E, 0x40, 0xAF, 0x38,
        };
        unsigned char decrypted[sizeof(encrypted)];
        gcm_decrypt(encrypted, sizeof(encrypted), nullptr, 0, tag, sizeof(tag), key, iv, sizeof(iv), decrypted);

        std::cout << ToHex(decrypted, sizeof(decrypted)) << std::endl;
    }

    return 0;
}
