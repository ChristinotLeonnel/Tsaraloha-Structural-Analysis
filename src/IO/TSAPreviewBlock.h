#pragma once

// Bloc d'aperçu non compressé placé APRÈS le payload d'un fichier .tsa (format ≥ 1.2).
//
//   [ en-tête 272 o ][ payload (compressé) … header.fileSize ][ bloc d'aperçu ][ PNG ]
//
// Comme l'aperçu d'en-tête des fichiers DWG, il permet à l'Explorateur Windows (extension
// TSAThumbnailProvider) d'obtenir la miniature en lisant ~300 octets + le PNG, sans décompresser
// ni analyser le modèle. L'image est celle du viewport TSA au moment de l'enregistrement
// (OccView::captureViewImage) : aucun second moteur de rendu.
//
// En-tête C++ standard uniquement : partagé par TSA, l'extension Explorer et les tests.

#include "TSAFileFormat.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace TSA::IO
{

constexpr uint32_t TSA_PREVIEW_MAGIC = 0x57565250;        // 'PRVW'
constexpr uint32_t TSA_PREVIEW_FORMAT_PNG = 1;
constexpr uint32_t TSA_PREVIEW_MAX_BYTES = 8u * 1024u * 1024u; // borne de sécurité côté lecteur
constexpr uint32_t TSA_PREVIEW_MAX_SIDE = 4096;

#pragma pack(push, 1)
struct TSAPreviewBlockHeader
{
    uint32_t magic = TSA_PREVIEW_MAGIC;
    uint32_t blockVersion = 1;
    uint32_t format = TSA_PREVIEW_FORMAT_PNG;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t dataSize = 0;  // octets d'image qui suivent immédiatement ce bloc
    uint32_t reserved[2] = { 0, 0 };
};
#pragma pack(pop)

static_assert(sizeof(TSAPreviewBlockHeader) == 32, "Disposition binaire du bloc d'aperçu figée");
static_assert(sizeof(TSAFileHeader) == 272, "Disposition binaire de l'en-tête .tsa figée (272 o, packée)");

struct TSAPreviewLocation
{
    uint64_t imageOffset = 0;
    uint32_t imageSize = 0;
    uint32_t width = 0;
    uint32_t height = 0;
};

/// Valide l'en-tête d'un fichier .tsa et indique où lire le bloc d'aperçu.
/// Renvoie false (sans exception) pour tout fichier non TSA, tronqué, d'une version majeure plus
/// récente ou sans aperçu : l'appelant affiche alors l'icône par défaut.
inline bool locatePreviewBlock(const TSAFileHeader& header, uint64_t streamLength, uint64_t* blockOffset)
{
    if (header.magic != TSA_FILE_MAGIC) return false;
    if (header.versionMajor > TSA_FORMAT_VERSION_MAJOR) return false;
    if ((header.flags & FLAG_HAS_PREVIEW_BLOCK) == 0) return false;
    if (header.payloadOffset < sizeof(TSAFileHeader) || header.fileSize < header.payloadOffset) return false;
    if (header.fileSize + sizeof(TSAPreviewBlockHeader) > streamLength) return false;
    *blockOffset = header.fileSize;
    return true;
}

/// Valide le bloc d'aperçu lu à l'adresse donnée par locatePreviewBlock().
inline bool validatePreviewBlock(const TSAPreviewBlockHeader& block, uint64_t blockOffset, uint64_t streamLength,
                                 TSAPreviewLocation* out)
{
    if (block.magic != TSA_PREVIEW_MAGIC || block.format != TSA_PREVIEW_FORMAT_PNG) return false;
    if (block.dataSize == 0 || block.dataSize > TSA_PREVIEW_MAX_BYTES) return false;
    if (block.width == 0 || block.height == 0 || block.width > TSA_PREVIEW_MAX_SIDE || block.height > TSA_PREVIEW_MAX_SIDE) return false;
    const uint64_t imageOffset = blockOffset + sizeof(TSAPreviewBlockHeader);
    if (imageOffset + block.dataSize > streamLength) return false;
    out->imageOffset = imageOffset;
    out->imageSize = block.dataSize;
    out->width = block.width;
    out->height = block.height;
    return true;
}

} // namespace TSA::IO
