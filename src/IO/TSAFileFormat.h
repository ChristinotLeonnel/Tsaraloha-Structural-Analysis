#pragma once

#include <cstdint>
#include <ProductIdentity.h>
#include <string>
#include <vector>

namespace TSA::IO
{

// -----------------------------------------------------------------------------
// Signatures (Little-Endian) : 'TSAF' (TSA File) = 0x46415354.
// Chaque produit écrit sa signature native (<ProductIdentity.h> : TSA → TSAF, TSALab → TSLB) et
// peut lire un format importé (TSALab lit les .tsa de TSA sans jamais les réécrire). Même conteneur
// à chunks pour tous les produits.
// -----------------------------------------------------------------------------
constexpr uint32_t TSA_FILE_MAGIC = 0x46415354;
constexpr uint32_t NATIVE_FILE_MAGIC = TSA::Product::kNativeFileMagic;

/// Signature lisible par le produit compilé (native, ou format importé s'il y en a un).
constexpr bool isReadableFileMagic(uint32_t magic)
{
    return magic == NATIVE_FILE_MAGIC || (TSA::Product::kLegacyFileMagic != 0 && magic == TSA::Product::kLegacyFileMagic);
}

// -----------------------------------------------------------------------------
// Versioning du format
// -----------------------------------------------------------------------------
constexpr uint16_t TSA_FORMAT_VERSION_MAJOR = 1;
constexpr uint16_t TSA_FORMAT_VERSION_MINOR = 7; // 1.1 : chunk LOAD ; 1.2 : bloc d'aperçu après le payload (TSAPreviewBlock.h) ; 1.3 : chunk BIMM ; 1.4 : chunk SETT ; 1.5 : chunk TOPO ; 1.6 : chunk DIMS ; 1.7 : chunk XTND

// Version de disposition interne du chunk LOAD (indépendante de la version du format)
constexpr uint32_t LOAD_CHUNK_LAYOUT_VERSION = 1;

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
    FLAG_HAS_THUMBNAIL = 0x00000010, // Contient une miniature 3D embarquée (PNG, chunk THMB)
    FLAG_HAS_PREVIEW_BLOCK = 0x00000020 // 1.2 : aperçu PNG non compressé après le payload (Explorateur Windows)
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
constexpr uint32_t CHUNK_SUPP = 0x50505553; // 'SUPP' : Appuis 3D et liaisons (6 DDL, raideurs, orientation)
constexpr uint32_t CHUNK_BARS = 0x53524142; // 'BARS' : Poutres & barres (section, matériau, gamma, excentrement, relâchements)
constexpr uint32_t CHUNK_COLS = 0x534C4F43; // 'COLS' : Poteaux verticaux et inclinés
constexpr uint32_t CHUNK_SLAB = 0x42414C53; // 'SLAB' : Dalles & planchers surfaciques (épaisseur, portance)
constexpr uint32_t CHUNK_WALL = 0x4C4C4157; // 'WALL' : Voiles et murs porteurs
constexpr uint32_t CHUNK_FNDN = 0x4E444E46; // 'FNDN' : Fondations (semelles isolées/filantes, radiers, pieux)
constexpr uint32_t CHUNK_TRUS = 0x53555254; // 'TRUS' : Barres de treillis & contreventements
constexpr uint32_t CHUNK_CABL = 0x4C424143; // 'CABL' : Câbles, haubans, suspentes et systèmes de tension
constexpr uint32_t CHUNK_LOAD = 0x44414F4C; // 'LOAD' : Cas de charges, combinaisons & chargements
constexpr uint32_t CHUNK_SETT = 0x54544553; // 'SETT' : Paramètres d'analyse (AnalysisContext) — JSON UTF-8 versionné (1.4)
constexpr uint32_t CHUNK_TOPO = 0x4F504F54; // 'TOPO' : Topologie et numérotation (TopologySettings) — JSON UTF-8 versionné (1.5)
constexpr uint32_t CHUNK_XTND = 0x444E5458; // 'XTND' : données d'échange non interprétées (TSA3D) — JSON UTF-8 opaque (1.7)
constexpr uint32_t CHUNK_DIMS = 0x534D4944; // 'DIMS' : Cotations 3D (DimensionSet : style + ancrages) — JSON UTF-8 versionné (1.6)
constexpr uint32_t CHUNK_RSLT = 0x544C5352; // 'RSLT' : Résultats de calcul EF (déplacements, efforts, contraintes)
constexpr uint32_t CHUNK_SNAP = 0x50414E53; // 'SNAP' : Snapshots mécaniques immuables de calcul et métadonnées d'extensions
constexpr uint32_t CHUNK_BIMM = 0x4D4D4942; // 'BIMM' : Couche BIM (produits physiques, GlobalId, Psets, mapping physique → analytique) — JSON UTF-8 versionné (1.3)

#pragma pack(push, 1)

/**
 * @brief En-tête binaire fixe du fichier .tsa (256 octets fixes)
 */
struct TSAFileHeader
{
    uint32_t magic = NATIVE_FILE_MAGIC;
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

    // Zone de réserve d'extensibilité (en-tête packé : 272 octets au total)
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
namespace Detail
{
inline const uint32_t* crc32Table()
{
    // Table du polynôme réfléchi 0xEDB88320, construite une seule fois (initialisation statique
    // thread-safe en C++11).
    static const auto table = [] {
        struct Table { uint32_t v[256]; } t{};
        for (uint32_t i = 0; i < 256; ++i)
        {
            uint32_t c = i;
            for (int j = 0; j < 8; ++j)
                c = (c & 1) ? ((c >> 1) ^ 0xEDB88320u) : (c >> 1);
            t.v[i] = c;
        }
        return t;
    }();
    return table.v;
}
} // namespace Detail

inline uint32_t computeCRC32(const uint8_t* data, size_t length)
{
    // Variante par table (un accès table par octet au lieu de 8 itérations) : résultat identique
    // à l'implémentation bit à bit d'origine, fichiers existants inchangés.
    const uint32_t* table = Detail::crc32Table();
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < length; ++i)
    {
        crc = table[(crc ^ data[i]) & 0xFFu] ^ (crc >> 8);
    }
    return ~crc;
}

} // namespace TSA::IO
