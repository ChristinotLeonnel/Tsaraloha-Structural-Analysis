#include "ProductTypes.h"

#include "../../Model/Model.h"

#include <cmath>
#include <iomanip>
#include <map>
#include <sstream>

namespace TSA::BIM
{

using TSA::Model::ElementKind;

namespace
{
enum class Family { None, Linear, Surface, Foundation };

/// Définition portée par un élément de calcul (ce qu'un type partage).
struct Definition
{
    Family family = Family::None;
    TSA::Model::Section section;
    TSA::Model::Material material;
    double thickness = 0.0;
};

Definition definitionOf(const TSA::Model::Model& m, const AnalyticalRef& r)
{
    Definition d;
    switch (r.kind)
    {
    case ElementKind::Beam:
        if (const auto* e = m.getBeam(r.id)) d = { Family::Linear, e->section(), e->material(), 0.0 };
        break;
    case ElementKind::Column:
        if (const auto* e = m.getColumn(r.id)) d = { Family::Linear, e->section(), e->material(), 0.0 };
        break;
    case ElementKind::TrussMember:
        if (const auto* e = m.getTrussMember(r.id)) d = { Family::Linear, e->section(), e->material(), 0.0 };
        break;
    case ElementKind::Slab:
        if (const auto* e = m.getSlab(r.id)) d = { Family::Surface, {}, e->material(), e->thickness() };
        break;
    case ElementKind::Wall:
        if (const auto* e = m.getWall(r.id)) d = { Family::Surface, {}, e->material(), e->thickness() };
        break;
    case ElementKind::Foundation:
        if (const auto* e = m.getFoundation(r.id)) d = { Family::Foundation, {}, e->material(), 0.0 };
        break;
    default:
        break;   // câbles, nœuds : hors des types de produits
    }
    return d;
}

std::string fmt(double v, int decimals)
{
    std::ostringstream o;
    o << std::fixed << std::setprecision(decimals) << v;
    return o.str();
}

bool sameSection(const TSA::Model::Section& a, const TSA::Model::Section& b)
{
    auto eq = [](double x, double y) { return std::abs(x - y) <= 1e-9; };
    return a.name == b.name && a.shape == b.shape && eq(a.width, b.width) && eq(a.height, b.height) && eq(a.diameter, b.diameter)
        && eq(a.tw, b.tw) && eq(a.tf, b.tf);
}

bool sameMaterial(const TSA::Model::Material& a, const TSA::Model::Material& b)
{
    return a.name == b.name && std::abs(a.E - b.E) <= 1e-6 * std::max(1.0, std::abs(a.E)) && std::abs(a.density - b.density) <= 1e-9
        && std::abs(a.nu - b.nu) <= 1e-12 && std::abs(a.fk - b.fk) <= 1e-6 * std::max(1.0, std::abs(a.fk));
}

std::string familyLabel(BimCategory c)
{
    switch (c)
    {
    case BimCategory::Beam: return "Poutre";
    case BimCategory::Column: return "Poteau";
    case BimCategory::Member: return "Barre";
    case BimCategory::Slab: return "Dalle";
    case BimCategory::Wall: return "Voile";
    case BimCategory::Footing: return "Semelle";
    case BimCategory::Pile: return "Pieu";
    case BimCategory::Plate: return "Plaque";
    default: return ifcEntity(c);
    }
}

/// Clé de regroupement : même catégorie et même définition → même type.
std::string signature(BimCategory c, const Definition& d)
{
    std::ostringstream k;
    k << categoryName(c) << '|' << static_cast<int>(d.family) << '|' << d.material.name << '|' << fmt(d.material.E, 0) << '|'
      << fmt(d.material.density, 3);
    if (d.family == Family::Linear)
        k << '|' << d.section.name << '|' << static_cast<int>(d.section.shape) << '|' << fmt(d.section.width, 6) << '|'
          << fmt(d.section.height, 6) << '|' << fmt(d.section.diameter, 6) << '|' << fmt(d.section.tw, 6) << '|' << fmt(d.section.tf, 6);
    if (d.family == Family::Surface) k << '|' << fmt(d.thickness, 6);
    return k.str();
}

std::string typeName(BimCategory c, const Definition& d)
{
    std::string n = familyLabel(c);
    if (d.family == Family::Linear) n += " " + d.section.name;
    if (d.family == Family::Surface) n += " ép. " + fmt(d.thickness, 2) + " m";
    return n + " · " + d.material.name;
}

/// Applique la définition d'un type à un élément de calcul ; true s'il a changé (notifié).
bool applyTo(TSA::Model::Model& m, const AnalyticalRef& r, const ProductType& t)
{
    const Definition d = definitionOf(m, r);
    if (d.family == Family::None) return false;
    const bool sectionDiffers = d.family == Family::Linear && !sameSection(d.section, t.section);
    const bool thicknessDiffers = d.family == Family::Surface && t.thickness > 0.0 && std::abs(d.thickness - t.thickness) > 1e-9;
    const bool materialDiffers = !sameMaterial(d.material, t.material);
    if (!sectionDiffers && !thicknessDiffers && !materialDiffers) return false;
    switch (r.kind)
    {
    case ElementKind::Beam:
        if (auto* e = m.getBeam(r.id)) { e->setSection(t.section); e->setMaterial(t.material); m.notifyBeamModified(r.id); }
        break;
    case ElementKind::Column:
        if (auto* e = m.getColumn(r.id)) { e->setSection(t.section); e->setMaterial(t.material); m.notifyColumnModified(r.id); }
        break;
    case ElementKind::TrussMember:
        if (auto* e = m.getTrussMember(r.id)) { e->setSection(t.section); e->setMaterial(t.material); m.notifyTrussMemberModified(r.id); }
        break;
    case ElementKind::Slab:
        if (auto* e = m.getSlab(r.id)) { if (t.thickness > 0.0) e->setThickness(t.thickness); e->setMaterial(t.material); m.notifySlabModified(r.id); }
        break;
    case ElementKind::Wall:
        if (auto* e = m.getWall(r.id)) { if (t.thickness > 0.0) e->setThickness(t.thickness); e->setMaterial(t.material); m.notifyWallModified(r.id); }
        break;
    case ElementKind::Foundation:
        if (auto* e = m.getFoundation(r.id)) { e->setMaterial(t.material); m.notifyFoundationModified(r.id); }
        break;
    default:
        return false;
    }
    return true;
}
} // namespace

int createTypesFromModel(TSA::Model::Model& model)
{
    // Produits à typer (copie : la synchronisation du BimModel peut avoir lieu pendant la boucle)
    std::vector<std::pair<int, std::pair<BimCategory, Definition>>> todo;
    for (const auto& [id, p] : model.bim().elements())
    {
        if (p.typeId != 0 || p.analytical.empty()) continue;
        const Definition d = definitionOf(model, p.analytical.front());
        if (d.family != Family::None) todo.push_back({ id, { p.category, d } });
    }
    auto& bim = model.bimForEdit();
    std::map<std::string, int> bySignature;
    for (const auto& [tid, t] : bim.types())
    {
        Definition d { t.thickness > 0.0 ? Family::Surface : Family::Linear, t.section, t.material, t.thickness };
        if (t.category == BimCategory::Footing || t.category == BimCategory::Pile) d.family = Family::Foundation;
        bySignature.emplace(signature(t.category, d), tid);
    }
    int created = 0;
    for (const auto& [productId, cd] : todo)
    {
        const auto& [category, d] = cd;
        const std::string key = signature(category, d);
        auto it = bySignature.find(key);
        if (it == bySignature.end())
        {
            ProductType t;
            t.category = category;
            t.name = typeName(category, d);
            t.section = d.section;
            t.material = d.material;
            t.thickness = d.family == Family::Surface ? d.thickness : 0.0;
            it = bySignature.emplace(key, bim.addType(t)).first;
            ++created;
        }
        bim.setProductType(productId, it->second);
    }
    return created;
}

int applyType(TSA::Model::Model& model, int typeId)
{
    const auto* t = model.bim().type(typeId);
    if (!t) return 0;
    const ProductType type = *t;   // copie : les notifications resynchronisent le BimModel
    std::vector<AnalyticalRef> refs;
    for (int pid : model.bim().occurrences(typeId))
        if (const auto* p = model.bim().element(pid)) refs.insert(refs.end(), p->analytical.begin(), p->analytical.end());
    int changed = 0;
    for (const auto& r : refs) changed += applyTo(model, r, type) ? 1 : 0;
    return changed;
}

int propagateFromOccurrence(TSA::Model::Model& model, int productId)
{
    const auto* p = model.bim().element(productId);
    if (!p || p->typeId == 0 || !model.bim().type(p->typeId)) return -1;
    const int typeId = p->typeId;
    Definition d;
    for (const auto& r : p->analytical)
        if ((d = definitionOf(model, r)).family != Family::None) break;
    if (d.family == Family::None) return -1;
    auto* t = model.bimForEdit().type(typeId);
    if (d.family == Family::Linear) t->section = d.section;
    if (d.family == Family::Surface) t->thickness = d.thickness;
    t->material = d.material;
    return applyType(model, typeId);
}

int completeTypeDefinitions(TSA::Model::Model& model, const std::vector<int>& typeIds)
{
    std::vector<std::pair<int, Definition>> found;
    for (int tid : typeIds)
    {
        if (!model.bim().type(tid)) continue;
        for (int pid : model.bim().occurrences(tid))
        {
            const auto* p = model.bim().element(pid);
            Definition d;
            for (const auto& r : p ? p->analytical : std::vector<AnalyticalRef> {})
                if ((d = definitionOf(model, r)).family != Family::None) break;
            if (d.family != Family::None) { found.push_back({ tid, d }); break; }
        }
    }
    for (const auto& [tid, d] : found)
    {
        auto* t = model.bimForEdit().type(tid);
        if (d.family == Family::Linear) t->section = d.section;
        if (d.family == Family::Surface) t->thickness = d.thickness;
        t->material = d.material;
        if (t->name.empty()) t->name = typeName(t->category, d);
    }
    return static_cast<int>(found.size());
}

std::string typeDeviation(const TSA::Model::Model& model, const PhysicalElement& product)
{
    const auto* t = model.bim().type(product.typeId);
    if (!t) return {};
    for (const auto& r : product.analytical)
    {
        const Definition d = definitionOf(model, r);
        if (d.family == Family::Linear && !sameSection(d.section, t->section))
            return "section " + d.section.name + " au lieu de " + t->section.name;
        if (d.family == Family::Surface && t->thickness > 0.0 && std::abs(d.thickness - t->thickness) > 1e-9)
            return "épaisseur " + fmt(d.thickness, 3) + " m au lieu de " + fmt(t->thickness, 3) + " m";
        if (d.family != Family::None && !sameMaterial(d.material, t->material))
            return "matériau " + d.material.name + " au lieu de " + t->material.name;
    }
    return {};
}

} // namespace TSA::BIM
