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

#include "cxxopt.h"
#include "profiler.h"
#include "sampling.h"
#include "atomic-bitset.h"
#include "aead.h"
#include "aead.cpp"

#include <openssl/evp.h>
#include <openssl/rand.h>

#ifdef HAS_ROARING
#include <roaring/roaring.h>
#endif

#define TRACE(...) printf("%s:%d ", __FILE__, __LINE__), printf(__VA_ARGS__), printf("\n")

int gcm_encrypt(const unsigned char *plaintext, int plaintext_len, const unsigned char *aad, int aad_len,
                const unsigned char *key, const unsigned char *iv, int iv_len, unsigned char *ciphertext,
                unsigned char *tag)
{
    EVP_CIPHER_CTX *ctx;

    int len;

    int ciphertext_len;

    /* Create and initialise the context */
    if (!(ctx = EVP_CIPHER_CTX_new())) return -1;

    /* Initialise the encryption operation. */
    if (1 != EVP_EncryptInit_ex(ctx, EVP_aes_128_gcm(), NULL, NULL, NULL)) return -1;

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

int gcm_decrypt(unsigned char *ciphertext, int ciphertext_len, const unsigned char *aad, int aad_len,
                const unsigned char *tag, size_t taglen, const unsigned char *key, const unsigned char *iv, int iv_len,
                unsigned char *plaintext)
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
    if (!EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, taglen, (void *)tag)) return -1;

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

std::string RandomBytes(size_t size)
{
    std::string buff;
    buff.resize(size);
    RAND_bytes((unsigned char *)buff.data(), size);
    return buff;
}

std::string FromHex(const std::string &hex)
{
    std::string bytes;
    bytes.resize((hex.size() + 1) >> 1);
    const char *src = hex.c_str();
    unsigned char *dst = (unsigned char *)bytes.data();

    auto ToInt = [](char ch) {
        return ((ch > '9' ? ch + 9 : ch) & 0x0F);
    };

    if (hex.size() & 1) {
        *dst++ = ToInt(*src++);
    }
    while (*src) {
        *(dst++) = ToInt(*src) << 4 | ToInt(src[1]);
        src += 2;
    }

    return bytes;
}
#define FROM_HEX(abcdef) FromHex(#abcdef)

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

static std::string ToHex(const std::string &bytes)
{
    return ToHex(bytes.data(), bytes.size());
}

int main()
{
    size_t repeat = getarg(0, "--repeats");
    size_t N = getarg(5, "--loops"), NP = getarg(6, "--threads");

    if (getarg(false, "--times", "--freq", "--bitset", "--aes", "--all")) {
        profiler::SetTitle("Benchmarks");

        profiler::Add("nothing", []() {
            return true;
        });
        profiler::AddMultiThread("nothing(threading)", []() {
            return true;
        });
    }

    if (getarg(false, "--times", "--all")) {
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

    if (getarg(false, "--freq", "--all")) {
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

    if (getarg(false, "--bitset", "--all")) {
        struct range {
            size_t min;
            size_t max;
            range(size_t min, size_t max) : min(min), max(max) {}
            size_t size()
            {
                return max - min;
            }
        };
        std::vector<range> ranges = {
            range(10, 20), range(100, 110),
            // range(10000, 10200)
        };

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

#ifdef HAS_ROARING
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
#endif

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
    }

    if (getarg(false, "--aes", "--all")) {
        std::string iv = RandomBytes(getarg(16, "--ivlen"));
        std::string key = RandomBytes(getarg(16, "--keylen"));
        std::string plaintext = RandomBytes(getarg(128, "--length"));

        std::cout << "IV: " << ToHex(iv) << std::endl;
        std::cout << "Key: " << ToHex(key) << std::endl;
        std::cout << "Plaintext: " << ToHex(plaintext) << std::endl << std::endl;

        AEAD_WITH_EVP encontext(AEAD_WITH_EVP::ENCRYPT, key, iv);
        AEAD_WITH_EVP decontext(AEAD_WITH_EVP::DECRYPT, key, iv);
        int which = getarg(0, "--which");

        AEAD_WITH_EVP encontext2(encontext), decontext2(decontext);

        auto aead_aes_once = [&](const std::string &key [[maybe_unused]], const std::string &iv [[maybe_unused]],
                                 const std::string &plaintext, bool debug = false) {
            std::string aad;
            std::string encrypted;

            if (which == 0 /* reuse context */) {
                if (int err = encontext.crypt(aad, plaintext, encrypted); err != 0) {
                    TRACE("Cryption failed");
                    return err;
                }
            } else if (which == 1 /* clone computed */) {
                AEAD_WITH_EVP context(encontext);
                if (int err = context.crypt(aad, plaintext, encrypted); err != 0) {
                    TRACE("Cryption failed");
                    return err;
                }
            } else if (which == 2 /* clone computed to allocated object */) {
                encontext2.clone(encontext);
                if (int err = encontext2.crypt(aad, plaintext, encrypted); err != 0) {
                    TRACE("Cryption failed");
                    return err;
                }
            }

            std::string decrypted;
            if (which == 0 /* reuse context */) {
                if (int err = decontext.crypt(aad, encrypted, decrypted); err != 0) {
                    TRACE("Cryption failed");
                    return err;
                }
            } else if (which == 1 /* clone computed */) {
                AEAD_WITH_EVP context(decontext);
                if (int err = context.crypt(aad, encrypted, decrypted); err != 0) {
                    TRACE("Cryption failed");
                    return err;
                }
            } else if (which == 2 /* clone computed to allocated object */) {
                decontext2.clone(decontext);
                if (int err = decontext2.crypt(aad, encrypted, decrypted); err != 0) {
                    TRACE("Cryption failed");
                    return err;
                }
            }

            if (debug) {
                std::cout << "Encrypted: " << ToHex(encrypted) << std::endl;
                std::cout << "Decrypted: " << ToHex(decrypted) << std::endl << std::endl;
                if (decrypted != plaintext) {
                    TRACE("Cryption failed");
                    exit(1);
                }
            }

            return 0;
        };

        auto ossl_aes_once = [](const std::string &key, const std::string &iv, const std::string &plaintext,
                                bool debug = false) {
            unsigned char encrypted[plaintext.size()], decrypted[plaintext.size()], tag[16] = {0};
            gcm_encrypt((const unsigned char *)plaintext.data(), plaintext.size(), nullptr, 0,
                        (const unsigned char *)key.data(), (const unsigned char *)iv.data(), iv.size(), encrypted, tag);
            gcm_decrypt(encrypted, sizeof(encrypted), nullptr, 0, tag, sizeof(tag), (const unsigned char *)key.data(),
                        (const unsigned char *)iv.data(), iv.size(), decrypted);

            if (debug) {
                std::cout << "Encrypted: " << ToHex(encrypted, sizeof(encrypted)) << ToHex(tag, sizeof(tag))
                          << std::endl;
                std::cout << "Decrypted: " << ToHex(decrypted, sizeof(decrypted)) << std::endl << std::endl;
                if (memcmp(plaintext.data(), decrypted, plaintext.size()) != 0) {
                    TRACE("Cryption failed");
                    exit(1);
                }
            }

            return 0;
        };

        aead_aes_once(key, iv, plaintext, true);
        aead_aes_once(key, iv, plaintext, true);
        ossl_aes_once(key, iv, plaintext, true);

        profiler::Add(
            "aead::aes",
            [&]() {
                return aead_aes_once(key, iv, plaintext) == 0;
            },
            repeat, 5);
        profiler::Add(
            "ossl::aes",
            [&]() {
                return ossl_aes_once(key, iv, plaintext) == 0;
            },
            repeat, 5);
        profiler::AsReference("ossl::aes");
    }

    return 0;
}
