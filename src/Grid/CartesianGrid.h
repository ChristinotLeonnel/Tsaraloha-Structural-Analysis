#pragma once

#include "GridDefinition.h"
#include <gp_Pnt.hxx>
#include <vector>
#include <utility>

namespace TSA::Grid
{

struct GridLineSegment
{
    gp_Pnt start;
    gp_Pnt end;
    std::string label;
    int index = -1;
    bool isXAxis = false; // true si c'est une ligne d'axe X (parallèle à Y)
    double zLevel = 0.0;
};

struct GridIntersection
{
    gp_Pnt point;
    int xIndex = -1;
    int yIndex = -1;
    int zIndex = -1;
    std::string labelX;
    std::string labelY;
    std::string labelZ;
};

class CartesianGrid
{
public:
    explicit CartesianGrid(const GridDefinition& definition);
    ~CartesianGrid() = default;

    const GridDefinition& definition() const { return m_definition; }
    void updateDefinition(const GridDefinition& definition);

    // Lignes d'axes de la grille (avec débord / extension pour les bulles)
    const std::vector<GridLineSegment>& xLines() const { return m_xLines; }
    const std::vector<GridLineSegment>& yLines() const { return m_yLines; }
    const std::vector<GridLineSegment>& allLines() const { return m_allLines; }

    // Intersections tridimensionnelles de la grille
    const std::vector<GridIntersection>& intersections() const { return m_intersections; }

    // Emplacements des étiquettes / bulles d'axes
    struct LabelAnchor
    {
        gp_Pnt position;
        std::string text;
        gp_Dir direction; // direction de la ligne pour orientation éventuelle
        bool isStart = true;
    };

    const std::vector<LabelAnchor>& labelAnchors() const { return m_labelAnchors; }

    // Limites de la grille
    double minX() const { return m_minX; }
    double maxX() const { return m_maxX; }
    double minY() const { return m_minY; }
    double maxY() const { return m_maxY; }
    double minZ() const { return m_minZ; }
    double maxZ() const { return m_maxZ; }

    // Extension au-delà des limites pour les bulles d'extrémités
    double extension() const { return m_extension; }
    void setExtension(double ext);

    // Calcul du point de snap le plus proche
    GridSnapResult findClosestSnap(const gp_Pnt& worldPoint, double snapToleranceWorld) const;

private:
    void computeGeometry();

private:
    GridDefinition m_definition;
    double m_extension = 1.2; // 1.2 mètres d'extension pour les bulles d'axes

    std::vector<GridLineSegment> m_xLines;
    std::vector<GridLineSegment> m_yLines;
    std::vector<GridLineSegment> m_allLines;
    std::vector<GridIntersection> m_intersections;
    std::vector<LabelAnchor> m_labelAnchors;

    double m_minX = 0.0;
    double m_maxX = 0.0;
    double m_minY = 0.0;
    double m_maxY = 0.0;
    double m_minZ = 0.0;
    double m_maxZ = 0.0;
};

} // namespace TSA::Grid
