#include "CartesianGrid.h"
#include <cmath>
#include <algorithm>
#include <limits>
#include <sstream>
#include <iomanip>

namespace TSA::Grid
{

CartesianGrid::CartesianGrid(const GridDefinition& definition)
    : m_definition(definition)
{
    computeGeometry();
}

void CartesianGrid::updateDefinition(const GridDefinition& definition)
{
    m_definition = definition;
    computeGeometry();
}

void CartesianGrid::setExtension(double ext)
{
    if (std::abs(m_extension - ext) > 1e-4)
    {
        m_extension = ext;
        computeGeometry();
    }
}

void CartesianGrid::computeGeometry()
{
    m_xLines.clear();
    m_yLines.clear();
    m_allLines.clear();
    m_intersections.clear();
    m_labelAnchors.clear();

    const auto& xPos = m_definition.xPositions();
    const auto& yPos = m_definition.yPositions();
    const auto& zLevels = m_definition.zLevels();
    const gp_Pnt& orig = m_definition.origin();

    if (xPos.empty() || yPos.empty())
    {
        return;
    }

    std::vector<double> levels = zLevels;
    if (levels.empty())
    {
        levels.push_back(0.0);
    }

    m_minX = orig.X() + xPos.front();
    m_maxX = orig.X() + xPos.back();
    m_minY = orig.Y() + yPos.front();
    m_maxY = orig.Y() + yPos.back();
    m_minZ = orig.Z() + levels.front();
    m_maxZ = orig.Z() + levels.back();

    double startY = m_minY - m_extension;
    double endY   = m_maxY + m_extension;
    double startX = m_minX - m_extension;
    double endX   = m_maxX + m_extension;

    // 1. Génération des lignes et intersections pour chaque niveau Z
    for (size_t k = 0; k < levels.size(); ++k)
    {
        double zVal = orig.Z() + levels[k];

        // Lignes d'axes X (parallèles à Y, à chaque X_i)
        for (size_t i = 0; i < xPos.size(); ++i)
        {
            double xVal = orig.X() + xPos[i];
            GridLineSegment seg;
            seg.start = gp_Pnt(xVal, startY, zVal);
            seg.end   = gp_Pnt(xVal, endY,   zVal);
            seg.label = m_definition.getXLabel(i);
            seg.index = static_cast<int>(i);
            seg.isXAxis = true;
            seg.zLevel = zVal;

            m_xLines.push_back(seg);
            m_allLines.push_back(seg);

            // Ancrages des bulles d'axe X
            m_labelAnchors.push_back({ seg.start, seg.label, gp_Dir(0, -1, 0), true });
            m_labelAnchors.push_back({ seg.end,   seg.label, gp_Dir(0,  1, 0), false });
        }

        // Lignes d'axes Y (parallèles à X, à chaque Y_j)
        for (size_t j = 0; j < yPos.size(); ++j)
        {
            double yVal = orig.Y() + yPos[j];
            GridLineSegment seg;
            seg.start = gp_Pnt(startX, yVal, zVal);
            seg.end   = gp_Pnt(endX,   yVal, zVal);
            seg.label = m_definition.getYLabel(j);
            seg.index = static_cast<int>(j);
            seg.isXAxis = false;
            seg.zLevel = zVal;

            m_yLines.push_back(seg);
            m_allLines.push_back(seg);

            // Ancrages des bulles d'axe Y
            m_labelAnchors.push_back({ seg.start, seg.label, gp_Dir(-1, 0, 0), true });
            m_labelAnchors.push_back({ seg.end,   seg.label, gp_Dir( 1, 0, 0), false });
        }

        // Intersections de grille (X_i, Y_j, Z_k)
        for (size_t i = 0; i < xPos.size(); ++i)
        {
            double xVal = orig.X() + xPos[i];
            for (size_t j = 0; j < yPos.size(); ++j)
            {
                double yVal = orig.Y() + yPos[j];
                GridIntersection inter;
                inter.point = gp_Pnt(xVal, yVal, zVal);
                inter.xIndex = static_cast<int>(i);
                inter.yIndex = static_cast<int>(j);
                inter.zIndex = static_cast<int>(k);
                inter.labelX = m_definition.getXLabel(i);
                inter.labelY = m_definition.getYLabel(j);
                inter.labelZ = m_definition.getZLabel(k);

                m_intersections.push_back(inter);
            }
        }
    }
}

GridSnapResult CartesianGrid::findClosestSnap(const gp_Pnt& worldPoint, double snapToleranceWorld) const
{
    GridSnapResult bestResult;
    bestResult.snapped = false;
    bestResult.distance = std::numeric_limits<double>::max();

    const gp_Pnt& orig = m_definition.origin();

    // 1. Accrochage prioritaire : Origine de la grille
    double distOrig = worldPoint.Distance(orig);
    if (distOrig <= snapToleranceWorld && distOrig < bestResult.distance)
    {
        bestResult.snapped = true;
        bestResult.point = orig;
        bestResult.type = GridSnapType::Origin;
        bestResult.distance = distOrig;
        bestResult.description = "Origine Grille (" + std::to_string(orig.X()) + ", " +
                                 std::to_string(orig.Y()) + ", " + std::to_string(orig.Z()) + ")";
    }

    // 2. Accrochage : Intersections de grille (Priorité très haute)
    for (const auto& inter : m_intersections)
    {
        double d = worldPoint.Distance(inter.point);
        if (d <= snapToleranceWorld && d < bestResult.distance)
        {
            bestResult.snapped = true;
            bestResult.point = inter.point;
            bestResult.type = GridSnapType::Intersection;
            bestResult.distance = d;

            std::ostringstream oss;
            oss << "Intersection Grille " << inter.labelX << "-" << inter.labelY
                << " [" << inter.labelZ << "] ("
                << std::fixed << std::setprecision(3)
                << inter.point.X() << ", " << inter.point.Y() << ", " << inter.point.Z() << " m)";
            bestResult.description = oss.str();
        }
    }

    // Si une intersection a été trouvée, on la privilégie immédiatement
    if (bestResult.snapped && bestResult.type == GridSnapType::Intersection)
    {
        return bestResult;
    }

    // 3. Accrochage : Lignes d'axes (projection orthogonale sur le segment de ligne)
    for (const auto& line : m_allLines)
    {
        gp_Vec vLine(line.start, line.end);
        double lenSq = vLine.SquareMagnitude();
        if (lenSq < 1e-6) continue;

        gp_Vec vPt(line.start, worldPoint);
        double t = (vPt.Dot(vLine)) / lenSq;
        t = std::clamp(t, 0.0, 1.0);

        gp_Pnt projPoint = line.start.Translated(vLine * t);
        double d = worldPoint.Distance(projPoint);

        if (d <= snapToleranceWorld && d < bestResult.distance)
        {
            bestResult.snapped = true;
            bestResult.point = projPoint;
            bestResult.type = GridSnapType::AxisLine;
            bestResult.distance = d;

            std::ostringstream oss;
            oss << "Axe Grille " << (line.isXAxis ? "X: " : "Y: ") << line.label
                << " (" << std::fixed << std::setprecision(3)
                << projPoint.X() << ", " << projPoint.Y() << ", " << projPoint.Z() << " m)";
            bestResult.description = oss.str();
        }
    }

    return bestResult;
}

} // namespace TSA::Grid
