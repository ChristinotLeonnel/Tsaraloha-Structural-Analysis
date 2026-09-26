#include "CableLibrary.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QDir>
#include <QStandardPaths>

namespace TSA::Library
{

CableLibrary& CableLibrary::instance()
{
    static CableLibrary lib;
    return lib;
}

CableLibrary::CableLibrary()
{
    initialize();
}

void CableLibrary::initialize()
{
    m_standardDefinitions = TSA::Model::CableDefinition::defaultLibrary();

    QString appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir dir(appData);
    if (!dir.exists()) dir.mkpath(".");
    m_storageFilePath = dir.filePath("cable_library_custom.json").toStdString();

    QFile file(QString::fromStdString(m_storageFilePath));
    if (file.open(QIODevice::ReadOnly))
    {
        importFromJson(QString::fromUtf8(file.readAll()).toStdString());
        file.close();
    }
}

void CableLibrary::save()
{
    QFile file(QString::fromStdString(m_storageFilePath));
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        std::string json = exportToJson();
        file.write(json.c_str(), static_cast<qint64>(json.size()));
        file.close();
    }
}

std::vector<TSA::Model::CableDefinition> CableLibrary::allDefinitions() const
{
    std::vector<TSA::Model::CableDefinition> res = m_standardDefinitions;
    res.insert(res.end(), m_customDefinitions.begin(), m_customDefinitions.end());
    return res;
}

bool CableLibrary::addCustomDefinition(const TSA::Model::CableDefinition& def)
{
    for (auto& item : m_customDefinitions)
    {
        if (item.name() == def.name() || item.id() == def.id())
        {
            item = def;
            save();
            return true;
        }
    }
    m_customDefinitions.push_back(def);
    save();
    return true;
}

bool CableLibrary::removeCustomDefinition(const std::string& name)
{
    for (auto it = m_customDefinitions.begin(); it != m_customDefinitions.end(); ++it)
    {
        if (it->name() == name || it->id() == name)
        {
            m_customDefinitions.erase(it);
            save();
            return true;
        }
    }
    return false;
}

const TSA::Model::CableDefinition* CableLibrary::findByName(const std::string& name) const
{
    for (const auto& def : m_customDefinitions)
    {
        if (def.name() == name) return &def;
    }
    for (const auto& def : m_standardDefinitions)
    {
        if (def.name() == name) return &def;
    }
    return nullptr;
}

const TSA::Model::CableDefinition* CableLibrary::findById(const std::string& id) const
{
    for (const auto& def : m_customDefinitions)
    {
        if (def.id() == id) return &def;
    }
    for (const auto& def : m_standardDefinitions)
    {
        if (def.id() == id) return &def;
    }
    return nullptr;
}

std::vector<TSA::Model::CableDefinition> CableLibrary::filter(
    std::optional<TSA::Model::CableType> type,
    std::optional<TSA::Model::StandardCode> standard,
    double minDiameter,
    double maxDiameter) const
{
    std::vector<TSA::Model::CableDefinition> res;
    auto all = allDefinitions();
    for (const auto& def : all)
    {
        if (type.has_value() && def.type() != type.value()) continue;
        if (standard.has_value() && def.standardCode() != standard.value()) continue;
        if (def.nominalDiameter() < minDiameter || def.nominalDiameter() > maxDiameter) continue;
        res.push_back(def);
    }
    return res;
}

std::string CableLibrary::exportToJson() const
{
    QJsonArray arr;
    for (const auto& def : m_customDefinitions)
    {
        QJsonObject obj;
        obj["id"] = QString::fromStdString(def.id());
        obj["name"] = QString::fromStdString(def.name());
        obj["type"] = QString::fromStdString(TSA::Model::cableTypeToString(def.type()));
        obj["standard"] = QString::fromStdString(def.standardName());
        obj["standardVersion"] = QString::fromStdString(def.standardVersion());
        obj["grade"] = QString::fromStdString(def.grade());
        obj["diameter"] = def.nominalDiameter();
        obj["area"] = def.metallicArea();
        obj["linearMass"] = def.linearMass();
        obj["elasticModulus"] = def.elasticModulus();
        obj["density"] = def.density();
        obj["characteristicStrength"] = def.characteristicStrength();
        obj["defaultInitialTension"] = def.defaultInitialTension();
        obj["tensionOnly"] = def.tensionOnly();
        arr.append(obj);
    }

    QJsonObject root;
    root["customCables"] = arr;
    QJsonDocument doc(root);
    return doc.toJson(QJsonDocument::Indented).toStdString();
}

bool CableLibrary::importFromJson(const std::string& jsonString)
{
    QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(jsonString));
    if (!doc.isObject()) return false;

    QJsonObject root = doc.object();
    if (!root.contains("customCables") || !root["customCables"].isArray()) return false;

    QJsonArray arr = root["customCables"].toArray();
    for (const auto& val : arr)
    {
        if (!val.isObject()) continue;
        QJsonObject obj = val.toObject();

        TSA::Model::CableDefinition def;
        def.setId(obj["id"].toString().toStdString());
        def.setName(obj["name"].toString().toStdString());
        def.setType(TSA::Model::stringToCableType(obj["type"].toString().toStdString()));
        def.setStandardName(obj["standard"].toString().toStdString());
        def.setStandardVersion(obj["standardVersion"].toString().toStdString());
        def.setGrade(obj["grade"].toString().toStdString());
        def.setNominalDiameter(obj["diameter"].toDouble(0.020));
        def.setMetallicArea(obj["area"].toDouble(3.14e-4));
        def.setLinearMass(obj["linearMass"].toDouble(2.46));
        def.setElasticModulus(obj["elasticModulus"].toDouble(195e9));
        def.setDensity(obj["density"].toDouble(7850.0));
        def.setCharacteristicStrength(obj["characteristicStrength"].toDouble(1860e6));
        def.setDefaultInitialTension(obj["defaultInitialTension"].toDouble(100e3));
        def.setTensionOnly(obj["tensionOnly"].toBool(true));

        addCustomDefinition(def);
    }
    return true;
}

} // namespace TSA::Library
