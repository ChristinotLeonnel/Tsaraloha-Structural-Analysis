#pragma once

// Chiffrement authentifié du contenu d'un .tsa protégé par mot de passe (BUG-167, format 3.2).
//
// Remplace l'ancien « chiffrement » par XOR avec le mot de passe (StandardEncryptionProvider, conservé pour LIRE les
// anciens fichiers, jamais pour écrire) : le mot de passe se retrouvait à partir de quelques octets connus du contenu, et
// aucune modification n'était détectée.
//
//   clé     = PBKDF2-HMAC-SHA-256(mot de passe UTF-8 NFC, sel aléatoire de 16 octets, itérations enregistrées), 32 octets
//   contenu = AES-256-GCM(clé, nonce aléatoire de 12 octets), étiquette d'authentification de 16 octets
//
// Disposition du bloc chiffré (payload du .tsa quand FLAG_ENCRYPTED_AEAD est posé) :
//   0  « TSAE » (4)   4 version = 1   5 dérivation = 1 (PBKDF2-HMAC-SHA-256)   6 chiffrement = 1 (AES-256-GCM)   7 réservé = 0
//   8  itérations (u32, petit-boutiste)   12 sel (16)   28 nonce (12)   40 contenu chiffré (n)   40 + n étiquette (16)
// Les 40 premiers octets (paramètres) sont authentifiés avec le contenu (données associées) : un paramètre modifié fait
// échouer le déchiffrement comme un mot de passe faux.
//
// Implémentation : CNG de Windows (bcrypt.dll, aucune dépendance ajoutée) ; ailleurs OpenSSL libcrypto quand CMake le trouve
// (TSA_HAS_OPENSSL_CRYPTO), sinon la protection est indisponible et le dit (available() == false), jamais un repli en clair.

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace TSA::IO::Crypto
{

constexpr uint32_t kDefaultIterations = 600000;   ///< PBKDF2-HMAC-SHA-256 : recommandation OWASP 2023
constexpr uint32_t kMinIterations = 10000;        ///< en deçà : fichier refusé (paramètre altéré)
constexpr uint32_t kMaxIterations = 10000000;     ///< au-delà : fichier refusé (déni de service à l'ouverture)
constexpr size_t kHeaderSize = 40;
constexpr size_t kTagSize = 16;

enum class Status
{
    Ok,
    WrongPassword,    ///< étiquette d'authentification refusée : mot de passe faux, ou contenu / paramètres modifiés
    Corrupt,          ///< bloc illisible (signature, version, tailles, itérations hors bornes)
    Unavailable       ///< aucune implémentation sur ce poste
};

/// Chiffrement authentifié disponible sur ce poste.
bool available();
/// Implémentation utilisée (« Windows CNG », « OpenSSL x.y », « aucune »).
std::string backendName();

/// Chiffre `plain` avec `password` (UTF-8, non vide) dans un bloc complet (en-tête, contenu, étiquette).
Status encrypt(const uint8_t* plain, size_t size, const std::string& password, std::vector<uint8_t>& out,
               uint32_t iterations = kDefaultIterations, std::string* error = nullptr);

/// Déchiffre un bloc produit par encrypt ; `out` n'est rempli qu'en cas de succès.
Status decrypt(const uint8_t* block, size_t size, const std::string& password, std::vector<uint8_t>& out);

/// Le bloc commence par la signature « TSAE » (contrôle rapide, sans déchiffrer).
bool looksEncrypted(const uint8_t* block, size_t size);

} // namespace TSA::IO::Crypto
