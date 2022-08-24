#include <map>
#include <queue>
#include <string>
#include <mutex>
#include <shared_mutex>
#include <condition_variable>
#include <fstream>
#include <iostream>
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

#include "resumable_cache.h"

#define AEAD_TRACE(...) // printf("%s:%d ", __FILE__, __LINE__), printf(__VA_ARGS__), printf("\n")

AEAD_WITH_EVP::AEAD_WITH_EVP(const EVP_CIPHER *method, size_t tagsize)
    : m_tagsize(tagsize), m_method(method), m_context(nullptr)
{
}

AEAD_WITH_EVP::~AEAD_WITH_EVP()
{
    if (m_context) {
#if OPENSSL_VERSION_NUMBER >= 0x10100000L
        EVP_CIPHER_CTX_free(m_context);
#else
        EVP_CIPHER_CTX_cleanup(m_context);
#endif
        m_context = nullptr;
        m_method = nullptr;
    }
}

#if OPENSSL_VERSION_NUMBER < 0x10100000L
#define EVP_CIPHER_CTX_encrypting(ctx) ((ctx)->encrypt)
#endif

int AEAD_WITH_EVP::reset()
{
    /* Reset the context(key/iv and mode not changed) */
    if (!EVP_CipherInit_ex(m_context, NULL, NULL, NULL, NULL, EVP_CIPHER_CTX_encrypting(m_context))) {
        AEAD_TRACE("Failed to initialise key and IV");
        return static_cast<int>(ERR_get_error());
    }

    return 0;
}

int AEAD_WITH_EVP::reset(int mode, const std::string &key, const std::string &iv)
{
    if (key.empty() || static_cast<size_t>(EVP_CIPHER_key_length(m_method)) != key.size()) {
        AEAD_TRACE("invalid key size(key.size() = %zu)", key.size());
        return EINVAL;
    }
    if (mode == AUTO) {
        mode = EVP_CIPHER_CTX_encrypting(m_context) ? ENCRYPT : DECRYPT;
    }

    /* create cipher context and do intialization */
    if (m_context == nullptr) {
#if OPENSSL_VERSION_NUMBER >= 0x10100000L
        m_context = EVP_CIPHER_CTX_new();
        if (!m_context) {
            return ENOMEM;
        }
#else
        m_context = &m_legacy_context;
#endif
        /* Initialise the cryption operation. */
        if (!EVP_CipherInit_ex(m_context, m_method, NULL, NULL, NULL, mode == ENCRYPT)) {
            AEAD_TRACE("Failed to initialise the cryption operation");
            return static_cast<int>(ERR_get_error());
        }
    }

    /* Set IV length if default 12 bytes (96 bits) is not appropriate */
    if (iv.size() != 0 && iv.size() != 12) {
        if (!EVP_CIPHER_CTX_ctrl(m_context, EVP_CTRL_GCM_SET_IVLEN, iv.size(), NULL)) {
            AEAD_TRACE("Failed to set IV length");
            return static_cast<int>(ERR_get_error());
        }
    }

    /* Initialise key and IV */
    if (!EVP_CipherInit_ex(m_context, NULL, NULL, (const unsigned char *)key.data(), (const unsigned char *)iv.data(),
                           mode == ENCRYPT)) {
        AEAD_TRACE("Failed to initialise key and IV");
        return static_cast<int>(ERR_get_error());
    }

    return 0;
}

int AEAD_WITH_EVP::crypt(const std::string &aad, const std::string &input, std::string &output)
{
    size_t outsz = 0;
    if (EVP_CIPHER_CTX_encrypting(m_context)) {
        outsz = input.size() + m_tagsize;
    } else {
        if (input.size() < m_tagsize) {
            return -EINVAL;
        }
        outsz = input.size() - m_tagsize;
        /* fallthough to verify gcm tag if outsz == 0 */
    }
    output.resize(outsz);
    unsigned char *out = (unsigned char *)output.c_str();

    /* Provide any AAD data. */
    int outl = 0;
    if (aad.size() && !EVP_CipherUpdate(m_context, NULL, &outl, (const unsigned char *)aad.data(), aad.size())) {
        AEAD_TRACE("Failed to intialize additional application data.");
        ERR_print_errors_fp(stderr);
        return static_cast<int>(ERR_get_error());
    }
    size_t inlen = input.size() - (!EVP_CIPHER_CTX_encrypting(m_context) ? m_tagsize : 0);

    /* Provide the message to be crypted, and obtain the crypted output. */
    assert(outsz < INT_MAX);
    outl = outsz;
    if (!EVP_CipherUpdate(m_context, out, &outl, (const unsigned char *)input.data(), inlen)) {
        AEAD_TRACE("Failed to obtain the crypted output.");
        return static_cast<int>(ERR_get_error());
    }
    assert(outsz >= outl);
    out += outl, outsz -= outl;

    if (!EVP_CIPHER_CTX_encrypting(m_context)) {
        /* Set the tag */
        if (!EVP_CIPHER_CTX_ctrl(m_context, EVP_CTRL_GCM_SET_TAG, m_tagsize,
                                 (void *)((unsigned char *)input.data() + inlen))) {
            AEAD_TRACE("Failed to get the tag");
            return static_cast<int>(ERR_get_error());
        }
    }

    /**
     * Finalise the cryption. Normally output bytes may be written at this stage,
     * but this does not occur in GCM mode
     */
    outl = outsz;
    if (!EVP_CipherFinal_ex(m_context, out, &outl)) {
        AEAD_TRACE("Failed to finalise the cryption");
        return static_cast<int>(ERR_get_error());
    }
    assert(outsz >= outl);
    out += outl, outsz -= outl;

    if (EVP_CIPHER_CTX_encrypting(m_context)) {
        /* Get the tag */
        if (!EVP_CIPHER_CTX_ctrl(m_context, EVP_CTRL_GCM_GET_TAG, m_tagsize, out)) {
            AEAD_TRACE("Failed to get the tag");
            return static_cast<int>(ERR_get_error());
        }
        assert(outsz >= m_tagsize);
        out += m_tagsize, outsz -= m_tagsize;
    }

    return 0;
}
