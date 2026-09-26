#include "ArbitraryGrid.h"
#include <gp_Vec.hxx>
#include <cmath>
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace TSA::Grid
{

ArbitraryGrid::ArbitraryGrid(const GridDefinition& definition)
    : m_definition(definition)
{
    rebuild();
}

void ArbitraryGrid::rebuild()
{
    computeGeometry();
}

void ArbitraryGrid::computeGeometry()
{
    m_renderLines.clear();
    m_intersections.clear();
    m_labelAnchors.clear();

    const auto& lines = m_definition.arbitraryLines();
    const double extensionLength = 50.0; // Extension par défaut pour une droite infinie
    const double bubbleOffset = m_definition.displaySettings().bubbleRadius + 0.20;

    for (size_t i = 0; i < lines.size(); ++i)
    {
        const auto& line = lines[i];
        gp_Vec v(line.p1, line.p2);
        double len = v.Magnitude();
        if (len < 1e-5)
            continue;

        gp_Vec u = v / len; // Vecteur unitaire

        gp_Pnt renderStart = line.p1;
        gp_Pnt renderEnd = line.p2;

        if (line.type == "droite")
        {
            renderStart = line.p1.Translated(-u * extensionLength);
            renderEnd = line.p2.Translated(u * extensionLength);
        }

        std::string lbl = line.label.empty() ? ("L" + std::to_string(i + 1)) : line.label;

        m_renderLines.push_back({ renderStart, renderEnd, lbl, line.type, line.isBold });

        // Position de l'étiquette et bulle d'axe à l'extrémité
        gp_Pnt anchorPos = (line.type == "droite")
            ? line.p2.Translated(u * 2.0)
            : line.p2.Translated(u * bubbleOffset);

        m_labelAnchors.push_back({ anchorPos, lbl, line.isBold });
    }

    // Calcul des intersections entre paires de lignes
    for (size_t i = 0; i < m_renderLines.size(); ++i)
    {
        for (size_t j = i + 1; j < m_renderLines.size(); ++j)
        {
            gp_Pnt interPt;
            if (computeLineIntersection(m_renderLines[i].start, m_renderLines[i].end,
                                       m_renderLines[j].start, m_renderLines[j].end,
                                       interPt))
            {
                // Vérifier si pas déjà présent
                bool exists = false;
                for (const auto& existing : m_intersections)
                {
                    if (existing.Distance(interPt) < 1e-3)
                    {
                        exists = true;
                        break;
                    }
                }
                if (!exists)
                {
                    m_intersections.push_back(interPt);
                }
            }
        }
    }
}

bool ArbitraryGrid::computeLineIntersection(const gp_Pnt& p1, const gp_Pnt& p2,
                                           const gp_Pnt& p3, const gp_Pnt& p4,
                                           gp_Pnt& outIntersection)
{
    // Calcul de l'intersection 2D (plan XY) ou quasi-coplanaires en 3D
    double x1 = p1.X(), y1 = p1.Y();
    double x2 = p2.X(), y2 = p2.Y();
    double x3 = p3.X(), y3 = p3.Y();
    double x4 = p4.X(), y4 = p4.Y();

    double denom = (x1 - x2) * (y3 - y4) - (y1 - y2) * (x3 - x4);
    if (std::abs(denom) < 1e-7)
        return false; // Lignes parallèles

    double t = ((x1 - x3) * (y3 - y4) - (y1 - y3) * (x3 - x4)) / denom;
    double u = -((x1 - x2) * (y1 - y3) - (y1 - y2) * (x1 - x3)) / denom;

    // Pour des segments finis, t et u doivent être dans [0, 1]
    if (t < -0.01 || t > 1.01 || u < -0.01 || u > 1.01)
        return false;

    double ix = x1 + t * (x2 - x1);
    double iy = y1 + t * (y2 - y1);
    double iz = p1.Z() + t * (p2.Z() - p1.Z());

    outIntersection = gp_Pnt(ix, iy, iz);
    return true;
}

GridSnapResult ArbitraryGrid::findClosestSnap(const gp_Pnt& worldPoint, double snapToleranceWorld) const
{
    GridSnapResult bestResult;
    bestResult.snapped = false;
    bestResult.distance = std::numeric_limits<double>::max();

    // 1. Accrochage prioritaire : Intersections entre lignes
    for (const auto& inter : m_intersections)
    {
        double d = worldPoint.Distance(inter);
        if (d <= snapToleranceWorld && d < bestResult.distance)
        {
            bestResult.snapped = true;
            bestResult.point = inter;
            bestResult.type = GridSnapType::Intersection;
            bestResult.distance = d;
            std::ostringstream oss;
            oss << "Intersection de lignes (" << std::fixed << std::setprecision(2)
                << inter.X() << ", " << inter.Y() << ", " << inter.Z() << ")";
            bestResult.description = oss.str();
        }
    }

    if (bestResult.snapped)
        return bestResult;

    // 2. Accrochage sur les points de passage P1 / P2 des lignes
    for (const auto& line : m_definition.arbitraryLines())
    {
        double d1 = worldPoint.Distance(line.p1);
        if (d1 <= snapToleranceWorld && d1 < bestResult.distance)
        {
            bestResult.snapped = true;
            bestResult.point = line.p1;
            bestResult.type = GridSnapType::AxisLine;
            bestResult.distance = d1;
            bestResult.description = "Point P1 de la ligne " + line.label;
        }

        double d2 = worldPoint.Distance(line.p2);
        if (d2 <= snapToleranceWorld && d2 < bestResult.distance)
        {
            bestResult.snapped = true;
            bestResult.point = line.p2;
            bestResult.type = GridSnapType::AxisLine;
            bestResult.distance = d2;
            bestResult.description = "Point P2 de la ligne " + line.label;
        }
    }

    if (bestResult.snapped)
        return bestResult;

    // 3. Projection orthogonale sur chaque ligne
    for (const auto& line : m_renderLines)
    {
        gp_Vec ab(line.start, line.end);
        double len2 = ab.SquareMagnitude();
        if (len2 < 1e-8)
            continue;

        gp_Vec ap(line.start, worldPoint);
        double param = ap.Dot(ab) / len2;

        if (param >= 0.0 && param <= 1.0)
        {
            gp_Pnt projPoint = line.start.Translated(ab * param);
            double d = worldPoint.Distance(projPoint);

            if (d <= snapToleranceWorld && d < bestResult.distance)
            {
                bestResult.snapped = true;
                bestResult.point = projPoint;
                bestResult.type = GridSnapType::AxisLine;
                bestResult.distance = d;
                bestResult.description = "Ligne " + line.label;
            }
        }
    }

    return bestResult;
}

} // namespace TSA::Grid
