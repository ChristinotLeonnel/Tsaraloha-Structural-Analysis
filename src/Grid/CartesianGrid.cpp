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

    auto minMaxX = std::minmax_element(xPos.begin(), xPos.end());
    auto minMaxY = std::minmax_element(yPos.begin(), yPos.end());
    auto minMaxZ = std::minmax_element(levels.begin(), levels.end());

    double rotRad = m_definition.rotationDeg() * M_PI / 180.0;
    double cosR = std::cos(rotRad);
    double sinR = std::sin(rotRad);

    auto transformPnt = [&](double dx, double dy, double dz) -> gp_Pnt {
        double rx = dx * cosR - dy * sinR;
        double ry = dx * sinR + dy * cosR;
        return gp_Pnt(orig.X() + rx, orig.Y() + ry, orig.Z() + dz);
    };

    auto transformDir = [&](double dx, double dy, double dz) -> gp_Dir {
        double rx = dx * cosR - dy * sinR;
        double ry = dx * sinR + dy * cosR;
        double len = std::hypot(rx, ry);
        if (len < 1e-8) return gp_Dir(0, 0, dz >= 0 ? 1 : -1);
        return gp_Dir(rx / len, ry / len, dz);
    };

    double minLx = *minMaxX.first;
    double maxLx = *minMaxX.second;
    double minLy = *minMaxY.first;
    double maxLy = *minMaxY.second;

    gp_Pnt p1 = transformPnt(minLx, minLy, *minMaxZ.first);
    gp_Pnt p2 = transformPnt(maxLx, minLy, *minMaxZ.first);
    gp_Pnt p3 = transformPnt(maxLx, maxLy, *minMaxZ.first);
    gp_Pnt p4 = transformPnt(minLx, maxLy, *minMaxZ.first);

    m_minX = std::min({p1.X(), p2.X(), p3.X(), p4.X()});
    m_maxX = std::max({p1.X(), p2.X(), p3.X(), p4.X()});
    m_minY = std::min({p1.Y(), p2.Y(), p3.Y(), p4.Y()});
    m_maxY = std::max({p1.Y(), p2.Y(), p3.Y(), p4.Y()});
    m_minZ = orig.Z() + *minMaxZ.first;
    m_maxZ = orig.Z() + *minMaxZ.second;

    double startLy = minLy - m_extension;
    double endLy   = maxLy + m_extension;
    double startLx = minLx - m_extension;
    double endLx   = maxLx + m_extension;

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
        double zVal = levels[k];

        // Lignes d'axes X (parallèles à Y, à chaque X_i)
        for (size_t i = 0; i < xPos.size(); ++i)
        {
            GridLineSegment seg;
            seg.start = transformPnt(xPos[i], startLy, zVal);
            seg.end   = transformPnt(xPos[i], endLy,   zVal);
            seg.label = m_definition.getXLabel(i);
            seg.index = static_cast<int>(i);
            seg.isXAxis = true;
            seg.zLevel = orig.Z() + zVal;

            m_xLines.push_back(seg);
            m_allLines.push_back(seg);

            // Ancrages des bulles d'axe X
            m_labelAnchors.push_back({ seg.start, seg.label, transformDir(0, -1, 0), true });
            m_labelAnchors.push_back({ seg.end,   seg.label, transformDir(0,  1, 0), false });
        }

        // Lignes d'axes Y (parallèles à X, à chaque Y_j)
        for (size_t j = 0; j < yPos.size(); ++j)
        {
            GridLineSegment seg;
            seg.start = transformPnt(startLx, yPos[j], zVal);
            seg.end   = transformPnt(endLx,   yPos[j], zVal);
            seg.label = m_definition.getYLabel(j);
            seg.index = static_cast<int>(j);
            seg.isXAxis = false;
            seg.zLevel = orig.Z() + zVal;

            m_yLines.push_back(seg);
            m_allLines.push_back(seg);

            // Ancrages des bulles d'axe Y
            m_labelAnchors.push_back({ seg.start, seg.label, transformDir(-1, 0, 0), true });
            m_labelAnchors.push_back({ seg.end,   seg.label, transformDir( 1, 0, 0), false });
        }

        // Intersections de grille (X_i, Y_j, Z_k)
        for (size_t i = 0; i < xPos.size(); ++i)
        {
            for (size_t j = 0; j < yPos.size(); ++j)
            {
                GridIntersection inter;
                inter.point = transformPnt(xPos[i], yPos[j], zVal);
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
        for (size_t j = 0; j < yPos.size(); ++j)
        {
            GridLineSegment vSeg;
            vSeg.start = transformPnt(xPos[i], yPos[j], levels.front());
            vSeg.end   = transformPnt(xPos[i], yPos[j], levels.back());
            vSeg.label = m_definition.getXLabel(i) + "-" + m_definition.getYLabel(j);
            vSeg.index = static_cast<int>(i * yPos.size() + j);
            vSeg.isXAxis = false;
            vSeg.zLevel = orig.Z() + levels.front();
            m_verticalConnectionLines.push_back(vSeg);
        }
    }

    // 3. Colonne de repérage vertical Z reliant continûment tous les étages
    m_verticalLevelLines.clear();
    m_levelBoundaryPlanes.clear();
    m_levelLabelAnchors.clear();

    double lxDatum = minLx - m_extension - 0.6;
    double lyDatum = minLy - m_extension - 0.6;
    double zBottom = levels.front() - 0.5;
    double zTop = levels.back() + 1.2;

    // Ligne verticale maîtresse Z
    GridLineSegment vertCol;
    vertCol.start = transformPnt(lxDatum, lyDatum, zBottom);
    vertCol.end   = transformPnt(lxDatum, lyDatum, zTop);
    vertCol.label = "Axe Vertical Niveaux Z";
    vertCol.isXAxis = false;
    m_verticalLevelLines.push_back(vertCol);

    // Flèche au sommet de l'axe vertical Z
    gp_Pnt arrowTip = transformPnt(lxDatum, lyDatum, zTop);
    gp_Pnt arrowLeft = transformPnt(lxDatum - 0.15, lyDatum, zTop - 0.30);
    gp_Pnt arrowRight = transformPnt(lxDatum + 0.15, lyDatum, zTop - 0.30);
    m_verticalLevelLines.push_back({ arrowLeft, arrowTip, "", -1, false, orig.Z() + zTop });
    m_verticalLevelLines.push_back({ arrowRight, arrowTip, "", -1, false, orig.Z() + zTop });

    for (size_t k = 0; k < levels.size(); ++k)
    {
        double zVal = levels[k];
        double realZ = orig.Z() + zVal;

        // Bras horizontal de niveau reliant la colonne Z à la grille
        gp_Pnt tickStart = transformPnt(lxDatum, lyDatum, zVal);
        gp_Pnt tickEnd   = transformPnt(lxDatum + 0.8, lyDatum, zVal);
        m_verticalLevelLines.push_back({ tickStart, tickEnd, "", static_cast<int>(k), false, realZ });

        // Symbole triangulaire de niveau génie civil au niveau Z_k
        gp_Pnt triTop   = transformPnt(lxDatum + 0.3, lyDatum, zVal);
        gp_Pnt triLeft  = transformPnt(lxDatum + 0.1, lyDatum, zVal - 0.2);
        gp_Pnt triRight = transformPnt(lxDatum + 0.5, lyDatum, zVal - 0.2);
        m_verticalLevelLines.push_back({ triLeft, triTop, "", -1, false, realZ });
        m_verticalLevelLines.push_back({ triTop, triRight, "", -1, false, realZ });
        m_verticalLevelLines.push_back({ triRight, triLeft, "", -1, false, realZ });

        // Ancrage d'étiquette de niveau (nom + élévation)
        std::ostringstream ss;
        ss << m_definition.getZLabel(k) << " [" << std::fixed << std::setprecision(2)
           << (realZ >= 0 ? "+" : "") << realZ << " m]";
        m_levelLabelAnchors.push_back({ transformPnt(lxDatum - 0.3, lyDatum, zVal), ss.str(), gp_Dir(0, 0, 1), true });

        // Cadre périmétrique du plancher au niveau Z_k
        gp_Pnt c1 = transformPnt(minLx, minLy, zVal);
        gp_Pnt c2 = transformPnt(maxLx, minLy, zVal);
        gp_Pnt c3 = transformPnt(maxLx, maxLy, zVal);
        gp_Pnt c4 = transformPnt(minLx, maxLy, zVal);
        m_levelBoundaryPlanes.push_back({ c1, c2, "", static_cast<int>(k), false, realZ });
        m_levelBoundaryPlanes.push_back({ c2, c3, "", static_cast<int>(k), false, realZ });
        m_levelBoundaryPlanes.push_back({ c3, c4, "", static_cast<int>(k), false, realZ });
        m_levelBoundaryPlanes.push_back({ c4, c1, "", static_cast<int>(k), false, realZ });
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
