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
                             int countZ, double spacingZ);

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

    // Cartésien
    std::vector<double> m_xPositions;
    std::vector<double> m_yPositions;
    std::vector<double> m_zLevels;

    // Cylindrique
    std::vector<double> m_radii;
    std::vector<double> m_angles; // en degrés

    // Libellés
    std::vector<std::string> m_xLabels;
    std::vector<std::string> m_yLabels;
    std::vector<std::string> m_zLabels;
    std::vector<std::string> m_radiusLabels;
    std::vector<std::string> m_angleLabels;
};

} // namespace TSA::Grid
