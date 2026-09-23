#include "GridDefinition.h"

#include <algorithm>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <chrono>

namespace TSA::Grid
{

static std::string generateUniqueId()
{
    static int counter = 1;
    auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    std::ostringstream oss;
    oss << "grid_" << now << "_" << counter++;
    return oss.str();
}

GridDefinition::GridDefinition()
    : m_id(generateUniqueId())
    , m_name("Main Grid")
    , m_type(GridType::Cartesian)
    , m_origin(0.0, 0.0, 0.0)
{
    // Grille par défaut professionnelle (X: 0, 5, 10, 15m ; Y: 0, 4, 8m ; Z: 0, 3, 6m)
    m_xPositions = { 0.0, 5.0, 10.0, 15.0 };
    m_yPositions = { 0.0, 4.0, 8.0 };
    m_zLevels    = { 0.0, 3.0, 6.0 };

    m_radii  = { 2.0, 4.0, 6.0, 8.0 };
    m_angles = { 0.0, 30.0, 60.0, 90.0, 120.0, 150.0, 180.0, 210.0, 240.0, 270.0, 300.0, 330.0 };

    ensureLabelsSynchronized();
}

GridDefinition::GridDefinition(const std::string& name, GridType type)
    : m_id(generateUniqueId())
    , m_name(name)
    , m_type(type)
    , m_origin(0.0, 0.0, 0.0)
{
    if (type == GridType::Cartesian)
    {
        m_xPositions = { 0.0, 5.0, 10.0, 15.0 };
        m_yPositions = { 0.0, 4.0, 8.0 };
        m_zLevels    = { 0.0, 3.0, 6.0 };
    }
    else
    {
        m_radii   = { 2.0, 4.0, 6.0, 8.0 };
        m_angles  = { 0.0, 30.0, 60.0, 90.0, 120.0, 150.0, 180.0, 210.0, 240.0, 270.0, 300.0, 330.0 };
        m_zLevels = { 0.0, 3.0, 6.0 };
    }

    ensureLabelsSynchronized();
}

void GridDefinition::setXPositions(const std::vector<double>& positions)
{
    m_xPositions = positions;
    std::sort(m_xPositions.begin(), m_xPositions.end());
    m_xPositions.erase(std::unique(m_xPositions.begin(), m_xPositions.end()), m_xPositions.end());
    ensureLabelsSynchronized();
}

void GridDefinition::setYPositions(const std::vector<double>& positions)
{
    m_yPositions = positions;
    std::sort(m_yPositions.begin(), m_yPositions.end());
    m_yPositions.erase(std::unique(m_yPositions.begin(), m_yPositions.end()), m_yPositions.end());
    ensureLabelsSynchronized();
}

void GridDefinition::setZLevels(const std::vector<double>& levels)
{
    m_zLevels = levels;
    std::sort(m_zLevels.begin(), m_zLevels.end());
    m_zLevels.erase(std::unique(m_zLevels.begin(), m_zLevels.end()), m_zLevels.end());
    ensureLabelsSynchronized();
}

void GridDefinition::setRadii(const std::vector<double>& radii)
{
    m_radii = radii;
    std::sort(m_radii.begin(), m_radii.end());
    m_radii.erase(std::unique(m_radii.begin(), m_radii.end()), m_radii.end());
    ensureLabelsSynchronized();
}

void GridDefinition::setAngles(const std::vector<double>& anglesDeg)
{
    m_angles = anglesDeg;
    for (double& a : m_angles)
    {
        while (a < 0.0) a += 360.0;
        while (a >= 360.0) a -= 360.0;
    }
    std::sort(m_angles.begin(), m_angles.end());
    m_angles.erase(std::unique(m_angles.begin(), m_angles.end()), m_angles.end());
    ensureLabelsSynchronized();
}

std::string GridDefinition::getXLabel(size_t index) const
{
    if (index < m_xLabels.size() && !m_xLabels[index].empty())
        return m_xLabels[index];
    return std::to_string(index + 1);
}

std::string GridDefinition::getYLabel(size_t index) const
{
    if (index < m_yLabels.size() && !m_yLabels[index].empty())
        return m_yLabels[index];

    // Lettres A, B, C, ... AA, AB etc.
    std::string label;
    int n = static_cast<int>(index);
    do
    {
        label = static_cast<char>('A' + (n % 26)) + label;
        n = (n / 26) - 1;
    } while (n >= 0);
    return label;
}

std::string GridDefinition::getZLabel(size_t index) const
{
    if (index < m_zLabels.size() && !m_zLabels[index].empty())
        return m_zLabels[index];

    if (index < m_zLevels.size())
    {
        std::ostringstream oss;
        oss << "Niv." << index << " (" << std::fixed << std::setprecision(2) << m_zLevels[index] << "m)";
        return oss.str();
    }
    return "Niv." + std::to_string(index);
}

std::string GridDefinition::getRadiusLabel(size_t index) const
{
    if (index < m_radiusLabels.size() && !m_radiusLabels[index].empty())
        return m_radiusLabels[index];

    if (index < m_radii.size())
    {
        std::ostringstream oss;
        oss << "R" << (index + 1) << " (" << std::fixed << std::setprecision(1) << m_radii[index] << "m)";
        return oss.str();
    }
    return "R" + std::to_string(index + 1);
}

std::string GridDefinition::getAngleLabel(size_t index) const
{
    if (index < m_angleLabels.size() && !m_angleLabels[index].empty())
        return m_angleLabels[index];

    if (index < m_angles.size())
    {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(0) << m_angles[index] << "°";
        return oss.str();
    }
    return std::to_string(index) + "°";
}

void GridDefinition::ensureLabelsSynchronized()
{
    // Synchronisation des labels X (Numéros 1, 2, 3...)
    if (m_xLabels.size() < m_xPositions.size())
    {
        for (size_t i = m_xLabels.size(); i < m_xPositions.size(); ++i)
        {
            m_xLabels.push_back(std::to_string(i + 1));
        }
    }
    else if (m_xLabels.size() > m_xPositions.size())
    {
        m_xLabels.resize(m_xPositions.size());
    }

    // Synchronisation des labels Y (Lettres A, B, C...)
    if (m_yLabels.size() < m_yPositions.size())
    {
        for (size_t i = m_yLabels.size(); i < m_yPositions.size(); ++i)
        {
            std::string label;
            int n = static_cast<int>(i);
            do
            {
                label = static_cast<char>('A' + (n % 26)) + label;
                n = (n / 26) - 1;
            } while (n >= 0);
            m_yLabels.push_back(label);
        }
    }
    else if (m_yLabels.size() > m_yPositions.size())
    {
        m_yLabels.resize(m_yPositions.size());
    }

    // Synchronisation des niveaux Z
    if (m_zLabels.size() < m_zLevels.size())
    {
        for (size_t i = m_zLabels.size(); i < m_zLevels.size(); ++i)
        {
            std::ostringstream oss;
            oss << "Niv." << i << " (" << std::fixed << std::setprecision(2) << m_zLevels[i] << "m)";
            m_zLabels.push_back(oss.str());
        }
    }
    else if (m_zLabels.size() > m_zLevels.size())
    {
        m_zLabels.resize(m_zLevels.size());
    }

    // Synchronisation des rayons
    if (m_radiusLabels.size() < m_radii.size())
    {
        for (size_t i = m_radiusLabels.size(); i < m_radii.size(); ++i)
        {
            std::ostringstream oss;
            oss << "R" << (i + 1) << " (" << std::fixed << std::setprecision(1) << m_radii[i] << "m)";
            m_radiusLabels.push_back(oss.str());
        }
    }
    else if (m_radiusLabels.size() > m_radii.size())
    {
        m_radiusLabels.resize(m_radii.size());
    }

    // Synchronisation des angles
    if (m_angleLabels.size() < m_angles.size())
    {
        for (size_t i = m_angleLabels.size(); i < m_angles.size(); ++i)
        {
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(0) << m_angles[i] << "°";
            m_angleLabels.push_back(oss.str());
        }
    }
    else if (m_angleLabels.size() > m_angles.size())
    {
        m_angleLabels.resize(m_angles.size());
    }
}

void GridDefinition::generateCartesian(int countX, double spacingX,
                                       int countY, double spacingY,
                                       int countZ, double spacingZ)
{
    m_type = GridType::Cartesian;
    m_xPositions.clear();
    m_yPositions.clear();
    m_zLevels.clear();

    for (int i = 0; i <= countX; ++i)
    {
        m_xPositions.push_back(i * spacingX);
    }
    for (int j = 0; j <= countY; ++j)
    {
        m_yPositions.push_back(j * spacingY);
    }
    for (int k = 0; k <= countZ; ++k)
    {
        m_zLevels.push_back(k * spacingZ);
    }

    m_xLabels.clear();
    m_yLabels.clear();
    m_zLabels.clear();
    ensureLabelsSynchronized();
}

void GridDefinition::generateCylindrical(int radiusCount, double radiusSpacing,
                                         int angleCount, double angleSpacingDeg,
                                         int countZ, double spacingZ)
{
    m_type = GridType::Cylindrical;
    m_radii.clear();
    m_angles.clear();
    m_zLevels.clear();

    for (int i = 1; i <= radiusCount; ++i)
    {
        m_radii.push_back(i * radiusSpacing);
    }

    for (int j = 0; j < angleCount; ++j)
    {
        double ang = j * angleSpacingDeg;
        if (ang < 360.0)
        {
            m_angles.push_back(ang);
        }
    }

    for (int k = 0; k <= countZ; ++k)
    {
        m_zLevels.push_back(k * spacingZ);
    }

    m_radiusLabels.clear();
    m_angleLabels.clear();
    m_zLabels.clear();
    ensureLabelsSynchronized();
}

static std::string vectorToJsonArray(const std::vector<double>& vec)
{
    std::ostringstream oss;
    oss << "[";
    for (size_t i = 0; i < vec.size(); ++i)
    {
        oss << vec[i];
        if (i + 1 < vec.size()) oss << ", ";
    }
    oss << "]";
    return oss.str();
}

static std::string stringVectorToJsonArray(const std::vector<std::string>& vec)
{
    std::ostringstream oss;
    oss << "[";
    for (size_t i = 0; i < vec.size(); ++i)
    {
        oss << "\"" << vec[i] << "\"";
        if (i + 1 < vec.size()) oss << ", ";
    }
    oss << "]";
    return oss.str();
}

std::string GridDefinition::toJson() const
{
    std::ostringstream oss;
    oss << "{\n";
    oss << "  \"id\": \"" << m_id << "\",\n";
    oss << "  \"name\": \"" << m_name << "\",\n";
    oss << "  \"type\": \"" << (m_type == GridType::Cartesian ? "Cartesian" : "Cylindrical") << "\",\n";
    oss << "  \"origin\": [" << m_origin.X() << ", " << m_origin.Y() << ", " << m_origin.Z() << "],\n";

    if (m_type == GridType::Cartesian)
    {
        oss << "  \"xPositions\": " << vectorToJsonArray(m_xPositions) << ",\n";
        oss << "  \"yPositions\": " << vectorToJsonArray(m_yPositions) << ",\n";
        oss << "  \"xLabels\": " << stringVectorToJsonArray(m_xLabels) << ",\n";
        oss << "  \"yLabels\": " << stringVectorToJsonArray(m_yLabels) << ",\n";
    }
    else
    {
        oss << "  \"radii\": " << vectorToJsonArray(m_radii) << ",\n";
        oss << "  \"angles\": " << vectorToJsonArray(m_angles) << ",\n";
        oss << "  \"radiusLabels\": " << stringVectorToJsonArray(m_radiusLabels) << ",\n";
        oss << "  \"angleLabels\": " << stringVectorToJsonArray(m_angleLabels) << ",\n";
    }

    oss << "  \"zLevels\": " << vectorToJsonArray(m_zLevels) << ",\n";
    oss << "  \"zLabels\": " << stringVectorToJsonArray(m_zLabels) << "\n";
    oss << "}";
    return oss.str();
}

static std::vector<double> parseDoubleArray(const std::string& json, const std::string& field)
{
    std::vector<double> result;
    std::string token = "\"" + field + "\"";
    size_t pos = json.find(token);
    if (pos == std::string::npos) return result;
    size_t open = json.find('[', pos);
    size_t close = json.find(']', open);
    if (open == std::string::npos || close == std::string::npos) return result;

    std::string inner = json.substr(open + 1, close - open - 1);
    std::stringstream ss(inner);
    std::string item;
    while (std::getline(ss, item, ','))
    {
        try { result.push_back(std::stod(item)); } catch (...) { /* jeton invalide ignoré */ }
    }
    return result;
}

static std::vector<std::string> parseStringArray(const std::string& json, const std::string& field)
{
    std::vector<std::string> result;
    std::string token = "\"" + field + "\"";
    size_t pos = json.find(token);
    if (pos == std::string::npos) return result;
    size_t open = json.find('[', pos);
    size_t close = json.find(']', open);
    if (open == std::string::npos || close == std::string::npos) return result;

    std::string inner = json.substr(open + 1, close - open - 1);
    size_t i = 0;
    while (i < inner.size())
    {
        size_t q1 = inner.find('"', i);
        if (q1 == std::string::npos) break;
        size_t q2 = inner.find('"', q1 + 1);
        if (q2 == std::string::npos) break;
        result.push_back(inner.substr(q1 + 1, q2 - q1 - 1));
        i = q2 + 1;
    }
    return result;
}

GridDefinition GridDefinition::fromJson(const std::string& jsonStr)
{
    GridDefinition def;
    // Simple parsing pour désérialisation
    auto findField = [&jsonStr](const std::string& field) -> std::string {
        std::string token = "\"" + field + "\"";
        size_t pos = jsonStr.find(token);
        if (pos == std::string::npos) return "";
        size_t colon = jsonStr.find(':', pos);
        if (colon == std::string::npos) return "";
        size_t start = colon + 1;
        while (start < jsonStr.size() && (jsonStr[start] == ' ' || jsonStr[start] == '\n' || jsonStr[start] == '\r'))
            start++;
        size_t end = jsonStr.find_first_of(",}\n\r", start);
        if (end == std::string::npos) end = jsonStr.size();
        std::string val = jsonStr.substr(start, end - start);
        if (!val.empty() && val.front() == '"') val = val.substr(1);
        if (!val.empty() && val.back() == '"') val.pop_back();
        return val;
    };

    std::string id = findField("id");
    if (!id.empty()) def.setId(id);

    std::string name = findField("name");
    if (!name.empty()) def.setName(name);

    std::string type = findField("type");
    const GridType parsedType = (type == "Cylindrical") ? GridType::Cylindrical : GridType::Cartesian;
    def.setType(parsedType);

    std::vector<double> origin = parseDoubleArray(jsonStr, "origin");
    if (origin.size() == 3)
        def.setOrigin(origin[0], origin[1], origin[2]);

    // Avant : seuls "name" et "type" étaient relus, toutes les positions,
    // rayons, angles et libellés étaient silencieusement perdus au rechargement.
    if (parsedType == GridType::Cartesian)
    {
        def.setXPositions(parseDoubleArray(jsonStr, "xPositions"));
        def.setXLabels(parseStringArray(jsonStr, "xLabels"));

        def.setYPositions(parseDoubleArray(jsonStr, "yPositions"));
        def.setYLabels(parseStringArray(jsonStr, "yLabels"));
    }
    else
    {
        def.setRadii(parseDoubleArray(jsonStr, "radii"));
        def.setRadiusLabels(parseStringArray(jsonStr, "radiusLabels"));

        def.setAngles(parseDoubleArray(jsonStr, "angles"));
        def.setAngleLabels(parseStringArray(jsonStr, "angleLabels"));
    }

    def.setZLevels(parseDoubleArray(jsonStr, "zLevels"));
    def.setZLabels(parseStringArray(jsonStr, "zLabels"));

    return def;
}

} // namespace TSA::Grid
