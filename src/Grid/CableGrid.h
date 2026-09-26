#pragma once

#include "GridDefinition.h"
#include <gp_Pnt.hxx>
#include <gp_Dir.hxx>
#include <vector>
#include <string>

namespace TSA::Grid
{

/**
 * @brief Type d'alignement ou de disposition pour grilles spécialisées câbles / ponts.
 */
enum class CableGridType
{
    BridgeSpan,     ///< Grille le long de l'axe longitudinal du pont (tablier, suspentes, pylônes)
    StayFan,        ///< Réseau de rayons de haubans (éventail / semi-éventail)
    CylindricalNet, ///< Réseau polaire / cylindrique pour réservoirs, dômes ou toitures suspendues
    CustomAlignment ///< Grille personnalisée le long d'une ligne d'ancrage
};

/**
 * @brief Paramètres de génération d'une grille spécialisée pour l'ingénierie des câbles (Cable Grid).
 */
class CableGrid
{
public:
    CableGrid();
    explicit CableGrid(const std::string& name, CableGridType type = CableGridType::BridgeSpan);

    const std::string& name() const { return m_name; }
    void setName(const std::string& name) { m_name = name; }

    CableGridType type() const { return m_type; }
    void setType(CableGridType type) { m_type = type; }

    const gp_Pnt& origin() const { return m_origin; }
    void setOrigin(const gp_Pnt& origin) { m_origin = origin; }
    void setOrigin(double x, double y, double z) { m_origin = gp_Pnt(x, y, z); }

    double rotationDeg() const { return m_rotationDeg; }
    void setRotationDeg(double deg) { m_rotationDeg = deg; }

    // Paramètres d'axe longitudinal de pont
    double totalSpan() const { return m_totalSpan; }
    void setTotalSpan(double span) { m_totalSpan = std::max(1.0, span); }

    double hangerSpacing() const { return m_hangerSpacing; }
    void setHangerSpacing(double spacing) { m_hangerSpacing = std::max(0.5, spacing); }

    double pylonStationLeft() const { return m_pylonStationLeft; }
    void setPylonStationLeft(double s) { m_pylonStationLeft = s; }

    double pylonStationRight() const { return m_pylonStationRight; }
    void setPylonStationRight(double s) { m_pylonStationRight = s; }

    double pylonHeight() const { return m_pylonHeight; }
    void setPylonHeight(double h) { m_pylonHeight = std::max(1.0, h); }

    double deckElevation() const { return m_deckElevation; }
    void setDeckElevation(double z) { m_deckElevation = z; }

    double halfDeckWidth() const { return m_halfDeckWidth; }
    void setHalfDeckWidth(double w) { m_halfDeckWidth = std::max(0.5, w); }

    // Paramètres polaires / éventail
    double startAngleDeg() const { return m_startAngleDeg; }
    void setStartAngleDeg(double a) { m_startAngleDeg = a; }

    double angleStepDeg() const { return m_angleStepDeg; }
    void setAngleStepDeg(double step) { m_angleStepDeg = step; }

    int stayCount() const { return m_stayCount; }
    void setStayCount(int count) { m_stayCount = std::max(1, count); }

    /**
     * @brief Convertit les paramètres en une GridDefinition standard native TSA
     * pour l'intégrer directement au GridManager et GridSnapManager.
     */
    GridDefinition toGridDefinition() const;

    /**
     * @brief Retourne les points clés d'accrochage générés (ancrages, têtes de pylône, stations de suspentes).
     */
    std::vector<gp_Pnt> generateKeyPoints() const;

private:
    std::string m_name = "CableGrid_1";
    CableGridType m_type = CableGridType::BridgeSpan;
    gp_Pnt m_origin{0.0, 0.0, 0.0};
    double m_rotationDeg = 0.0;

    double m_totalSpan = 200.0;        // 200 m
    double m_hangerSpacing = 10.0;     // 10 m
    double m_pylonStationLeft = 50.0;  // Pylône gauche à x = 50 m
    double m_pylonStationRight = 150.0;// Pylône droit à x = 150 m
    double m_pylonHeight = 45.0;       // Sommet pylône à z = 45 m
    double m_deckElevation = 10.0;     // Tablier à z = 10 m
    double m_halfDeckWidth = 6.0;      // Nappes de câbles à y = +/- 6 m

    double m_startAngleDeg = 15.0;
    double m_angleStepDeg = 5.0;
    int m_stayCount = 8;
};

} // namespace TSA::Grid
