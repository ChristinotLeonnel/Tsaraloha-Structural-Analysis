#include "GridDefinition.h"

#include <QByteArray>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>

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
    m_xPositions.erase(std::unique(m_xPositions.begin(), m_xPositions.end(),
        [](double a, double b) { return std::abs(a - b) < 1e-6; }), m_xPositions.end());
    ensureLabelsSynchronized();
}

void GridDefinition::setYPositions(const std::vector<double>& positions)
{
    m_yPositions = positions;
    std::sort(m_yPositions.begin(), m_yPositions.end());
    m_yPositions.erase(std::unique(m_yPositions.begin(), m_yPositions.end(),
        [](double a, double b) { return std::abs(a - b) < 1e-6; }), m_yPositions.end());
    ensureLabelsSynchronized();
}

void GridDefinition::setZLevels(const std::vector<double>& levels)
{
    m_zLevels = levels;
    std::sort(m_zLevels.begin(), m_zLevels.end());
    m_zLevels.erase(std::unique(m_zLevels.begin(), m_zLevels.end(),
        [](double a, double b) { return std::abs(a - b) < 1e-6; }), m_zLevels.end());
    ensureLabelsSynchronized();
}

void GridDefinition::setRadii(const std::vector<double>& radii)
{
    m_radii = radii;
    std::sort(m_radii.begin(), m_radii.end());
    m_radii.erase(std::unique(m_radii.begin(), m_radii.end(),
        [](double a, double b) { return std::abs(a - b) < 1e-6; }), m_radii.end());
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
    // Correction : comme les autres setters (positions, niveaux, rayons), on
    // déduplique avec une tolérance plutôt qu'une égalité stricte. Sans cela,
    // deux angles très proches après la normalisation [0, 360[ ci-dessus
    // (erreurs d'arrondi flottant) pouvaient rester considérés distincts.
    m_angles.erase(std::unique(m_angles.begin(), m_angles.end(),
        [](double a, double b) { return std::abs(a - b) < 1e-6; }), m_angles.end());
    updateAngularSectorFromAngles();
    ensureLabelsSynchronized();
}

void GridDefinition::setAngularPatterns(const std::vector<AngularPattern>& patterns)
{
    m_angularPatterns = patterns;
    generateAnglesFromPatterns();
}

void GridDefinition::addAngularPattern(const AngularPattern& pattern)
{
    m_angularPatterns.push_back(pattern);
    generateAnglesFromPatterns();
}

void GridDefinition::clearAngularPatterns()
{
    m_angularPatterns.clear();
}

void GridDefinition::generateAnglesFromPatterns()
{
    if (m_angularPatterns.empty())
        return;

    std::vector<double> newAngles;
    for (const auto& pat : m_angularPatterns)
    {
        for (int i = 0; i <= pat.repeatCount; ++i)
        {
            double ang = pat.startAngle + i * pat.angleStep;
            while (ang >= 360.0) ang -= 360.0;
            while (ang < 0.0) ang += 360.0;
            newAngles.push_back(ang);
        }
    }
    setAngles(newAngles);
}

void GridDefinition::updateAngularSectorFromAngles()
{
    if (m_angles.empty()) return;
    if (m_angles.size() == 1)
    {
        m_startAngleDeg = m_angles.front();
        m_totalAngleDeg = 0.0;
        return;
    }

    double minAng = m_angles.front();
    double maxAng = m_angles.back();
    double span = maxAng - minAng;

    bool covers360 = (span >= 360.0 - 1e-4);
    for (const auto& pat : m_angularPatterns)
    {
        if (pat.repeatCount * pat.angleStep >= 360.0 - 1e-4)
        {
            covers360 = true;
            break;
        }
    }

    if (covers360)
    {
        m_startAngleDeg = minAng;
        m_totalAngleDeg = 360.0;
    }
    else
    {
        m_startAngleDeg = minAng;
        m_totalAngleDeg = span;
    }
    m_angularDivisions = static_cast<int>(m_angles.size() - 1);
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
                                         int countZ, double spacingZ,
                                         double startAngleDeg,
                                         double totalAngleDeg)
{
    m_type = GridType::Cylindrical;
    m_radii.clear();
    m_angles.clear();
    m_zLevels.clear();

    m_startAngleDeg = startAngleDeg;
    m_totalAngleDeg = (totalAngleDeg <= 0.0) ? 360.0 : (totalAngleDeg > 360.0 ? 360.0 : totalAngleDeg);
    m_angularDivisions = (angleCount > 0) ? angleCount : 1;

    for (int i = 1; i <= radiusCount; ++i)
    {
        m_radii.push_back(i * radiusSpacing);
    }

    const bool isFull = (m_totalAngleDeg >= 360.0 - 1e-4);
    int numSteps = isFull ? angleCount : (angleCount + 1);
    for (int j = 0; j < numSteps; ++j)
    {
        double ang = m_startAngleDeg + j * angleSpacingDeg;
        m_angles.push_back(ang);
    }

    for (int k = 0; k <= countZ; ++k)
    {
        m_zLevels.push_back(k * spacingZ);
    }

    m_radiusLabels.clear();
    m_angleLabels.clear();
    m_zLabels.clear();

    m_angularPatterns.clear();
    AngularPattern pat;
    pat.startAngle = startAngleDeg;
    pat.repeatCount = angleCount;
    pat.angleStep = angleSpacingDeg;
    m_angularPatterns.push_back(pat);

    ensureLabelsSynchronized();
}

namespace
{
QJsonArray toJsonArray(const std::vector<double>& values)
{
    QJsonArray a;
    for (double v : values) a.append(v);
    return a;
}

QJsonArray toJsonArray(const std::vector<std::string>& values)
{
    QJsonArray a;
    for (const auto& v : values) a.append(QString::fromStdString(v));
    return a;
}

QJsonArray toJsonArray(const std::vector<bool>& values)
{
    QJsonArray a;
    for (bool v : values) a.append(v);
    return a;
}

QJsonArray toJsonArray(const gp_Pnt& p)
{
    return QJsonArray { p.X(), p.Y(), p.Z() };
}

std::vector<double> doublesFrom(const QJsonValue& v)
{
    std::vector<double> out;
    for (const auto& e : v.toArray()) out.push_back(e.toDouble());
    return out;
}

std::vector<std::string> stringsFrom(const QJsonValue& v)
{
    std::vector<std::string> out;
    for (const auto& e : v.toArray()) out.push_back(e.toString().toStdString());
    return out;
}

std::vector<bool> boolsFrom(const QJsonValue& v)
{
    std::vector<bool> out;
    for (const auto& e : v.toArray()) out.push_back(e.toBool());
    return out;
}

gp_Pnt pointFrom(const QJsonValue& v, const gp_Pnt& fallback)
{
    const QJsonArray a = v.toArray();
    return a.size() >= 3 ? gp_Pnt(a[0].toDouble(), a[1].toDouble(), a[2].toDouble()) : fallback;
}

const char* typeKey(GridType type)
{
    if (type == GridType::Cylindrical) return "Cylindrical";
    if (type == GridType::Arbitrary) return "Arbitrary";
    return "Cartesian";
}
} // namespace

// Sérialisation par QJsonDocument. L'ancienne écriture à la main n'échappait pas les chaînes (un nom
// « Bâtiment A, aile Est » était relu « Bâtiment A », un guillemet rendait le JSON invalide) et
// écrivait les nombres avec 6 chiffres significatifs (origine 500 123,456 m relue 500 123 m).
// QJsonDocument échappe les chaînes et écrit les réels au plus court qui se relit à l'identique.
QJsonObject GridDefinition::toJsonObject() const
{
    QJsonObject o;
    o["id"] = QString::fromStdString(m_id);
    o["name"] = QString::fromStdString(m_name);
    o["type"] = QString::fromLatin1(typeKey(m_type));
    o["origin"] = toJsonArray(m_origin);
    o["rotationDeg"] = m_rotationDeg;
    o["isVisible"] = m_isVisible;
    o["isActive"] = m_isActive;
    o["showLabels"] = m_showLabels;
    o["showIntersections"] = m_showIntersections;

    if (m_type == GridType::Cartesian)
    {
        o["xPositions"] = toJsonArray(m_xPositions);
        o["yPositions"] = toJsonArray(m_yPositions);
        o["xLabels"] = toJsonArray(m_xLabels);
        o["yLabels"] = toJsonArray(m_yLabels);
        o["xIsBold"] = toJsonArray(m_xIsBold);
        o["yIsBold"] = toJsonArray(m_yIsBold);
    }
    else if (m_type == GridType::Cylindrical)
    {
        o["radii"] = toJsonArray(m_radii);
        o["angles"] = toJsonArray(m_angles);
        o["startAngleDeg"] = m_startAngleDeg;
        o["totalAngleDeg"] = m_totalAngleDeg;
        o["angularDivisions"] = m_angularDivisions;
        o["radiusLabels"] = toJsonArray(m_radiusLabels);
        o["angleLabels"] = toJsonArray(m_angleLabels);
        QJsonArray patterns;
        for (const auto& pat : m_angularPatterns)
            patterns.append(QJsonObject { { "startAngle", pat.startAngle },
                                          { "repeatCount", pat.repeatCount },
                                          { "angleStep", pat.angleStep } });
        o["angularPatterns"] = patterns;
    }
    else
    {
        QJsonArray lines;
        for (const auto& line : m_arbitraryLines)
            lines.append(QJsonObject { { "label", QString::fromStdString(line.label) },
                                       { "type", QString::fromStdString(line.type) },
                                       { "isBold", line.isBold },
                                       { "p1", toJsonArray(line.p1) },
                                       { "p2", toJsonArray(line.p2) } });
        o["arbitraryLines"] = lines;
    }

    o["zLevels"] = toJsonArray(m_zLevels);
    o["zLabels"] = toJsonArray(m_zLabels);
    o["zIsBold"] = toJsonArray(m_zIsBold);
    o["displaySettings"] = QJsonObject {
        { "lineColor", QString::fromStdString(m_displaySettings.lineColor) },
        { "lineStyle", QString::fromStdString(m_displaySettings.lineStyle) },
        { "lineWidth", m_displaySettings.lineWidth },
        { "extension", m_displaySettings.extension },
        { "bubbleRadius", m_displaySettings.bubbleRadius },
        { "showBubbles", m_displaySettings.showBubbles },
        { "labelsBothEnds", m_displaySettings.labelsBothEnds } };
    return o;
}

std::string GridDefinition::toJson() const
{
    // Indenté (lisible) ; GridManager écrit le format compact dans le .tsa et l'historique.
    return QJsonDocument(toJsonObject()).toJson(QJsonDocument::Indented).toStdString();
}

GridDefinition GridDefinition::fromJsonObject(const QJsonObject& o)
{
    // Même ordre d'appel des mutateurs que l'ancienne lecture : les positions synchronisent des
    // libellés par défaut que les libellés enregistrés remplacent ensuite.
    GridDefinition def;
    const QString id = o.value("id").toString();
    if (!id.isEmpty()) def.setId(id.toStdString());
    const QString name = o.value("name").toString();
    if (!name.isEmpty()) def.setName(name.toStdString());

    const QString type = o.value("type").toString();
    const GridType parsedType = type == "Cylindrical" ? GridType::Cylindrical
                              : type == "Arbitrary"   ? GridType::Arbitrary
                                                      : GridType::Cartesian;
    def.setType(parsedType);

    const QJsonArray origin = o.value("origin").toArray();
    if (origin.size() == 3)
        def.setOrigin(origin[0].toDouble(), origin[1].toDouble(), origin[2].toDouble());
    if (o.contains("rotationDeg")) def.setRotationDeg(o.value("rotationDeg").toDouble());
    if (o.contains("isVisible")) def.setVisible(o.value("isVisible").toBool());
    if (o.contains("isActive")) def.setActive(o.value("isActive").toBool());
    if (o.contains("showLabels")) def.setShowLabels(o.value("showLabels").toBool());
    if (o.contains("showIntersections")) def.setShowIntersections(o.value("showIntersections").toBool());

    if (parsedType == GridType::Cartesian)
    {
        def.setXPositions(doublesFrom(o.value("xPositions")));
        def.setXLabels(stringsFrom(o.value("xLabels")));
        def.setXIsBold(boolsFrom(o.value("xIsBold")));
        def.setYPositions(doublesFrom(o.value("yPositions")));
        def.setYLabels(stringsFrom(o.value("yLabels")));
        def.setYIsBold(boolsFrom(o.value("yIsBold")));
    }
    else if (parsedType == GridType::Cylindrical)
    {
        def.setRadii(doublesFrom(o.value("radii")));
        def.setRadiusLabels(stringsFrom(o.value("radiusLabels")));
        def.setAngles(doublesFrom(o.value("angles")));
        def.setAngleLabels(stringsFrom(o.value("angleLabels")));
        if (o.contains("startAngleDeg")) def.setStartAngleDeg(o.value("startAngleDeg").toDouble());
        if (o.contains("totalAngleDeg")) def.setTotalAngleDeg(o.value("totalAngleDeg").toDouble());
        if (o.contains("angularDivisions")) def.setAngularDivisions(o.value("angularDivisions").toInt());
        std::vector<AngularPattern> pats;
        for (const auto& v : o.value("angularPatterns").toArray())
        {
            const QJsonObject po = v.toObject();
            AngularPattern pat;
            pat.startAngle = po.value("startAngle").toDouble();
            pat.repeatCount = po.value("repeatCount").toInt();
            pat.angleStep = po.value("angleStep").toDouble();
            pats.push_back(pat);
        }
        if (!pats.empty()) def.setAngularPatterns(pats);
    }
    else
    {
        for (const auto& v : o.value("arbitraryLines").toArray())
        {
            const QJsonObject lo = v.toObject();
            ArbitraryLine line;
            line.label = lo.value("label").toString().toStdString();
            line.type = lo.value("type").toString().toStdString();
            if (line.type.empty()) line.type = "droite";
            line.isBold = lo.value("isBold").toBool();
            line.p1 = pointFrom(lo.value("p1"), line.p1);
            line.p2 = pointFrom(lo.value("p2"), line.p2);
            def.addArbitraryLine(line);
        }
    }

    def.setZLevels(doublesFrom(o.value("zLevels")));
    def.setZLabels(stringsFrom(o.value("zLabels")));
    def.setZIsBold(boolsFrom(o.value("zIsBold")));

    if (o.contains("displaySettings"))
    {
        const QJsonObject d = o.value("displaySettings").toObject();
        GridDisplaySettings ds;
        if (const QString c = d.value("lineColor").toString(); !c.isEmpty()) ds.lineColor = c.toStdString();
        if (const QString st = d.value("lineStyle").toString(); !st.isEmpty()) ds.lineStyle = st.toStdString();
        ds.lineWidth = d.value("lineWidth").toDouble(ds.lineWidth);
        ds.extension = d.value("extension").toDouble(ds.extension);
        ds.bubbleRadius = d.value("bubbleRadius").toDouble(ds.bubbleRadius);
        ds.showBubbles = d.value("showBubbles").toBool(ds.showBubbles);
        ds.labelsBothEnds = d.value("labelsBothEnds").toBool(ds.labelsBothEnds);
        def.setDisplaySettings(ds);
    }
    return def;
}

GridDefinition GridDefinition::fromJson(const std::string& json)
{
    QJsonParseError error {};
    const QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(json), &error);
    if (error.error == QJsonParseError::NoError && doc.isObject())
        return fromJsonObject(doc.object());
    // Fichier écrit par l'ancien sérialiseur avec un guillemet non échappé : JSON invalide, relu
    // comme auparavant pour ne pas perdre la grille.
    return fromLegacyJson(json);
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

static std::vector<bool> parseBoolArray(const std::string& json, const std::string& field)
{
    std::vector<bool> result;
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
        while (!item.empty() && (item.front() == ' ' || item.front() == '\t')) item.erase(0, 1);
        while (!item.empty() && (item.back() == ' ' || item.back() == '\t')) item.pop_back();
        result.push_back(item == "true");
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

GridDefinition GridDefinition::fromLegacyJson(const std::string& jsonStr)
{
    GridDefinition def;
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
    GridType parsedType = GridType::Cartesian;
    if (type == "Cylindrical") parsedType = GridType::Cylindrical;
    else if (type == "Arbitrary") parsedType = GridType::Arbitrary;
    def.setType(parsedType);

    std::vector<double> origin = parseDoubleArray(jsonStr, "origin");
    if (origin.size() == 3)
        def.setOrigin(origin[0], origin[1], origin[2]);

    std::string rotStr = findField("rotationDeg");
    if (!rotStr.empty())
    {
        try { def.setRotationDeg(std::stod(rotStr)); } catch (...) {}
    }

    std::string visStr = findField("isVisible");
    if (!visStr.empty()) def.setVisible(visStr == "true");

    std::string actStr = findField("isActive");
    if (!actStr.empty()) def.setActive(actStr == "true");

    std::string lblStr = findField("showLabels");
    if (!lblStr.empty()) def.setShowLabels(lblStr == "true");

    std::string intStr = findField("showIntersections");
    if (!intStr.empty()) def.setShowIntersections(intStr == "true");

    if (parsedType == GridType::Cartesian)
    {
        def.setXPositions(parseDoubleArray(jsonStr, "xPositions"));
        def.setXLabels(parseStringArray(jsonStr, "xLabels"));
        def.setXIsBold(parseBoolArray(jsonStr, "xIsBold"));

        def.setYPositions(parseDoubleArray(jsonStr, "yPositions"));
        def.setYLabels(parseStringArray(jsonStr, "yLabels"));
        def.setYIsBold(parseBoolArray(jsonStr, "yIsBold"));
    }
    else if (parsedType == GridType::Cylindrical)
    {
        def.setRadii(parseDoubleArray(jsonStr, "radii"));
        def.setRadiusLabels(parseStringArray(jsonStr, "radiusLabels"));

        def.setAngles(parseDoubleArray(jsonStr, "angles"));
        def.setAngleLabels(parseStringArray(jsonStr, "angleLabels"));

        std::string saStr = findField("startAngleDeg");
        if (!saStr.empty()) { try { def.setStartAngleDeg(std::stod(saStr)); } catch (...) {} }

        std::string taStr = findField("totalAngleDeg");
        if (!taStr.empty()) { try { def.setTotalAngleDeg(std::stod(taStr)); } catch (...) {} }

        std::string divStr = findField("angularDivisions");
        if (!divStr.empty()) { try { def.setAngularDivisions(std::stoi(divStr)); } catch (...) {} }

        // Lecture de angularPatterns si présent
        size_t apPos = jsonStr.find("\"angularPatterns\"");
        if (apPos != std::string::npos)
        {
            size_t openBracket = jsonStr.find('[', apPos);
            if (openBracket != std::string::npos)
            {
                int bDepth = 0;
                size_t closeBracket = std::string::npos;
                for (size_t k = openBracket; k < jsonStr.size(); ++k)
                {
                    if (jsonStr[k] == '[') ++bDepth;
                    else if (jsonStr[k] == ']')
                    {
                        --bDepth;
                        if (bDepth == 0)
                        {
                            closeBracket = k;
                            break;
                        }
                    }
                }
                if (closeBracket != std::string::npos)
                {
                    std::string arrContent = jsonStr.substr(openBracket + 1, closeBracket - openBracket - 1);
                    std::vector<AngularPattern> pats;
                    size_t objStart = 0;
                    while ((objStart = arrContent.find('{', objStart)) != std::string::npos)
                    {
                        size_t objEnd = arrContent.find('}', objStart);
                        if (objEnd == std::string::npos) break;
                        std::string objStr = arrContent.substr(objStart, objEnd - objStart + 1);
                        objStart = objEnd + 1;

                        AngularPattern p;
                        auto extractVal = [&](const std::string& field) -> double {
                            size_t fpos = objStr.find("\"" + field + "\"");
                            if (fpos == std::string::npos) return 0.0;
                            size_t colon = objStr.find(':', fpos);
                            if (colon == std::string::npos) return 0.0;
                            size_t comma = objStr.find_first_of(",}", colon);
                            std::string val = objStr.substr(colon + 1, (comma != std::string::npos ? comma - colon - 1 : std::string::npos));
                            try { return std::stod(val); } catch (...) { return 0.0; }
                        };
                        p.startAngle = extractVal("startAngle");
                        p.repeatCount = static_cast<int>(extractVal("repeatCount"));
                        p.angleStep = extractVal("angleStep");
                        pats.push_back(p);
                    }
                    if (!pats.empty())
                    {
                        def.setAngularPatterns(pats);
                    }
                }
            }
        }
    }
    else // Arbitrary
    {
        // Découpage manuel des objets arbitraires dans "arbitraryLines"
        size_t arrPos = jsonStr.find("\"arbitraryLines\"");
        if (arrPos != std::string::npos)
        {
            size_t openBracket = jsonStr.find('[', arrPos);
            if (openBracket != std::string::npos)
            {
                int bDepth = 0;
                size_t closeBracket = std::string::npos;
                for (size_t k = openBracket; k < jsonStr.size(); ++k)
                {
                    if (jsonStr[k] == '[') ++bDepth;
                    else if (jsonStr[k] == ']')
                    {
                        --bDepth;
                        if (bDepth == 0)
                        {
                            closeBracket = k;
                            break;
                        }
                    }
                }

                if (closeBracket != std::string::npos)
                {
                    std::string arrContent = jsonStr.substr(openBracket + 1, closeBracket - openBracket - 1);
                    size_t objStart = 0;
                    while ((objStart = arrContent.find('{', objStart)) != std::string::npos)
                    {
                        size_t objEnd = arrContent.find('}', objStart);
                        if (objEnd == std::string::npos) break;
                        std::string objStr = arrContent.substr(objStart, objEnd - objStart + 1);
                        objStart = objEnd + 1;

                    ArbitraryLine line;
                    auto findObjField = [&objStr](const std::string& f) -> std::string {
                        std::string tok = "\"" + f + "\"";
                        size_t p = objStr.find(tok);
                        if (p == std::string::npos) return "";
                        size_t c = objStr.find(':', p);
                        if (c == std::string::npos) return "";
                        size_t s = c + 1;
                        while (s < objStr.size() && (objStr[s] == ' ' || objStr[s] == '\"')) s++;
                        size_t e = objStr.find_first_of("\",}", s);
                        if (e == std::string::npos) e = objStr.size();
                        return objStr.substr(s, e - s);
                    };

                    line.label = findObjField("label");
                    line.type = findObjField("type");
                    if (line.type.empty()) line.type = "droite";
                    line.isBold = (findObjField("isBold") == "true");

                    std::vector<double> p1Vec = parseDoubleArray(objStr, "p1");
                    if (p1Vec.size() >= 3) line.p1 = gp_Pnt(p1Vec[0], p1Vec[1], p1Vec[2]);
                    std::vector<double> p2Vec = parseDoubleArray(objStr, "p2");
                    if (p2Vec.size() >= 3) line.p2 = gp_Pnt(p2Vec[0], p2Vec[1], p2Vec[2]);

                    def.addArbitraryLine(line);
                    objStart = objEnd + 1;
                }
            }
        }
    }
}

    def.setZLevels(parseDoubleArray(jsonStr, "zLevels"));
    def.setZLabels(parseStringArray(jsonStr, "zLabels"));
    def.setZIsBold(parseBoolArray(jsonStr, "zIsBold"));

    // Parse displaySettings
    size_t dsPos = jsonStr.find("\"displaySettings\"");
    if (dsPos != std::string::npos)
    {
        size_t dsOpen = jsonStr.find('{', dsPos);
        size_t dsClose = jsonStr.find('}', dsOpen);
        if (dsOpen != std::string::npos && dsClose != std::string::npos)
        {
            std::string dsStr = jsonStr.substr(dsOpen + 1, dsClose - dsOpen - 1);
            auto findDsField = [&dsStr](const std::string& f) -> std::string {
                std::string tok = "\"" + f + "\"";
                size_t p = dsStr.find(tok);
                if (p == std::string::npos) return "";
                size_t c = dsStr.find(':', p);
                if (c == std::string::npos) return "";
                size_t s = c + 1;
                while (s < dsStr.size() && (dsStr[s] == ' ' || dsStr[s] == '\"')) s++;
                size_t e = dsStr.find_first_of("\",}", s);
                if (e == std::string::npos) e = dsStr.size();
                return dsStr.substr(s, e - s);
            };

            GridDisplaySettings ds;
            std::string col = findDsField("lineColor");
            if (!col.empty()) ds.lineColor = col;
            std::string sty = findDsField("lineStyle");
            if (!sty.empty()) ds.lineStyle = sty;
            std::string lw = findDsField("lineWidth");
            if (!lw.empty()) { try { ds.lineWidth = std::stod(lw); } catch (...) {} }
            std::string ext = findDsField("extension");
            if (!ext.empty()) { try { ds.extension = std::stod(ext); } catch (...) {} }
            std::string br = findDsField("bubbleRadius");
            if (!br.empty()) { try { ds.bubbleRadius = std::stod(br); } catch (...) {} }
            std::string sb = findDsField("showBubbles");
            if (!sb.empty()) ds.showBubbles = (sb == "true");
            std::string be = findDsField("labelsBothEnds");
            if (!be.empty()) ds.labelsBothEnds = (be == "true");

            def.setDisplaySettings(ds);
        }
    }

    return def;
}

} // namespace TSA::Grid
