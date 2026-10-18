#pragma once

// Grille du plan de travail : lignes aux multiples du pas d'accrochage du plan (WorkPlane::gridSpacingX/Y, mètres),
// dans son repère local (U, V), sur le rectangle du panneau du plan. Le pas affiché grandit avec le dézoom (pas du plan
// × 1, 2, 5, 10, 20…) pour garder un écart lisible à l'écran ; toutes les lignes affichées restent des lignes
// d'accrochage, et l'accrochage à la grille utilise ce même pas (ce que l'on voit est ce que l'on accroche). Une ligne
// sur cinq est accentuée. Non sélectionnable : n'interfère ni avec la sélection ni avec l'accrochage aux objets.

#include <AIS_InteractiveObject.hxx>
#include <Quantity_Color.hxx>

namespace TSA::Viewer
{

class WorkPlaneGrid : public AIS_InteractiveObject
{
    DEFINE_STANDARD_RTTI_INLINE(WorkPlaneGrid, AIS_InteractiveObject)

public:
    struct Layout
    {
        double halfU = 0.0, halfV = 0.0;       ///< demi-dimensions du rectangle (m)
        double centerU = 0.0, centerV = 0.0;   ///< centre du rectangle dans le repère du plan (m)
        double stepU = 1.0, stepV = 1.0;       ///< pas affichés (m)
        int majorEvery = 5;
        bool operator==(const Layout& o) const
        {
            return halfU == o.halfU && halfV == o.halfV && centerU == o.centerU && centerV == o.centerV && stepU == o.stepU
                   && stepV == o.stepV && majorEvery == o.majorEvery;
        }
    };

    /// Lignes au plus par direction : au-delà, le pas est encore agrandi (densité et coût bornés).
    static constexpr int kMaxLinesPerDirection = 400;

    /// Pas affiché : baseStep × k (k = 1, 2, 5, 10, 20, 50…), le plus petit donnant au moins minPixels entre deux
    /// lignes à l'écran et au plus kMaxLinesPerDirection lignes sur la longueur 2 × half.
    static double displayedStep(double baseStep, double worldPerPixel, double half, double minPixels = 12.0);
    /// Nombre de lignes tracées dans une direction (multiples du pas dans [center - half, center + half]).
    static int lineCount(double half, double step, double center = 0.0);

    /// Retourne true si la disposition change (présentation à recalculer).
    bool setLayout(const Layout& layout);
    const Layout& layout() const noexcept { return m_layout; }
    bool setColors(const Quantity_Color& minor, const Quantity_Color& major);

    bool AcceptDisplayMode(const int mode) const override { return mode == 0; }

protected:
    void Compute(const Handle(PrsMgr_PresentationManager)& prsMgr, const Handle(Prs3d_Presentation)& prs, const int mode) override;
    void ComputeSelection(const Handle(SelectMgr_Selection)&, const int) override {}

private:
    Layout m_layout;
    Quantity_Color m_minor = Quantity_Color(0.45, 0.52, 0.60, Quantity_TOC_sRGB);
    Quantity_Color m_major = Quantity_Color(0.62, 0.72, 0.84, Quantity_TOC_sRGB);
};

} // namespace TSA::Viewer
