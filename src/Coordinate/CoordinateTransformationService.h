#pragma once

#include "WorkPlane.h"
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>
#include <gp_Ax3.hxx>
#include <memory>

namespace TSA::Coordinate
{

/**
 * @brief Service central de conversion et transformation de coordonnées.
 * Assure la cohérence géométrique entre :
 * - WCS (World Coordinate System - Repère Global absolu)
 * - UCS (User Coordinate System - Repère Utilisateur / Plan de travail)
 * - LCS (Local Coordinate System - Repère propre à chaque élément structural)
 */
class CoordinateTransformationService
{
public:
    static CoordinateTransformationService& instance();

    const WorkPlane& activeWorkPlane() const noexcept { return m_activeWorkPlane; }
    bool hasActiveWorkPlane() const noexcept { return true; }
    void setActiveWorkPlane(const WorkPlane& wp);

    // WCS <-> UCS
    gp_Pnt wcsToUcs(const gp_Pnt& worldPoint) const;
    gp_Pnt ucsToWcs(const gp_Pnt& ucsPoint) const;

    gp_Vec wcsVectorToUcs(const gp_Vec& worldVec) const;
    gp_Vec ucsVectorToWcs(const gp_Vec& ucsVec) const;

    // WCS <-> Local Element System (axe X = longitudinal de start à end, Y/Z = axes d'inertie section avec angle beta)
    static gp_Ax3 computeElementLocalFrame(const gp_Pnt& startPt, const gp_Pnt& endPt, double betaAngleDeg = 0.0);

    static gp_Pnt wcsToElementLocal(const gp_Pnt& worldPoint, const gp_Pnt& startPt, const gp_Pnt& endPt, double betaAngleDeg = 0.0);
    static gp_Pnt elementLocalToWcs(const gp_Pnt& localPoint, const gp_Pnt& startPt, const gp_Pnt& endPt, double betaAngleDeg = 0.0);

private:
    CoordinateTransformationService();
    ~CoordinateTransformationService() = default;

    WorkPlane m_activeWorkPlane;
};

} // namespace TSA::Coordinate
