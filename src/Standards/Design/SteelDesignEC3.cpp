#include "SteelDesignEC3.h"
#include <algorithm>

namespace TSA::Standards::Design
{

double SteelDesignEC3::imperfectionFactor(EC3BucklingCurve curve)
{
    switch (curve)
    {
    case EC3BucklingCurve::a0: return 0.13;
    case EC3BucklingCurve::a:  return 0.21;
    case EC3BucklingCurve::b:  return 0.34;
    case EC3BucklingCurve::c:  return 0.49;
    case EC3BucklingCurve::d:  return 0.76;
    default:                   return 0.34;
    }
}

double SteelDesignEC3::tensionResistance(double A_m2, double fy_Pa, double gammaM0)
{
    if (gammaM0 <= 0.1) gammaM0 = 1.00;
    return (A_m2 * fy_Pa) / gammaM0;
}

double SteelDesignEC3::compressionResistance(double A_m2, double fy_Pa, double gammaM0)
{
    if (gammaM0 <= 0.1) gammaM0 = 1.00;
    return (A_m2 * fy_Pa) / gammaM0;
}

double SteelDesignEC3::bendingResistance(double Wel_m3, double fy_Pa, double gammaM0)
{
    if (gammaM0 <= 0.1) gammaM0 = 1.00;
    return (Wel_m3 * fy_Pa) / gammaM0;
}

double SteelDesignEC3::shearResistance(double Av_m2, double fy_Pa, double gammaM0)
{
    if (gammaM0 <= 0.1) gammaM0 = 1.00;
    return (Av_m2 * (fy_Pa / std::sqrt(3.0))) / gammaM0;
}

EC3BucklingResult SteelDesignEC3::calculateBuckling(const EC3BucklingInput& input)
{
    EC3BucklingResult res;

    // 1. Contrôles de validité défensifs
    if (std::isnan(input.Lcr) || input.Lcr <= 1e-4)
    {
        res.summary = QStringLiteral("Longueur de flambement Lcr invalide ou nulle");
        return res;
    }
    if (std::isnan(input.A) || input.A <= 1e-7)
    {
        res.summary = QStringLiteral("Aire de section droite A invalide ou nulle");
        return res;
    }
    if (std::isnan(input.I) || input.I <= 1e-13)
    {
        res.summary = QStringLiteral("Moment d'inertie I invalide ou nul");
        return res;
    }
    if (input.E <= 1e6 || input.fy <= 1e6 || input.gammaM1 <= 0.1)
    {
        res.summary = QStringLiteral("Propriétés mécaniques E, fy ou gammaM1 invalides");
        return res;
    }

    // 2. Caractéristiques géométriques et élancements
    res.radiusOfGyration = std::sqrt(input.I / input.A);
    if (res.radiusOfGyration <= 1e-6)
    {
        res.summary = QStringLiteral("Rayon de giration quasi nul");
        return res;
    }

    res.slenderness = input.Lcr / res.radiusOfGyration;
    res.eulerSlenderness = M_PI * std::sqrt(input.E / input.fy);
    if (res.eulerSlenderness <= 1e-4)
    {
        res.summary = QStringLiteral("Élancement d'Euler de référence invalide");
        return res;
    }

    res.reducedSlenderness = res.slenderness / res.eulerSlenderness;
    res.imperfectionFactor = imperfectionFactor(input.curve);

    // 3. Calcul du coefficient de réduction au flambement chi (EN 1993-1-1 §6.3.1.2)
    if (res.reducedSlenderness <= 0.2)
    {
        // Flambement non prépondérant pour les barres très courtes (lambda_bar <= 0.2)
        res.phi = 0.5 * (1.0 + res.imperfectionFactor * (res.reducedSlenderness - 0.2) + res.reducedSlenderness * res.reducedSlenderness);
        res.chi = 1.0;
    }
    else
    {
        double lbar = res.reducedSlenderness;
        res.phi = 0.5 * (1.0 + res.imperfectionFactor * (lbar - 0.2) + lbar * lbar);
        double delta = res.phi * res.phi - lbar * lbar;
        if (delta < 0.0) delta = 0.0;
        res.chi = 1.0 / (res.phi + std::sqrt(delta));
        if (res.chi > 1.0) res.chi = 1.0;
    }

    // 4. Effort résistant Nb,Rd et ratio d'utilisation
    res.Nb_Rd = (res.chi * input.A * input.fy) / input.gammaM1;

    double Ned = std::abs(input.Ned);
    if (std::isnan(Ned) || std::isinf(Ned)) Ned = 0.0;

    res.utilizationRatio = (res.Nb_Rd > 1e-4) ? (Ned / res.Nb_Rd) : 999.0;
    res.pass = (res.utilizationRatio <= 1.0001);
    res.valid = true;

    res.summary = QString("Vérification flambement EC3 : lambda_bar = %1, chi = %2, Nb,Rd = %3 kN, eta = %4% (%5)")
                      .arg(res.reducedSlenderness, 0, 'f', 2)
                      .arg(res.chi, 0, 'f', 3)
                      .arg(res.Nb_Rd / 1000.0, 0, 'f', 1)
                      .arg(res.utilizationRatio * 100.0, 0, 'f', 1)
                      .arg(res.pass ? QStringLiteral("CONFORME") : QStringLiteral("NON CONFORME"));

    return res;
}

EC3BucklingCurve SteelDesignEC3::defaultBucklingCurve(TSA::Model::SectionShape shape, bool strongAxis)
{
    switch (shape)
    {
    case TSA::Model::SectionShape::IShape:
        return strongAxis ? EC3BucklingCurve::a : EC3BucklingCurve::b;
    case TSA::Model::SectionShape::Pipe:
    case TSA::Model::SectionShape::Circular:
        return EC3BucklingCurve::a;
    case TSA::Model::SectionShape::BoxHollow:
        return EC3BucklingCurve::b;
    case TSA::Model::SectionShape::UPN:
        return EC3BucklingCurve::c;
    case TSA::Model::SectionShape::Angle:
    case TSA::Model::SectionShape::TSection:
        return EC3BucklingCurve::b;
    case TSA::Model::SectionShape::Rectangular:
    default:
        return EC3BucklingCurve::b;
    }
}

EC3BucklingResult SteelDesignEC3::calculateFromBar(
    const TSA::Model::Section& section,
    const TSA::Model::Material& steelMat,
    double memberLength_m,
    double beta,
    double Ned_N,
    bool strongAxis)
{
    EC3BucklingInput in;
    in.Lcr = (beta > 1e-4 ? beta : 1.0) * memberLength_m;
    in.A = section.area();
    in.I = strongAxis ? section.iy() : section.iz();
    in.E = steelMat.mechanical.E();
    in.fy = steelMat.mechanical.fk();
    in.Ned = Ned_N;
    in.curve = defaultBucklingCurve(section.shape, strongAxis);

    auto& annex = TSA::Standards::NationalAnnexConfig::instance();
    in.gammaM1 = annex.safetyFactors().gammaM1;

    return calculateBuckling(in);
}

} // namespace TSA::Standards::Design
