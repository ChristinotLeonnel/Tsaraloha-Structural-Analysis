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

#include <vector>
#include <map>
#include <string>
#include <cstring>
#include <algorithm>

namespace TSA::IO
{
using namespace Detail;

bool TSAFileReader::readProjectChunk(const uint8_t* data, size_t size, std::string* outProjectName, std::string* outAuthor, std::string* /*errorMessage*/)
{
    size_t off = 0;
    std::string projName, authorName, units;
    double scale = 1.0;
    readString(data, size, off, projName);
    readString(data, size, off, authorName);
    readString(data, size, off, units);
    readDouble(data, size, off, scale);

    if (outProjectName) *outProjectName = projName;
    if (outAuthor) *outAuthor = authorName;
    return true;
}

bool TSAFileReader::readThumbnailChunk(const uint8_t* data, size_t size, QImage* outThumbnail, std::string* /*errorMessage*/)
{
    if (!outThumbnail) return true;
    size_t off = 0;
    uint32_t w = 0, h = 0, formatType = 0, dataLen = 0;
    if (!readU32(data, size, off, w)) return false;
    if (!readU32(data, size, off, h)) return false;
    if (!readU32(data, size, off, formatType)) return false;
    if (!readU32(data, size, off, dataLen)) return false;
    if (off + dataLen > size) return false;

    QImage img;
    if (img.loadFromData(reinterpret_cast<const uchar*>(data + off), static_cast<int>(dataLen), "PNG"))
    {
        *outThumbnail = img;
        return true;
    }
    return false;
}

bool TSAFileReader::readCoordinateChunk(const uint8_t* data, size_t size, TSA::Coordinate::CoordinateSystem* cs, std::string* /*errorMessage*/)
{
    if (!cs) return true;
    size_t off = 0;
    std::string json;
    if (!readString(data, size, off, json)) return false;
    cs->deserializeFromJson(json);
    return true;
}

bool TSAFileReader::readGridChunk(const uint8_t* data, size_t size, TSA::Grid::GridManager* gm, std::string* /*errorMessage*/)
{
    if (!gm) return true;
    size_t off = 0;
    std::string json;
    if (!readString(data, size, off, json)) return false;
    gm->deserializeFromJson(json);
    return true;
}

bool TSAFileReader::readNodeChunk(const uint8_t* data, size_t size, uint32_t count, std::map<int, TSA::Model::Node>& nodes, std::string* errorMessage)
{
    if (count > MAX_SAFE_NODES)
    {
        if (errorMessage) *errorMessage = "Nombre de nœuds anormalement élevé dans le fichier.";
        return false;
    }
    size_t off = 0;
    for (uint32_t i = 0; i < count; ++i)
    {
        int32_t id = 0;
        std::string name, levelId, color;
        double x = 0, y = 0, z = 0;
        uint8_t supp = 0;

        if (!readI32(data, size, off, id)) return false;
        if (!readString(data, size, off, name)) return false;
        if (!readDouble(data, size, off, x)) return false;
        if (!readDouble(data, size, off, y)) return false;
        if (!readDouble(data, size, off, z)) return false;
        if (!readString(data, size, off, levelId)) return false;
        if (!readU8(data, size, off, supp)) return false;
        if (!readString(data, size, off, color)) return false;

        TSA::Model::Node n(id, x, y, z, levelId, name);
        n.setSupportType(static_cast<TSA::Model::SupportType>(supp));
        n.setColor(color);
        nodes[id] = n;
    }
    return true;
}

bool TSAFileReader::readSupportChunk(const uint8_t* data, size_t size, uint32_t count, std::map<int, TSA::Model::Node>& nodes, std::string* errorMessage)
{
    if (count > MAX_SAFE_NODES)
    {
        if (errorMessage) *errorMessage = "Nombre d'appuis anormalement élevé dans le fichier .tsa.";
        return false;
    }
    size_t off = 0;
    for (uint32_t i = 0; i < count; ++i)
    {
        int32_t id = 0, refElemId = 0;
        uint8_t tx = 0, ty = 0, tz = 0, rx = 0, ry = 0, rz = 0, orient = 0;
        double kx = 0, ky = 0, kz = 0, krx = 0, kry = 0, krz = 0;
        double dirX = 0, dirY = 0, dirZ = 1.0;

        if (!readI32(data, size, off, id)) return false;
        if (!readU8(data, size, off, tx)) return false;
        if (!readU8(data, size, off, ty)) return false;
        if (!readU8(data, size, off, tz)) return false;
        if (!readU8(data, size, off, rx)) return false;
        if (!readU8(data, size, off, ry)) return false;
        if (!readU8(data, size, off, rz)) return false;
        if (!readDouble(data, size, off, kx)) return false;
        if (!readDouble(data, size, off, ky)) return false;
        if (!readDouble(data, size, off, kz)) return false;
        if (!readDouble(data, size, off, krx)) return false;
        if (!readDouble(data, size, off, kry)) return false;
        if (!readDouble(data, size, off, krz)) return false;
        if (!readU8(data, size, off, orient)) return false;
        if (!readDouble(data, size, off, dirX)) return false;
        if (!readDouble(data, size, off, dirY)) return false;
        if (!readDouble(data, size, off, dirZ)) return false;
        if (!readI32(data, size, off, refElemId)) return false;

        auto it = nodes.find(id);
        if (it != nodes.end())
        {
            TSA::Model::SupportDefinition s(
                static_cast<TSA::Model::DOFState>(tx),
                static_cast<TSA::Model::DOFState>(ty),
                static_cast<TSA::Model::DOFState>(tz),
                static_cast<TSA::Model::DOFState>(rx),
                static_cast<TSA::Model::DOFState>(ry),
                static_cast<TSA::Model::DOFState>(rz),
                kx, ky, kz, krx, kry, krz,
                static_cast<TSA::Model::SupportOrientationType>(orient),
                dirX, dirY, dirZ,
                refElemId
            );
            it->second.setSupport(s);
        }
    }
    return true;
}

bool TSAFileReader::readBarChunk(const uint8_t* data, size_t size, uint32_t count, std::map<int, TSA::Model::Beam>& beams, std::string* errorMessage)
{
    if (count > MAX_SAFE_ELEMENTS)
    {
        if (errorMessage) *errorMessage = "Nombre de barres anormalement élevé dans le fichier.";
        return false;
    }
    size_t off = 0;
    for (uint32_t i = 0; i < count; ++i)
    {
        int32_t id = 0, startId = 0, endId = 0;
        std::string name, color;
        uint8_t role = 0, ecc = 0;
        TSA::Model::Section sec;
        TSA::Model::Material mat;
        double rot = 0;

        if (!readI32(data, size, off, id)) return false;
        if (!readString(data, size, off, name)) return false;
        if (!readI32(data, size, off, startId)) return false;
        if (!readI32(data, size, off, endId)) return false;
        if (!readU8(data, size, off, role)) return false;
        if (!deserializeSection(data, size, off, sec)) return false;
        if (!deserializeMaterial(data, size, off, mat)) return false;
        if (!readDouble(data, size, off, rot)) return false;
        if (!readU8(data, size, off, ecc)) return false;

        uint8_t sfx = 0, sfy = 0, sfz = 0, smx = 0, smy = 0, smz = 0;
        if (!readU8(data, size, off, sfx)) return false;
        if (!readU8(data, size, off, sfy)) return false;
        if (!readU8(data, size, off, sfz)) return false;
        if (!readU8(data, size, off, smx)) return false;
        if (!readU8(data, size, off, smy)) return false;
        if (!readU8(data, size, off, smz)) return false;

        uint8_t efx = 0, efy = 0, efz = 0, emx = 0, emy = 0, emz = 0;
        if (!readU8(data, size, off, efx)) return false;
        if (!readU8(data, size, off, efy)) return false;
        if (!readU8(data, size, off, efz)) return false;
        if (!readU8(data, size, off, emx)) return false;
        if (!readU8(data, size, off, emy)) return false;
        if (!readU8(data, size, off, emz)) return false;

        if (!readString(data, size, off, color)) return false;

        TSA::Model::Beam b(id, startId, endId, sec, mat, static_cast<TSA::Model::BarRole>(role), rot, name);
        b.setEccentricity(static_cast<TSA::Model::BarEccentricity>(ecc));

        TSA::Model::EndRelease sr;
        sr.fx = (sfx != 0); sr.fy = (sfy != 0); sr.fz = (sfz != 0);
        sr.mx = (smx != 0); sr.my = (smy != 0); sr.mz = (smz != 0);
        b.setStartRelease(sr);

        TSA::Model::EndRelease er;
        er.fx = (efx != 0); er.fy = (efy != 0); er.fz = (efz != 0);
        er.mx = (emx != 0); er.my = (emy != 0); er.mz = (emz != 0);
        b.setEndRelease(er);

        b.setColor(color);
        beams[id] = b;
    }
    return true;
}

bool TSAFileReader::readColumnChunk(const uint8_t* data, size_t size, uint32_t count, std::map<int, TSA::Model::Column>& columns, std::string* errorMessage)
{
    if (count > MAX_SAFE_ELEMENTS)
    {
        if (errorMessage) *errorMessage = "Nombre de poteaux anormalement élevé dans le fichier.";
        return false;
    }
    size_t off = 0;
    for (uint32_t i = 0; i < count; ++i)
    {
        int32_t id = 0, startId = 0, endId = 0;
        std::string name, color;
        TSA::Model::Section sec;
        TSA::Model::Material mat;
        double rot = 0;

        if (!readI32(data, size, off, id)) return false;
        if (!readString(data, size, off, name)) return false;
        if (!readI32(data, size, off, startId)) return false;
        if (!readI32(data, size, off, endId)) return false;
        if (!deserializeSection(data, size, off, sec)) return false;
        if (!deserializeMaterial(data, size, off, mat)) return false;
        if (!readDouble(data, size, off, rot)) return false;
        if (!readString(data, size, off, color)) return false;

        TSA::Model::Column col(id, startId, endId, sec, mat, rot, name);
        col.setColor(color);
        columns[id] = col;
    }
    return true;
}

bool TSAFileReader::readSlabChunk(const uint8_t* data, size_t size, uint32_t count, std::map<int, TSA::Model::Slab>& slabs, std::string* errorMessage)
{
    if (count > MAX_SAFE_ELEMENTS)
    {
        if (errorMessage) *errorMessage = "Nombre de dalles anormalement élevé dans le fichier.";
        return false;
    }
    size_t off = 0;
    for (uint32_t i = 0; i < count; ++i)
    {
        int32_t id = 0;
        std::string name, color;
        uint16_t numNodes = 0;
        double thickness = 0.20;
        TSA::Model::Material mat;
        uint8_t slabType = 0;

        if (!readI32(data, size, off, id)) return false;
        if (!readString(data, size, off, name)) return false;
        if (!readU16(data, size, off, numNodes)) return false;

        std::vector<int> nodeIds;
        nodeIds.reserve(numNodes);
        for (uint16_t k = 0; k < numNodes; ++k)
        {
            int32_t nid = 0;
            if (!readI32(data, size, off, nid)) return false;
            nodeIds.push_back(nid);
        }

        if (!readDouble(data, size, off, thickness)) return false;
        if (!deserializeMaterial(data, size, off, mat)) return false;
        if (!readU8(data, size, off, slabType)) return false;
        if (!readString(data, size, off, color)) return false;

        TSA::Model::Slab s(id, nodeIds, thickness, name, static_cast<TSA::Model::SlabType>(slabType));
        s.setMaterial(mat);
        s.setColor(color);
        slabs[id] = s;
    }
    return true;
}

bool TSAFileReader::readWallChunk(const uint8_t* data, size_t size, uint32_t count, std::map<int, TSA::Model::Wall>& walls, std::string* errorMessage)
{
    if (count > MAX_SAFE_ELEMENTS)
    {
        if (errorMessage) *errorMessage = "Nombre de voiles anormalement élevé dans le fichier.";
        return false;
    }
    size_t off = 0;
    for (uint32_t i = 0; i < count; ++i)
    {
        int32_t id = 0, startId = 0, endId = 0;
        std::string name, color;
        double h = 3.0, t = 0.20, offVal = 0.0;
        TSA::Model::Material mat;

        if (!readI32(data, size, off, id)) return false;
        if (!readString(data, size, off, name)) return false;
        if (!readI32(data, size, off, startId)) return false;
        if (!readI32(data, size, off, endId)) return false;
        if (!readDouble(data, size, off, h)) return false;
        if (!readDouble(data, size, off, t)) return false;
        if (!readDouble(data, size, off, offVal)) return false;
        if (!deserializeMaterial(data, size, off, mat)) return false;
        if (!readString(data, size, off, color)) return false;

        TSA::Model::Wall w(id, startId, endId, h, t, name);
        w.setOffset(offVal);
        w.setMaterial(mat);
        w.setColor(color);
        walls[id] = w;
    }
    return true;
}

bool TSAFileReader::readFoundationChunk(const uint8_t* data, size_t size, uint32_t count, std::map<int, TSA::Model::Foundation>& foundations, std::string* errorMessage)
{
    if (count > MAX_SAFE_ELEMENTS)
    {
        if (errorMessage) *errorMessage = "Nombre de fondations anormalement élevé dans le fichier.";
        return false;
    }
    size_t off = 0;
    for (uint32_t i = 0; i < count; ++i)
    {
        int32_t id = 0, nodeId = 0;
        std::string name, color;
        uint8_t ftype = 0;
        double wa = 1.5, lb = 1.5, hh = 0.5, cap = 250.0;
        TSA::Model::Material mat;

        if (!readI32(data, size, off, id)) return false;
        if (!readString(data, size, off, name)) return false;
        if (!readI32(data, size, off, nodeId)) return false;
        if (!readU8(data, size, off, ftype)) return false;
        if (!readDouble(data, size, off, wa)) return false;
        if (!readDouble(data, size, off, lb)) return false;
        if (!readDouble(data, size, off, hh)) return false;
        if (!deserializeMaterial(data, size, off, mat)) return false;
        if (!readDouble(data, size, off, cap)) return false;
        if (!readString(data, size, off, color)) return false;

        TSA::Model::Foundation f(id, nodeId, wa, lb, hh, name, static_cast<TSA::Model::FoundationType>(ftype));
        f.setMaterial(mat);
        f.setSoilBearingCapacity(cap);
        f.setColor(color);
        foundations[id] = f;
    }
    return true;
}

bool TSAFileReader::readTrussChunk(const uint8_t* data, size_t size, uint32_t count, std::map<int, TSA::Model::TrussMember>& trussMembers, std::string* errorMessage)
{
    if (count > MAX_SAFE_ELEMENTS)
    {
        if (errorMessage) *errorMessage = "Nombre d'éléments de treillis anormalement élevé dans le fichier.";
        return false;
    }
    size_t off = 0;
    for (uint32_t i = 0; i < count; ++i)
    {
        int32_t id = 0, startId = 0, endId = 0;
        std::string name, color;
        uint8_t role = 0;
        TSA::Model::Section sec;
        TSA::Model::Material mat;

        if (!readI32(data, size, off, id)) return false;
        if (!readString(data, size, off, name)) return false;
        if (!readI32(data, size, off, startId)) return false;
        if (!readI32(data, size, off, endId)) return false;
        if (!readU8(data, size, off, role)) return false;
        if (!deserializeSection(data, size, off, sec)) return false;
        if (!deserializeMaterial(data, size, off, mat)) return false;
        if (!readString(data, size, off, color)) return false;

        TSA::Model::TrussMember t(id, startId, endId, sec.diameter > 0.0 ? sec.diameter : sec.width, name, static_cast<TSA::Model::TrussMemberRole>(role));
        t.setSection(sec);
        t.setMaterial(mat);
        t.setColor(color);
        trussMembers[id] = t;
    }
    return true;
}

bool TSAFileReader::readCableChunk(const uint8_t* data, size_t size, uint32_t count, std::map<int, TSA::Model::Cable>& cables, std::string* errorMessage)
{
    if (count > MAX_SAFE_ELEMENTS)
    {
        if (errorMessage) *errorMessage = "Nombre excessif de câbles dans le chunk CABL.";
        return false;
    }

    size_t off = 0;
    for (uint32_t i = 0; i < count; ++i)
    {
        int32_t id = 0, startId = 0, endId = 0;
        std::string name;
        uint8_t typeVal = 0, geomModeVal = 0;

        if (!readI32(data, size, off, id)) return false;
        if (!readString(data, size, off, name)) return false;
        if (!readI32(data, size, off, startId)) return false;
        if (!readI32(data, size, off, endId)) return false;
        if (!readU8(data, size, off, typeVal)) return false;
        if (!readU8(data, size, off, geomModeVal)) return false;

        TSA::Model::Cable cable(id, startId, endId, name, static_cast<TSA::Model::CableType>(typeVal));
        cable.setGeometryMode(static_cast<TSA::Model::CableGeometryMode>(geomModeVal));

        // Definition
        TSA::Model::CableDefinition def;
        std::string defId, defName, stdStr, stdVer, grade;
        uint8_t defType = 0;
        double nomDia = 0.0, area = 0.0, E = 0.0, dens = 0.0, fpk = 0.0, fu = 0.0, initT = 0.0;
        uint8_t tensionOnly = 1;
        TSA::Model::Section sec;
        TSA::Model::Material mat;

        if (!readString(data, size, off, defId)) return false;
        if (!readString(data, size, off, defName)) return false;
        if (!readU8(data, size, off, defType)) return false;
        if (!readString(data, size, off, stdStr)) return false;
        if (!readString(data, size, off, stdVer)) return false;
        if (!readString(data, size, off, grade)) return false;
        if (!readDouble(data, size, off, nomDia)) return false;
        if (!readDouble(data, size, off, area)) return false;
        if (!readDouble(data, size, off, E)) return false;
        if (!readDouble(data, size, off, dens)) return false;
        if (!readDouble(data, size, off, fpk)) return false;
        if (!readDouble(data, size, off, fu)) return false;
        if (!readDouble(data, size, off, initT)) return false;
        if (!readU8(data, size, off, tensionOnly)) return false;
        if (!deserializeSection(data, size, off, sec)) return false;
        if (!deserializeMaterial(data, size, off, mat)) return false;

        def.setId(defId);
        def.setName(defName);
        def.setType(static_cast<TSA::Model::CableType>(defType));
        def.setStandardName(stdStr);
        def.setStandardVersion(stdVer);
        def.setGrade(grade);
        def.setNominalDiameter(nomDia);
        def.setMetallicArea(area);
        def.setElasticModulus(E);
        def.setDensity(dens);
        def.setCharacteristicStrength(fpk);
        def.setUltimateStrength(fu);
        def.setDefaultInitialTension(initT);
        def.setTensionOnly(tensionOnly != 0);
        cable.setDefinition(def);
        cable.setSection(sec);
        cable.setMaterial(mat);

        // Geometry
        double sag = 0.0, catH = 0.0, catW = 0.0;
        if (!readDouble(data, size, off, sag)) return false;
        if (!readDouble(data, size, off, catH)) return false;
        if (!readDouble(data, size, off, catW)) return false;
        cable.geometry().setSag(sag);
        cable.geometry().setHorizontalTensionH(catH);
        cable.geometry().setLinearWeightW(catW);

        // Prestress
        double prT = 0.0, prEps = 0.0, prSlip = 0.0, prMu = 0.0, prK = 0.0;
        if (!readDouble(data, size, off, prT)) return false;
        if (!readDouble(data, size, off, prEps)) return false;
        if (!readDouble(data, size, off, prSlip)) return false;
        if (!readDouble(data, size, off, prMu)) return false;
        if (!readDouble(data, size, off, prK)) return false;
        cable.prestress().initialTension = prT;
        cable.prestress().initialStrain = prEps;
        cable.prestress().anchorageSlip = prSlip;
        cable.prestress().frictionCoeff = prMu;
        cable.prestress().wobbleCoeff = prK;

        // Anchors
        uint8_t aStartType = 0, aEndType = 0;
        double aStartCap = 0.0, aStartSlip = 0.0, aStartDia = 0.0, aStartLen = 0.0;
        double aEndCap = 0.0, aEndSlip = 0.0, aEndDia = 0.0, aEndLen = 0.0;
        if (!readU8(data, size, off, aStartType)) return false;
        if (!readDouble(data, size, off, aStartCap)) return false;
        if (!readDouble(data, size, off, aStartSlip)) return false;
        if (!readDouble(data, size, off, aStartDia)) return false;
        if (!readDouble(data, size, off, aStartLen)) return false;

        if (!readU8(data, size, off, aEndType)) return false;
        if (!readDouble(data, size, off, aEndCap)) return false;
        if (!readDouble(data, size, off, aEndSlip)) return false;
        if (!readDouble(data, size, off, aEndDia)) return false;
        if (!readDouble(data, size, off, aEndLen)) return false;

        cable.startAnchor().setType(static_cast<TSA::Model::AnchorType>(aStartType));
        cable.startAnchor().setCapacity(aStartCap);
        cable.startAnchor().setSlip(aStartSlip);
        cable.startAnchor().setSocketDiameter(aStartDia);
        cable.startAnchor().setSocketLength(aStartLen);

        cable.endAnchor().setType(static_cast<TSA::Model::AnchorType>(aEndType));
        cable.endAnchor().setCapacity(aEndCap);
        cable.endAnchor().setSlip(aEndSlip);
        cable.endAnchor().setSocketDiameter(aEndDia);
        cable.endAnchor().setSocketLength(aEndLen);

        // Analysis
        uint8_t anTO = 1, anLD = 1, anGN = 1;
        double minThresh = 0.0;
        if (!readU8(data, size, off, anTO)) return false;
        if (!readU8(data, size, off, anLD)) return false;
        if (!readU8(data, size, off, anGN)) return false;
        if (!readDouble(data, size, off, minThresh)) return false;
        cable.analysisProperties().tensionOnly = (anTO != 0);
        cable.analysisProperties().largeDisplacement = (anLD != 0);
        cable.analysisProperties().geometricNonlinearity = (anGN != 0);
        cable.analysisProperties().minTensionThreshold = minThresh;

        cables[id] = cable;
    }
    return true;
}

bool TSAFileReader::readSnapshotChunk(const uint8_t* data, size_t size, uint32_t count,
                                      std::map<std::string, TSA::ExtensionSystem::MechanicalSnapshot>& snapshots,
                                      std::map<std::string, TSA::ExtensionSystem::DefinitionReference>& references,
                                      std::string* errorMessage)
{
    if (count > MAX_SAFE_MATERIALS * 10)
    {
        if (errorMessage) *errorMessage = "Nombre excessif de snapshots de calcul dans le fichier.";
        return false;
    }

    size_t off = 0;
    for (uint32_t i = 0; i < count; ++i)
    {
        std::string key;
        std::string libId, libPre, defId, defPre;
        int32_t libMaj = 1, libMin = 0, libPat = 0;
        int32_t defMaj = 1, defMin = 0, defPat = 0;
        double E = 0.0, nu = 0.0, rho = 0.0, fk = 0.0, fy = 0.0, alpha = 0.0;

        if (!readString(data, size, off, key)) return false;

        if (!readString(data, size, off, libId)) return false;
        if (!readI32(data, size, off, libMaj)) return false;
        if (!readI32(data, size, off, libMin)) return false;
        if (!readI32(data, size, off, libPat)) return false;
        if (!readString(data, size, off, libPre)) return false;

        if (!readString(data, size, off, defId)) return false;
        if (!readI32(data, size, off, defMaj)) return false;
        if (!readI32(data, size, off, defMin)) return false;
        if (!readI32(data, size, off, defPat)) return false;
        if (!readString(data, size, off, defPre)) return false;

        if (!readDouble(data, size, off, E)) return false;
        if (!readDouble(data, size, off, nu)) return false;
        if (!readDouble(data, size, off, rho)) return false;
        if (!readDouble(data, size, off, fk)) return false;
        if (!readDouble(data, size, off, fy)) return false;
        if (!readDouble(data, size, off, alpha)) return false;

        TSA::ExtensionSystem::MechanicalSnapshot snap;
        snap.youngModulus = E;
        snap.poissonRatio = nu;
        snap.density = rho;
        snap.characteristicStrength = fk;
        snap.yieldStrength = fy;
        snap.thermalCoeff = alpha;
        snapshots[key] = snap;

        TSA::ExtensionSystem::DefinitionReference ref;
        ref.libraryId = libId;
        ref.libraryVersion = TSA::ExtensionSystem::SemanticVersion(libMaj, libMin, libPat, libPre);
        ref.definitionId = defId;
        ref.definitionVersion = TSA::ExtensionSystem::SemanticVersion(defMaj, defMin, defPat, defPre);
        references[key] = ref;
    }

    return true;
}

} // namespace TSA::IO
