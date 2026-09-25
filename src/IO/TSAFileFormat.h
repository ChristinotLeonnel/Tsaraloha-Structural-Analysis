#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace TSA::IO
{

// -----------------------------------------------------------------------------
// Magic Signature: 'TSAF' (TSA File) en Little-Endian = 0x46415354
// -----------------------------------------------------------------------------
constexpr uint32_t TSA_FILE_MAGIC = 0x46415354;

// -----------------------------------------------------------------------------
// Versioning du format
// -----------------------------------------------------------------------------
constexpr uint16_t TSA_FORMAT_VERSION_MAJOR = 1;
constexpr uint16_t TSA_FORMAT_VERSION_MINOR = 0;

// Application Version
constexpr uint32_t TSA_APP_VERSION_MAJOR = 1;
constexpr uint32_t TSA_APP_VERSION_MINOR = 0;
constexpr uint32_t TSA_APP_VERSION_PATCH = 0;

// -----------------------------------------------------------------------------
// Flags binaires du Header
// -----------------------------------------------------------------------------
enum TSAFileFlags : uint32_t
{
    FLAG_NONE       = 0x00000000,
    FLAG_COMPRESSED = 0x00000001, // Données compressées (zlib / deflate)
    FLAG_ENCRYPTED  = 0x00000002, // Données protégées / chiffrées
    FLAG_HAS_MESH      = 0x00000004, // Contient un maillage EF pré-généré
    FLAG_HAS_RESULT    = 0x00000008, // Contient des résultats de calcul EF
    FLAG_HAS_THUMBNAIL = 0x00000010  // Contient une miniature 3D embarquée (PNG)
};

// -----------------------------------------------------------------------------
// Limites de sécurité du lecteur (Anti-Crash / Buffer-Overflow / DoS)
// -----------------------------------------------------------------------------
constexpr uint32_t MAX_SAFE_NODES       = 1000000;
constexpr uint32_t MAX_SAFE_ELEMENTS    = 1000000;
constexpr uint32_t MAX_SAFE_SECTIONS    = 100000;
constexpr uint32_t MAX_SAFE_MATERIALS   = 100000;
constexpr uint32_t MAX_SAFE_STRING_LEN  = 65536;
constexpr uint64_t MAX_SAFE_PAYLOAD_SIZE = 1024ULL * 1024ULL * 1024ULL; // 1 Go

// -----------------------------------------------------------------------------
// Identifiants de Chunks extensibles (FourCC 4 octets)
// -----------------------------------------------------------------------------
constexpr uint32_t CHUNK_PROJ = 0x4A4F5250; // 'PROJ' : Métadonnées du projet, unités, code
constexpr uint32_t CHUNK_THMB = 0x424D4854; // 'THMB' : Miniature 3D (Thumbnail) PNG/JPEG
constexpr uint32_t CHUNK_COOR = 0x524F4F43; // 'COOR' : Système de coordonnées (axes X, Y, Z, niveaux)
constexpr uint32_t CHUNK_GRID = 0x44495247; // 'GRID' : Grilles 3D et repères cartésiens / cylindriques
constexpr uint32_t CHUNK_NODE = 0x45444F4E; // 'NODE' : Nœuds structuraux (id, x, y, z, support)
constexpr uint32_t CHUNK_BARS = 0x53524142; // 'BARS' : Poutres & barres (section, matériau, gamma, excentrement, relâchements)
constexpr uint32_t CHUNK_COLS = 0x534C4F43; // 'COLS' : Poteaux verticaux et inclinés
constexpr uint32_t CHUNK_SLAB = 0x42414C53; // 'SLAB' : Dalles & planchers surfaciques (épaisseur, portance)
constexpr uint32_t CHUNK_WALL = 0x4C4C4157; // 'WALL' : Voiles et murs porteurs
constexpr uint32_t CHUNK_FNDN = 0x4E444E46; // 'FNDN' : Fondations (semelles isolées/filantes, radiers, pieux)
constexpr uint32_t CHUNK_TRUS = 0x53555254; // 'TRUS' : Barres de treillis & contreventements
constexpr uint32_t CHUNK_LOAD = 0x44414F4C; // 'LOAD' : Cas de charges, combinaisons & chargements
constexpr uint32_t CHUNK_SETT = 0x54544553; // 'SETT' : Paramètres d'analyse & calcul
constexpr uint32_t CHUNK_RSLT = 0x544C5352; // 'RSLT' : Résultats de calcul EF (déplacements, efforts, contraintes)

#pragma pack(push, 1)

/**
 * @brief En-tête binaire fixe du fichier .tsa (256 octets fixes)
 */
struct TSAFileHeader
{
    uint32_t magic = TSA_FILE_MAGIC;
    uint16_t versionMajor = TSA_FORMAT_VERSION_MAJOR;
    uint16_t versionMinor = TSA_FORMAT_VERSION_MINOR;
    uint32_t appVersionMajor = TSA_APP_VERSION_MAJOR;
    uint32_t appVersionMinor = TSA_APP_VERSION_MINOR;
    uint32_t appVersionPatch = TSA_APP_VERSION_PATCH;
    uint32_t flags = FLAG_NONE;
    uint32_t headerSize = sizeof(TSAFileHeader);
    uint32_t checksumCRC32 = 0;     // Somme de contrôle CRC32 calculée sur les données du payload
    uint64_t fileSize = 0;          // Taille totale du fichier en octets
    uint64_t uncompressedSize = 0;  // Taille décompressée du payload en octets
    uint64_t payloadOffset = sizeof(TSAFileHeader); // Décalage vers le 1er chunk

    // Informations Projet
    char projectName[64] = "Projet TSA";
    char author[64] = "TSA Engineer";
    char creationTimestamp[24] = "";
    char lastModifiedTimestamp[24] = "";

    // Zone de réserve d'extensibilité (alignement à 256 octets)
    uint8_t reserved[40] = {0};
};

/**
 * @brief En-tête d'un bloc extensible (Chunk)
 */
struct TSAChunkHeader
{
    uint32_t chunkId = 0;       // FourCC (ex: 'NODE', 'BARS', etc.)
    uint32_t chunkSize = 0;     // Longueur des données du chunk en octets
    uint32_t elementCount = 0;  // Nombre d'éléments contenus dans le bloc
    uint32_t reserved = 0;
};

#pragma pack(pop)

// -----------------------------------------------------------------------------
// Calcul de la somme de contrôle CRC32 (Standard IEEE 802.3)
// -----------------------------------------------------------------------------
inline uint32_t computeCRC32(const uint8_t* data, size_t length)
{
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < length; ++i)
    {
        crc ^= data[i];
        for (int j = 0; j < 8; ++j)
        {
            if (crc & 1)
                crc = (crc >> 1) ^ 0xEDB88320;
            else
                crc >>= 1;
        }
    }
    return ~crc;
}

} // namespace TSA::IO
