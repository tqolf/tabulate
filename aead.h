#pragma once

#include <string>
#include <openssl/evp.h>

#if OPENSSL_VERSION_NUMBER < 0x10100000L
#define EVP_CIPHER_CTX_encrypting(ctx) ((ctx)->encrypt)
#endif

class AEAD_WITH_EVP {
  public:
    AEAD_WITH_EVP();
    AEAD_WITH_EVP(const EVP_CIPHER *method);
    AEAD_WITH_EVP(int mode, const std::string &key, const std::string &iv);
    AEAD_WITH_EVP(const AEAD_WITH_EVP &another);
    AEAD_WITH_EVP(AEAD_WITH_EVP &&another);
    AEAD_WITH_EVP &operator=(const AEAD_WITH_EVP &another);
    AEAD_WITH_EVP &operator=(AEAD_WITH_EVP &&another);
    ~AEAD_WITH_EVP();

    /**
     * @brief clone context
     *
     * @param another initialized context to clone
     * @return int 0 if success; error occurred otherwise
     */
    int clone(const AEAD_WITH_EVP &another);

    /**
     * @brief (re)intialize context
     *
     * @param mode encrypt, decrypt, or inherit from pervious operation
     * @param key cyrption key
     * @param iv initial vector
     * @return int 0 if success; error occurred otherwise
     */
    enum { AUTO = -1, ENCRYPT = 0, DECRYPT = 1 };
    int reset(int mode, const std::string &key, const std::string &iv);

    /**
     * @brief stateless cryption
     *
     * @param aad application additional data
     * @param input |plaintext| for encryption; |ciphertext| + |tag|(gcm) for decryption
     * @param output |ciphertext| + |tag|(gcm) for encryption; |plaintext| for decryption
     * @param taglen tag length for gcm
     * @return int 0 if success; error occurred otherwise
     */
    int crypt(const std::string &aad, const std::string &input, std::string &output,
              size_t taglen = EVP_GCM_TLS_TAG_LEN);

    static inline int encrypt(const std::string &key, const std::string &iv, const std::string &aad,
                              const std::string &input, std::string &output)
    {
        AEAD_WITH_EVP context(ENCRYPT, key, iv);
        return context.crypt(aad, input, output);
    }

    static inline int decrypt(const std::string &key, const std::string &iv, const std::string &aad,
                              const std::string &input, std::string &output)
    {
        AEAD_WITH_EVP context(DECRYPT, key, iv);
        return context.crypt(aad, input, output);
    }

    /**
     * @brief select default method
     *
     * @param keylen key length in bytes
     * @return const EVP_CIPHER* pointer to method
     */
    static inline const EVP_CIPHER *select_default(size_t keylen)
    {
        switch (keylen) {
            case 16:
                return EVP_aes_128_gcm();
            case 24:
                return EVP_aes_192_gcm();
            case 32:
                return EVP_aes_256_gcm();
        }
        return nullptr;
    }

  private:
    bool m_dirty;
    std::string m_iv;
    EVP_CIPHER_CTX *m_context;
    const EVP_CIPHER *m_method;
#if OPENSSL_VERSION_NUMBER < 0x10100000L
    EVP_CIPHER_CTX m_legacy_context;
#endif
};
