#include "WorkPlaneManager.h"
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>

namespace TSA::Coordinate
{

WorkPlaneManager::WorkPlaneManager(QObject* parent)
    : QObject(parent)
{
    resetToDefault();
}

void WorkPlaneManager::resetToDefault()
{
    m_workPlanes.clear();
    m_nextId = 1;

    WorkPlane wpXY = WorkPlane::xy(0.0, "Plan XY (Base)");
    wpXY.setId(m_nextId++);
    wpXY.setActive(true);
    wpXY.setVisible(true);

    WorkPlane wpXZ = WorkPlane::xz(0.0, "Plan XZ (Façade)");
    wpXZ.setId(m_nextId++);
    wpXZ.setActive(false);
    wpXZ.setVisible(false);

    WorkPlane wpYZ = WorkPlane::yz(0.0, "Plan YZ (Pignon)");
    wpYZ.setId(m_nextId++);
    wpYZ.setActive(false);
    wpYZ.setVisible(false);

    m_workPlanes[wpXY.id()] = wpXY;
    m_workPlanes[wpXZ.id()] = wpXZ;
    m_workPlanes[wpYZ.id()] = wpYZ;

    m_activeWorkPlaneId = wpXY.id();
}

void WorkPlaneManager::clear()
{
    m_workPlanes.clear();
    m_activeWorkPlaneId = -1;
    m_nextId = 1;
}

int WorkPlaneManager::addWorkPlane(const WorkPlane& wp)
{
    WorkPlane copy = wp;
    int id = (copy.id() > 0 && m_workPlanes.find(copy.id()) == m_workPlanes.end()) ? copy.id() : m_nextId++;
    copy.setId(id);
    m_workPlanes[id] = copy;
    if (m_workPlanes.size() == 1 || copy.isActive())
    {
        setActiveWorkPlane(id);
    }
    emit workPlaneAdded(id);
    return id;
}

bool WorkPlaneManager::removeWorkPlane(int id)
{
    auto it = m_workPlanes.find(id);
    if (it == m_workPlanes.end())
        return false;

    m_workPlanes.erase(it);
    emit workPlaneRemoved(id);

    if (m_activeWorkPlaneId == id)
    {
        if (!m_workPlanes.empty())
        {
            setActiveWorkPlane(m_workPlanes.begin()->first);
        }
        else
        {
            m_activeWorkPlaneId = -1;
            emit activeWorkPlaneChanged(-1);
        }
    }
    return true;
}

WorkPlane* WorkPlaneManager::getWorkPlane(int id)
{
    auto it = m_workPlanes.find(id);
    return (it != m_workPlanes.end()) ? &it->second : nullptr;
}

const WorkPlane* WorkPlaneManager::getWorkPlane(int id) const
{
    auto it = m_workPlanes.find(id);
    return (it != m_workPlanes.end()) ? &it->second : nullptr;
}

void WorkPlaneManager::updateWorkPlane(const WorkPlane& wp)
{
    auto it = m_workPlanes.find(wp.id());
    if (it != m_workPlanes.end())
    {
        it->second = wp;
        emit workPlaneModified(wp.id());
        if (wp.id() == m_activeWorkPlaneId)
        {
            emit activeWorkPlaneChanged(wp.id());
        }
    }
    else
    {
        addWorkPlane(wp);
    }
}

bool WorkPlaneManager::setActiveWorkPlane(int id)
{
    auto it = m_workPlanes.find(id);
    if (it == m_workPlanes.end())
        return false;

    for (auto& [wpId, wp] : m_workPlanes)
    {
        wp.setActive(wpId == id);
    }

    m_activeWorkPlaneId = id;
    emit activeWorkPlaneChanged(id);
    return true;
}

WorkPlane* WorkPlaneManager::activeWorkPlane()
{
    return getWorkPlane(m_activeWorkPlaneId);
}

const WorkPlane* WorkPlaneManager::activeWorkPlane() const
{
    return getWorkPlane(m_activeWorkPlaneId);
}

std::string WorkPlaneManager::serializeToJson() const
{
    QJsonObject root;
    root["activeId"] = m_activeWorkPlaneId;
    root["nextId"] = m_nextId;

    QJsonArray arr;
    for (const auto& [id, wp] : m_workPlanes)
    {
        QJsonParseError err;
        QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(wp.serializeToJson()), &err);
        if (!doc.isNull() && doc.isObject())
        {
            arr.append(doc.object());
        }
    }
    root["planes"] = arr;

    QJsonDocument outDoc(root);
    return outDoc.toJson(QJsonDocument::Indented).toStdString();
}

bool WorkPlaneManager::deserializeFromJson(const std::string& jsonStr)
{
    QByteArray bytes = QByteArray::fromRawData(jsonStr.data(), static_cast<int>(jsonStr.size()));
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(bytes, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject())
        return false;

    QJsonObject root = doc.object();
    int activeId = root.value("activeId").toInt(1);
    m_nextId = root.value("nextId").toInt(1);

    m_workPlanes.clear();
    QJsonArray arr = root.value("planes").toArray();
    for (const auto& val : arr)
    {
        if (val.isObject())
        {
            QJsonDocument pDoc(val.toObject());
            WorkPlane wp = WorkPlane::deserializeFromJson(pDoc.toJson().toStdString());
            m_workPlanes[wp.id()] = wp;
            if (wp.id() >= m_nextId)
            {
                m_nextId = wp.id() + 1;
            }
        }
    }

    if (m_workPlanes.find(activeId) != m_workPlanes.end())
    {
        setActiveWorkPlane(activeId);
    }
    else if (!m_workPlanes.empty())
    {
        setActiveWorkPlane(m_workPlanes.begin()->first);
    }

    return true;
}

} // namespace TSA::Coordinate
