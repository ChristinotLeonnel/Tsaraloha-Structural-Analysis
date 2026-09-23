#include "LevelManager.h"
#include <algorithm>
#include <sstream>
#include <cmath>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

namespace TSA::Coordinate
{

LevelManager::LevelManager(QObject* parent)
    : QObject(parent)
{
}

void LevelManager::sortLevels()
{
    std::sort(m_levels.begin(), m_levels.end(), [](const Level& a, const Level& b) {
        return a.elevation < b.elevation;
    });
}

std::string LevelManager::generateDefaultName(size_t index, double elevation) const
{
    std::ostringstream ss;
    if (index == 0 && std::abs(elevation) < 1e-4)
    {
        return "RDC";
    }
    ss << "Niveau " << index;
    return ss.str();
}

Level* LevelManager::addLevel(const std::string& name, double elevation)
{
    std::string id = "lvl_" + std::to_string(++m_nextLevelCounter);
    std::string actualName = name.empty() ? generateDefaultName(m_levels.size(), elevation) : name;
    return addLevelWithId(id, actualName, elevation);
}

Level* LevelManager::addLevelWithId(const std::string& id, const std::string& name, double elevation)
{
    // Vérifier si l'identifiant existe déjà
    for (auto& lvl : m_levels)
    {
        if (lvl.id == id)
        {
            lvl.name = name;
            lvl.elevation = elevation;
            sortLevels();
            emit levelModified(id);
            emit levelsChanged();
            return getLevel(id);
        }
    }

    m_levels.emplace_back(id, name, elevation, true);
    sortLevels();

    emit levelAdded(id);
    emit levelsChanged();

    return getLevel(id);
}

bool LevelManager::removeLevel(const std::string& id)
{
    auto it = std::find_if(m_levels.begin(), m_levels.end(), [&id](const Level& l) {
        return l.id == id;
    });

    if (it != m_levels.end())
    {
        m_levels.erase(it);
        emit levelRemoved(id);
        emit levelsChanged();
        return true;
    }
    return false;
}

Level* LevelManager::getLevel(const std::string& id)
{
    for (auto& lvl : m_levels)
    {
        if (lvl.id == id)
            return &lvl;
    }
    return nullptr;
}

const Level* LevelManager::getLevel(const std::string& id) const
{
    for (const auto& lvl : m_levels)
    {
        if (lvl.id == id)
            return &lvl;
    }
    return nullptr;
}

Level* LevelManager::getLevelByIndex(size_t index)
{
    if (index < m_levels.size())
        return &m_levels[index];
    return nullptr;
}

const Level* LevelManager::getLevelByIndex(size_t index) const
{
    if (index < m_levels.size())
        return &m_levels[index];
    return nullptr;
}

int LevelManager::getLevelIndex(const std::string& id) const
{
    for (size_t i = 0; i < m_levels.size(); ++i)
    {
        if (m_levels[i].id == id)
            return static_cast<int>(i);
    }
    return -1;
}

bool LevelManager::setLevelElevation(const std::string& id, double newElevation)
{
    auto* lvl = getLevel(id);
    if (!lvl)
        return false;

    if (std::abs(lvl->elevation - newElevation) < 1e-6)
        return false;

    double oldElevation = lvl->elevation;
    lvl->elevation = newElevation;
    sortLevels();

    emit levelElevationChanged(id, oldElevation, newElevation);
    emit levelModified(id);
    emit levelsChanged();
    return true;
}

bool LevelManager::setLevelName(const std::string& id, const std::string& newName)
{
    auto* lvl = getLevel(id);
    if (!lvl)
        return false;

    lvl->name = newName;
    emit levelModified(id);
    emit levelsChanged();
    return true;
}

bool LevelManager::setLevelVisible(const std::string& id, bool visible)
{
    auto* lvl = getLevel(id);
    if (!lvl)
        return false;

    lvl->visible = visible;
    emit levelModified(id);
    emit levelsChanged();
    return true;
}

const Level* LevelManager::findLevelAtElevation(double z, double tolerance) const
{
    for (const auto& lvl : m_levels)
    {
        if (std::abs(lvl.elevation - z) <= tolerance)
            return &lvl;
    }
    return nullptr;
}

const Level* LevelManager::findClosestLevel(double z) const
{
    if (m_levels.empty())
        return nullptr;

    const Level* closest = &m_levels[0];
    double minDiff = std::abs(m_levels[0].elevation - z);

    for (size_t i = 1; i < m_levels.size(); ++i)
    {
        double diff = std::abs(m_levels[i].elevation - z);
        if (diff < minDiff)
        {
            minDiff = diff;
            closest = &m_levels[i];
        }
    }
    return closest;
}

std::vector<double> LevelManager::elevationList() const
{
    std::vector<double> res;
    res.reserve(m_levels.size());
    for (const auto& lvl : m_levels)
    {
        res.push_back(lvl.elevation);
    }
    return res;
}

void LevelManager::setFromElevations(const std::vector<double>& elevations, const std::vector<std::string>& names)
{
    m_levels.clear();
    m_nextLevelCounter = 0;

    for (size_t i = 0; i < elevations.size(); ++i)
    {
        std::string id = "lvl_" + std::to_string(++m_nextLevelCounter);
        std::string name = (i < names.size() && !names[i].empty())
                               ? names[i]
                               : generateDefaultName(i, elevations[i]);
        m_levels.emplace_back(id, name, elevations[i], true);
    }

    sortLevels();
    emit levelsChanged();
}

void LevelManager::setFromSpacings(double baseElevation, const std::vector<double>& spacings, const std::vector<std::string>& names)
{
    std::vector<double> elevations;
    elevations.push_back(baseElevation);
    double cur = baseElevation;
    for (double d : spacings)
    {
        cur += d;
        elevations.push_back(cur);
    }
    setFromElevations(elevations, names);
}

std::vector<double> LevelManager::getSpacings() const
{
    std::vector<double> deltas;
    if (m_levels.size() < 2)
        return deltas;

    deltas.reserve(m_levels.size() - 1);
    for (size_t i = 1; i < m_levels.size(); ++i)
    {
        deltas.push_back(m_levels[i].elevation - m_levels[i - 1].elevation);
    }
    return deltas;
}

void LevelManager::clear()
{
    m_levels.clear();
    m_nextLevelCounter = 0;
    emit levelsChanged();
}

std::string LevelManager::serializeToJson() const
{
    QJsonArray arr;
    for (const auto& lvl : m_levels)
    {
        QJsonObject obj;
        obj["id"] = QString::fromStdString(lvl.id);
        obj["name"] = QString::fromStdString(lvl.name);
        obj["elevation"] = lvl.elevation;
        obj["visible"] = lvl.visible;
        arr.append(obj);
    }
    QJsonDocument doc(arr);
    return doc.toJson(QJsonDocument::Compact).toStdString();
}

void LevelManager::deserializeFromJson(const std::string& json)
{
    QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(json));
    if (!doc.isArray())
        return;

    m_levels.clear();
    QJsonArray arr = doc.array();
    for (const auto& val : arr)
    {
        if (val.isObject())
        {
            QJsonObject obj = val.toObject();
            Level lvl;
            lvl.id = obj["id"].toString().toStdString();
            lvl.name = obj["name"].toString().toStdString();
            lvl.elevation = obj["elevation"].toDouble();
            lvl.visible = obj["visible"].toBool(true);
            m_levels.push_back(lvl);
        }
    }
    sortLevels();

    // Recalculer le compteur pour éviter les collisions d'ID après désérialisation
    m_nextLevelCounter = 0;
    for (const auto& lvl : m_levels)
    {
        if (lvl.id.size() > 4 && lvl.id.substr(0, 4) == "lvl_")
        {
            try {
                int n = std::stoi(lvl.id.substr(4));
                if (n > m_nextLevelCounter) m_nextLevelCounter = n;
            } catch (...) {}
        }
    }

    emit levelsChanged();
}

} // namespace TSA::Coordinate
