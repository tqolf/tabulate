#include <string>
#include <stdio.h>
#include <assert.h>
#include <openssl/evp.h>
#include <openssl/err.h>
#include "aead.h"

#define AEAD_TRACE(...) printf("%s:%d ", __FILE__, __LINE__), printf(__VA_ARGS__), printf("\n")

#if OPENSSL_VERSION_NUMBER < 0x10100000L
#define EVP_CIPHER_CTX_encrypting(ctx) ((ctx)->encrypt)
#endif

AEAD_WITH_EVP::AEAD_WITH_EVP() : m_dirty(false), m_context(nullptr), m_method(nullptr) {}

AEAD_WITH_EVP::AEAD_WITH_EVP(const EVP_CIPHER *method) : m_dirty(false), m_context(nullptr), m_method(method) {}

AEAD_WITH_EVP::AEAD_WITH_EVP(int mode, const std::string &key, const std::string &iv)
    : AEAD_WITH_EVP(select_default(key.size()))
{
    if (reset(mode, key, iv) != 0) {
        AEAD_TRACE("Failed to intialize context with given mode, key, and iv");
    }
}

AEAD_WITH_EVP::AEAD_WITH_EVP(const AEAD_WITH_EVP &another) : m_dirty(false), m_context(nullptr), m_method(nullptr)
{
    if (this->clone(another) != 0) {
        AEAD_TRACE("Failed to clone given object");
    }
}

AEAD_WITH_EVP::AEAD_WITH_EVP(AEAD_WITH_EVP &&another)
{
    this->m_iv = another.m_iv, another.m_iv.clear();
    this->m_dirty = another.m_dirty, another.m_dirty = false;
    this->m_method = another.m_method, another.m_method = nullptr;
    this->m_context = another.m_context, another.m_context = nullptr;
}

AEAD_WITH_EVP &AEAD_WITH_EVP::operator=(const AEAD_WITH_EVP &another)
{
    if (this->clone(another) != 0) {
        AEAD_TRACE("Failed to clone given object");
    }

    return *this;
}

AEAD_WITH_EVP &AEAD_WITH_EVP::operator=(AEAD_WITH_EVP &&another)
{
    this->m_iv = another.m_iv, another.m_iv.clear();
    this->m_dirty = another.m_dirty, another.m_dirty = false;
    this->m_method = another.m_method, another.m_method = nullptr;
    this->m_context = another.m_context, another.m_context = nullptr;

    return *this;
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

int AEAD_WITH_EVP::clone(const AEAD_WITH_EVP &another)
{
    if (another.m_context) {
#if OPENSSL_VERSION_NUMBER >= 0x10100000L
        if (m_context != nullptr) {
            EVP_CIPHER_CTX_free(m_context);
        }
        if ((m_context = EVP_CIPHER_CTX_new()) == nullptr) {
            AEAD_TRACE("Out of memory");
            return -ENOMEM;
        }
#else
        m_context = &m_legacy_context;
#endif
        if (!EVP_CIPHER_CTX_copy(this->m_context, another.m_context)) {
            AEAD_TRACE("Failed to copy context");
            ERR_print_errors_fp(stderr);
            return -EINVAL;
        }
    }
    this->m_iv = another.m_iv;
    this->m_dirty = another.m_dirty;
    this->m_method = another.m_method;

    return 0;
}

int AEAD_WITH_EVP::reset(int mode, const std::string &key, const std::string &iv)
{
    if (key.empty() || m_method == nullptr || static_cast<size_t>(EVP_CIPHER_key_length(m_method)) != key.size()) {
        AEAD_TRACE("Invalid method or key(size=%zu)", key.size());
        return -EINVAL;
    }
    if (mode == AUTO) {
        mode = EVP_CIPHER_CTX_encrypting(m_context) ? ENCRYPT : DECRYPT;
    }

    /* create cipher context and do intialization */
    if (m_context == nullptr) {
#if OPENSSL_VERSION_NUMBER >= 0x10100000L
        m_context = EVP_CIPHER_CTX_new();
        if (!m_context) {
            return -ENOMEM;
        }
#else
        m_context = &m_legacy_context;
#endif
        /* Initialise the cryption operation. */
        if (!EVP_CipherInit_ex(m_context, m_method, NULL, NULL, NULL, mode == ENCRYPT)) {
            AEAD_TRACE("Failed to initialise the cryption operation");
            return -EINVAL; // static_cast<int>(ERR_get_error());
        }
    }

    /* Set IV length if default length is not appropriate */
    if (iv.size() != 0 && iv.size() != 12) {
        if (!EVP_CIPHER_CTX_ctrl(m_context, EVP_CTRL_GCM_SET_IVLEN, iv.size(), NULL)) {
            AEAD_TRACE("Failed to set IV length");
            return -EINVAL; // static_cast<int>(ERR_get_error());
        }
    }

    /* Initialise key and IV */
    if (!EVP_CipherInit_ex(m_context, NULL, NULL, (const unsigned char *)key.data(), (const unsigned char *)iv.data(),
                           mode == ENCRYPT)) {
        AEAD_TRACE("Failed to initialise key and IV");
        return -EINVAL; // static_cast<int>(ERR_get_error());
    }
    m_iv = iv;
    m_dirty = false;

    return 0;
}

int AEAD_WITH_EVP::crypt(const std::string &aad, const std::string &input, std::string &output, size_t taglen)
{
    size_t outsz = 0;
    if (m_context == nullptr) {
        AEAD_TRACE("Unintialized context");
        return -EINVAL;
    }
    if (EVP_CIPHER_CTX_encrypting(m_context)) {
        outsz = input.size() + taglen;
    } else {
        if (input.size() < taglen) {
            AEAD_TRACE("Invalid size of input, maybe no tag included");
            return -EINVAL;
        }
        outsz = input.size() - taglen;
        /* fallthough to verify gcm tag if outsz == 0 */
    }
    output.resize(outsz);
    unsigned char *out = (unsigned char *)output.c_str();

    if (m_dirty) {
        /* Reset the context(key/iv and mode not changed) */
        if (!EVP_CipherInit_ex(m_context, NULL, NULL, NULL, (const unsigned char *)m_iv.data(), -1)) {
            AEAD_TRACE("Failed to initialise key and IV");
            return -EINVAL; // static_cast<int>(ERR_get_error());
        }
        // AEAD_TRACE("Succeed to reset the context(key/iv and mode not changed)");
    }
    m_dirty = true;

    /* Provide any AAD data. */
    int outl = 0;
    if (aad.size() && !EVP_CipherUpdate(m_context, NULL, &outl, (const unsigned char *)aad.data(), aad.size())) {
        AEAD_TRACE("Failed to intialize additional application data");
        return -EINVAL; // static_cast<int>(ERR_get_error());
    }
    size_t inlen = input.size() - (!EVP_CIPHER_CTX_encrypting(m_context) ? taglen : 0);

    /* Provide the message to be crypted, and obtain the crypted output. */
    if (inlen) {
        assert(outsz < INT_MAX);
        outl = outsz;
        if (!EVP_CipherUpdate(m_context, out, &outl, (const unsigned char *)input.data(), inlen)) {
            AEAD_TRACE("Failed to obtain the crypted output");
            return -EINVAL; // static_cast<int>(ERR_get_error());
        }
        assert(outsz >= outl);
        out += outl, outsz -= outl;
    }

    if (!EVP_CIPHER_CTX_encrypting(m_context)) {
        /* Set the tag */
        if (!EVP_CIPHER_CTX_ctrl(m_context, EVP_CTRL_GCM_SET_TAG, taglen,
                                 (void *)((unsigned char *)input.data() + inlen))) {
            AEAD_TRACE("Failed to get the tag");
            return -EINVAL; // static_cast<int>(ERR_get_error());
        }
    }

    /**
     * Finalise the cryption. Normally output bytes may be written at this stage,
     * but this does not occur in GCM mode
     */
    outl = outsz;
    if (!EVP_CipherFinal_ex(m_context, out, &outl)) {
        AEAD_TRACE("Failed to finalise the cryption");
        return -EINVAL; // static_cast<int>(ERR_get_error());
    }
    assert(outsz >= outl);
    out += outl, outsz -= outl;

    if (EVP_CIPHER_CTX_encrypting(m_context)) {
        /* Get the tag */
        if (!EVP_CIPHER_CTX_ctrl(m_context, EVP_CTRL_GCM_GET_TAG, taglen, out)) {
            AEAD_TRACE("Failed to get the tag");
            return -EINVAL; // static_cast<int>(ERR_get_error());
        }
        assert(outsz >= taglen);
        out += taglen, outsz -= taglen;
    }

    return 0;
}
