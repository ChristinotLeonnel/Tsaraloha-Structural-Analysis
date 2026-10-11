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
    // Sans les dimensions (h/b, tf) : courbe la plus défavorable compatible avec la forme (Tableau 6.2).
    // Auparavant : I laminé a/b (non sécuritaire pour h/b ≤ 1,2 : b/c), plein et T en b (Tableau : c), tubes b.
    switch (shape)
    {
    case TSA::Model::SectionShape::IShape:
        return strongAxis ? EC3BucklingCurve::b : EC3BucklingCurve::c;
    case TSA::Model::SectionShape::Angle:
        return EC3BucklingCurve::b;
    case TSA::Model::SectionShape::Pipe:
    case TSA::Model::SectionShape::BoxHollow:
    case TSA::Model::SectionShape::Circular:
    case TSA::Model::SectionShape::UPN:
    case TSA::Model::SectionShape::TSection:
    case TSA::Model::SectionShape::Rectangular:
    default:
        return EC3BucklingCurve::c;
    }
}

QString SteelDesignEC3::curveName(EC3BucklingCurve c)
{
    switch (c)
    {
    case EC3BucklingCurve::a0: return QStringLiteral("a0");
    case EC3BucklingCurve::a: return QStringLiteral("a");
    case EC3BucklingCurve::b: return QStringLiteral("b");
    case EC3BucklingCurve::c: return QStringLiteral("c");
    default: return QStringLiteral("d");
    }
}

EC3BucklingCurve SteelDesignEC3::bucklingCurve(const TSA::Model::Section& s, double fy, bool strongAxis, QString* reason)
{
    using C = EC3BucklingCurve;
    const bool s460 = fy >= 460.0e6 - 1.0;
    auto say = [&](const QString& r, C c) {
        if (reason) *reason = r;
        return c;
    };
    switch (s.shape)
    {
    case TSA::Model::SectionShape::IShape:
    {
        const double hb = s.width > 0 ? s.height / s.width : 0.0, tf = s.tf * 1000.0;
        const bool rolled = s.rootRadius() > 0.0;
        if (!rolled)   // profil reconstitué soudé
            return tf <= 40.0 ? say(QStringLiteral("I soudé, tf ≤ 40 mm"), strongAxis ? C::b : C::c)
                              : say(QStringLiteral("I soudé, tf > 40 mm"), strongAxis ? C::c : C::d);
        if (hb > 1.2)
        {
            if (tf <= 40.0) return say(QStringLiteral("I laminé, h/b > 1,2, tf ≤ 40 mm"), s460 ? C::a0 : (strongAxis ? C::a : C::b));
            if (tf <= 100.0) return say(QStringLiteral("I laminé, h/b > 1,2, 40 < tf ≤ 100 mm"), s460 ? C::a : (strongAxis ? C::b : C::c));
        }
        else
        {
            if (tf <= 100.0) return say(QStringLiteral("I laminé, h/b ≤ 1,2, tf ≤ 100 mm"), s460 ? C::a : (strongAxis ? C::b : C::c));
            return say(QStringLiteral("I laminé, h/b ≤ 1,2, tf > 100 mm"), s460 ? C::c : C::d);
        }
        return say(QStringLiteral("I laminé, h/b > 1,2, tf > 100 mm (hors tableau)"), C::d);
    }
    case TSA::Model::SectionShape::Pipe:
    case TSA::Model::SectionShape::BoxHollow:
        return say(QStringLiteral("profil creux, fabrication non précisée : formé à froid (sécuritaire)"), C::c);
    case TSA::Model::SectionShape::Angle:
        return say(QStringLiteral("cornière"), C::b);
    case TSA::Model::SectionShape::UPN:
    case TSA::Model::SectionShape::TSection:
    case TSA::Model::SectionShape::Rectangular:
    case TSA::Model::SectionShape::Circular:
    default:
        return say(QStringLiteral("U, T ou section pleine"), C::c);
    }
}

int SteelDesignEC3::compressionClass(const TSA::Model::Section& s, double fy)
{
    if (fy <= 0) return 0;
    const double eps = std::sqrt(235.0e6 / fy);
    auto classOf = [](double ratio, double l1, double l2, double l3) { return ratio <= l1 ? 1 : ratio <= l2 ? 2 : ratio <= l3 ? 3 : 4; };
    switch (s.shape)
    {
    case TSA::Model::SectionShape::IShape:
    {
        const double r = s.rootRadius();
        const double cWeb = s.height - 2.0 * s.tf - 2.0 * r, cFlange = (s.width - s.tw - 2.0 * r) / 2.0;
        if (s.tw <= 0 || s.tf <= 0) return 0;
        const int web = classOf(cWeb / s.tw, 33 * eps, 38 * eps, 42 * eps);           // paroi interne comprimée
        const int flange = classOf(cFlange / s.tf, 9 * eps, 10 * eps, 14 * eps);       // paroi en console comprimée
        return std::max(web, flange);
    }
    case TSA::Model::SectionShape::BoxHollow:
    {
        if (s.tw <= 0) return 0;
        const double t2 = s.tf > 0 ? s.tf : s.tw;
        const int a = classOf((s.height - 3.0 * s.tw) / s.tw, 33 * eps, 38 * eps, 42 * eps);   // c ≈ h − 3t (Tableau 5.2, note)
        const int b = classOf((s.width - 3.0 * t2) / t2, 33 * eps, 38 * eps, 42 * eps);
        return std::max(a, b);
    }
    case TSA::Model::SectionShape::Pipe:
        return s.tw > 0 ? classOf(s.diameter / s.tw, 50 * eps * eps, 70 * eps * eps, 90 * eps * eps) : 0;
    case TSA::Model::SectionShape::Angle:
        // Cornière comprimée : classe 3 si h/t ≤ 15ε et (b + h)/2t ≤ 11,5ε, sinon classe 4
        return (s.tw > 0) ? ((s.height / s.tw <= 15 * eps && (s.width + s.height) / (2.0 * s.tw) <= 11.5 * eps) ? 3 : 4) : 0;
    case TSA::Model::SectionShape::Rectangular:
    case TSA::Model::SectionShape::Circular:
        return 1;   // section pleine
    default:
        return 0;   // UPN, T : non classés par TSA
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
    QString reason;
    in.curve = bucklingCurve(section, in.fy, strongAxis, &reason);

    auto& annex = TSA::Standards::NationalAnnexConfig::instance();
    in.gammaM1 = annex.safetyFactors().gammaM1;

    EC3BucklingResult res = calculateBuckling(in);
    res.sectionClass = compressionClass(section, in.fy);
    res.curveReason = QStringLiteral("courbe %1 (Tableau 6.2 : %2)").arg(curveName(in.curve), reason);
    if (res.sectionClass == 4)
    {
        // Classe 4 : Nb,Rd = χ·Aeff·fy/γM1 ; Aeff (EN 1993-1-5) non calculée : aucune conclusion avec l'aire brute
        res.valid = false;
        res.pass = false;
        res.summary = QStringLiteral("Section de classe 4 en compression : aire efficace (EN 1993-1-5) non calculée, vérification non effectuée");
    }
    else
        res.summary += QStringLiteral(" ; ") + res.curveReason + (res.sectionClass ? QStringLiteral(", classe %1").arg(res.sectionClass) : QStringLiteral(", classe non déterminée"));
    return res;
}

} // namespace TSA::Standards::Design
