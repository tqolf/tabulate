#include "aead.cpp"
#include "resumable_cache.h"

class ResumableCacheCoder_AES {
  public:
    ResumableCacheCoder_AES(std::string dir) : m_dir(std::move(dir))
    {
        m_suffix = ".las";
        m_key = "This is key ....";
        m_iv = "This is Initial Vector";
        m_aad = "Application Addtional Data ...";
        m_key.resize(16);
        m_iv.resize(12);
    }

    std::string label(const std::string &key)
    {
        return key + m_suffix;
        ;
    }

    int encode(const std::string &key, const std::string &from, std::string &label, std::string &to)
    {
        label = key + m_suffix;
        return AEAD_WITH_EVP::encrypt(m_key, m_iv, m_aad, from, to);
    }

    int decode(const std::string &label, const std::string &from, std::string &key, std::string &to)
    {
        if (label.size() < m_suffix.size()
            || label.compare(label.size() - m_suffix.size(), m_suffix.size(), m_suffix) != 0) {
            return -EINVAL;
        }
        key = label.substr(0, label.length() - m_suffix.length());
        return AEAD_WITH_EVP::decrypt(m_key, m_iv, m_aad, from, to);
    }

  private:
    std::string m_dir, m_suffix;
    std::string m_key, m_iv, m_aad;
};

template <typename Key, typename Value>
class MyHelper : public caching::ResumableCacheHelperMap_Filesystem<Key, Value, ResumableCacheCoder_AES> {
  public:
    MyHelper(std::string dir)
        : caching::ResumableCacheHelperMap_Filesystem<Key, Value, ResumableCacheCoder_AES>(
            dir, std::make_shared<ResumableCacheCoder_AES>(dir))
    {
    }
};

class MyCacher {
  public:
    MyCacher(std::string dir) : m_cacher(std::make_shared<MyHelper<std::string, std::string>>(dir + "/.cache")) {}

    caching::ResumableCache<std::string, std::string, MyHelper> &GetCacher()
    {
        return m_cacher;
    }

  private:
    caching::ResumableCache<std::string, std::string, MyHelper> m_cacher;
};

int main()
{
    char tmp[128];
    getcwd(tmp, sizeof(tmp));
    MyCacher mycaher(tmp);

#define DO_SET(key, val)                                                                                \
    do {                                                                                                \
        bool has_old = false;                                                                           \
        std::string old_val;                                                                            \
        if (mycaher.GetCacher().get(key, old_val) == caching::RESUMABLECACHE_OK) {                      \
            has_old = true;                                                                             \
        }                                                                                               \
        if (int err = mycaher.GetCacher().set(key, val); err >= 0) {                                    \
            std::cout << __LINE__ << ": SET: KEY = " << key << ", VALUE = " << val;                     \
            if (has_old) {                                                                              \
                std::cout << " (FROM = " << old_val << ")";                                             \
            }                                                                                           \
            std::cout << ", "                                                                           \
                      << (err == caching::RESUMABLECACHE_SET_INSERT                                     \
                              ? "NEW"                                                                   \
                              : (err == caching::RESUMABLECACHE_SET_NOTCHG ? "NOT CHANGED" : "UPDATE")) \
                      << std::endl;                                                                     \
        } else {                                                                                        \
            std::cerr << __LINE__ << ": SET: failed, KEY = " << (key) << std::endl;                     \
        }                                                                                               \
    } while (0);

#define DO_GET(key)                                                                          \
    if (std::string val; mycaher.GetCacher().get(key, val) == caching::RESUMABLECACHE_OK) {  \
        std::cout << __LINE__ << ": GET: KEY = " << key << ", VALUE = " << val << std::endl; \
    } else {                                                                                 \
        std::cerr << __LINE__ << ": GET: not found, KEY = " << key << std::endl;             \
    }

    for (int i = 0; i < 1000; i++) {
        if (mycaher.GetCacher().size() == 0) {
            DO_SET("key1", "val1")
            DO_SET("key2", "val2")
        } else {
            DO_GET("key1")
            DO_GET("key2")
            DO_SET("key1", "val15")
            DO_SET("key2", "val22")
            DO_GET("key3")
            DO_SET("key3", "val3")
            DO_GET("key3")
        }

        {
            AEAD_WITH_EVP cipher;
            std::string key = "This is key ....";
            std::string iv = "This is Initial Vector";
            iv.resize(12);
            key.resize(16);

            auto ToHex = [](const std::string &str) {
                std::stringstream ss;
                const char HEX_CHARS[] = "0123456789ABCDEF";
                for (auto ch : str) {
                    ss << HEX_CHARS[(ch >> 4) & 0x0F] << HEX_CHARS[ch & 0x0F];
                }
                return ss.str();
            };

            std::string input = "input ...", encrypted, decrypted;
            cipher.reset(AEAD_WITH_EVP::ENCRYPT, key, iv);
            cipher.crypt("aad", input, encrypted);

            cipher.reset(AEAD_WITH_EVP::DECRYPT, key, iv);
            cipher.crypt("aad", encrypted, decrypted);

            std::cout << "Plain: " << ToHex(input) << std::endl;
            std::cout << "Encrypted: " << ToHex(encrypted) << std::endl;
            std::cout << "Decrypted: " << ToHex(decrypted) << std::endl;
        }
    }

    return 0;
}
