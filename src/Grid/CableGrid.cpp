#include "CableGrid.h"
#include <cmath>
#include <algorithm>

namespace TSA::Grid
{

CableGrid::CableGrid()
    : m_name("CableGrid_1")
    , m_type(CableGridType::BridgeSpan)
{
}

CableGrid::CableGrid(const std::string& name, CableGridType type)
    : m_name(name)
    , m_type(type)
{
}

GridDefinition CableGrid::toGridDefinition() const
{
    GridDefinition def(m_name, (m_type == CableGridType::CylindricalNet) ? GridType::Cylindrical : GridType::Cartesian);
    def.setOrigin(m_origin);
    def.setRotationDeg(m_rotationDeg);

    if (m_type == CableGridType::CylindricalNet)
    {
        std::vector<double> radii;
        for (int i = 1; i <= m_stayCount; ++i)
        {
            radii.push_back(i * m_hangerSpacing);
        }
        def.setRadii(radii);

        std::vector<double> angles;
        for (int i = 0; i < m_stayCount; ++i)
        {
            double a = std::fmod(m_startAngleDeg + i * m_angleStepDeg, 360.0);
            angles.push_back(a);
        }
        std::sort(angles.begin(), angles.end());
        def.setAngles(angles);

        def.setZLevels({ m_deckElevation, m_deckElevation + m_pylonHeight });
        return def;
    }

    // Cas BridgeSpan / StayFan : Grille cartésienne structurée
    std::vector<double> xPos;
    std::vector<std::string> xLabels;

    // Massif d'ancrage gauche
    xPos.push_back(0.0);
    xLabels.push_back("Ancrage_G");

    // Pylône gauche
    if (m_pylonStationLeft > 0.0 && m_pylonStationLeft < m_totalSpan)
    {
        xPos.push_back(m_pylonStationLeft);
        xLabels.push_back("Pylone_1");
    }

    // Stations de suspentes le long de la travée centrale
    double curX = m_pylonStationLeft + m_hangerSpacing;
    int hIdx = 1;
    while (curX < m_pylonStationRight - 1e-3)
    {
        xPos.push_back(curX);
        xLabels.push_back("S" + std::to_string(hIdx++));
        curX += m_hangerSpacing;
    }

    // Pylône droit
    if (m_pylonStationRight > m_pylonStationLeft && m_pylonStationRight < m_totalSpan)
    {
        xPos.push_back(m_pylonStationRight);
        xLabels.push_back("Pylone_2");
    }

    // Massif d'ancrage droit
    xPos.push_back(m_totalSpan);
    xLabels.push_back("Ancrage_D");

    def.setXPositions(xPos);
    def.setXLabels(xLabels);

    // Positions Y : Nappe gauche, Axe longitudinal, Nappe droite
    def.setYPositions({ -m_halfDeckWidth, 0.0, m_halfDeckWidth });
    def.setYLabels({ "Nappe_G", "Axe", "Nappe_D" });

    // Niveaux Z : Fondations (0), Tablier, Sommet des pylônes
    def.setZLevels({ 0.0, m_deckElevation, m_pylonHeight });
    def.setZLabels({ "Fondations", "Tablier", "Sommet_Pylones" });

    return def;
}

std::vector<gp_Pnt> CableGrid::generateKeyPoints() const
{
    std::vector<gp_Pnt> pts;

    // Têtes de pylônes (gauche et droite)
    pts.push_back(gp_Pnt(m_pylonStationLeft, -m_halfDeckWidth, m_pylonHeight));
    pts.push_back(gp_Pnt(m_pylonStationLeft, m_halfDeckWidth, m_pylonHeight));
    pts.push_back(gp_Pnt(m_pylonStationRight, -m_halfDeckWidth, m_pylonHeight));
    pts.push_back(gp_Pnt(m_pylonStationRight, m_halfDeckWidth, m_pylonHeight));

    // Massifs d'ancrage
    pts.push_back(gp_Pnt(0.0, -m_halfDeckWidth, 0.0));
    pts.push_back(gp_Pnt(0.0, m_halfDeckWidth, 0.0));
    pts.push_back(gp_Pnt(m_totalSpan, -m_halfDeckWidth, 0.0));
    pts.push_back(gp_Pnt(m_totalSpan, m_halfDeckWidth, 0.0));

    // Stations de tablier
    double x = m_pylonStationLeft;
    while (x <= m_pylonStationRight)
    {
        pts.push_back(gp_Pnt(x, -m_halfDeckWidth, m_deckElevation));
        pts.push_back(gp_Pnt(x, m_halfDeckWidth, m_deckElevation));
        x += m_hangerSpacing;
    }

    return pts;
}

} // namespace TSA::Grid
