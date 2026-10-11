#include "ConcreteDesignEC2.h"
#include <algorithm>

namespace TSA::Standards::Design
{

EC2BeamBendingResult ConcreteDesignEC2::calculateRectangularBeamFlexure(const EC2BeamBendingInput& input)
{
    EC2BeamBendingResult res;

    // 1. Contrôles défensifs des dimensions et paramètres matériaux
    if (std::isnan(input.b) || std::isnan(input.h) || input.b <= 1e-4 || input.h <= 1e-4)
    {
        res.summary = QStringLiteral("Dimensions de section invalides (b ou h <= 0)");
        return res;
    }

    double d = input.d > 1e-4 ? input.d : (input.h - input.cover);
    if (d <= 1e-4 || d >= input.h)
    {
        res.summary = QStringLiteral("Hauteur utile d invalide ou incohérente avec la hauteur totale");
        return res;
    }

    if (input.fck <= 1.0e6 || input.fyk <= 1.0e6 || input.gammaC <= 0.1 || input.gammaS <= 0.1)
    {
        res.summary = QStringLiteral("Paramètres de résistance ou coefficients partiels invalides");
        return res;
    }

    res.effectiveDepth = d;
    res.fcd = (input.alphaCC * input.fck) / input.gammaC;
    res.fyd = input.fyk / input.gammaS;

    double Med = std::abs(input.Med);
    if (std::isnan(Med) || std::isinf(Med))
    {
        Med = 0.0;
    }

    // Résistance à la traction moyenne fctm (EN 1992-1-1 Tableau 3.1)
    double fck_MPa = input.fck / 1.0e6;
    if (fck_MPa > 90.0)
    {
        res.summary = QStringLiteral("fck > 90 MPa : hors du domaine de l'EN 1992-1-1 (Tableau 3.1)");
        return res;
    }
    double fctm = (fck_MPa <= 50.0 ? 0.30 * std::pow(fck_MPa, 2.0 / 3.0) : 2.12 * std::log(1.0 + (fck_MPa + 8.0) / 10.0)) * 1.0e6; // Pa
    // Bloc rectangulaire (§3.1.7(3)) et déformation ultime (Tableau 3.1)
    res.lambda = fck_MPa <= 50.0 ? 0.8 : 0.8 - (fck_MPa - 50.0) / 400.0;
    res.eta = fck_MPa <= 50.0 ? 1.0 : 1.0 - (fck_MPa - 50.0) / 200.0;
    res.epsCu3 = fck_MPa <= 50.0 ? 3.5e-3 : (2.6 + 35.0 * std::pow((90.0 - fck_MPa) / 100.0, 4)) * 1e-3;
    constexpr double Es = 200.0e9;   // §3.2.7(4)
    const double epsYd = res.fyd / Es;
    const double alphaLim = res.epsCu3 / (res.epsCu3 + epsYd);   // limite entre pivots A et B : aciers tendus plastifiés

    // Section minimale d'armatures tendues (EN 1992-1-1 §9.2.1.1)
    res.As_min = std::max(0.26 * (fctm / input.fyk) * input.b * d, 0.0013 * input.b * d);
    res.As_max = 0.04 * input.b * input.h;

    // Cas sollicitation nulle
    if (Med < 1e-3)
    {
        res.valid = true;
        res.mu_cu = 0.0;
        res.z = 0.9 * d;
        res.As_required = 0.0;
        res.As_provided = res.As_min;
        res.utilizationRatio = 0.0;
        res.summary = QStringLiteral("Sollicitation négligeable : ferraillage minimal réglementaire appliqué");
        return res;
    }

    // 2. Calcul du moment réduit agissant mu_cu (bloc rectangulaire simplifié)
    double denom = input.b * d * d * res.fcd;
    if (denom <= 1e-9)
    {
        res.summary = QStringLiteral("Dénominateur de capacité nulle");
        return res;
    }

    res.mu_cu = Med / denom;
    // μ = η·λ·α·(1 − λ·α/2) : moment réduit du bloc rectangulaire de hauteur λ·x
    res.mu_lu = res.eta * res.lambda * alphaLim * (1.0 - res.lambda * alphaLim / 2.0);
    res.utilizationRatio = res.mu_cu / res.mu_lu;

    if (res.mu_cu > res.mu_lu)
    {
        // Aciers comprimés (section doublement armée) : le béton reprend M_lu avec x = αu·d, le complément
        // ΔM = M_Ed − M_lu est équilibré par un couple As2·σs2 / As·fyd de bras (d − d2).
        const double d2 = input.d2 > 1e-4 ? input.d2 : input.cover;
        if (d2 >= alphaLim * d)
        {
            res.summary = QStringLiteral("Aciers comprimés inefficaces (d2 ≥ x_lim) : section à redimensionner");
            return res;
        }
        const double Mlu = res.mu_lu * denom;
        const double zlu = d * (1.0 - res.lambda * alphaLim / 2.0);
        const double xlu = alphaLim * d;
        const double epsS2 = res.epsCu3 * (xlu - d2) / xlu;
        const double sigmaS2 = std::min(res.fyd, Es * epsS2);
        res.alpha_u = alphaLim;
        res.z = zlu;
        res.As2_required = (Med - Mlu) / ((d - d2) * sigmaS2);
        res.As_required = Mlu / (zlu * res.fyd) + res.As2_required * sigmaS2 / res.fyd;
        res.As_provided = std::max(res.As_required, res.As_min);
        res.exceedsMaximum = res.As_provided + res.As2_required > res.As_max;
        res.valid = true;
        res.requiresCompressionSteel = true;
        res.summary = QString("μcu = %1 > μlu = %2 : aciers comprimés requis, As = %3 cm², As2 = %4 cm²%5")
                          .arg(res.mu_cu, 0, 'f', 3)
                          .arg(res.mu_lu, 0, 'f', 3)
                          .arg(res.As_provided * 1e4, 0, 'f', 2)
                          .arg(res.As2_required * 1e4, 0, 'f', 2)
                          .arg(res.exceedsMaximum ? QStringLiteral(" — au-delà de 0,04·Ac (§9.2.1.1(3))") : QString());
        return res;
    }

    // 3. Section avec armatures tendues seules : λ·α = 1 − √(1 − 2μ/η)
    double radical = 1.0 - 2.0 * res.mu_cu / res.eta;
    if (radical < 0.0) radical = 0.0;
    res.alpha_u = (1.0 - std::sqrt(radical)) / res.lambda;

    // Bras de levier z = d·(1 − λ·α/2)
    res.z = d * (1.0 - res.lambda * res.alpha_u / 2.0);
    if (res.z <= 1e-4) res.z = 0.8 * d;

    // Section d'acier tendu requise
    res.As_required = Med / (res.z * res.fyd);
    res.As_provided = std::max(res.As_required, res.As_min);
    res.exceedsMaximum = res.As_provided > res.As_max;

    res.valid = true;
    res.requiresCompressionSteel = false;
    res.summary = QString("Dimensionnement réussi : As = %1 cm² (As_min = %2 cm², ratio = %3%)")
                      .arg(res.As_provided * 1.0e4, 0, 'f', 2)
                      .arg(res.As_min * 1.0e4, 0, 'f', 2)
                      .arg(res.utilizationRatio * 100.0, 0, 'f', 1);

    return res;
}

EC2BeamBendingResult ConcreteDesignEC2::calculateFromModel(
    const TSA::Model::Section& section,
    const TSA::Model::Material& concreteMat,
    double Med_Nm,
    double fyk_Pa,
    double cover_m)
{
    if (section.shape != TSA::Model::SectionShape::Rectangular)
    {
        EC2BeamBendingResult res;
        res.summary = QStringLiteral("Section non rectangulaire : flexion simple EC2 non calculée par TSA");
        return res;
    }
    EC2BeamBendingInput in;
    in.b = section.width;
    in.h = section.height;
    in.cover = cover_m;
    in.fck = concreteMat.mechanical.fk();
    in.fyk = fyk_Pa;
    in.Med = Med_Nm;

    auto& annex = TSA::Standards::NationalAnnexConfig::instance();
    auto factors = annex.safetyFactors();
    in.gammaC = factors.gammaC;
    in.gammaS = factors.gammaS;

    return calculateRectangularBeamFlexure(in);
}

} // namespace TSA::Standards::Design
