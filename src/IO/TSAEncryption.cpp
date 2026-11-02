#include "TSAEncryption.h"

#include <cstring>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <bcrypt.h>
#ifndef NT_SUCCESS
#define NT_SUCCESS(status) (((NTSTATUS)(status)) >= 0)
#endif
#elif defined(TSA_HAS_OPENSSL_CRYPTO)
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/opensslv.h>
#include <openssl/rand.h>
#endif

namespace TSA::IO::Crypto
{
namespace
{
constexpr uint8_t kMagic[4] = { 'T', 'S', 'A', 'E' };
constexpr uint8_t kVersion = 1;
constexpr uint8_t kKdfPbkdf2Sha256 = 1;
constexpr uint8_t kCipherAes256Gcm = 1;
constexpr size_t kSaltSize = 16;
constexpr size_t kNonceSize = 12;
constexpr size_t kKeySize = 32;
constexpr size_t kOffsetIterations = 8;
constexpr size_t kOffsetSalt = 12;
constexpr size_t kOffsetNonce = 28;

/// Effacement d'un secret que le compilateur ne peut pas supprimer.
void wipe(void* p, size_t n)
{
#ifdef _WIN32
    SecureZeroMemory(p, n);
#elif defined(TSA_HAS_OPENSSL_CRYPTO)
    OPENSSL_cleanse(p, n);
#else
    volatile uint8_t* v = static_cast<volatile uint8_t*>(p);
    while (n--) *v++ = 0;
#endif
}

struct KeyGuard
{
    uint8_t key[kKeySize] = {};
    ~KeyGuard() { wipe(key, sizeof(key)); }
};

void putU32(uint8_t* p, uint32_t v)
{
    p[0] = uint8_t(v);
    p[1] = uint8_t(v >> 8);
    p[2] = uint8_t(v >> 16);
    p[3] = uint8_t(v >> 24);
}

uint32_t getU32(const uint8_t* p)
{
    return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}

#ifdef _WIN32
// ---------------------------------------------------------------------------------------------------- Windows CNG
struct AlgHandle
{
    BCRYPT_ALG_HANDLE h = nullptr;
    ~AlgHandle()
    {
        if (h) BCryptCloseAlgorithmProvider(h, 0);
    }
};
struct KeyHandle
{
    BCRYPT_KEY_HANDLE h = nullptr;
    ~KeyHandle()
    {
        if (h) BCryptDestroyKey(h);
    }
};

bool randomBytes(uint8_t* p, size_t n)
{
    return NT_SUCCESS(BCryptGenRandom(nullptr, p, static_cast<ULONG>(n), BCRYPT_USE_SYSTEM_PREFERRED_RNG));
}

bool deriveKey(const std::string& password, const uint8_t* salt, uint32_t iterations, uint8_t* key)
{
    AlgHandle prf;
    if (!NT_SUCCESS(BCryptOpenAlgorithmProvider(&prf.h, BCRYPT_SHA256_ALGORITHM, nullptr, BCRYPT_ALG_HANDLE_HMAC_FLAG))) return false;
    return NT_SUCCESS(BCryptDeriveKeyPBKDF2(prf.h, reinterpret_cast<PUCHAR>(const_cast<char*>(password.data())),
                                            static_cast<ULONG>(password.size()), const_cast<PUCHAR>(salt), static_cast<ULONG>(kSaltSize),
                                            iterations, key, static_cast<ULONG>(kKeySize), 0));
}

bool openGcmKey(const uint8_t* key, AlgHandle& aes, KeyHandle& handle)
{
    if (!NT_SUCCESS(BCryptOpenAlgorithmProvider(&aes.h, BCRYPT_AES_ALGORITHM, nullptr, 0))) return false;
    if (!NT_SUCCESS(BCryptSetProperty(aes.h, BCRYPT_CHAINING_MODE, reinterpret_cast<PUCHAR>(const_cast<wchar_t*>(BCRYPT_CHAIN_MODE_GCM)),
                                      static_cast<ULONG>(sizeof(BCRYPT_CHAIN_MODE_GCM)), 0)))
        return false;
    return NT_SUCCESS(BCryptGenerateSymmetricKey(aes.h, &handle.h, nullptr, 0, const_cast<PUCHAR>(key), static_cast<ULONG>(kKeySize), 0));
}

/// AES-256-GCM ; `tag` écrit (chiffrement) ou vérifié (déchiffrement). Retour : 1 succès, 0 étiquette refusée, -1 erreur.
int gcm(bool encrypting, const uint8_t* key, const uint8_t* nonce, const uint8_t* aad, size_t aadSize, const uint8_t* in, size_t size,
        uint8_t* out, uint8_t* tag)
{
    if (size > 0xFFFFFFFFull) return -1;
    AlgHandle aes;
    KeyHandle k;
    if (!openGcmKey(key, aes, k)) return -1;
    BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO info;
    BCRYPT_INIT_AUTH_MODE_INFO(info);
    info.pbNonce = const_cast<PUCHAR>(nonce);
    info.cbNonce = static_cast<ULONG>(kNonceSize);
    info.pbAuthData = const_cast<PUCHAR>(aad);
    info.cbAuthData = static_cast<ULONG>(aadSize);
    info.pbTag = tag;
    info.cbTag = static_cast<ULONG>(kTagSize);
    ULONG written = 0;
    const NTSTATUS st = encrypting
        ? BCryptEncrypt(k.h, const_cast<PUCHAR>(in), static_cast<ULONG>(size), &info, nullptr, 0, out, static_cast<ULONG>(size), &written, 0)
        : BCryptDecrypt(k.h, const_cast<PUCHAR>(in), static_cast<ULONG>(size), &info, nullptr, 0, out, static_cast<ULONG>(size), &written, 0);
    if (st == static_cast<NTSTATUS>(0xC000A002L)) return 0;   // STATUS_AUTH_TAG_MISMATCH
    return NT_SUCCESS(st) && written == size ? 1 : -1;
}

#elif defined(TSA_HAS_OPENSSL_CRYPTO)
// ---------------------------------------------------------------------------------------------------- OpenSSL libcrypto
bool randomBytes(uint8_t* p, size_t n)
{
    return RAND_bytes(p, static_cast<int>(n)) == 1;
}

bool deriveKey(const std::string& password, const uint8_t* salt, uint32_t iterations, uint8_t* key)
{
    return PKCS5_PBKDF2_HMAC(password.data(), static_cast<int>(password.size()), salt, static_cast<int>(kSaltSize),
                             static_cast<int>(iterations), EVP_sha256(), static_cast<int>(kKeySize), key)
           == 1;
}

int gcm(bool encrypting, const uint8_t* key, const uint8_t* nonce, const uint8_t* aad, size_t aadSize, const uint8_t* in, size_t size,
        uint8_t* out, uint8_t* tag)
{
    if (size > 0x7FFFFFFFull) return -1;
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return -1;
    int result = -1;
    int len = 0;
    if (encrypting)
    {
        if (EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) == 1
            && EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, static_cast<int>(kNonceSize), nullptr) == 1
            && EVP_EncryptInit_ex(ctx, nullptr, nullptr, key, nonce) == 1
            && EVP_EncryptUpdate(ctx, nullptr, &len, aad, static_cast<int>(aadSize)) == 1
            && EVP_EncryptUpdate(ctx, out, &len, in, static_cast<int>(size)) == 1 && EVP_EncryptFinal_ex(ctx, out + len, &len) == 1
            && EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, static_cast<int>(kTagSize), tag) == 1)
            result = 1;
    }
    else if (EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) == 1
             && EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, static_cast<int>(kNonceSize), nullptr) == 1
             && EVP_DecryptInit_ex(ctx, nullptr, nullptr, key, nonce) == 1
             && EVP_DecryptUpdate(ctx, nullptr, &len, aad, static_cast<int>(aadSize)) == 1
             && EVP_DecryptUpdate(ctx, out, &len, in, static_cast<int>(size)) == 1
             && EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, static_cast<int>(kTagSize), tag) == 1)
    {
        result = EVP_DecryptFinal_ex(ctx, out + len, &len) == 1 ? 1 : 0;   // 0 : étiquette refusée
    }
    EVP_CIPHER_CTX_free(ctx);
    return result;
}
#endif
} // namespace

bool available()
{
#if defined(_WIN32) || defined(TSA_HAS_OPENSSL_CRYPTO)
    return true;
#else
    return false;
#endif
}

std::string backendName()
{
#ifdef _WIN32
    return "Windows CNG (AES-256-GCM, PBKDF2-HMAC-SHA-256)";
#elif defined(TSA_HAS_OPENSSL_CRYPTO)
    return std::string(OPENSSL_VERSION_TEXT) + " (AES-256-GCM, PBKDF2-HMAC-SHA-256)";
#else
    return "aucune";
#endif
}

bool looksEncrypted(const uint8_t* block, size_t size)
{
    return block && size >= kHeaderSize + kTagSize && std::memcmp(block, kMagic, sizeof(kMagic)) == 0;
}

Status encrypt(const uint8_t* plain, size_t size, const std::string& password, std::vector<uint8_t>& out, uint32_t iterations,
               std::string* error)
{
#if defined(_WIN32) || defined(TSA_HAS_OPENSSL_CRYPTO)
    auto fail = [&](const char* what) {
        if (error) *error = what;
        out.clear();
        return Status::Corrupt;
    };
    if (password.empty()) return fail("mot de passe vide");
    if (iterations < kMinIterations || iterations > kMaxIterations) return fail("nombre d'itérations hors bornes");
    out.assign(kHeaderSize + size + kTagSize, 0);
    uint8_t* h = out.data();
    std::memcpy(h, kMagic, sizeof(kMagic));
    h[4] = kVersion;
    h[5] = kKdfPbkdf2Sha256;
    h[6] = kCipherAes256Gcm;
    h[7] = 0;
    putU32(h + kOffsetIterations, iterations);
    if (!randomBytes(h + kOffsetSalt, kSaltSize) || !randomBytes(h + kOffsetNonce, kNonceSize)) return fail("générateur aléatoire indisponible");
    KeyGuard key;
    if (!deriveKey(password, h + kOffsetSalt, iterations, key.key)) return fail("dérivation de la clé impossible");
    if (gcm(true, key.key, h + kOffsetNonce, h, kHeaderSize, plain, size, h + kHeaderSize, h + kHeaderSize + size) != 1)
        return fail("chiffrement AES-GCM impossible");
    return Status::Ok;
#else
    (void)plain;
    (void)size;
    (void)password;
    (void)iterations;
    out.clear();
    if (error) *error = "aucune implémentation du chiffrement sur ce poste";
    return Status::Unavailable;
#endif
}

Status decrypt(const uint8_t* block, size_t size, const std::string& password, std::vector<uint8_t>& out)
{
#if defined(_WIN32) || defined(TSA_HAS_OPENSSL_CRYPTO)
    if (!looksEncrypted(block, size) || block[4] != kVersion || block[5] != kKdfPbkdf2Sha256 || block[6] != kCipherAes256Gcm)
        return Status::Corrupt;
    const uint32_t iterations = getU32(block + kOffsetIterations);
    if (iterations < kMinIterations || iterations > kMaxIterations) return Status::Corrupt;
    if (password.empty()) return Status::WrongPassword;
    const size_t n = size - kHeaderSize - kTagSize;
    KeyGuard key;
    if (!deriveKey(password, block + kOffsetSalt, iterations, key.key)) return Status::Corrupt;
    std::vector<uint8_t> plain(n);
    uint8_t tag[kTagSize];
    std::memcpy(tag, block + kHeaderSize + n, kTagSize);
    const int r = gcm(false, key.key, block + kOffsetNonce, block, kHeaderSize, block + kHeaderSize, n, plain.data(), tag);
    if (r != 1)
    {
        wipe(plain.data(), plain.size());
        return r == 0 ? Status::WrongPassword : Status::Corrupt;
    }
    out = std::move(plain);
    return Status::Ok;
#else
    (void)block;
    (void)size;
    (void)password;
    (void)out;
    return Status::Unavailable;
#endif
}

} // namespace TSA::IO::Crypto
