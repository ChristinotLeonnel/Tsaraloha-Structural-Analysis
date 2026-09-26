#pragma once

#include "GridType.h"
#include <gp_Pnt.hxx>
#include <string>
#include <vector>

namespace TSA::Grid
{

class GridDefinition
{
public:
    GridDefinition();
    explicit GridDefinition(const std::string& name, GridType type = GridType::Cartesian);
    ~GridDefinition() = default;

    const std::string& id() const { return m_id; }
    void setId(const std::string& id) { m_id = id; }

    const std::string& name() const { return m_name; }
    void setName(const std::string& name) { m_name = name; }

    GridType type() const { return m_type; }
    void setType(GridType type) { m_type = type; }

    const gp_Pnt& origin() const { return m_origin; }
    void setOrigin(const gp_Pnt& origin) { m_origin = origin; }
    void setOrigin(double x, double y, double z) { m_origin = gp_Pnt(x, y, z); }

    double rotationDeg() const { return m_rotationDeg; }
    void setRotationDeg(double deg) { m_rotationDeg = deg; }

    bool isVisible() const { return m_isVisible; }
    void setVisible(bool visible) { m_isVisible = visible; }

    bool isActive() const { return m_isActive; }
    void setActive(bool active) { m_isActive = active; }

    bool showLabels() const { return m_showLabels; }
    void setShowLabels(bool show) { m_showLabels = show; }

    bool showIntersections() const { return m_showIntersections; }
    void setShowIntersections(bool show) { m_showIntersections = show; }

    // --- Coordonnées Cartésiennes (Positions explicites et irrégulières) ---
    const std::vector<double>& xPositions() const { return m_xPositions; }
    void setXPositions(const std::vector<double>& positions);

    const std::vector<double>& yPositions() const { return m_yPositions; }
    void setYPositions(const std::vector<double>& positions);

    const std::vector<double>& zLevels() const { return m_zLevels; }
    void setZLevels(const std::vector<double>& levels);

    // --- Coordonnées Cylindriques (Rayons et Angles) ---
    const std::vector<double>& radii() const { return m_radii; }
    void setRadii(const std::vector<double>& radii);

    // Angles en degrés [0..360[
    const std::vector<double>& angles() const { return m_angles; }
    void setAngles(const std::vector<double>& anglesDeg);

    // Paramètres angulaires sectoriels (Robot Structural Analysis style)
    double startAngleDeg() const { return m_startAngleDeg; }
    void setStartAngleDeg(double deg) { m_startAngleDeg = deg; }
    double startAngle() const { return m_startAngleDeg; }
    void setStartAngle(double deg) { m_startAngleDeg = deg; }

    double totalAngleDeg() const { return m_totalAngleDeg; }
    void setTotalAngleDeg(double deg) { m_totalAngleDeg = (deg <= 0.0) ? 360.0 : (deg > 360.0 ? 360.0 : deg); }
    double totalAngle() const { return m_totalAngleDeg; }
    void setTotalAngle(double deg) { setTotalAngleDeg(deg); }

    int angularDivisions() const { return m_angularDivisions; }
    void setAngularDivisions(int divs) { m_angularDivisions = std::max(1, divs); }

    int radialDivisions() const { return static_cast<int>(m_radii.size()); }
    void setRadialDivisions(int /*divs*/) {}

    // --- Libellés personnalisés ---
    const std::vector<std::string>& xLabels() const { return m_xLabels; }
    void setXLabels(const std::vector<std::string>& labels) { m_xLabels = labels; }

    const std::vector<std::string>& yLabels() const { return m_yLabels; }
    void setYLabels(const std::vector<std::string>& labels) { m_yLabels = labels; }

    const std::vector<std::string>& zLabels() const { return m_zLabels; }
    void setZLabels(const std::vector<std::string>& labels) { m_zLabels = labels; }

    const std::vector<std::string>& radiusLabels() const { return m_radiusLabels; }
    void setRadiusLabels(const std::vector<std::string>& labels) { m_radiusLabels = labels; }

    const std::vector<std::string>& angleLabels() const { return m_angleLabels; }
    void setAngleLabels(const std::vector<std::string>& labels) { m_angleLabels = labels; }

    // Helpers d'obtention de libellé avec fallback automatique
    std::string getXLabel(size_t index) const;
    std::string getYLabel(size_t index) const;
    std::string getZLabel(size_t index) const;
    std::string getRadiusLabel(size_t index) const;
    std::string getAngleLabel(size_t index) const;

    // --- Générateurs automatiques ---
    void generateCartesian(int countX, double spacingX,
                           int countY, double spacingY,
                           int countZ, double spacingZ);

    void generateCylindrical(int radiusCount, double radiusSpacing,
                             int angleCount, double angleSpacingDeg,
                             int countZ, double spacingZ,
                             double startAngleDeg = 0.0,
                             double totalAngleDeg = 360.0);

    // --- Lignes de construction arbitraires ---
    const std::vector<ArbitraryLine>& arbitraryLines() const { return m_arbitraryLines; }
    void setArbitraryLines(const std::vector<ArbitraryLine>& lines) { m_arbitraryLines = lines; }
    void addArbitraryLine(const ArbitraryLine& line) { m_arbitraryLines.push_back(line); }
    void clearArbitraryLines() { m_arbitraryLines.clear(); }

    // --- Paramètres d'affichage graphique ---
    const GridDisplaySettings& displaySettings() const { return m_displaySettings; }
    void setDisplaySettings(const GridDisplaySettings& settings) { m_displaySettings = settings; }

    // --- Style Gras par axe ---
    const std::vector<bool>& xIsBold() const { return m_xIsBold; }
    bool xIsBold(size_t index) const { return index < m_xIsBold.size() ? m_xIsBold[index] : false; }
    void setXIsBold(const std::vector<bool>& bold) { m_xIsBold = bold; }

    const std::vector<bool>& yIsBold() const { return m_yIsBold; }
    bool yIsBold(size_t index) const { return index < m_yIsBold.size() ? m_yIsBold[index] : false; }
    void setYIsBold(const std::vector<bool>& bold) { m_yIsBold = bold; }

    const std::vector<bool>& zIsBold() const { return m_zIsBold; }
    bool zIsBold(size_t index) const { return index < m_zIsBold.size() ? m_zIsBold[index] : false; }
    void setZIsBold(const std::vector<bool>& bold) { m_zIsBold = bold; }

    // --- Sérialisation JSON ---
    std::string toJson() const;
    static GridDefinition fromJson(const std::string& json);

private:
    void ensureLabelsSynchronized();

private:
    std::string m_id;
    std::string m_name;
    GridType m_type = GridType::Cartesian;
    gp_Pnt m_origin = gp_Pnt(0.0, 0.0, 0.0);
    double m_rotationDeg = 0.0;
    bool m_isVisible = true;
    bool m_isActive = false;
    bool m_showLabels = true;
    bool m_showIntersections = true;

    // Cartésien
    std::vector<double> m_xPositions;
    std::vector<double> m_yPositions;
    std::vector<double> m_zLevels;

    // Cylindrique
    std::vector<double> m_radii;
    std::vector<double> m_angles; // en degrés
    double m_startAngleDeg = 0.0;
    double m_totalAngleDeg = 360.0;
    int m_angularDivisions = 12;

    // Libellés
    std::vector<std::string> m_xLabels;
    std::vector<std::string> m_yLabels;
    std::vector<std::string> m_zLabels;
    std::vector<std::string> m_radiusLabels;
    std::vector<std::string> m_angleLabels;

    // Lignes arbitraires
    std::vector<ArbitraryLine> m_arbitraryLines;

    // Paramètres graphiques
    GridDisplaySettings m_displaySettings;

    // Styles gras
    std::vector<bool> m_xIsBold;
    std::vector<bool> m_yIsBold;
    std::vector<bool> m_zIsBold;
};

} // namespace TSA::Grid
