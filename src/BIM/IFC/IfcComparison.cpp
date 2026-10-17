#include "IfcComparison.h"

#include "../../Model/Model.h"
#include "../Core/BimModel.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace TSA::BIM::Ifc
{

namespace
{
/// Valeurs égales aux arrondis d'écriture STEP près (réels relatifs à 1e-9).
bool sameValue(const PropertyValue& a, const PropertyValue& b)
{
    if (a.type != b.type || a.text != b.text || a.flag != b.flag) return false;
    return std::abs(a.number - b.number) <= 1e-9 * std::max({ 1.0, std::abs(a.number), std::abs(b.number) });
}

/// Jeux de propriétés de A tous retrouvés dans B (B peut en porter davantage : Psets calculés par TSA à l'export).
bool propertySetsKept(const PhysicalElement& a, const PhysicalElement& b, std::string* missing)
{
    for (const auto& set : a.propertySets)
    {
        const auto* other = b.propertySet(set.name);
        if (!other)
        {
            *missing = set.name;
            return false;
        }
        for (const auto& [name, value] : set.properties)
        {
            auto it = other->properties.find(name);
            if (it == other->properties.end() || !sameValue(value, it->second))
            {
                *missing = set.name + "." + name;
                return false;
            }
        }
    }
    return true;
}
} // namespace

bool IfcComparison::identical() const
{
    return productsA == productsB && guidsPreserved == productsA && classesPreserved == productsA && namesPreserved == productsA
        && propertySetsPreserved == productsA && statusPreserved == productsA && geometryWithinTolerance == geometryCompared
        && nodesA == nodesB && membersA == membersB && surfacesA == surfacesB && foundationsA == foundationsB;
}

std::string IfcComparison::summary() const
{
    auto ratio = [this](int n) { return std::to_string(n) + "/" + std::to_string(productsA); };
    char deviation[32];
    std::snprintf(deviation, sizeof deviation, "%.2e", maxBoundsDeviation);
    return std::string(identical() ? "Aller-retour conforme" : "Aller-retour avec écarts") + " : " + std::to_string(productsA) + " → "
        + std::to_string(productsB) + " produit(s) ; GlobalId " + ratio(guidsPreserved) + ", classes " + ratio(classesPreserved)
        + ", noms " + ratio(namesPreserved) + ", Psets " + ratio(propertySetsPreserved) + ", états " + ratio(statusPreserved)
        + " ; géométrie " + std::to_string(geometryWithinTolerance) + "/" + std::to_string(geometryCompared) + " (écart max "
        + deviation + " m, tolérance " + std::to_string(tolerance) + " m) ; calcul : " + std::to_string(nodesA) + "/"
        + std::to_string(nodesB) + " nœuds, " + std::to_string(membersA) + "/" + std::to_string(membersB) + " barres, "
        + std::to_string(surfacesA) + "/" + std::to_string(surfacesB) + " surfaces, " + std::to_string(foundationsA) + "/"
        + std::to_string(foundationsB) + " fondations";
}

IfcComparison compareModels(const TSA::Model::Model& a, const TSA::Model::Model& b, double tolerance)
{
    IfcComparison c;
    c.tolerance = tolerance;
    const auto& A = a.bim();
    const auto& B = b.bim();
    c.productsA = static_cast<int>(A.elements().size());
    c.productsB = static_cast<int>(B.elements().size());
    auto note = [&c](const std::string& text) {
        if (c.differences.size() < 50) c.differences.push_back(text);
    };
    for (const auto& [id, pa] : A.elements())
    {
        const PhysicalElement* pb = B.elementByGlobalId(pa.globalId);
        const std::string label = pa.globalId + " (" + (pa.ifcClass.empty() ? std::string("TSA") : pa.ifcClass) + " « " + pa.name + " »)";
        if (!pb)
        {
            note(label + " : absent après l'aller-retour");
            continue;
        }
        ++c.guidsPreserved;
        // Produit créé dans TSA (sans classe d'origine) : la classe lue est celle que TSA lui a donnée à l'export.
        if (pb->ifcClass == pa.ifcClass || pa.ifcClass.empty()) ++c.classesPreserved;
        else note(label + " : classe relue « " + pb->ifcClass + " »");
        if (pb->name == pa.name) ++c.namesPreserved;
        else note(label + " : nom relu « " + pb->name + " »");
        std::string missing;
        if (propertySetsKept(pa, *pb, &missing)) ++c.propertySetsPreserved;
        else note(label + " : propriété perdue ou modifiée « " + missing + " »");
        if (pb->status == pa.status) ++c.statusPreserved;
        else note(label + " : état pour le calcul modifié");
        if (pa.geometry && pa.geometry->valid())
        {
            if (!pb->geometry || !pb->geometry->valid())
            {
                note(label + " : géométrie perdue");
                continue;
            }
            ++c.geometryCompared;
            const auto ba = pa.geometry->bounds();
            const auto bb = pb->geometry->bounds();
            double deviation = 0.0;
            for (int k = 0; k < 6; ++k) deviation = std::max(deviation, std::abs(ba[k] - bb[k]));
            c.maxBoundsDeviation = std::max(c.maxBoundsDeviation, deviation);
            if (deviation <= tolerance) ++c.geometryWithinTolerance;
            else note(label + " : emprise décalée de " + std::to_string(deviation) + " m");
        }
    }
    c.nodesA = static_cast<int>(a.nodes().size());
    c.nodesB = static_cast<int>(b.nodes().size());
    c.membersA = static_cast<int>(a.beams().size() + a.columns().size() + a.trussMembers().size() + a.cables().size());
    c.membersB = static_cast<int>(b.beams().size() + b.columns().size() + b.trussMembers().size() + b.cables().size());
    c.surfacesA = static_cast<int>(a.slabs().size() + a.walls().size());
    c.surfacesB = static_cast<int>(b.slabs().size() + b.walls().size());
    c.foundationsA = static_cast<int>(a.foundations().size());
    c.foundationsB = static_cast<int>(b.foundations().size());
    return c;
}

} // namespace TSA::BIM::Ifc
