#pragma once

#include <map>
#include <queue>
#include <string>
#include <mutex>
#include <atomic>
#include <shared_mutex>
#include <condition_variable>
#include <fstream>
#include <sstream>
#include <iostream>
// #include <filesystem>
#include <thread>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <time.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <openssl/evp.h>
#include <openssl/err.h>
#include <google/protobuf/message.h>
#include <google/protobuf/util/message_differencer.h>

#ifndef RESUMABLECACHE_TRACE
#define RESUMABLECACHE_TRACE(fmt, ...) printf("%3d: " fmt "\n", __LINE__, ##__VA_ARGS__)
#endif

/* filesystem - WTF c++17 not supported ? */
inline bool recurse_mkdir(const char *dir)
{
    size_t len;
    char tmp[256], *p = nullptr;
    if (int ret = snprintf(tmp, sizeof(tmp), "%s", dir); ret > 0 && (size_t)ret < sizeof(tmp)) {
        len = ret;
    } else {
        return false;
    }
    if (tmp[len - 1] == '/') {
        tmp[len - 1] = '\0';
    }
    for (p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            mkdir(tmp, S_IRWXU);
            *p = '/';
        }
    }
    mkdir(tmp, S_IRWXU);

    return true;
}

class AEAD_WITH_EVP {
  public:
    enum { AUTO = -1, ENCRYPT = 0, DECRYPT = 1 };
    AEAD_WITH_EVP(const EVP_CIPHER *method = EVP_aes_128_gcm(), size_t tagsize = EVP_GCM_TLS_TAG_LEN);
    ~AEAD_WITH_EVP();

    int reset();
    int reset(int mode, const std::string &key, const std::string &iv);
    int crypt(const std::string &aad, const std::string &input, std::string &output);

    static inline int encrypt(const std::string &key, const std::string &iv, const std::string &aad,
                              const std::string &input, std::string &output)
    {
        AEAD_WITH_EVP context;
        if (int err = context.reset(ENCRYPT, key, iv); err != 0) {
            return err;
        }
        return context.crypt(aad, input, output);
    }
    static inline int decrypt(const std::string &key, const std::string &iv, const std::string &aad,
                              const std::string &input, std::string &output)
    {
        AEAD_WITH_EVP context;
        if (int err = context.reset(DECRYPT, key, iv); err != 0) {
            return err;
        }
        return context.crypt(aad, input, output);
    }

  private:
    const size_t m_tagsize;
    const EVP_CIPHER *m_method;
    EVP_CIPHER_CTX *m_context;
#if OPENSSL_VERSION_NUMBER < 0x10100000L
    EVP_CIPHER_CTX m_legacy_context;
#endif
};

namespace caching
{
enum {
    RESUMABLECACHE_OK = 0,
    RESUMABLECACHE_NOT_FOUND = -1,
    RESUMABLECACHE_SET_EXPIRED = -2,
    RESUMABLECACHE_SET_NOTCHG = 0,
    RESUMABLECACHE_SET_INSERT = 1,
    RESUMABLECACHE_SET_UPDATE = 2,
};

struct ResumableCacheGeneration {
    time_t ctime;         // create time
    time_t mtime;         // modify time
    time_t atime;         // access time
    unsigned int version; // data version
};

template <typename T, typename std::enable_if<std::is_base_of<google::protobuf::Message, T>::value, bool>::type = 0>
inline bool operator==(const T &lfs, const T &rhs)
{
    return google::protobuf::util::MessageDifferencer::Equivalent(lfs, rhs);
}

template <typename T, typename std::enable_if<std::is_base_of<google::protobuf::Message, T>::value, bool>::type = 0>
inline bool operator!=(const T &lfs, const T &rhs)
{
    return !google::protobuf::util::MessageDifferencer::Equivalent(lfs, rhs);
}

template <typename Key, typename Value, template <typename, typename> class Helper>
class ResumableCache {
  public:
    ResumableCache(std::shared_ptr<Helper<Key, Value>> helper) : m_shutdown(false), m_helper(helper)
    {
        m_persistent_thread = std::thread(&ResumableCache::persistent_worker, this);
    }
    ~ResumableCache()
    {
        m_shutdown = true;
        m_cond.notify_all();
        m_persistent_thread.join();
    }

    size_t size() noexcept
    {
        return m_helper->size();
    }

    std::vector<Key> keys() noexcept
    {
        return m_helper->keys();
    }

    size_t unsaved_size() noexcept
    {
        std::unique_lock<std::mutex> guard(m_mutex);
        return m_unsaved_keys.size();
    }

    std::queue<Key> unsaved_keys() noexcept
    {
        std::unique_lock<std::mutex> guard(m_mutex);
        return m_unsaved_keys;
    }

    int del(const Key &key)
    {
        return m_helper->del(key);
    }

    int get(const Key &key, Value &value, ResumableCacheGeneration *generation = nullptr) noexcept
    {
        return m_helper->get(key, value, generation);
    }

    int set(const Key &key, const Value &value, ResumableCacheGeneration *generation = nullptr) noexcept
    {
        int err = m_helper->set(key, value, generation);
        if (err == RESUMABLECACHE_SET_INSERT || err == RESUMABLECACHE_SET_UPDATE) {
            m_unsaved_keys.push(key);
            m_cond.notify_one();
        }

        return err;
    }

  private:
    void persistent_worker() noexcept
    {
        while (true) {
            std::unique_lock<std::mutex> guard(m_mutex);
            m_cond.wait(guard, [this] {
                return m_shutdown || !m_unsaved_keys.empty();
            });

            std::queue<Key> failed_keys;
            while (!m_unsaved_keys.empty()) {
                auto key = m_unsaved_keys.front();
                if (m_helper->dump(m_unsaved_keys.front()) != 0) {
                    // add to back of queue
                    failed_keys.push(key);
                    RESUMABLECACHE_TRACE("PERSISTENT WORKER: dump failed, label=%s",
                                         m_helper->description(key).c_str());
                }
                m_unsaved_keys.pop();
                RESUMABLECACHE_TRACE("PERSISTENT WORKER: SAVED, label=%s", m_helper->description(key).c_str());
            }
            if (m_shutdown) {
                if (failed_keys.empty()) {
                    RESUMABLECACHE_TRACE("PERSISTENT WORKER: (SHUTDOWN) ALL PAIRS have been saved");
                } else {
                    RESUMABLECACHE_TRACE("PERSISTENT WORKER: (SHUTDOWN) failed to dump %zu keys, %zu keys saved",
                                         failed_keys.size(), size() - failed_keys.size());
                }
                return;
            }

            if (!failed_keys.empty()) {
                RESUMABLECACHE_TRACE("PERSISTENT WORKER: failed to dump %zu keys this round, try next round",
                                     failed_keys.size());
                m_unsaved_keys = failed_keys;
            }
        }
    }

  private:
    std::mutex m_mutex;
    std::condition_variable m_cond;
    std::queue<Key> m_unsaved_keys;

    std::atomic<bool> m_shutdown;
    std::thread m_persistent_thread;

    std::shared_ptr<Helper<Key, Value>> m_helper;
};

template <typename Key, typename Value, class Coder>
class ResumableCacheHelperMap_Filesystem {
  public:
    ResumableCacheHelperMap_Filesystem(std::string dir, std::shared_ptr<Coder> coder)
        : m_dir(std::move(dir)), m_coder(coder)
    {
        if (m_dir[m_dir.length() - 1] != '/') {
            m_dir += "/";
        }

        DIR *d = opendir(m_dir.c_str());
        if (d != nullptr) {
            struct dirent *ent = nullptr;
            while ((ent = readdir(d)) != nullptr) {
                if (strcmp(ent->d_name, ".") != 0 && strcmp(ent->d_name, "..") != 0) {
                    RESUMABLECACHE_TRACE("SCAN PAIR: maybe, label=%s", ent->d_name);
                    std::string label = ent->d_name;
                    if (ent->d_type == DT_REG) {
                        Key key;
                        ValueDetails details;
                        if (load(m_dir + ent->d_name, key, details.value, &details.generation) == 0) {
                            m_cached_pairs[key] = details;
                        } else {
                            RESUMABLECACHE_TRACE("SCAN PAIR: fail, label=%s", ent->d_name);
                        }
                    }
                }
            }
        }
    }

    size_t size() noexcept
    {
        std::shared_lock<std::shared_mutex> guard(m_mutex);
        return m_cached_pairs.size();
    }

    std::vector<Key> keys() noexcept
    {
        std::shared_lock<std::shared_mutex> guard(m_mutex);
        std::vector<Key> cached_keys;
        for (auto pair : m_cached_pairs) {
            cached_keys.push_back(pair.first);
        }
        return cached_keys;
    }

    template <typename T>
    std::string description(const T &t) noexcept
    {
        if constexpr (std::is_same_v<T, std::string>) {
            if (std::count_if(t.begin(), t.end(),
                              [](unsigned char c) {
                                  return std::isprint(c);
                              })
                == t.size()) {
                return t;
            } else {
                std::stringstream ss;
                const char HEX_CHARS[] = "0123456789ABCDEF";
                for (auto ch : t) {
                    ss << HEX_CHARS[(ch >> 4) & 0x0F] << HEX_CHARS[ch & 0x0F];
                }
                return ss.str();
            }
        } else if constexpr (std::is_base_of<google::protobuf::Message, T>::value) {
            return t.ShortDedebugString();
        } else {
            return "<Unkown>";
        }
    }

    int del(const Key &key)
    {
        {
            std::unique_lock<std::shared_mutex> guard(m_mutex);
            m_cached_pairs.erase(key);
        }
        std::string path = m_dir + m_coder->label(key);
        remove(path.c_str());

        return RESUMABLECACHE_OK;
    }

    int get(const Key &key, Value &value, ResumableCacheGeneration *generation) noexcept
    {
        std::shared_lock<std::shared_mutex> guard(m_mutex);
        if (m_cached_pairs.count(key)) {
            auto &detail = m_cached_pairs[key];
            value = detail.value;
            if (generation != nullptr) {
                generation->ctime = detail.generation.ctime;
                generation->atime = detail.generation.atime;
                generation->mtime = detail.generation.mtime;
                generation->version = detail.generation.version;
            }
            detail.generation.atime = time(nullptr); // don't change version

            return RESUMABLECACHE_OK;
        }

        return RESUMABLECACHE_NOT_FOUND;
    }

    int set(const Key &key, const Value &value, ResumableCacheGeneration *generation) noexcept
    {
        int err = RESUMABLECACHE_SET_NOTCHG;
        std::unique_lock<std::shared_mutex> guard(m_mutex);
        if (m_cached_pairs.count(key)) {
            auto &detail = m_cached_pairs[key];
            if (detail.value != value) {
                if (generation != nullptr && detail.generation.version != generation->version) {
                    RESUMABLECACHE_TRACE("UPDATE PAIR: expired, label=%s", description(key).c_str());
                    return RESUMABLECACHE_SET_EXPIRED;
                }
                err = RESUMABLECACHE_SET_UPDATE;
                detail.value = value;
                detail.generation.version++;
                if (generation != nullptr) {
                    if (generation->atime != detail.generation.atime) {
                        detail.generation.atime = generation->atime;
                    }
                    if (generation->mtime != detail.generation.mtime) {
                        detail.generation.mtime = generation->mtime;
                    }
                    if (generation->ctime != detail.generation.ctime) {
                        detail.generation.ctime = generation->ctime;
                    }
                } else {
                    detail.generation.atime = detail.generation.mtime = time(nullptr);
                }

                RESUMABLECACHE_TRACE("UPDATE PAIR: label=%s", description(key).c_str());
            }
        } else {
            err = RESUMABLECACHE_SET_INSERT;
            m_cached_pairs[key] = ValueDetails(value);

            RESUMABLECACHE_TRACE("INSERT PAIR: label=%s", description(key).c_str());
        }

        return err;
    }

    enum { TAG_CTIME = 1, TAG_MTIME = 2, TAG_ATIME = 3, TAG_VERSION = 4, TAG_DETAILS = 100 };
    int dump(const Key &key) noexcept
    {
        ValueDetails detail;
        {
            std::shared_lock<std::shared_mutex> guard(m_mutex);
            if (m_cached_pairs.count(key)) {
                detail = m_cached_pairs[key];
            } else {
                return RESUMABLECACHE_NOT_FOUND;
            }
        }

        int err;
        std::string label, encoded;
        if ((err = m_coder->encode(key, detail.value, label, encoded)) != 0) {
            RESUMABLECACHE_TRACE("SVAE PAIR: failed, label=%s", description(key).c_str());
            return err;
        }
        std::string path = m_dir + label;
        RESUMABLECACHE_TRACE("SAVE PAIR: encoded-length = %zu", encoded.size());
        if (const size_t pos = path.rfind('/'); pos != std::string::npos) {
            std::string directory = path.substr(0, pos);
            if (!recurse_mkdir(directory.c_str())) {
                RESUMABLECACHE_TRACE("SAVE PAIR: create directories failed, %s", directory.c_str());
                return -errno;
            }
            // std::filesystem::create_directories(directory);
        }

        std::string tmppath = path + ".doing." + std::to_string(time(nullptr));
        std::ofstream wf(tmppath, std::ios::out | std::ios::binary | std::ios::trunc);
        if (!wf.good()) {
            RESUMABLECACHE_TRACE("SAVE PAIR: open file failed, label=%s", description(key).c_str());
            return -errno;
        }
        struct {
            uint32_t tag;
            uint32_t len;
            const void *val;
        } TLVs[] = {
            {TAG_CTIME, sizeof(uint32_t), &detail.generation.ctime},
            {TAG_MTIME, sizeof(uint32_t), &detail.generation.mtime},
            {TAG_ATIME, sizeof(uint32_t), &detail.generation.atime},
            {TAG_VERSION, sizeof(uint32_t), &detail.generation.version},
            {TAG_DETAILS, static_cast<uint32_t>(encoded.size()), (const void *)encoded.data()},
        };
        for (size_t i = 0; i < sizeof(TLVs) / sizeof(TLVs[0]); i++) {
            RESUMABLECACHE_TRACE("SAVE PAIR: tag = %u, len = %u", TLVs[i].tag, TLVs[i].len);
            wf.write((const char *)(&TLVs[i].tag), sizeof(TLVs[i].tag));
            wf.write((const char *)(&TLVs[i].len), sizeof(TLVs[i].len));
            wf.write((const char *)(TLVs[i].val), TLVs[i].len);
        }
        wf.close();
        if (rename(tmppath.c_str(), path.c_str()) != 0) {
            RESUMABLECACHE_TRACE("SAVE PAIR: fail, label=%s, %s", description(key).c_str(), strerror(errno));
            return -errno;
        }
        // std::filesystem::rename(tmppath, path);
        RESUMABLECACHE_TRACE("SAVE PAIR: DONE, label=%s", description(key).c_str());

        return 0;
    }

    int load(const std::string &path, Key &key, Value &value, ResumableCacheGeneration *generation) noexcept
    {
        int err = 0;
        std::string label = path.substr(m_dir.length());
        if (path.find(".doing.") != std::string::npos /* reserved for internal using */) {
            return -EINVAL;
        }

        if (generation != nullptr) {
            generation->version = 0;
        }

        std::ifstream rf(path, std::ios::in | std::ios::binary);
        if (!rf.good()) {
            return -errno;
        }
        while (rf.peek() != EOF) {
            uint32_t tag, len, tmp;
            rf.read((char *)&tag, sizeof(tag));
            rf.read((char *)&len, sizeof(len));
            RESUMABLECACHE_TRACE("LOAD PAIR: tag = %u, len = %u", tag, len);

            switch (tag) {
                case TAG_CTIME:
                    if (len != sizeof(uint32_t)) {
                        RESUMABLECACHE_TRACE("LOAD PAIR: create time failed, label=%s, len = %u", label.c_str(), len);
                        return -EINVAL;
                    }
                    rf.read((char *)&tmp, sizeof(uint32_t));
                    if (generation != nullptr) {
                        generation->ctime = tmp;
                    }
                    break;
                case TAG_MTIME:
                    if (len != sizeof(uint32_t)) {
                        RESUMABLECACHE_TRACE("LOAD PAIR: modify time failed, label=%s, len = %u", label.c_str(), len);
                        return -EINVAL;
                    }
                    rf.read((char *)&tmp, sizeof(uint32_t));
                    if (generation != nullptr) {
                        generation->mtime = tmp;
                    }
                    break;
                case TAG_ATIME:
                    if (len != sizeof(uint32_t)) {
                        RESUMABLECACHE_TRACE("LOAD PAIR: access time failed, label=%s, len = %u", label.c_str(), len);
                        return -EINVAL;
                    }
                    rf.read((char *)&tmp, sizeof(uint32_t));
                    if (generation != nullptr) {
                        generation->atime = tmp;
                    }
                    break;
                case TAG_VERSION: {
                    if (len != sizeof(uint32_t)) {
                        RESUMABLECACHE_TRACE("LOAD PAIR: access time failed, label=%s, len = %u", label.c_str(), len);
                        return -EINVAL;
                    }
                    rf.read((char *)&tmp, sizeof(uint32_t));
                    if (generation != nullptr) {
                        generation->version = tmp;
                    }
                } break;
                case TAG_DETAILS: {
                    RESUMABLECACHE_TRACE("LOAD PAIR: label=%s, len = %u", label.c_str(), len);
                    std::string encoded(len, 0);
                    rf.read((char *)encoded.data(), len);
                    if ((err = m_coder->decode(label, encoded, key, value)) != 0) {
                        RESUMABLECACHE_TRACE("LOAD PAIR: decode failed, label=%s", label.c_str());
                        return err;
                    }
                } break;
                default:
                    /* skip unkown field */
                    rf.seekg(len, rf.cur);
                    break;
            }
        }
        if (generation != nullptr) {
            auto fromtimestamp = [](time_t t) -> std::string {
                char s[32];
                struct tm tm;
                localtime_r(&t, &tm);
                if (strftime(s, sizeof(s), "%Y-%m-%d %H:%M:%S", &tm) != 0) {
                    return std::string(s);
                }

                return "";
            };
            RESUMABLECACHE_TRACE("LOAD PAIR: version=%d, atime=%s, mtime=%s, ctime=%s", generation->version,
                                 fromtimestamp(generation->atime).c_str(), fromtimestamp(generation->mtime).c_str(),
                                 fromtimestamp(generation->ctime).c_str());
        }

        return err;
    }

  private:
    struct ValueDetails {
        Value value;
        ResumableCacheGeneration generation;
        ValueDetails() {}
        ValueDetails(const Value &value) : value(value)
        {
            generation.version = 0;
            generation.atime = generation.mtime = generation.ctime = time(nullptr);
        }
    };
    std::string m_dir;
    std::shared_ptr<Coder> m_coder;

    mutable std::shared_mutex m_mutex;
    std::map<std::string, ValueDetails> m_cached_pairs;
};
} // namespace caching
