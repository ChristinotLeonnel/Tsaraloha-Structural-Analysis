#include "CoordinateSystem.h"
#include "../Model/Model.h"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <iomanip>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

namespace TSA::Coordinate
{

CoordinateSystem::CoordinateSystem(QObject* parent)
    : QObject(parent)
    , m_levelManager(std::make_shared<LevelManager>(this))
{
    connect(m_levelManager.get(), &LevelManager::levelsChanged, this, &CoordinateSystem::coordinatesChanged);
}

std::string CoordinateSystem::indexToLetter(size_t index)
{
    std::string result;
    index++;
    while (index > 0)
    {
        index--;
        result = static_cast<char>('A' + (index % 26)) + result;
        index /= 26;
    }
    return result;
}

void CoordinateSystem::synchronizeLabels()
{
    while (m_xLabels.size() < m_xPositions.size())
    {
        m_xLabels.push_back(std::to_string(m_xLabels.size() + 1));
    }
    if (m_xLabels.size() > m_xPositions.size())
    {
        m_xLabels.resize(m_xPositions.size());
    }

    while (m_yLabels.size() < m_yPositions.size())
    {
        m_yLabels.push_back(indexToLetter(m_yLabels.size()));
    }
    if (m_yLabels.size() > m_yPositions.size())
    {
        m_yLabels.resize(m_yPositions.size());
    }
}

void CoordinateSystem::setXPositions(const std::vector<double>& positions)
{
    m_xPositions = positions;
    std::sort(m_xPositions.begin(), m_xPositions.end());
    m_xPositions.erase(std::unique(m_xPositions.begin(), m_xPositions.end(), [](double a, double b) {
        return std::abs(a - b) < 1e-6;
    }), m_xPositions.end());

    synchronizeLabels();
    emit coordinatesChanged();
}

void CoordinateSystem::setXSpacings(double startX, const std::vector<double>& spacings)
{
    std::vector<double> positions;
    positions.push_back(startX);
    double cur = startX;
    for (double d : spacings)
    {
        cur += d;
        positions.push_back(cur);
    }
    setXPositions(positions);
}

std::vector<double> CoordinateSystem::getXSpacings() const
{
    std::vector<double> deltas;
    if (m_xPositions.size() < 2)
        return deltas;

    deltas.reserve(m_xPositions.size() - 1);
    for (size_t i = 1; i < m_xPositions.size(); ++i)
    {
        deltas.push_back(m_xPositions[i] - m_xPositions[i - 1]);
    }
    return deltas;
}

void CoordinateSystem::setYPositions(const std::vector<double>& positions)
{
    m_yPositions = positions;
    std::sort(m_yPositions.begin(), m_yPositions.end());
    m_yPositions.erase(std::unique(m_yPositions.begin(), m_yPositions.end(), [](double a, double b) {
        return std::abs(a - b) < 1e-6;
    }), m_yPositions.end());

    synchronizeLabels();
    emit coordinatesChanged();
}

void CoordinateSystem::setYSpacings(double startY, const std::vector<double>& spacings)
{
    std::vector<double> positions;
    positions.push_back(startY);
    double cur = startY;
    for (double d : spacings)
    {
        cur += d;
        positions.push_back(cur);
    }
    setYPositions(positions);
}

std::vector<double> CoordinateSystem::getYSpacings() const
{
    std::vector<double> deltas;
    if (m_yPositions.size() < 2)
        return deltas;

    deltas.reserve(m_yPositions.size() - 1);
    for (size_t i = 1; i < m_yPositions.size(); ++i)
    {
        deltas.push_back(m_yPositions[i] - m_yPositions[i - 1]);
    }
    return deltas;
}

std::vector<double> CoordinateSystem::zLevels() const
{
    return m_levelManager ? m_levelManager->elevationList() : std::vector<double>{};
}

size_t CoordinateSystem::zCount() const
{
    return m_levelManager ? m_levelManager->levelCount() : 0;
}

void CoordinateSystem::setZLevels(const std::vector<double>& elevations)
{
    if (m_levelManager)
    {
        m_levelManager->setFromElevations(elevations);
    }
}

void CoordinateSystem::setZSpacings(double startZ, const std::vector<double>& spacings)
{
    if (m_levelManager)
    {
        m_levelManager->setFromSpacings(startZ, spacings);
    }
}

std::vector<double> CoordinateSystem::getZSpacings() const
{
    return m_levelManager ? m_levelManager->getSpacings() : std::vector<double>{};
}

Point3D CoordinateSystem::gridPoint(int ix, int iy, int iz) const
{
    double x = 0.0, y = 0.0, z = 0.0;
    if (ix >= 0 && static_cast<size_t>(ix) < m_xPositions.size())
        x = m_xPositions[ix];

    if (iy >= 0 && static_cast<size_t>(iy) < m_yPositions.size())
        y = m_yPositions[iy];

    if (m_levelManager && iz >= 0 && static_cast<size_t>(iz) < m_levelManager->levelCount())
    {
        const auto* lvl = m_levelManager->getLevelByIndex(iz);
        if (lvl) z = lvl->elevation;
    }

    return Point3D(x, y, z);
}

void CoordinateSystem::setXLabels(const std::vector<std::string>& labels)
{
    m_xLabels = labels;
    synchronizeLabels();
    emit coordinatesChanged();
}

void CoordinateSystem::setYLabels(const std::vector<std::string>& labels)
{
    m_yLabels = labels;
    synchronizeLabels();
    emit coordinatesChanged();
}

std::string CoordinateSystem::getXLabel(size_t index) const
{
    if (index < m_xLabels.size())
        return m_xLabels[index];
    return std::to_string(index + 1);
}

std::string CoordinateSystem::getYLabel(size_t index) const
{
    if (index < m_yLabels.size())
        return m_yLabels[index];
    return indexToLetter(index);
}

std::string CoordinateSystem::getZLabel(size_t index) const
{
    if (m_levelManager)
    {
        const auto* lvl = m_levelManager->getLevelByIndex(index);
        if (lvl)
            return lvl->name;
    }
    return "Niveau " + std::to_string(index);
}

bool CoordinateSystem::findClosestX(double x, double tol, double& outX, int& outIdx) const
{
    outIdx = -1;
    double minDiff = tol + 1.0;
    for (size_t i = 0; i < m_xPositions.size(); ++i)
    {
        double diff = std::abs(m_xPositions[i] - x);
        if (diff <= tol && diff < minDiff)
        {
            minDiff = diff;
            outX = m_xPositions[i];
            outIdx = static_cast<int>(i);
        }
    }
    return (outIdx != -1);
}

bool CoordinateSystem::findClosestY(double y, double tol, double& outY, int& outIdx) const
{
    outIdx = -1;
    double minDiff = tol + 1.0;
    for (size_t i = 0; i < m_yPositions.size(); ++i)
    {
        double diff = std::abs(m_yPositions[i] - y);
        if (diff <= tol && diff < minDiff)
        {
            minDiff = diff;
            outY = m_yPositions[i];
            outIdx = static_cast<int>(i);
        }
    }
    return (outIdx != -1);
}

bool CoordinateSystem::findClosestZ(double z, double tol, double& outZ, int& outIdx) const
{
    outIdx = -1;
    double minDiff = tol + 1.0;
    std::vector<double> levels = zLevels();
    for (size_t i = 0; i < levels.size(); ++i)
    {
        double diff = std::abs(levels[i] - z);
        if (diff <= tol && diff < minDiff)
        {
            minDiff = diff;
            outZ = levels[i];
            outIdx = static_cast<int>(i);
        }
    }
    return (outIdx != -1);
}

bool CoordinateSystem::findClosestIntersection(const Point3D& rawPnt, double tol, Point3D& outPnt, int& outIx, int& outIy, int& outIz) const
{
    double cx = 0.0, cy = 0.0, cz = 0.0;
    bool foundX = findClosestX(rawPnt.x, tol, cx, outIx);
    bool foundY = findClosestY(rawPnt.y, tol, cy, outIy);
    bool foundZ = findClosestZ(rawPnt.z, tol, cz, outIz);

    if (foundX && foundY && foundZ)
    {
        outPnt = Point3D(cx, cy, cz);
        return true;
    }
    return false;
}

void CoordinateSystem::setDefaultBuildingCoordinates()
{
    setXPositions({0.0, 6.0, 12.0, 18.0});
    setYPositions({0.0, 4.0, 8.0, 12.0});

    std::vector<double> zElevations = {0.0, 3.0, 6.5, 10.0};
    std::vector<std::string> zNames = {"RDC (Z=0.00)", "Étage 1 (Z=3.00)", "Étage 2 (Z=6.50)", "Toiture (Z=10.00)"};
    m_levelManager->setFromElevations(zElevations, zNames);
}

std::string CoordinateSystem::serializeToJson() const
{
    QJsonObject root;

    QJsonArray xArr;
    for (double x : m_xPositions) xArr.append(x);
    root["xPositions"] = xArr;

    QJsonArray yArr;
    for (double y : m_yPositions) yArr.append(y);
    root["yPositions"] = yArr;

    QJsonArray xlArr;
    for (const auto& s : m_xLabels) xlArr.append(QString::fromStdString(s));
    root["xLabels"] = xlArr;

    QJsonArray ylArr;
    for (const auto& s : m_yLabels) ylArr.append(QString::fromStdString(s));
    root["yLabels"] = ylArr;

    if (m_levelManager)
    {
        QJsonDocument levelDoc = QJsonDocument::fromJson(
            QByteArray::fromStdString(m_levelManager->serializeToJson()));
        if (levelDoc.isArray())
            root["levels"] = levelDoc.array();
    }

    QJsonDocument doc(root);
    return doc.toJson(QJsonDocument::Compact).toStdString();
}

void CoordinateSystem::deserializeFromJson(const std::string& json)
{
    QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(json));
    if (!doc.isObject())
        return;

    QJsonObject root = doc.object();

    m_xPositions.clear();
    for (const auto& v : root["xPositions"].toArray()) m_xPositions.push_back(v.toDouble());

    m_yPositions.clear();
    for (const auto& v : root["yPositions"].toArray()) m_yPositions.push_back(v.toDouble());

    m_xLabels.clear();
    for (const auto& v : root["xLabels"].toArray()) m_xLabels.push_back(v.toString().toStdString());

    m_yLabels.clear();
    for (const auto& v : root["yLabels"].toArray()) m_yLabels.push_back(v.toString().toStdString());

    if (m_levelManager && root.contains("levels"))
    {
        if (root["levels"].isArray())
        {
            QJsonDocument lvlDoc(root["levels"].toArray());
            m_levelManager->deserializeFromJson(lvlDoc.toJson(QJsonDocument::Compact).toStdString());
        }
        else if (root["levels"].isString())
        {
            // Rétrocompatibilité avec l'ancien format double-encodé
            m_levelManager->deserializeFromJson(root["levels"].toString().toStdString());
        }
    }

    synchronizeLabels();
    emit coordinatesChanged();
}

std::vector<DetectedPlaneInfo> CoordinateSystem::detectStructuralPlanes(WorkPlaneAxis axis, const TSA::Model::Model* model) const
{
    std::vector<DetectedPlaneInfo> result;
    const double tol = 0.05; // 5 cm de tolérance pour le regroupement de plans proches

    auto formatOffset = [](double val) {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(2) << val;
        return oss.str();
    };

    if (axis == WorkPlaneAxis::Z)
    {
        // 1. Niveaux existants du LevelManager
        if (m_levelManager && !m_levelManager->isEmpty())
        {
            for (const auto& lvl : m_levelManager->levels())
            {
                DetectedPlaneInfo info;
                info.axis = WorkPlaneAxis::Z;
                info.offset = lvl.elevation;
                info.name = lvl.name.empty() ? ("Z = " + formatOffset(lvl.elevation) + " m") : lvl.name;
                info.id = lvl.id;
                result.push_back(info);
            }
        }
        else
        {
            // Collecter les Z uniques des nœuds du modèle
            std::vector<double> zVals;
            if (model)
            {
                for (const auto& [nid, node] : model->nodes())
                {
                    double z = node.z();
                    bool found = false;
                    for (double existing : zVals)
                    {
                        if (std::abs(existing - z) <= tol) { found = true; break; }
                    }
                    if (!found) zVals.push_back(z);
                }
            }
            std::sort(zVals.begin(), zVals.end());
            if (zVals.empty()) zVals.push_back(0.0);

            for (size_t i = 0; i < zVals.size(); ++i)
            {
                DetectedPlaneInfo info;
                info.axis = WorkPlaneAxis::Z;
                info.offset = zVals[i];
                info.name = "Niveau Z = " + formatOffset(zVals[i]) + " m";
                info.id = "z_" + std::to_string(i);
                result.push_back(info);
            }
        }
    }
    else if (axis == WorkPlaneAxis::X)
    {
        // 1. Positions X de la grille cartésienne
        std::vector<std::pair<double, std::string>> xVals;
        for (size_t i = 0; i < m_xPositions.size(); ++i)
        {
            std::string label = (i < m_xLabels.size() && !m_xLabels[i].empty())
                ? (m_xLabels[i] + " (X = " + formatOffset(m_xPositions[i]) + " m)")
                : ("Axe X = " + formatOffset(m_xPositions[i]) + " m");
            xVals.push_back({ m_xPositions[i], label });
        }

        // 2. Coordonnées X des nœuds du modèle
        if (model)
        {
            for (const auto& [nid, node] : model->nodes())
            {
                double x = node.x();
                bool found = false;
                for (const auto& existing : xVals)
                {
                    if (std::abs(existing.first - x) <= tol) { found = true; break; }
                }
                if (!found)
                {
                    xVals.push_back({ x, "Axe X = " + formatOffset(x) + " m" });
                }
            }
        }

        std::sort(xVals.begin(), xVals.end(), [](const auto& a, const auto& b) {
            return a.first < b.first;
        });

        if (xVals.empty())
        {
            xVals.push_back({ 0.0, "Axe X = 0.00 m" });
        }

        for (size_t i = 0; i < xVals.size(); ++i)
        {
            DetectedPlaneInfo info;
            info.axis = WorkPlaneAxis::X;
            info.offset = xVals[i].first;
            info.name = xVals[i].second;
            info.id = "x_" + std::to_string(i);
            result.push_back(info);
        }
    }
    else if (axis == WorkPlaneAxis::Y)
    {
        // 1. Positions Y de la grille cartésienne
        std::vector<std::pair<double, std::string>> yVals;
        for (size_t i = 0; i < m_yPositions.size(); ++i)
        {
            std::string label = (i < m_yLabels.size() && !m_yLabels[i].empty())
                ? (m_yLabels[i] + " (Y = " + formatOffset(m_yPositions[i]) + " m)")
                : ("Axe Y = " + formatOffset(m_yPositions[i]) + " m");
            yVals.push_back({ m_yPositions[i], label });
        }

        // 2. Coordonnées Y des nœuds du modèle
        if (model)
        {
            for (const auto& [nid, node] : model->nodes())
            {
                double y = node.y();
                bool found = false;
                for (const auto& existing : yVals)
                {
                    if (std::abs(existing.first - y) <= tol) { found = true; break; }
                }
                if (!found)
                {
                    yVals.push_back({ y, "Axe Y = " + formatOffset(y) + " m" });
                }
            }
        }

        std::sort(yVals.begin(), yVals.end(), [](const auto& a, const auto& b) {
            return a.first < b.first;
        });

        if (yVals.empty())
        {
            yVals.push_back({ 0.0, "Axe Y = 0.00 m" });
        }

        for (size_t i = 0; i < yVals.size(); ++i)
        {
            DetectedPlaneInfo info;
            info.axis = WorkPlaneAxis::Y;
            info.offset = yVals[i].first;
            info.name = yVals[i].second;
            info.id = "y_" + std::to_string(i);
            result.push_back(info);
        }
    }

    return result;
}

} // namespace TSA::Coordinate
