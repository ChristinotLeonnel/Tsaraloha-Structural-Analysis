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

    // Résistance à la traction moyenne fctm (EN 1992-1-1 Tableau 3.1) pour fck <= 50 MPa
    double fck_MPa = input.fck / 1.0e6;
    double fctm = 0.30 * std::pow(fck_MPa, 2.0 / 3.0) * 1.0e6; // Pa

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
    res.mu_lu = 0.372; // Limite sans armatures comprimées pour B500B (pivot B)
    res.utilizationRatio = res.mu_cu / res.mu_lu;

    if (res.mu_cu > res.mu_lu)
    {
        res.valid = true;
        res.requiresCompressionSteel = true;
        res.z = 0.8 * d;
        res.As_required = Med / (res.z * res.fyd);
        res.As_provided = std::min(res.As_required, res.As_max);
        res.summary = QStringLiteral("Moment ELU supérieur à la capacité sans aciers comprimés (mu_cu > mu_lu)");
        return res;
    }

    // 3. Section avec armatures tendues seules
    // Position relative de l'axe neutre alpha_u = 1.25 * (1 - sqrt(1 - 2 * mu_cu))
    double radical = 1.0 - 2.0 * res.mu_cu;
    if (radical < 0.0) radical = 0.0;
    double alpha_u = 1.25 * (1.0 - std::sqrt(radical));

    // Bras de levier z = d * (1 - 0.4 * alpha_u)
    res.z = d * (1.0 - 0.4 * alpha_u);
    if (res.z <= 1e-4) res.z = 0.8 * d;

    // Section d'acier tendu requise
    res.As_required = Med / (res.z * res.fyd);
    res.As_provided = std::max(res.As_required, res.As_min);

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
