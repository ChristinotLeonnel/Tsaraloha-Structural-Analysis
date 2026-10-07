#include "TSAFile.h"
#include "TSAFile_BinaryUtils.h"

#include "../Coordinate/CoordinateSystem.h"
#include "../Grid/GridManager.h"
#include "../Model/Node.h"
#include "../Model/Beam.h"
#include "../Model/Column.h"
#include "../Model/Slab.h"
#include "../Model/Wall.h"
#include "../Model/Foundation.h"
#include "../Model/TrussMember.h"
#include "../Model/Cable/Cable.h"
#include "../Model/Cable/CableDefinition.h"

#include <QJsonDocument>

#include <vector>
#include <map>
#include <string>
#include <cstring>
#include <algorithm>

namespace TSA::IO
{
using namespace Detail;

void TSAFileWriter::writeProjectChunk(std::vector<uint8_t>& buffer, const std::string& projectName, const std::string& author)
{
    std::vector<uint8_t> chunkData;
    writeString(chunkData, projectName);
    writeString(chunkData, author);
    writeString(chunkData, "SI_METRIC"); // Unités : mètres, Pa, N, kg
    writeDouble(chunkData, 1.0);         // Échelle des longueurs (1.0 = mètres)

    TSAChunkHeader ch;
    ch.chunkId = CHUNK_PROJ;
    ch.chunkSize = static_cast<uint32_t>(chunkData.size());
    ch.elementCount = 1;

    const uint8_t* chBytes = reinterpret_cast<const uint8_t*>(&ch);
    buffer.insert(buffer.end(), chBytes, chBytes + sizeof(ch));
    buffer.insert(buffer.end(), chunkData.begin(), chunkData.end());
}

void TSAFileWriter::writeThumbnailChunk(std::vector<uint8_t>& buffer, const QByteArray& pngData)
{
    if (pngData.isEmpty()) return;

    std::vector<uint8_t> chunkData;
    uint32_t width = 512, height = 512, formatType = 1; // 1 = PNG
    uint32_t dataLen = static_cast<uint32_t>(pngData.size());
    writeU32(chunkData, width);
    writeU32(chunkData, height);
    writeU32(chunkData, formatType);
    writeU32(chunkData, dataLen);
    chunkData.insert(chunkData.end(), reinterpret_cast<const uint8_t*>(pngData.constData()),
                     reinterpret_cast<const uint8_t*>(pngData.constData()) + pngData.size());

    TSAChunkHeader ch;
    ch.chunkId = CHUNK_THMB;
    ch.chunkSize = static_cast<uint32_t>(chunkData.size());
    ch.elementCount = 1;

    const uint8_t* chBytes = reinterpret_cast<const uint8_t*>(&ch);
    buffer.insert(buffer.end(), chBytes, chBytes + sizeof(ch));
    buffer.insert(buffer.end(), chunkData.begin(), chunkData.end());
}

void TSAFileWriter::writeCoordinateChunk(std::vector<uint8_t>& buffer, const TSA::Coordinate::CoordinateSystem* cs)
{
    if (!cs) return;
    std::string json = cs->serializeToJson();
    std::vector<uint8_t> chunkData;
    writeString(chunkData, json);

    TSAChunkHeader ch;
    ch.chunkId = CHUNK_COOR;
    ch.chunkSize = static_cast<uint32_t>(chunkData.size());
    ch.elementCount = 1;

    const uint8_t* chBytes = reinterpret_cast<const uint8_t*>(&ch);
    buffer.insert(buffer.end(), chBytes, chBytes + sizeof(ch));
    buffer.insert(buffer.end(), chunkData.begin(), chunkData.end());
}

void TSAFileWriter::writeGridChunk(std::vector<uint8_t>& buffer, const TSA::Grid::GridManager* gm)
{
    if (!gm) return;
    std::string json = gm->serializeToJson();
    std::vector<uint8_t> chunkData;
    writeString(chunkData, json);

    TSAChunkHeader ch;
    ch.chunkId = CHUNK_GRID;
    ch.chunkSize = static_cast<uint32_t>(chunkData.size());
    ch.elementCount = static_cast<uint32_t>(gm->grids().size());

    const uint8_t* chBytes = reinterpret_cast<const uint8_t*>(&ch);
    buffer.insert(buffer.end(), chBytes, chBytes + sizeof(ch));
    buffer.insert(buffer.end(), chunkData.begin(), chunkData.end());
}

void TSAFileWriter::writeNodeChunk(std::vector<uint8_t>& buffer, const std::map<int, TSA::Model::Node>& nodes)
{
    std::vector<uint8_t> chunkData;
    for (const auto& [id, n] : nodes)
    {
        writeI32(chunkData, n.id());
        writeString(chunkData, n.name());
        writeDouble(chunkData, n.x());
        writeDouble(chunkData, n.y());
        writeDouble(chunkData, n.z());
        writeString(chunkData, n.levelId());
        writeU8(chunkData, static_cast<uint8_t>(n.supportType()));
        writeString(chunkData, n.color());
    }

    TSAChunkHeader ch;
    ch.chunkId = CHUNK_NODE;
    ch.chunkSize = static_cast<uint32_t>(chunkData.size());
    ch.elementCount = static_cast<uint32_t>(nodes.size());

    const uint8_t* chBytes = reinterpret_cast<const uint8_t*>(&ch);
    buffer.insert(buffer.end(), chBytes, chBytes + sizeof(ch));
    buffer.insert(buffer.end(), chunkData.begin(), chunkData.end());
}

void TSAFileWriter::writeSupportChunk(std::vector<uint8_t>& buffer, const std::map<int, TSA::Model::Node>& nodes)
{
    std::vector<uint8_t> chunkData;
    uint32_t count = 0;
    for (const auto& [id, n] : nodes)
    {
        const auto& s = n.support();
        if (s.isFree()) continue;
        ++count;
        writeI32(chunkData, n.id());
        writeU8(chunkData, static_cast<uint8_t>(s.tx()));
        writeU8(chunkData, static_cast<uint8_t>(s.ty()));
        writeU8(chunkData, static_cast<uint8_t>(s.tz()));
        writeU8(chunkData, static_cast<uint8_t>(s.rx()));
        writeU8(chunkData, static_cast<uint8_t>(s.ry()));
        writeU8(chunkData, static_cast<uint8_t>(s.rz()));
        writeDouble(chunkData, s.kx());
        writeDouble(chunkData, s.ky());
        writeDouble(chunkData, s.kz());
        writeDouble(chunkData, s.krx());
        writeDouble(chunkData, s.kry());
        writeDouble(chunkData, s.krz());
        writeU8(chunkData, static_cast<uint8_t>(s.orientationType()));
        writeDouble(chunkData, s.customDirX());
        writeDouble(chunkData, s.customDirY());
        writeDouble(chunkData, s.customDirZ());
        writeI32(chunkData, s.referenceElementId());
    }

    if (count == 0) return;

    TSAChunkHeader ch;
    ch.chunkId = CHUNK_SUPP;
    ch.chunkSize = static_cast<uint32_t>(chunkData.size());
    ch.elementCount = count;

    const uint8_t* chBytes = reinterpret_cast<const uint8_t*>(&ch);
    buffer.insert(buffer.end(), chBytes, chBytes + sizeof(ch));
    buffer.insert(buffer.end(), chunkData.begin(), chunkData.end());
}

void TSAFileWriter::writeBarChunk(std::vector<uint8_t>& buffer, const std::map<int, TSA::Model::Beam>& beams)
{
    std::vector<uint8_t> chunkData;
    for (const auto& [id, b] : beams)
    {
        writeI32(chunkData, b.id());
        writeString(chunkData, b.name());
        writeI32(chunkData, b.startNodeId());
        writeI32(chunkData, b.endNodeId());
        writeU8(chunkData, static_cast<uint8_t>(b.role()));
        serializeSection(chunkData, b.section());
        serializeMaterial(chunkData, b.material());
        writeDouble(chunkData, b.rotation());
        writeU8(chunkData, static_cast<uint8_t>(b.eccentricity()));

        const auto& sr = b.startRelease();
        writeU8(chunkData, sr.fx ? 1 : 0);
        writeU8(chunkData, sr.fy ? 1 : 0);
        writeU8(chunkData, sr.fz ? 1 : 0);
        writeU8(chunkData, sr.mx ? 1 : 0);
        writeU8(chunkData, sr.my ? 1 : 0);
        writeU8(chunkData, sr.mz ? 1 : 0);

        const auto& er = b.endRelease();
        writeU8(chunkData, er.fx ? 1 : 0);
        writeU8(chunkData, er.fy ? 1 : 0);
        writeU8(chunkData, er.fz ? 1 : 0);
        writeU8(chunkData, er.mx ? 1 : 0);
        writeU8(chunkData, er.my ? 1 : 0);
        writeU8(chunkData, er.mz ? 1 : 0);

        writeString(chunkData, b.color());
    }

    TSAChunkHeader ch;
    ch.chunkId = CHUNK_BARS;
    ch.chunkSize = static_cast<uint32_t>(chunkData.size());
    ch.elementCount = static_cast<uint32_t>(beams.size());

    const uint8_t* chBytes = reinterpret_cast<const uint8_t*>(&ch);
    buffer.insert(buffer.end(), chBytes, chBytes + sizeof(ch));
    buffer.insert(buffer.end(), chunkData.begin(), chunkData.end());
}

void TSAFileWriter::writeColumnChunk(std::vector<uint8_t>& buffer, const std::map<int, TSA::Model::Column>& columns)
{
    std::vector<uint8_t> chunkData;
    for (const auto& [id, c] : columns)
    {
        writeI32(chunkData, c.id());
        writeString(chunkData, c.name());
        writeI32(chunkData, c.startNodeId());
        writeI32(chunkData, c.endNodeId());
        serializeSection(chunkData, c.section());
        serializeMaterial(chunkData, c.material());
        writeDouble(chunkData, c.rotation());
        writeString(chunkData, c.color());
    }

    TSAChunkHeader ch;
    ch.chunkId = CHUNK_COLS;
    ch.chunkSize = static_cast<uint32_t>(chunkData.size());
    ch.elementCount = static_cast<uint32_t>(columns.size());

    const uint8_t* chBytes = reinterpret_cast<const uint8_t*>(&ch);
    buffer.insert(buffer.end(), chBytes, chBytes + sizeof(ch));
    buffer.insert(buffer.end(), chunkData.begin(), chunkData.end());
}

void TSAFileWriter::writeSlabChunk(std::vector<uint8_t>& buffer, const std::map<int, TSA::Model::Slab>& slabs)
{
    std::vector<uint8_t> chunkData;
    for (const auto& [id, s] : slabs)
    {
        writeI32(chunkData, s.id());
        writeString(chunkData, s.name());

        const auto& nids = s.nodeIds();
        writeU16(chunkData, static_cast<uint16_t>(nids.size()));
        for (int nid : nids)
        {
            writeI32(chunkData, nid);
        }

        writeDouble(chunkData, s.thickness());
        serializeMaterial(chunkData, s.material());
        writeU8(chunkData, static_cast<uint8_t>(s.slabType()));
        writeString(chunkData, s.color());
    }

    TSAChunkHeader ch;
    ch.chunkId = CHUNK_SLAB;
    ch.chunkSize = static_cast<uint32_t>(chunkData.size());
    ch.elementCount = static_cast<uint32_t>(slabs.size());

    const uint8_t* chBytes = reinterpret_cast<const uint8_t*>(&ch);
    buffer.insert(buffer.end(), chBytes, chBytes + sizeof(ch));
    buffer.insert(buffer.end(), chunkData.begin(), chunkData.end());
}

void TSAFileWriter::writeWallChunk(std::vector<uint8_t>& buffer, const std::map<int, TSA::Model::Wall>& walls)
{
    std::vector<uint8_t> chunkData;
    for (const auto& [id, w] : walls)
    {
        writeI32(chunkData, w.id());
        writeString(chunkData, w.name());
        writeI32(chunkData, w.startNodeId());
        writeI32(chunkData, w.endNodeId());
        writeDouble(chunkData, w.height());
        writeDouble(chunkData, w.thickness());
        writeDouble(chunkData, w.offset());
        serializeMaterial(chunkData, w.material());
        writeString(chunkData, w.color());
    }

    TSAChunkHeader ch;
    ch.chunkId = CHUNK_WALL;
    ch.chunkSize = static_cast<uint32_t>(chunkData.size());
    ch.elementCount = static_cast<uint32_t>(walls.size());

    const uint8_t* chBytes = reinterpret_cast<const uint8_t*>(&ch);
    buffer.insert(buffer.end(), chBytes, chBytes + sizeof(ch));
    buffer.insert(buffer.end(), chunkData.begin(), chunkData.end());
}

void TSAFileWriter::writeFoundationChunk(std::vector<uint8_t>& buffer, const std::map<int, TSA::Model::Foundation>& foundations)
{
    std::vector<uint8_t> chunkData;
    for (const auto& [id, f] : foundations)
    {
        writeI32(chunkData, f.id());
        writeString(chunkData, f.name());
        writeI32(chunkData, f.nodeId());
        writeU8(chunkData, static_cast<uint8_t>(f.foundationType()));
        writeDouble(chunkData, f.widthA());
        writeDouble(chunkData, f.lengthB());
        writeDouble(chunkData, f.heightH());
        serializeMaterial(chunkData, f.material());
        writeDouble(chunkData, f.soilBearingCapacity());
        writeString(chunkData, f.color());
    }

    TSAChunkHeader ch;
    ch.chunkId = CHUNK_FNDN;
    ch.chunkSize = static_cast<uint32_t>(chunkData.size());
    ch.elementCount = static_cast<uint32_t>(foundations.size());

    const uint8_t* chBytes = reinterpret_cast<const uint8_t*>(&ch);
    buffer.insert(buffer.end(), chBytes, chBytes + sizeof(ch));
    buffer.insert(buffer.end(), chunkData.begin(), chunkData.end());
}

void TSAFileWriter::writeTrussChunk(std::vector<uint8_t>& buffer, const std::map<int, TSA::Model::TrussMember>& trussMembers)
{
    std::vector<uint8_t> chunkData;
    for (const auto& [id, t] : trussMembers)
    {
        writeI32(chunkData, t.id());
        writeString(chunkData, t.name());
        writeI32(chunkData, t.startNodeId());
        writeI32(chunkData, t.endNodeId());
        writeU8(chunkData, static_cast<uint8_t>(t.role()));
        serializeSection(chunkData, t.section());
        serializeMaterial(chunkData, t.material());
        writeString(chunkData, t.color());
    }

    TSAChunkHeader ch;
    ch.chunkId = CHUNK_TRUS;
    ch.chunkSize = static_cast<uint32_t>(chunkData.size());
    ch.elementCount = static_cast<uint32_t>(trussMembers.size());

    const uint8_t* chBytes = reinterpret_cast<const uint8_t*>(&ch);
    buffer.insert(buffer.end(), chBytes, chBytes + sizeof(ch));
    buffer.insert(buffer.end(), chunkData.begin(), chunkData.end());
}

void TSAFileWriter::writeCableChunk(std::vector<uint8_t>& buffer, const std::map<int, TSA::Model::Cable>& cables)
{
    std::vector<uint8_t> chunkData;
    for (const auto& [id, c] : cables)
    {
        writeI32(chunkData, c.id());
        writeString(chunkData, c.name());
        writeI32(chunkData, c.startNodeId());
        writeI32(chunkData, c.endNodeId());
        writeU8(chunkData, static_cast<uint8_t>(c.type()));
        writeU8(chunkData, static_cast<uint8_t>(c.geometryMode()));

        // Definition
        const auto& def = c.definition();
        writeString(chunkData, def.id());
        writeString(chunkData, def.name());
        writeU8(chunkData, static_cast<uint8_t>(def.type()));
        writeString(chunkData, def.standardName());
        writeString(chunkData, def.standardVersion());
        writeString(chunkData, def.grade());
        writeDouble(chunkData, def.nominalDiameter());
        writeDouble(chunkData, def.metallicArea());
        writeDouble(chunkData, def.elasticModulus());
        writeDouble(chunkData, def.density());
        writeDouble(chunkData, def.characteristicStrength());
        writeDouble(chunkData, def.ultimateStrength());
        writeDouble(chunkData, def.defaultInitialTension());
        writeU8(chunkData, def.tensionOnly() ? 1 : 0);
        serializeSection(chunkData, c.section());
        serializeMaterial(chunkData, c.material());

        // Geometry
        const auto& geom = c.geometry();
        writeDouble(chunkData, geom.sag());
        writeDouble(chunkData, geom.horizontalTensionH());
        writeDouble(chunkData, geom.linearWeightW());

        // Prestress
        const auto& pr = c.prestress();
        writeDouble(chunkData, pr.initialTension);
        writeDouble(chunkData, pr.initialStrain);
        writeDouble(chunkData, pr.anchorageSlip);
        writeDouble(chunkData, pr.frictionCoeff);
        writeDouble(chunkData, pr.wobbleCoeff);

        // Anchors
        const auto& aStart = c.startAnchor();
        writeU8(chunkData, static_cast<uint8_t>(aStart.type()));
        writeDouble(chunkData, aStart.capacity());
        writeDouble(chunkData, aStart.slip());
        writeDouble(chunkData, aStart.socketDiameter());
        writeDouble(chunkData, aStart.socketLength());

        const auto& aEnd = c.endAnchor();
        writeU8(chunkData, static_cast<uint8_t>(aEnd.type()));
        writeDouble(chunkData, aEnd.capacity());
        writeDouble(chunkData, aEnd.slip());
        writeDouble(chunkData, aEnd.socketDiameter());
        writeDouble(chunkData, aEnd.socketLength());

        // Analysis
        const auto& an = c.analysisProperties();
        writeU8(chunkData, an.tensionOnly ? 1 : 0);
        writeU8(chunkData, an.largeDisplacement ? 1 : 0);
        writeU8(chunkData, an.geometricNonlinearity ? 1 : 0);
        writeDouble(chunkData, an.minTensionThreshold);
    }

    TSAChunkHeader ch;
    ch.chunkId = CHUNK_CABL;
    ch.chunkSize = static_cast<uint32_t>(chunkData.size());
    ch.elementCount = static_cast<uint32_t>(cables.size());

    const uint8_t* chBytes = reinterpret_cast<const uint8_t*>(&ch);
    buffer.insert(buffer.end(), chBytes, chBytes + sizeof(ch));
    buffer.insert(buffer.end(), chunkData.begin(), chunkData.end());
}

void TSAFileWriter::writeSnapshotChunk(std::vector<uint8_t>& buffer,
                                       const std::map<std::string, TSA::ExtensionSystem::MechanicalSnapshot>& snapshots,
                                       const std::map<std::string, TSA::ExtensionSystem::DefinitionReference>& references)
{
    std::vector<uint8_t> chunkData;
    for (const auto& [key, snap] : snapshots)
    {
        writeString(chunkData, key);

        auto itRef = references.find(key);
        if (itRef != references.end())
        {
            const auto& ref = itRef->second;
            writeString(chunkData, ref.libraryId);
            writeI32(chunkData, ref.libraryVersion.major);
            writeI32(chunkData, ref.libraryVersion.minor);
            writeI32(chunkData, ref.libraryVersion.patch);
            writeString(chunkData, ref.libraryVersion.prerelease);

            writeString(chunkData, ref.definitionId);
            writeI32(chunkData, ref.definitionVersion.major);
            writeI32(chunkData, ref.definitionVersion.minor);
            writeI32(chunkData, ref.definitionVersion.patch);
            writeString(chunkData, ref.definitionVersion.prerelease);
        }
        else
        {
            writeString(chunkData, "");
            writeI32(chunkData, 1);
            writeI32(chunkData, 0);
            writeI32(chunkData, 0);
            writeString(chunkData, "");

            writeString(chunkData, key);
            writeI32(chunkData, 1);
            writeI32(chunkData, 0);
            writeI32(chunkData, 0);
            writeString(chunkData, "");
        }

        writeDouble(chunkData, snap.youngModulus);
        writeDouble(chunkData, snap.poissonRatio);
        writeDouble(chunkData, snap.density);
        writeDouble(chunkData, snap.characteristicStrength);
        writeDouble(chunkData, snap.yieldStrength);
        writeDouble(chunkData, snap.thermalCoeff);
    }

    TSAChunkHeader ch;
    ch.chunkId = CHUNK_SNAP;
    ch.chunkSize = static_cast<uint32_t>(chunkData.size());
    ch.elementCount = static_cast<uint32_t>(snapshots.size());

    const uint8_t* chBytes = reinterpret_cast<const uint8_t*>(&ch);
    buffer.insert(buffer.end(), chBytes, chBytes + sizeof(ch));
    buffer.insert(buffer.end(), chunkData.begin(), chunkData.end());
}

// -----------------------------------------------------------------------------
// CHUNK 'LOAD' : cas de charges, combinaisons, charges nodales et charges sur barres.
// Disposition (version de disposition 1, en tête du chunk pour les évolutions futures) :
//   u32 layoutVersion | i32 activeCase | i32 nextNodal | i32 nextMember | i32 nextCase | i32 nextCombo
//   u32 nCases  x { i32 id, str name, u8 category, u8 selfWeight, f64 selfWeightFactor, str description }
//   u32 nCombos x { i32 id, str name, u8 type, u32 nFactors x { i32 caseId, f64 factor } }
//   u32 nNodal  x { i32 id, i32 nodeId, i32 caseId, f64 fx fy fz mx my mz, u8 coordSys, str name }
//   u32 nMember x { i32 id, i32 elementId, i32 caseId, u8 type, f64 q1 q2, u8 direction, u8 coordSys,
//                   f64 x1 x2, u8 relative, str name, u8 targetType }
// -----------------------------------------------------------------------------
void TSAFileWriter::writeLoadChunk(std::vector<uint8_t>& buffer, const TSA::Model::LoadManager::LoadSnapshot& loads)
{
    std::vector<uint8_t> chunkData;
    writeU32(chunkData, LOAD_CHUNK_LAYOUT_VERSION);
    writeI32(chunkData, loads.activeLoadCaseId);
    writeI32(chunkData, loads.nextNodalLoadId);
    writeI32(chunkData, loads.nextMemberLoadId);
    writeI32(chunkData, loads.nextLoadCaseId);
    writeI32(chunkData, loads.nextCombinationId);

    writeU32(chunkData, static_cast<uint32_t>(loads.loadCases.size()));
    for (const auto& [id, lc] : loads.loadCases)
    {
        writeI32(chunkData, id);
        writeString(chunkData, lc.name());
        writeU8(chunkData, static_cast<uint8_t>(lc.category()));
        writeU8(chunkData, lc.isSelfWeightIncluded() ? 1 : 0);
        writeDouble(chunkData, lc.selfWeightFactor());
        writeString(chunkData, lc.description());
    }

    writeU32(chunkData, static_cast<uint32_t>(loads.combinations.size()));
    for (const auto& [id, combo] : loads.combinations)
    {
        writeI32(chunkData, id);
        writeString(chunkData, combo.name());
        writeU8(chunkData, static_cast<uint8_t>(combo.type()));
        writeU32(chunkData, static_cast<uint32_t>(combo.caseFactors().size()));
        for (const auto& [caseId, factor] : combo.caseFactors())
        {
            writeI32(chunkData, caseId);
            writeDouble(chunkData, factor);
        }
    }

    writeU32(chunkData, static_cast<uint32_t>(loads.nodalLoads.size()));
    for (const auto& [id, nl] : loads.nodalLoads)
    {
        writeI32(chunkData, id);
        writeI32(chunkData, nl.nodeId());
        writeI32(chunkData, nl.loadCaseId());
        writeDouble(chunkData, nl.fx());
        writeDouble(chunkData, nl.fy());
        writeDouble(chunkData, nl.fz());
        writeDouble(chunkData, nl.mx());
        writeDouble(chunkData, nl.my());
        writeDouble(chunkData, nl.mz());
        writeU8(chunkData, static_cast<uint8_t>(nl.coordSystem()));
        writeString(chunkData, nl.name());
    }

    writeU32(chunkData, static_cast<uint32_t>(loads.memberLoads.size()));
    for (const auto& [id, ml] : loads.memberLoads)
    {
        writeI32(chunkData, id);
        writeI32(chunkData, ml.elementId());
        writeI32(chunkData, ml.loadCaseId());
        writeU8(chunkData, static_cast<uint8_t>(ml.type()));
        writeDouble(chunkData, ml.q1());
        writeDouble(chunkData, ml.q2());
        writeU8(chunkData, static_cast<uint8_t>(ml.direction()));
        writeU8(chunkData, static_cast<uint8_t>(ml.coordSystem()));
        writeDouble(chunkData, ml.x1());
        writeDouble(chunkData, ml.x2());
        writeU8(chunkData, ml.isRelativePosition() ? 1 : 0);
        writeString(chunkData, ml.name());
        writeU8(chunkData, static_cast<uint8_t>(ml.targetType()));
    }

    TSAChunkHeader ch;
    ch.chunkId = CHUNK_LOAD;
    ch.chunkSize = static_cast<uint32_t>(chunkData.size());
    ch.elementCount = static_cast<uint32_t>(loads.nodalLoads.size() + loads.memberLoads.size());

    const uint8_t* chBytes = reinterpret_cast<const uint8_t*>(&ch);
    buffer.insert(buffer.end(), chBytes, chBytes + sizeof(ch));
    buffer.insert(buffer.end(), chunkData.begin(), chunkData.end());
}

void TSAFileWriter::writeBimChunk(std::vector<uint8_t>& buffer, const TSA::BIM::BimModel& bim)
{
    // JSON UTF-8 versionné (champ "schema") : la couche BIM évolue sans casser le format binaire.
    const QByteArray json = QJsonDocument(bim.toJson()).toJson(QJsonDocument::Compact);

    TSAChunkHeader ch;
    ch.chunkId = CHUNK_BIMM;
    ch.chunkSize = static_cast<uint32_t>(json.size());
    ch.elementCount = static_cast<uint32_t>(bim.elements().size());

    const uint8_t* chBytes = reinterpret_cast<const uint8_t*>(&ch);
    buffer.insert(buffer.end(), chBytes, chBytes + sizeof(ch));
    buffer.insert(buffer.end(), reinterpret_cast<const uint8_t*>(json.constData()),
                  reinterpret_cast<const uint8_t*>(json.constData()) + json.size());
}

void TSAFileWriter::writeSettingsChunk(std::vector<uint8_t>& buffer, const std::string& json)
{
    // Paramètres d'analyse (AnalysisContext::toJson) : JSON UTF-8 versionné par son champ "schema".
    TSAChunkHeader ch;
    ch.chunkId = CHUNK_SETT;
    ch.chunkSize = static_cast<uint32_t>(json.size());
    ch.elementCount = 1;

    const uint8_t* chBytes = reinterpret_cast<const uint8_t*>(&ch);
    buffer.insert(buffer.end(), chBytes, chBytes + sizeof(ch));
    buffer.insert(buffer.end(), json.begin(), json.end());
}

} // namespace TSA::IO
