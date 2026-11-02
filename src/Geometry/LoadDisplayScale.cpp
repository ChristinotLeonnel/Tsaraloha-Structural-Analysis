#include "LoadDisplayScale.h"

#include "../Model/Load/LoadManager.h"
#include "../Model/Model.h"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <vector>

namespace TSA::Geometry
{

const char* loadFamilyUnit(LoadFamily f)
{
    switch (f)
    {
    case LoadFamily::Force: return "kN";
    case LoadFamily::Moment: return "kN·m";
    case LoadFamily::LineForce: return "kN/m";
    case LoadFamily::LineMoment: return "kN·m/m";
    case LoadFamily::Pressure: return "kN/m²";
    default: return "";
    }
}

const char* loadFamilyName(LoadFamily f)
{
    switch (f)
    {
    case LoadFamily::Force: return "forces";
    case LoadFamily::Moment: return "moments";
    case LoadFamily::LineForce: return "charges linéiques";
    case LoadFamily::LineMoment: return "moments répartis";
    case LoadFamily::Pressure: return "charges surfaciques";
    default: return "";
    }
}

LoadFamilyMaxima loadFamilyMaxima(const TSA::Model::LoadManager& lm)
{
    LoadFamilyMaxima m;
    std::array<std::vector<double>, static_cast<int>(LoadFamily::Count)> values;
    auto put = [&](LoadFamily f, double v) {
        if (!std::isfinite(v) || std::abs(v) <= 0.0) return;
        auto& slot = m.max[static_cast<int>(f)];
        slot = std::max(slot, std::abs(v));
        values[static_cast<int>(f)].push_back(std::abs(v));
    };
    for (const auto& [id, nl] : lm.nodalLoads())
    {
        put(LoadFamily::Force, std::sqrt(nl.fx() * nl.fx() + nl.fy() * nl.fy() + nl.fz() * nl.fz()));
        put(LoadFamily::Moment, std::sqrt(nl.mx() * nl.mx() + nl.my() * nl.my() + nl.mz() * nl.mz()));
    }
    for (const auto& [id, ml] : lm.memberLoads())
    {
        using TSA::Model::LoadType;
        const double q = std::max(std::abs(ml.q1()), ml.type() == LoadType::MemberLinear ? std::abs(ml.q2()) : 0.0);
        if (ml.type() == LoadType::MemberPoint) put(LoadFamily::Force, q);
        else if (ml.type() == LoadType::MemberMoment) put(LoadFamily::LineMoment, q);
        else put(LoadFamily::LineForce, q);
    }
    for (const auto& [id, sl] : lm.surfaceLoads())
        put(sl.distribution == TSA::Model::SurfaceLoadDistribution::Point ? LoadFamily::Force : LoadFamily::Pressure, sl.value);
    for (int f = 0; f < static_cast<int>(LoadFamily::Count); ++f)
    {
        auto& v = values[static_cast<std::size_t>(f)];
        m.scale[f] = m.max[f];
        if (v.size() < 5) continue;   // trop peu de charges pour parler de valeur hors norme
        const std::size_t k = static_cast<std::size_t>(std::floor(0.9 * static_cast<double>(v.size() - 1)));
        std::nth_element(v.begin(), v.begin() + static_cast<std::ptrdiff_t>(k), v.end());
        const double p90 = v[k];
        if (p90 > 0.0 && m.max[f] > 3.0 * p90) m.scale[f] = 3.0 * p90;
    }
    return m;
}

double structureLoadReferenceLength(const TSA::Model::Model& model)
{
    std::vector<double> columns, bars;
    double lo[3] = { 1e300, 1e300, 1e300 }, hi[3] = { -1e300, -1e300, -1e300 };
    for (const auto& [id, n] : model.nodes())
    {
        const double c[3] = { n.x(), n.y(), n.z() };
        for (int i = 0; i < 3; ++i) lo[i] = std::min(lo[i], c[i]), hi[i] = std::max(hi[i], c[i]);
    }
    // Élément quasi vertical (|Δz| > 0,9 L) : hauteur d'étage, qu'il soit rangé parmi les poteaux ou les barres.
    auto add = [&](int a, int b) {
        const auto* na = model.getNode(a);
        const auto* nb = model.getNode(b);
        if (!na || !nb) return;
        const double dz = nb->z() - na->z();
        const double L = std::hypot(nb->x() - na->x(), nb->y() - na->y(), dz);
        if (!(L > 1e-6)) return;
        bars.push_back(L);
        if (std::abs(dz) > 0.9 * L) columns.push_back(L);
    };
    for (const auto& [id, c] : model.columns()) add(c.startNodeId(), c.endNodeId());
    for (const auto& [id, b] : model.beams()) add(b.startNodeId(), b.endNodeId());
    auto median = [](std::vector<double>& v) {
        std::nth_element(v.begin(), v.begin() + static_cast<std::ptrdiff_t>(v.size() / 2), v.end());
        return v[v.size() / 2];
    };
    if (bars.empty()) return 1.0;
    double ref = columns.empty() ? 0.30 * median(bars) : 0.35 * median(columns);
    const double span = model.nodes().empty() ? 0.0 : std::hypot(hi[0] - lo[0], hi[1] - lo[1], hi[2] - lo[2]);
    if (span > 1e-6) ref = std::clamp(ref, 0.01 * span, 0.10 * span);   // petit modèle : jamais plus de 10 % de l'envergure
    return std::isfinite(ref) && ref > 1e-6 ? ref : 1.0;
}

double loadArrowLength(double intensity, double familyMax, double referenceLength, const LoadScaleSettings& s)
{
    const double ref = std::max(0.0, referenceLength) * (s.factor > 0.0 ? s.factor : 1.0);
    const double a = std::abs(intensity);
    if (!(a > 0.0) || !(familyMax > 0.0)) return 0.0;   // charge nulle : aucune flèche
    const double maxLen = ref * std::max(0.05, s.maxRatio);
    if (s.mode == LoadScaleMode::Normalized) return maxLen;
    const double len = maxLen * std::min(1.0, a / familyMax);
    const double minLen = ref * (s.mode == LoadScaleMode::Automatic ? std::clamp(s.minRatio, 0.0, 1.0) : 0.03);
    return std::clamp(len, std::min(minLen, maxLen), maxLen);
}

std::string loadScaleLegend(const LoadFamilyMaxima& maxima, double referenceLength, const LoadScaleSettings& s)
{
    std::ostringstream o;
    o.precision(3);
    const double ref = std::max(0.0, referenceLength) * (s.factor > 0.0 ? s.factor : 1.0) * std::max(0.05, s.maxRatio);
    o << "CHARGES — "
      << (s.mode == LoadScaleMode::Normalized ? "longueurs normalisées (intensité = texte)"
                                              : (s.mode == LoadScaleMode::Proportional ? "longueurs proportionnelles" : "longueurs proportionnelles (minimum garanti)"));
    for (int f = 0; f < static_cast<int>(LoadFamily::Count); ++f)
    {
        const double mx = maxima.max[f];
        if (!(mx > 0.0)) continue;
        o << "\n" << loadFamilyName(static_cast<LoadFamily>(f)) << " : max " << mx << " " << loadFamilyUnit(static_cast<LoadFamily>(f));
        if (s.mode != LoadScaleMode::Normalized && ref > 0.0) o << " ↔ " << ref << " m de flèche";
    }
    return o.str();
}

} // namespace TSA::Geometry
