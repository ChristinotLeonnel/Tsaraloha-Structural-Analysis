#include "LoadDisplayScale.h"

#include "../Model/Load/LoadManager.h"

#include <algorithm>
#include <cmath>
#include <sstream>

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
    auto put = [&](LoadFamily f, double v) {
        auto& slot = m.max[static_cast<int>(f)];
        if (std::isfinite(v)) slot = std::max(slot, std::abs(v));
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
    return m;
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
