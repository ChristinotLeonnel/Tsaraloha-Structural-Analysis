#include "LibraryManager.h"
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <cmath>

namespace TSA::Library
{

static QString sectionShapeToString(TSA::Model::SectionShape shape)
{
    switch (shape)
    {
    case TSA::Model::SectionShape::Rectangular: return "Rectangular";
    case TSA::Model::SectionShape::Circular:    return "Circular";
    case TSA::Model::SectionShape::IShape:      return "IShape";
    case TSA::Model::SectionShape::Pipe:        return "Pipe";
    case TSA::Model::SectionShape::BoxHollow:   return "BoxHollow";
    case TSA::Model::SectionShape::UPN:         return "UPN";
    case TSA::Model::SectionShape::Angle:       return "Angle";
    case TSA::Model::SectionShape::TSection:    return "TSection";
    }
    return "Rectangular";
}

static TSA::Model::SectionShape stringToSectionShape(const QString& str)
{
    if (str == "Circular")   return TSA::Model::SectionShape::Circular;
    if (str == "IShape")     return TSA::Model::SectionShape::IShape;
    if (str == "Pipe")       return TSA::Model::SectionShape::Pipe;
    if (str == "BoxHollow")  return TSA::Model::SectionShape::BoxHollow;
    if (str == "UPN")        return TSA::Model::SectionShape::UPN;
    if (str == "Angle")      return TSA::Model::SectionShape::Angle;
    if (str == "TSection")   return TSA::Model::SectionShape::TSection;
    return TSA::Model::SectionShape::Rectangular;
}

LibraryManager& LibraryManager::instance()
{
    static LibraryManager mgr;
    return mgr;
}

LibraryManager::LibraryManager()
{
    QString baseDir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    if (baseDir.isEmpty())
    {
        baseDir = QDir::currentPath();
    }
    m_libraryDir = baseDir + "/Library";
    QDir().mkpath(m_libraryDir);

    populateDefaultStandardData();
    initialize();
}

void LibraryManager::initialize()
{
    loadSections();
    loadMaterials();
    loadColors();
    loadTextures();
    loadTemplates();
}

void LibraryManager::saveAll()
{
    saveSections();
    saveMaterials();
    saveColors();
    saveTextures();
    saveTemplates();
}

void LibraryManager::populateDefaultStandardData()
{
    m_standardSections = TSA::Model::Section::defaultLibrary();

    m_standardMaterials = TSA::Model::Material::defaultLibrary();

    m_colors = {
        { "Béton Gris",       "#808080", "Béton" },
        { "Béton Foncé",      "#555555", "Béton" },
        { "Acier Bleu",       "#1E90FF", "Acier" },
        { "Acier Galvanisé",  "#A9A9A9", "Acier" },
        { "Poteau Rouge",     "#D9534F", "Général" },
        { "Poutre Verte",     "#5CB85C", "Général" },
        { "Dalle Cyan",       "#5BC0DE", "Général" },
        { "Fondation Marron", "#8B4513", "Général" },
        { "Bois Naturel",     "#CD853F", "Bois" }
    };
}

std::vector<TSA::Model::Section> LibraryManager::allSections() const
{
    std::vector<TSA::Model::Section> result = m_standardSections;
    result.insert(result.end(), m_customSections.begin(), m_customSections.end());
    return result;
}

bool LibraryManager::addCustomSection(const TSA::Model::Section& section)
{
    for (auto& s : m_customSections)
    {
        if (s.name == section.name)
        {
            s = section;
            saveSections();
            return true;
        }
    }
    m_customSections.push_back(section);
    saveSections();
    return true;
}

bool LibraryManager::removeCustomSection(const std::string& name)
{
    for (auto it = m_customSections.begin(); it != m_customSections.end(); ++it)
    {
        if (it->name == name)
        {
            m_customSections.erase(it);
            saveSections();
            return true;
        }
    }
    return false;
}

const TSA::Model::Section* LibraryManager::findSectionByName(const std::string& name) const
{
    for (const auto& s : m_customSections)
    {
        if (s.name == name) return &s;
    }
    for (const auto& s : m_standardSections)
    {
        if (s.name == name) return &s;
    }
    return nullptr;
}

std::vector<TSA::Model::Material> LibraryManager::allMaterials() const
{
    std::vector<TSA::Model::Material> result = m_standardMaterials;
    result.insert(result.end(), m_customMaterials.begin(), m_customMaterials.end());
    return result;
}

bool LibraryManager::addCustomMaterial(const TSA::Model::Material& material)
{
    for (auto& m : m_customMaterials)
    {
        if (m.name == material.name)
        {
            m = material;
            saveMaterials();
            return true;
        }
    }
    m_customMaterials.push_back(material);
    saveMaterials();
    return true;
}

bool LibraryManager::removeCustomMaterial(const std::string& name)
{
    for (auto it = m_customMaterials.begin(); it != m_customMaterials.end(); ++it)
    {
        if (it->name == name)
        {
            m_customMaterials.erase(it);
            saveMaterials();
            return true;
        }
    }
    return false;
}

const TSA::Model::Material* LibraryManager::findMaterialByName(const std::string& name) const
{
    for (const auto& m : m_customMaterials)
    {
        if (m.name == name) return &m;
    }
    for (const auto& m : m_standardMaterials)
    {
        if (m.name == name) return &m;
    }
    return nullptr;
}

bool LibraryManager::addCustomColor(const QString& name, const QString& hexCode, const QString& category)
{
    for (auto& c : m_colors)
    {
        if (c.name.compare(name, Qt::CaseInsensitive) == 0)
        {
            c.hexCode = hexCode;
            c.category = category;
            saveColors();
            return true;
        }
    }
    m_colors.push_back({ name, hexCode, category });
    saveColors();
    return true;
}

bool LibraryManager::removeCustomColor(const QString& name)
{
    for (auto it = m_colors.begin(); it != m_colors.end(); ++it)
    {
        if (it->name.compare(name, Qt::CaseInsensitive) == 0)
        {
            m_colors.erase(it);
            saveColors();
            return true;
        }
    }
    return false;
}

QColor LibraryManager::getColor(const QString& name, const QColor& defaultColor) const
{
    for (const auto& c : m_colors)
    {
        if (c.name.compare(name, Qt::CaseInsensitive) == 0)
        {
            QColor col(c.hexCode);
            if (col.isValid()) return col;
        }
    }
    return defaultColor;
}

bool LibraryManager::addCustomTexture(const QString& name, const QString& filePath, const QString& category, double repU, double repV)
{
    QString id = QString("tex_%1").arg(m_textures.size() + 1);
    for (auto& t : m_textures)
    {
        if (t.name.compare(name, Qt::CaseInsensitive) == 0)
        {
            t.filePath = filePath;
            t.category = category;
            t.repeatU = repU;
            t.repeatV = repV;
            saveTextures();
            return true;
        }
    }
    m_textures.push_back({ id, name, category, filePath, repU, repV });
    saveTextures();
    return true;
}

bool LibraryManager::removeCustomTexture(const QString& id)
{
    for (auto it = m_textures.begin(); it != m_textures.end(); ++it)
    {
        if (it->id == id || it->name == id)
        {
            m_textures.erase(it);
            saveTextures();
            return true;
        }
    }
    return false;
}

bool LibraryManager::addStructureTemplate(const QString& name, const QString& category, const QString& description, const TSA::Model::Model::ModelStateSnapshot& snapshot)
{
    QString id = QString("template_%1").arg(m_templates.size() + 1);
    for (auto& t : m_templates)
    {
        if (t.name.compare(name, Qt::CaseInsensitive) == 0)
        {
            t.category = category;
            t.description = description;
            t.snapshot = snapshot;
            saveTemplates();
            return true;
        }
    }
    m_templates.push_back({ id, name, category, description, snapshot });
    saveTemplates();
    return true;
}

bool LibraryManager::removeStructureTemplate(const QString& id)
{
    for (auto it = m_templates.begin(); it != m_templates.end(); ++it)
    {
        if (it->id == id || it->name == id)
        {
            m_templates.erase(it);
            saveTemplates();
            return true;
        }
    }
    return false;
}

bool LibraryManager::instantiateTemplateInModel(const QString& templateId, TSA::Model::Model* targetModel, double offsetX, double offsetY, double offsetZ)
{
    if (!targetModel) return false;
    const StructureTemplate* selected = nullptr;
    for (const auto& t : m_templates)
    {
        if (t.id == templateId || t.name == templateId)
        {
            selected = &t;
            break;
        }
    }
    if (!selected) return false;

    targetModel->pushUndoState(QString("Insertion %1").arg(selected->name).toStdString());

    // Table de correspondance ancien ID nœud -> nouvel ID nœud
    std::map<int, int> nodeMap;
    for (const auto& [oldId, node] : selected->snapshot.nodes)
    {
        int newId = targetModel->addNode(node.x() + offsetX, node.y() + offsetY, node.z() + offsetZ, node.levelId(), node.name());
        nodeMap[oldId] = newId;
    }

    // Instanciation des barres / poutres
    for (const auto& [oldId, beam] : selected->snapshot.beams)
    {
        auto itA = nodeMap.find(beam.startNodeId());
        auto itB = nodeMap.find(beam.endNodeId());
        if (itA != nodeMap.end() && itB != nodeMap.end())
        {
            targetModel->addBar(itA->second, itB->second, beam.section(), beam.material(), beam.role(), beam.rotation(), beam.name());
        }
    }

    // Instanciation des poteaux
    for (const auto& [oldId, col] : selected->snapshot.columns)
    {
        auto itA = nodeMap.find(col.startNodeId());
        auto itB = nodeMap.find(col.endNodeId());
        if (itA != nodeMap.end() && itB != nodeMap.end())
        {
            targetModel->addColumn(itA->second, itB->second, col.section(), col.material(), col.rotation(), col.name());
        }
    }

    // Instanciation des dalles
    for (const auto& [oldId, slab] : selected->snapshot.slabs)
    {
        std::vector<int> mappedNodes;
        for (int nid : slab.nodeIds())
        {
            auto it = nodeMap.find(nid);
            if (it != nodeMap.end()) mappedNodes.push_back(it->second);
        }
        if (mappedNodes.size() >= 3)
        {
            targetModel->addSlab(mappedNodes, slab.thickness(), slab.name(), slab.slabType());
        }
    }

    return true;
}

// --- Sérialisation JSON ---

void LibraryManager::loadSections()
{
    QFile file(m_libraryDir + "/sections.json");
    if (!file.open(QIODevice::ReadOnly)) return;

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isArray()) return;

    m_customSections.clear();
    QJsonArray arr = doc.array();
    for (const auto& val : arr)
    {
        QJsonObject obj = val.toObject();
        TSA::Model::Section s;
        s.name = obj["name"].toString().toStdString();
        s.shape = stringToSectionShape(obj["shape"].toString());
        s.width = obj["width"].toDouble(0.30);
        s.height = obj["height"].toDouble(0.30);
        s.diameter = obj["diameter"].toDouble(0.30);
        s.tw = obj["tw"].toDouble(0.010);
        s.tf = obj["tf"].toDouble(0.015);
        if (!s.name.empty())
        {
            m_customSections.push_back(s);
        }
    }
}

void LibraryManager::saveSections()
{
    QFile file(m_libraryDir + "/sections.json");
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) return;

    QJsonArray arr;
    for (const auto& s : m_customSections)
    {
        QJsonObject obj;
        obj["name"] = QString::fromStdString(s.name);
        obj["shape"] = sectionShapeToString(s.shape);
        obj["width"] = s.width;
        obj["height"] = s.height;
        obj["diameter"] = s.diameter;
        obj["tw"] = s.tw;
        obj["tf"] = s.tf;
        arr.append(obj);
    }
    file.write(QJsonDocument(arr).toJson(QJsonDocument::Indented));
}

void LibraryManager::loadMaterials()
{
    QFile file(m_libraryDir + "/materials.json");
    if (!file.open(QIODevice::ReadOnly)) return;

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isArray()) return;

    m_customMaterials.clear();
    QJsonArray arr = doc.array();
    for (const auto& val : arr)
    {
        QJsonObject obj = val.toObject();
        TSA::Model::Material m;
        m.id = obj["id"].toInt(0);
        m.name = obj["name"].toString().toStdString();
        m.type = static_cast<TSA::Model::MaterialType>(obj["type"].toInt(static_cast<int>(TSA::Model::MaterialType::Custom)));
        m.E = obj["E"].toDouble(30e9);
        m.nu = obj["nu"].toDouble(0.2);
        m.density = obj["rho"].toDouble(2500.0);
        m.fk = obj["fk"].toDouble(25e6);
        m.thermalCoeff = obj["thermalCoeff"].toDouble(1e-5);
        m.syncMechanical();

        if (obj.contains("baseColor")) m.visual.baseColor = obj["baseColor"].toString().toStdString();
        if (obj.contains("roughness")) m.visual.roughness = obj["roughness"].toDouble(0.85);
        if (obj.contains("metallic")) m.visual.metallic = obj["metallic"].toDouble(0.0);
        if (obj.contains("transparency")) m.visual.transparency = obj["transparency"].toDouble(0.0);
        if (obj.contains("textureName")) m.visual.textureName = obj["textureName"].toString().toStdString();

        if (!m.name.empty())
        {
            m_customMaterials.push_back(m);
        }
    }
}

void LibraryManager::saveMaterials()
{
    QFile file(m_libraryDir + "/materials.json");
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) return;

    QJsonArray arr;
    for (const auto& m : m_customMaterials)
    {
        QJsonObject obj;
        obj["id"] = m.id;
        obj["name"] = QString::fromStdString(m.name);
        obj["type"] = static_cast<int>(m.type);
        obj["E"] = m.E;
        obj["nu"] = m.nu;
        obj["rho"] = m.density;
        obj["fk"] = m.fk;
        obj["thermalCoeff"] = m.thermalCoeff;
        obj["baseColor"] = QString::fromStdString(m.visual.baseColor);
        obj["roughness"] = m.visual.roughness;
        obj["metallic"] = m.visual.metallic;
        obj["transparency"] = m.visual.transparency;
        obj["textureName"] = QString::fromStdString(m.visual.textureName);
        arr.append(obj);
    }
    file.write(QJsonDocument(arr).toJson(QJsonDocument::Indented));
}

void LibraryManager::loadColors()
{
    QFile file(m_libraryDir + "/colors.json");
    if (!file.open(QIODevice::ReadOnly)) return;

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isArray()) return;

    m_colors.clear();
    QJsonArray arr = doc.array();
    for (const auto& val : arr)
    {
        QJsonObject obj = val.toObject();
        CustomColor c;
        c.name = obj["name"].toString();
        c.hexCode = obj["hexCode"].toString();
        c.category = obj["category"].toString("Mes Couleurs");
        if (!c.name.isEmpty())
        {
            m_colors.push_back(c);
        }
    }
    if (m_colors.empty())
    {
        populateDefaultStandardData();
    }
}

void LibraryManager::saveColors()
{
    QFile file(m_libraryDir + "/colors.json");
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) return;

    QJsonArray arr;
    for (const auto& c : m_colors)
    {
        QJsonObject obj;
        obj["name"] = c.name;
        obj["hexCode"] = c.hexCode;
        obj["category"] = c.category;
        arr.append(obj);
    }
    file.write(QJsonDocument(arr).toJson(QJsonDocument::Indented));
}

void LibraryManager::loadTextures()
{
    QFile file(m_libraryDir + "/textures.json");
    if (!file.open(QIODevice::ReadOnly)) return;

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isArray()) return;

    m_textures.clear();
    QJsonArray arr = doc.array();
    for (const auto& val : arr)
    {
        QJsonObject obj = val.toObject();
        CustomTexture t;
        t.id = obj["id"].toString();
        t.name = obj["name"].toString();
        t.category = obj["category"].toString("Mes Textures");
        t.filePath = obj["filePath"].toString();
        t.repeatU = obj["repeatU"].toDouble(1.0);
        t.repeatV = obj["repeatV"].toDouble(1.0);
        if (!t.name.isEmpty())
        {
            m_textures.push_back(t);
        }
    }
}

void LibraryManager::saveTextures()
{
    QFile file(m_libraryDir + "/textures.json");
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) return;

    QJsonArray arr;
    for (const auto& t : m_textures)
    {
        QJsonObject obj;
        obj["id"] = t.id;
        obj["name"] = t.name;
        obj["category"] = t.category;
        obj["filePath"] = t.filePath;
        obj["repeatU"] = t.repeatU;
        obj["repeatV"] = t.repeatV;
        arr.append(obj);
    }
    file.write(QJsonDocument(arr).toJson(QJsonDocument::Indented));
}

void LibraryManager::loadTemplates()
{
    QFile file(m_libraryDir + "/templates.json");
    if (!file.open(QIODevice::ReadOnly)) return;

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isArray()) return;

    m_templates.clear();
    QJsonArray arr = doc.array();
    for (const auto& val : arr)
    {
        QJsonObject obj = val.toObject();
        StructureTemplate t;
        t.id = obj["id"].toString();
        t.name = obj["name"].toString();
        t.category = obj["category"].toString("Mes Structures");
        t.description = obj["description"].toString();

        // Nœuds
        QJsonArray nodeArr = obj["nodes"].toArray();
        for (const auto& nVal : nodeArr)
        {
            QJsonObject nObj = nVal.toObject();
            int id = nObj["id"].toInt();
            double x = nObj["x"].toDouble();
            double y = nObj["y"].toDouble();
            double z = nObj["z"].toDouble();
            t.snapshot.nodes[id] = TSA::Model::Node(id, x, y, z);
        }

        // Poutres
        QJsonArray beamArr = obj["beams"].toArray();
        for (const auto& bVal : beamArr)
        {
            QJsonObject bObj = bVal.toObject();
            int id = bObj["id"].toInt();
            int sn = bObj["start"].toInt();
            int en = bObj["end"].toInt();
            double w = bObj["width"].toDouble(0.30);
            double h = bObj["height"].toDouble(0.50);
            t.snapshot.beams[id] = TSA::Model::Beam(id, sn, en, w, h);
        }

        // Poteaux
        QJsonArray colArr = obj["columns"].toArray();
        for (const auto& cVal : colArr)
        {
            QJsonObject cObj = cVal.toObject();
            int id = cObj["id"].toInt();
            int sn = cObj["start"].toInt();
            int en = cObj["end"].toInt();
            double w = cObj["width"].toDouble(0.30);
            double h = cObj["height"].toDouble(0.30);
            t.snapshot.columns[id] = TSA::Model::Column(id, sn, en, w, h);
        }

        if (!t.name.isEmpty())
        {
            m_templates.push_back(t);
        }
    }
}

void LibraryManager::saveTemplates()
{
    QFile file(m_libraryDir + "/templates.json");
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) return;

    QJsonArray arr;
    for (const auto& t : m_templates)
    {
        QJsonObject obj;
        obj["id"] = t.id;
        obj["name"] = t.name;
        obj["category"] = t.category;
        obj["description"] = t.description;

        QJsonArray nodeArr;
        for (const auto& [nid, node] : t.snapshot.nodes)
        {
            QJsonObject nObj;
            nObj["id"] = nid;
            nObj["x"] = node.x();
            nObj["y"] = node.y();
            nObj["z"] = node.z();
            nodeArr.append(nObj);
        }
        obj["nodes"] = nodeArr;

        QJsonArray beamArr;
        for (const auto& [bid, beam] : t.snapshot.beams)
        {
            QJsonObject bObj;
            bObj["id"] = bid;
            bObj["start"] = beam.startNodeId();
            bObj["end"] = beam.endNodeId();
            bObj["width"] = beam.section().width;
            bObj["height"] = beam.section().height;
            beamArr.append(bObj);
        }
        obj["beams"] = beamArr;

        QJsonArray colArr;
        for (const auto& [cid, col] : t.snapshot.columns)
        {
            QJsonObject cObj;
            cObj["id"] = cid;
            cObj["start"] = col.startNodeId();
            cObj["end"] = col.endNodeId();
            cObj["width"] = col.section().width;
            cObj["height"] = col.section().height;
            colArr.append(cObj);
        }
        obj["columns"] = colArr;

        arr.append(obj);
    }
    file.write(QJsonDocument(arr).toJson(QJsonDocument::Indented));
}

} // namespace TSA::Library
