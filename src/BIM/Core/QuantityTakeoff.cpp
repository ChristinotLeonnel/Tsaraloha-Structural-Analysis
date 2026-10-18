#include "QuantityTakeoff.h"

#include "../../Model/Model.h"

#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace TSA::BIM
{

using TSA::Model::ElementKind;

namespace
{
std::string fmt(double v, int decimals)
{
    std::ostringstream o;
    o << std::fixed << std::setprecision(decimals) << v;
    return o.str();
}

bool point(const TSA::Model::Model& m, int nodeId, gp_Pnt& out)
{
    const auto* n = m.getNode(nodeId);
    if (!n) return false;
    out = gp_Pnt(n->x(), n->y(), n->z());
    return true;
}

double distance(const TSA::Model::Model& m, int a, int b)
{
    gp_Pnt pa, pb;
    return point(m, a, pa) && point(m, b, pb) ? pa.Distance(pb) : 0.0;
}

double openingsArea(const std::vector<TSA::Model::SlabOpening>& openings)
{
    double a = 0.0;
    for (const auto& o : openings) a += std::abs(TSA::Model::polygonArea(o.polygon));
    return a;
}

/// Élément linéaire : longueur d'axe × aire de section.
ElementQuantities linear(const TSA::Model::Model& m, const AnalyticalRef& ref, const char* family, int a, int b,
                         const TSA::Model::Section& section, double sectionArea, const TSA::Model::Material& material)
{
    ElementQuantities q;
    q.ref = ref;
    q.family = family;
    q.material = material.name;
    q.section = section.name;
    q.length = distance(m, a, b);
    q.volume = q.length * sectionArea;
    q.mass = q.volume * material.density;
    return q;
}
} // namespace

void QuantityTotals::add(const ElementQuantities& q)
{
    ++count;
    length += q.length;
    area += q.area;
    volume += q.volume;
    mass += q.mass;
}

std::optional<ElementQuantities> elementQuantities(const TSA::Model::Model& m, const AnalyticalRef& ref)
{
    std::optional<ElementQuantities> out;
    switch (ref.kind)
    {
    case ElementKind::Beam:
        if (const auto* e = m.getBeam(ref.id))
            out = linear(m, ref, e->role() == TSA::Model::BarRole::Beam ? "Poutre" : "Barre", e->startNodeId(), e->endNodeId(),
                         e->section(), e->section().area(), e->material());
        break;
    case ElementKind::Column:
        if (const auto* e = m.getColumn(ref.id))
            out = linear(m, ref, "Poteau", e->startNodeId(), e->endNodeId(), e->section(), e->section().area(), e->material());
        break;
    case ElementKind::TrussMember:
        if (const auto* e = m.getTrussMember(ref.id))
            out = linear(m, ref, "Treillis", e->startNodeId(), e->endNodeId(), e->section(), e->section().area(), e->material());
        break;
    case ElementKind::Cable:
        if (const auto* e = m.getCable(ref.id))
            out = linear(m, ref, "Câble", e->startNodeId(), e->endNodeId(), e->section(), e->metallicArea(), e->material());
        break;
    case ElementKind::Slab:
        if (const auto* e = m.getSlab(ref.id))
        {
            // Aire du contour (normale de Newell, contour plan validé à la création).
            gp_Vec n(0, 0, 0);
            const auto& ids = e->nodeIds();
            for (size_t i = 0; i < ids.size(); ++i)
            {
                gp_Pnt c, d;
                if (!point(m, ids[i], c) || !point(m, ids[(i + 1) % ids.size()], d)) continue;
                n += gp_Vec((c.Y() - d.Y()) * (c.Z() + d.Z()), (c.Z() - d.Z()) * (c.X() + d.X()), (c.X() - d.X()) * (c.Y() + d.Y()));
            }
            ElementQuantities q;
            q.ref = ref;
            q.family = "Dalle";
            q.material = e->material().name;
            q.section = "ép. " + fmt(e->thickness(), 3) + " m";
            q.grossArea = n.Magnitude() / 2.0;
            q.openingArea = openingsArea(e->openings());
            q.area = std::max(0.0, q.grossArea - q.openingArea);
            q.volume = q.area * e->thickness();
            q.mass = q.volume * e->material().density;
            out = q;
        }
        break;
    case ElementKind::Wall:
        if (const auto* e = m.getWall(ref.id))
        {
            ElementQuantities q;
            q.ref = ref;
            q.family = "Voile";
            q.material = e->material().name;
            q.section = "ép. " + fmt(e->thickness(), 3) + " m";
            q.length = e->length(m);   // en plan : élévation en parallélogramme si la base est inclinée
            q.grossArea = q.length * e->height();
            q.openingArea = openingsArea(e->openings());
            q.area = std::max(0.0, q.grossArea - q.openingArea);
            q.volume = q.area * e->thickness();
            q.mass = q.volume * e->material().density;
            out = q;
        }
        break;
    case ElementKind::Foundation:
        if (const auto* e = m.getFoundation(ref.id))
        {
            ElementQuantities q;
            q.ref = ref;
            q.family = "Fondation";
            q.material = e->material().name;
            q.section = fmt(e->widthA(), 2) + " × " + fmt(e->lengthB(), 2) + " × " + fmt(e->heightH(), 2) + " m";
            q.grossArea = e->widthA() * e->lengthB();
            q.area = q.grossArea;
            q.volume = q.area * e->heightH();
            q.mass = q.volume * e->material().density;
            out = q;
        }
        break;
    default:
        break;
    }
    if (out) out->label = ref.label();
    return out;
}

QuantityTotals productQuantities(const TSA::Model::Model& m, const PhysicalElement& product)
{
    QuantityTotals t;
    for (const auto& ref : product.analytical)
        if (const auto q = elementQuantities(m, ref)) t.add(*q);
    return t;
}

QuantityReport takeoff(const TSA::Model::Model& m)
{
    QuantityReport r;
    auto collect = [&](ElementKind kind, const auto& map) {
        for (const auto& [id, e] : map)
        {
            (void)e;
            const AnalyticalRef ref { kind, id };
            const auto q = elementQuantities(m, ref);
            if (!q) continue;
            r.elements.push_back(*q);
            r.byMaterial[q->material].add(*q);
            r.bySection[q->section].add(*q);
            r.byFamily[q->family].add(*q);
            r.total.add(*q);
            if (const auto* p = m.bim().physicalOf(ref)) r.byProduct[p->id].add(*q);
        }
    };
    collect(ElementKind::Column, m.columns());
    collect(ElementKind::Beam, m.beams());
    collect(ElementKind::TrussMember, m.trussMembers());
    collect(ElementKind::Cable, m.cables());
    collect(ElementKind::Slab, m.slabs());
    collect(ElementKind::Wall, m.walls());
    collect(ElementKind::Foundation, m.foundations());
    return r;
}

} // namespace TSA::BIM
