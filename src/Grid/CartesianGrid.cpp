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
    m_verticalConnectionLines.clear();
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

    // Pré-allocation des vecteurs (tailles connues à l'avance)
    size_t nLevels = levels.size();
    size_t nX = xPos.size();
    size_t nY = yPos.size();
    m_xLines.reserve(nLevels * nX);
    m_yLines.reserve(nLevels * nY);
    m_allLines.reserve(nLevels * (nX + nY));
    m_intersections.reserve(nLevels * nX * nY);
    m_labelAnchors.reserve(nLevels * (2 * nX + 2 * nY));
    m_verticalConnectionLines.reserve(nX * nY);

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

    // 2. Lignes de connexion verticales à chaque intersection (X_i, Y_j) reliant tous les étages
    for (size_t i = 0; i < xPos.size(); ++i)
    {
        double xVal = orig.X() + xPos[i];
        for (size_t j = 0; j < yPos.size(); ++j)
        {
            double yVal = orig.Y() + yPos[j];
            GridLineSegment vSeg;
            vSeg.start = gp_Pnt(xVal, yVal, m_minZ);
            vSeg.end   = gp_Pnt(xVal, yVal, m_maxZ);
            vSeg.label = m_definition.getXLabel(i) + "-" + m_definition.getYLabel(j);
            vSeg.index = static_cast<int>(i * yPos.size() + j);
            vSeg.isXAxis = false;
            vSeg.zLevel = m_minZ;
            m_verticalConnectionLines.push_back(vSeg);
        }
    }

    // 3. Colonne de repérage vertical Z reliant continûment tous les étages
    m_verticalLevelLines.clear();
    m_levelBoundaryPlanes.clear();
    m_levelLabelAnchors.clear();

    double xDatum = m_minX - m_extension - 0.6;
    double yDatum = m_minY - m_extension - 0.6;
    double zBottom = m_minZ - 0.5;
    double zTop = m_maxZ + 1.2;

    // Ligne verticale maîtresse Z
    GridLineSegment vertCol;
    vertCol.start = gp_Pnt(xDatum, yDatum, zBottom);
    vertCol.end = gp_Pnt(xDatum, yDatum, zTop);
    vertCol.label = "Axe Vertical Niveaux Z";
    vertCol.isXAxis = false;
    m_verticalLevelLines.push_back(vertCol);

    // Flèche au sommet de l'axe vertical Z
    gp_Pnt arrowTip(xDatum, yDatum, zTop);
    gp_Pnt arrowLeft(xDatum - 0.15, yDatum, zTop - 0.30);
    gp_Pnt arrowRight(xDatum + 0.15, yDatum, zTop - 0.30);
    m_verticalLevelLines.push_back({ arrowLeft, arrowTip, "", -1, false, zTop });
    m_verticalLevelLines.push_back({ arrowRight, arrowTip, "", -1, false, zTop });

    for (size_t k = 0; k < levels.size(); ++k)
    {
        double zVal = orig.Z() + levels[k];

        // Bras horizontal de niveau reliant la colonne Z à la grille
        gp_Pnt tickStart(xDatum, yDatum, zVal);
        gp_Pnt tickEnd(xDatum + 0.8, yDatum, zVal);
        m_verticalLevelLines.push_back({ tickStart, tickEnd, "", static_cast<int>(k), false, zVal });

        // Symbole triangulaire de niveau génie civil au niveau Z_k
        gp_Pnt triTop(xDatum + 0.3, yDatum, zVal);
        gp_Pnt triLeft(xDatum + 0.1, yDatum, zVal - 0.2);
        gp_Pnt triRight(xDatum + 0.5, yDatum, zVal - 0.2);
        m_verticalLevelLines.push_back({ triLeft, triTop, "", -1, false, zVal });
        m_verticalLevelLines.push_back({ triTop, triRight, "", -1, false, zVal });
        m_verticalLevelLines.push_back({ triRight, triLeft, "", -1, false, zVal });

        // Ancrage d'étiquette de niveau (nom + élévation)
        std::ostringstream ss;
        ss << m_definition.getZLabel(k) << " [" << std::fixed << std::setprecision(2)
           << (zVal >= 0 ? "+" : "") << zVal << " m]";
        m_levelLabelAnchors.push_back({ gp_Pnt(xDatum - 0.3, yDatum, zVal), ss.str(), gp_Dir(0, 0, 1), true });

        // Cadre périmétrique du plancher au niveau Z_k
        gp_Pnt c1(m_minX, m_minY, zVal);
        gp_Pnt c2(m_maxX, m_minY, zVal);
        gp_Pnt c3(m_maxX, m_maxY, zVal);
        gp_Pnt c4(m_minX, m_maxY, zVal);
        m_levelBoundaryPlanes.push_back({ c1, c2, "", static_cast<int>(k), false, zVal });
        m_levelBoundaryPlanes.push_back({ c2, c3, "", static_cast<int>(k), false, zVal });
        m_levelBoundaryPlanes.push_back({ c3, c4, "", static_cast<int>(k), false, zVal });
        m_levelBoundaryPlanes.push_back({ c4, c1, "", static_cast<int>(k), false, zVal });
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
    const GridIntersection* bestInter = nullptr;
    for (const auto& inter : m_intersections)
    {
        double d = worldPoint.Distance(inter.point);
        if (d <= snapToleranceWorld && d < bestResult.distance)
        {
            bestResult.snapped = true;
            bestResult.point = inter.point;
            bestResult.type = GridSnapType::Intersection;
            bestResult.distance = d;
            bestInter = &inter;
        }
    }

    // Si une intersection a été trouvée, construire la description une seule fois et retourner
    if (bestResult.snapped && bestResult.type == GridSnapType::Intersection)
    {
        std::ostringstream oss;
        oss << "Intersection Grille " << bestInter->labelX << "-" << bestInter->labelY
            << " [" << bestInter->labelZ << "] ("
            << std::fixed << std::setprecision(3)
            << bestInter->point.X() << ", " << bestInter->point.Y() << ", " << bestInter->point.Z() << " m)";
        bestResult.description = oss.str();
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

    // 4. Accrochage : Colonne d'axe vertical et bras d'étages
    for (const auto& line : m_verticalLevelLines)
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
            bestResult.type = GridSnapType::LevelPlane;
            bestResult.distance = d;

            std::ostringstream oss;
            oss << "Axe vertical de niveau Z (" << std::fixed << std::setprecision(3)
                << projPoint.Z() << " m)";
            bestResult.description = oss.str();
        }
    }

    return bestResult;
}

} // namespace TSA::Grid
