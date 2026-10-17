#include "ElementCreation.h"

#include "../Coordinate/GeometryTolerance.h"
#include "../Model/Model.h"
#include "../UndoRedo/EditTransaction.h"

#include <gp_Vec.hxx>

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <set>
#include <sstream>

namespace TSA::Interaction
{

namespace
{
using TSA::Coordinate::GeometryTolerance;

std::string fmt(double v, int decimals = 3)
{
    std::ostringstream o;
    o << std::fixed << std::setprecision(decimals) << v;
    return o.str();
}

std::string fmtPoint(const gp_Pnt& p)
{
    return "(" + fmt(p.X()) + " ; " + fmt(p.Y()) + " ; " + fmt(p.Z()) + ")";
}

/// Élément linéaire existant, pour les contrôles de doublon et de connexion.
struct LinearItem
{
    std::string label;   ///< avec article : « la poutre B3 », « le poteau C2 »
    int start = -1;
    int end = -1;
};

std::vector<LinearItem> linearItems(const TSA::Model::Model& m, bool bars, bool truss, bool cables, bool walls)
{
    std::vector<LinearItem> items;
    if (bars)
    {
        for (const auto& [id, e] : m.beams())
            items.push_back({ (e.role() == TSA::Model::BarRole::Beam ? "la poutre B" : "la barre B") + std::to_string(id), e.startNodeId(), e.endNodeId() });
        for (const auto& [id, e] : m.columns())
            items.push_back({ "le poteau C" + std::to_string(id), e.startNodeId(), e.endNodeId() });
    }
    if (truss)
        for (const auto& [id, e] : m.trussMembers())
            items.push_back({ "la barre de treillis TR" + std::to_string(id), e.startNodeId(), e.endNodeId() });
    if (cables)
        for (const auto& [id, e] : m.cables())
            items.push_back({ "le câble CB" + std::to_string(id), e.startNodeId(), e.endNodeId() });
    if (walls)
        for (const auto& [id, e] : m.walls())
            items.push_back({ "le voile W" + std::to_string(id), e.startNodeId(), e.endNodeId() });
    return items;
}

bool nodePosition(const TSA::Model::Model& m, int id, gp_Pnt& out)
{
    const auto* n = m.getNode(id);
    if (!n) return false;
    out = gp_Pnt(n->x(), n->y(), n->z());
    return true;
}

/// Paramètres (s, t) des points les plus proches des segments [p1, p2] et [q1, q2].
void closestParameters(const gp_Pnt& p1, const gp_Pnt& p2, const gp_Pnt& q1, const gp_Pnt& q2, double& s, double& t)
{
    const gp_Vec d1(p1, p2), d2(q1, q2), r(q1, p1);
    const double a = d1.Dot(d1), e = d2.Dot(d2), f = d2.Dot(r);
    const double c = d1.Dot(r), b = d1.Dot(d2);
    const double denom = a * e - b * b;
    s = denom > 1e-18 ? std::clamp((b * f - c * e) / denom, 0.0, 1.0) : 0.0;
    t = e > 1e-18 ? (b * s + f) / e : 0.0;
    if (t < 0.0) { t = 0.0; s = a > 1e-18 ? std::clamp(-c / a, 0.0, 1.0) : 0.0; }
    else if (t > 1.0) { t = 1.0; s = a > 1e-18 ? std::clamp((b - c) / a, 0.0, 1.0) : 0.0; }
}

gp_Pnt lerp(const gp_Pnt& a, const gp_Pnt& b, double t)
{
    return gp_Pnt(a.X() + (b.X() - a.X()) * t, a.Y() + (b.Y() - a.Y()) * t, a.Z() + (b.Z() - a.Z()) * t);
}

/// Intersection propre (hors extrémités partagées) de deux segments d'un même plan, en 2D.
bool segmentsCross2d(double ax, double ay, double bx, double by, double cx, double cy, double dx, double dy)
{
    auto orient = [](double px, double py, double qx, double qy, double rx, double ry) {
        return (qx - px) * (ry - py) - (qy - py) * (rx - px);
    };
    const double eps = 1e-12;
    const double o1 = orient(ax, ay, bx, by, cx, cy), o2 = orient(ax, ay, bx, by, dx, dy);
    const double o3 = orient(cx, cy, dx, dy, ax, ay), o4 = orient(cx, cy, dx, dy, bx, by);
    if (((o1 > eps && o2 < -eps) || (o1 < -eps && o2 > eps)) && ((o3 > eps && o4 < -eps) || (o3 < -eps && o4 > eps)))
        return true;
    // Côtés colinéaires qui se recouvrent
    auto onSegment = [eps](double px, double py, double qx, double qy, double rx, double ry, double o) {
        return std::abs(o) <= eps && std::min(px, qx) - eps <= rx && rx <= std::max(px, qx) + eps
            && std::min(py, qy) - eps <= ry && ry <= std::max(py, qy) + eps;
    };
    return onSegment(ax, ay, bx, by, cx, cy, o1) || onSegment(ax, ay, bx, by, dx, dy, o2)
        || onSegment(cx, cy, dx, dy, ax, ay, o3) || onSegment(cx, cy, dx, dy, bx, by, o4);
}
} // namespace

int ElementCreation::existingNode(const DrawPoint& p) const
{
    if (p.nodeId > 0 && m_model.getNode(p.nodeId)) return p.nodeId;
    int best = -1;
    double bestDist = GeometryTolerance::nodeCoincidence;
    for (const auto& [id, n] : m_model.nodes())
    {
        const double d = p.point.Distance(gp_Pnt(n.x(), n.y(), n.z()));
        if (d <= bestDist)
        {
            best = id;
            bestDist = d;
        }
    }
    return best;
}

gp_Pnt ElementCreation::position(const DrawPoint& p) const
{
    gp_Pnt out = p.point;
    const int id = existingNode(p);
    if (id > 0) nodePosition(m_model, id, out);
    return out;
}

int ElementCreation::resolveNode(const DrawPoint& p, CreationResult& r)
{
    const int id = existingNode(p);
    if (id > 0) return id;
    const int created = m_model.addNode(p.point.X(), p.point.Y(), p.point.Z());
    if (created > 0) r.createdNodeIds.push_back(created);
    return created;
}

std::string ElementCreation::checkLinear(const DrawPoint& a, const DrawPoint& b) const
{
    const int na = existingNode(a), nb = existingNode(b);
    if ((na > 0 && na == nb) || position(a).Distance(position(b)) <= GeometryTolerance::nodeCoincidence)
        return "Les deux extrémités sont confondues : un élément de longueur nulle n'est pas créé.";
    return {};
}

std::string ElementCreation::checkDuplicateLinear(LinearFamily family, const DrawPoint& a, const DrawPoint& b) const
{
    const int na = existingNode(a), nb = existingNode(b);
    if (na <= 0 || nb <= 0) return {};   // une extrémité nouvelle : pas de doublon possible
    const auto items = linearItems(m_model, family == LinearFamily::Bar, family == LinearFamily::Truss,
                                   family == LinearFamily::Cable, family == LinearFamily::Wall);
    for (const auto& it : items)
        if ((it.start == na && it.end == nb) || (it.start == nb && it.end == na))
            return "N" + std::to_string(na) + " et N" + std::to_string(nb) + " sont déjà reliés par " + it.label
                + " : élément en double non créé.";
    return {};
}

std::vector<std::string> ElementCreation::unconnectedWarnings(const std::vector<DrawPoint>& points, bool segment) const
{
    std::vector<std::string> warnings;
    const double tol = GeometryTolerance::nodeCoincidence;
    const double interior = 1e-6;
    const auto items = linearItems(m_model, true, true, true, false);

    for (const auto& p : points)
    {
        if (existingNode(p) > 0) continue;   // nœud existant : connexion réelle
        for (const auto& it : items)
        {
            gp_Pnt s, e;
            if (!nodePosition(m_model, it.start, s) || !nodePosition(m_model, it.end, e)) continue;
            double u = 0.0, v = 0.0;
            closestParameters(p.point, p.point, s, e, u, v);
            if (v > interior && v < 1.0 - interior && lerp(s, e, v).Distance(p.point) <= tol)
                warnings.push_back("Le point " + fmtPoint(p.point) + " est sur " + it.label
                    + " sans nœud commun : aucune connexion n'est créée (outil Diviser ou Intersecter pour les relier).");
        }
    }

    if (segment && points.size() == 2)
    {
        const gp_Pnt a = position(points[0]), b = position(points[1]);
        const int na = existingNode(points[0]), nb = existingNode(points[1]);
        for (const auto& it : items)
        {
            if (it.start == na || it.start == nb || it.end == na || it.end == nb) continue;   // nœud partagé
            gp_Pnt s, e;
            if (!nodePosition(m_model, it.start, s) || !nodePosition(m_model, it.end, e)) continue;
            double u = 0.0, v = 0.0;
            closestParameters(a, b, s, e, u, v);
            const bool interiorBoth = u > interior && u < 1.0 - interior && v > interior && v < 1.0 - interior;
            if (interiorBoth && lerp(a, b, u).Distance(lerp(s, e, v)) <= tol)
                warnings.push_back("L'élément croise " + it.label + " en " + fmtPoint(lerp(a, b, u))
                    + " sans nœud commun : un croisement n'est pas une connexion (outil Intersecter pour les relier).");
        }
    }
    return warnings;
}

std::string ElementCreation::checkSlab(const std::vector<DrawPoint>& contour) const
{
    if (contour.size() < 3)
        return "Une dalle demande au moins 3 points de contour.";

    std::vector<gp_Pnt> pts;
    pts.reserve(contour.size());
    for (const auto& p : contour) pts.push_back(position(p));
    const double tol = GeometryTolerance::nodeCoincidence;

    for (size_t i = 0; i < pts.size(); ++i)
        for (size_t j = i + 1; j < pts.size(); ++j)
            if (pts[i].Distance(pts[j]) <= tol)
                return "Les points " + std::to_string(i + 1) + " et " + std::to_string(j + 1)
                    + " du contour sont confondus.";

    // Plan d'appui : trois points les plus écartés (indépendant du sens de parcours ; la normale de
    // Newell, elle, s'annule pour un contour croisé dont les deux boucles se compensent).
    const gp_Pnt& p0 = pts[0];
    size_t farthest = 1;
    for (size_t i = 2; i < pts.size(); ++i)
        if (p0.Distance(pts[i]) > p0.Distance(pts[farthest])) farthest = i;
    gp_Vec normal(0, 0, 0);
    for (size_t i = 1; i < pts.size(); ++i)
    {
        const gp_Vec c = gp_Vec(p0, pts[farthest]).Crossed(gp_Vec(p0, pts[i]));
        if (c.Magnitude() > normal.Magnitude()) normal = c;
    }
    if (normal.Magnitude() <= tol * p0.Distance(pts[farthest]))
        return "Les points du contour sont alignés : la dalle n'a pas de surface.";
    normal.Normalize();
    gp_Pnt centroid(0, 0, 0);
    for (const auto& p : pts) centroid.SetXYZ(centroid.XYZ() + p.XYZ());
    centroid.SetXYZ(centroid.XYZ() / static_cast<double>(pts.size()));

    double maxDev = 0.0;
    for (const auto& p : pts)
        maxDev = std::max(maxDev, std::abs(gp_Vec(centroid, p).Dot(normal)));
    if (maxDev > tol)
        return "Le contour n'est pas plan : un point s'écarte de " + fmt(maxDev) + " m du plan moyen (tolérance "
            + fmt(tol) + " m).";

    // Contour simple : aucun côté ne coupe un côté non adjacent (coordonnées dans le plan).
    gp_Vec ex = std::abs(normal.X()) < 0.9 ? gp_Vec(1, 0, 0) : gp_Vec(0, 1, 0);
    ex = ex - normal * ex.Dot(normal);
    ex.Normalize();
    const gp_Vec ey = normal.Crossed(ex);
    std::vector<std::pair<double, double>> uv;
    for (const auto& p : pts)
    {
        const gp_Vec d(centroid, p);
        uv.emplace_back(d.Dot(ex), d.Dot(ey));
    }
    const size_t n = uv.size();
    for (size_t i = 0; i < n; ++i)
        for (size_t j = i + 1; j < n; ++j)
        {
            const bool adjacent = j == i + 1 || (i == 0 && j == n - 1);
            if (adjacent) continue;
            const auto& [ax, ay] = uv[i];
            const auto& [bx, by] = uv[(i + 1) % n];
            const auto& [cx, cy] = uv[j];
            const auto& [dx, dy] = uv[(j + 1) % n];
            if (segmentsCross2d(ax, ay, bx, by, cx, cy, dx, dy))
                return "Le contour se recoupe (côtés " + std::to_string(i + 1) + " et " + std::to_string(j + 1)
                    + ") : tracez un polygone simple.";
        }

    // Doublon : une dalle existante sur exactement les mêmes nœuds.
    std::set<int> ids;
    for (const auto& p : contour)
    {
        const int id = existingNode(p);
        if (id <= 0) return {};
        ids.insert(id);
    }
    for (const auto& [id, s] : m_model.slabs())
        if (std::set<int>(s.nodeIds().begin(), s.nodeIds().end()) == ids)
            return "La dalle S" + std::to_string(id) + " occupe déjà ce contour : dalle en double non créée.";
    return {};
}

CreationResult ElementCreation::createNode(const DrawPoint& p)
{
    CreationResult r;
    if (const int id = existingNode(p); id > 0)
    {
        r.message = "Le nœud N" + std::to_string(id) + " existe déjà à cet emplacement : aucun nœud créé.";
        return r;
    }
    TSA::UndoRedo::EditTransaction tx(m_model, "Création Nœud");
    const int id = resolveNode(p, r);
    if (id <= 0)
    {
        tx.rollback();
        r.createdNodeIds.clear();
        r.message = "Le nœud n'a pas pu être créé.";
        return r;
    }
    tx.commit();
    r.success = true;
    r.elementId = id;
    r.nodeIds = { id };
    return r;
}

CreationResult ElementCreation::createBar(const DrawPoint& a, const DrawPoint& b, const TSA::Model::BarProperties& props,
                                          const CreationSetup& setup)
{
    CreationResult r;
    const bool column = props.role == TSA::Model::BarRole::Column;
    r.message = checkLinear(a, b);
    if (r.message.empty() && props.section.area() <= 0.0)
        r.message = "La section « " + props.section.name + " » n'a pas d'aire : choisissez une section valide.";
    if (r.message.empty() && props.material.E <= 0.0)
        r.message = "Le matériau « " + props.material.name + " » n'a pas de module d'élasticité : choisissez un matériau valide.";
    if (r.message.empty()) r.message = checkDuplicateLinear(LinearFamily::Bar, a, b);
    if (!r.message.empty()) return r;

    const auto warnings = unconnectedWarnings({ a, b }, true);
    const char* label = column ? "Création Poteau" : props.role == TSA::Model::BarRole::Beam ? "Création Poutre" : "Création Barre";
    TSA::UndoRedo::EditTransaction tx(m_model, label);
    const int s = resolveNode(a, r), e = resolveNode(b, r);
    int id = -1;
    if (s > 0 && e > 0)
    {
        if (column)
        {
            id = m_model.addColumn(s, e, props.section, props.material, props.rotation, props.name);
            if (auto* c = m_model.getColumn(id); c && !props.color.empty()) c->setColor(props.color);
        }
        else
        {
            id = m_model.addBar(props, s, e);
        }
    }
    if (id <= 0)
    {
        tx.rollback();
        r = {};
        r.message = "L'élément n'a pas pu être créé.";
        return r;
    }
    if (setup) setup(m_model, id);
    tx.commit();
    r.success = true;
    r.elementId = id;
    r.nodeIds = { s, e };
    r.warnings = warnings;
    return r;
}

CreationResult ElementCreation::createTrussMember(const DrawPoint& a, const DrawPoint& b, const TSA::Model::TrussPreset& preset,
                                                  const CreationSetup& setup)
{
    CreationResult r;
    r.message = checkLinear(a, b);
    if (r.message.empty() && preset.section.area() <= 0.0)
        r.message = "La section de treillis n'a pas d'aire : réglez-la dans les préréglages.";
    if (r.message.empty()) r.message = checkDuplicateLinear(LinearFamily::Truss, a, b);
    if (!r.message.empty()) return r;

    const auto warnings = unconnectedWarnings({ a, b }, true);
    TSA::UndoRedo::EditTransaction tx(m_model, "Création Barre de Treillis");
    const int s = resolveNode(a, r), e = resolveNode(b, r);
    const int id = (s > 0 && e > 0) ? m_model.addTrussMember(s, e, preset.section.diameter, "", preset.role) : -1;
    auto* member = m_model.getTrussMember(id);
    if (!member)
    {
        tx.rollback();
        r = {};
        r.message = "La barre de treillis n'a pas pu être créée.";
        return r;
    }
    member->setSection(preset.section);
    member->setMaterial(preset.material);
    if (!preset.color.empty()) member->setColor(preset.color);
    if (setup) setup(m_model, id);
    tx.commit();
    r.success = true;
    r.elementId = id;
    r.nodeIds = { s, e };
    r.warnings = warnings;
    return r;
}

CreationResult ElementCreation::createCable(const DrawPoint& a, const DrawPoint& b, TSA::Model::CableType type,
                                            const CreationSetup& setup)
{
    CreationResult r;
    r.message = checkLinear(a, b);
    if (r.message.empty()) r.message = checkDuplicateLinear(LinearFamily::Cable, a, b);
    if (!r.message.empty()) return r;

    const auto warnings = unconnectedWarnings({ a, b }, true);
    TSA::UndoRedo::EditTransaction tx(m_model, "Création Câble");
    const int s = resolveNode(a, r), e = resolveNode(b, r);
    const int id = (s > 0 && e > 0) ? m_model.addCable(s, e, type) : -1;
    if (id <= 0)
    {
        tx.rollback();
        r = {};
        r.message = "Le câble n'a pas pu être créé.";
        return r;
    }
    if (setup) setup(m_model, id);
    tx.commit();
    r.success = true;
    r.elementId = id;
    r.nodeIds = { s, e };
    r.warnings = warnings;
    return r;
}

CreationResult ElementCreation::createSlab(const std::vector<DrawPoint>& contour, const TSA::Model::SlabPreset& preset,
                                           const std::string& name, TSA::Model::SlabType type, const CreationSetup& setup)
{
    CreationResult r;
    r.message = preset.thickness > 0.0 ? checkSlab(contour) : "L'épaisseur de la dalle doit être positive.";
    if (!r.message.empty()) return r;

    const auto warnings = unconnectedWarnings(contour, false);
    TSA::UndoRedo::EditTransaction tx(m_model, "Création Dalle");
    std::vector<int> ids;
    for (const auto& p : contour)
    {
        const int id = resolveNode(p, r);
        if (id <= 0) break;
        ids.push_back(id);
    }
    const int slabId = ids.size() == contour.size() ? m_model.addSlab(ids, preset.thickness, name, type) : -1;
    auto* slab = m_model.getSlab(slabId);
    if (!slab)
    {
        tx.rollback();
        r = {};
        r.message = "La dalle n'a pas pu être créée.";
        return r;
    }
    slab->setMaterial(preset.material);
    if (!preset.color.empty()) slab->setColor(preset.color);
    if (setup) setup(m_model, slabId);
    tx.commit();
    r.success = true;
    r.elementId = slabId;
    r.nodeIds = ids;
    r.warnings = warnings;
    return r;
}

CreationResult ElementCreation::createWall(const DrawPoint& a, const DrawPoint& b, const TSA::Model::WallPreset& preset,
                                           const std::string& name, const CreationSetup& setup)
{
    CreationResult r;
    if (preset.height <= 0.0 || preset.thickness <= 0.0)
        r.message = "La hauteur et l'épaisseur du voile doivent être positives.";
    if (r.message.empty()) r.message = checkLinear(a, b);
    if (r.message.empty()) r.message = checkDuplicateLinear(LinearFamily::Wall, a, b);
    if (!r.message.empty()) return r;

    // Base inclinée : refusée. Le voile est extrudé verticalement depuis sa base ; sa surface de calcul
    // (TSA::Mesh::wallSurface) prend U le long de la base et V vertical : avec une pente, ce repère n'est plus
    // orthogonal (ouvertures, maillage et charges mal placés) et l'aire L × H est surestimée.
    const double dz = std::abs(position(a).Z() - position(b).Z());
    if (dz > GeometryTolerance::nodeCoincidence)
    {
        r.message = "Les deux points de base du voile ne sont pas à la même cote (écart de " + fmt(dz) + " m, tolérance "
            + fmt(GeometryTolerance::nodeCoincidence) + " m) : TSA ne sait pas encore calculer un voile à base inclinée. "
              "Tracez le voile entre deux points de même cote.";
        return r;
    }

    const auto warnings = unconnectedWarnings({ a, b }, false);
    TSA::UndoRedo::EditTransaction tx(m_model, "Création Voile");
    const int s = resolveNode(a, r), e = resolveNode(b, r);
    const int id = (s > 0 && e > 0) ? m_model.addWall(s, e, preset.height, preset.thickness, name) : -1;
    auto* wall = m_model.getWall(id);
    if (!wall)
    {
        tx.rollback();
        r = {};
        r.message = "Le voile n'a pas pu être créé.";
        return r;
    }
    wall->setOffset(preset.offset);
    wall->setMaterial(preset.material);
    if (!preset.color.empty()) wall->setColor(preset.color);
    if (setup) setup(m_model, id);
    tx.commit();
    r.success = true;
    r.elementId = id;
    r.nodeIds = { s, e };
    r.warnings = warnings;
    return r;
}

CreationResult ElementCreation::createFoundation(const DrawPoint& p, const TSA::Model::FoundationPreset& preset,
                                                 const CreationSetup& setup)
{
    CreationResult r;
    if (preset.widthA <= 0.0 || preset.lengthB <= 0.0 || preset.heightH <= 0.0)
    {
        r.message = "Les dimensions de la semelle doivent être positives (préréglages).";
        return r;
    }
    if (const int node = existingNode(p); node > 0)
        for (const auto& [id, f] : m_model.foundations())
            if (f.nodeId() == node)
            {
                r.message = "Le nœud N" + std::to_string(node) + " porte déjà la fondation F" + std::to_string(id)
                    + " : fondation en double non créée.";
                return r;
            }

    const auto warnings = unconnectedWarnings({ p }, false);
    TSA::UndoRedo::EditTransaction tx(m_model, "Création Fondation");
    const int node = resolveNode(p, r);
    const int id = node > 0 ? m_model.addFoundation(node, preset.widthA, preset.lengthB, preset.heightH, "", preset.type) : -1;
    auto* f = m_model.getFoundation(id);
    if (!f)
    {
        tx.rollback();
        r = {};
        r.message = "La fondation n'a pas pu être créée.";
        return r;
    }
    f->setMaterial(preset.material);
    if (!preset.color.empty()) f->setColor(preset.color);
    if (setup) setup(m_model, id);
    tx.commit();
    r.success = true;
    r.elementId = id;
    r.nodeIds = { node };
    r.warnings = warnings;
    return r;
}

} // namespace TSA::Interaction
