#pragma once

// Marqueur d'accrochage façon CAO : petit symbole par type (□ extrémité, △ milieu, ⊙ centre,
// ✕ intersection, ⊥ perpendiculaire, ⧗ proche, ◇ grille…) + libellé, de TAILLE CONSTANTE EN PIXELS,
// ancré exactement sur le point d'accrochage (persistance zoom + rotation), dessiné au-dessus de la
// géométrie et non sélectionnable. Remplace l'ancien cube de 0,20 m (taille monde).

#include "GridType.h"

#include <AIS_InteractiveObject.hxx>

#include <string>

namespace TSA::Grid
{

class SnapMarker : public AIS_InteractiveObject
{
    DEFINE_STANDARD_RTTI_INLINE(SnapMarker, AIS_InteractiveObject)

public:
    SnapMarker();

    /// Met à jour type, position et libellé ; retourne true si la présentation doit être recalculée
    /// (type ou libellé changés) — un simple déplacement ne recalcule rien.
    bool setSnap(const GridSnapResult& snap);
    /// Facteur d'échelle des pixels (écrans haute densité).
    void setPixelScale(double scale);

    /// Libellé affiché (« Milieu · Poutre B3 »).
    static std::string labelFor(const GridSnapResult& snap);

    bool AcceptDisplayMode(const int mode) const override { return mode == 0; }

protected:
    void Compute(const Handle(PrsMgr_PresentationManager)& prsMgr, const Handle(Prs3d_Presentation)& prs, const int mode) override;
    void ComputeSelection(const Handle(SelectMgr_Selection)&, const int) override {}

private:
    GridSnapType m_type = GridSnapType::None;
    SnapSource m_source = SnapSource::None;
    std::string m_label;
    double m_scale = 1.0;
};

} // namespace TSA::Grid
