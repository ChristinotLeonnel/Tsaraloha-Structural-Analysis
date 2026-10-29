#include "SurfaceApi.h"

#include "../Analysis/Engine/AnalysisEngine.h"
#include "../BIM/Core/BimModel.h"
#include "../Mesh/MeshGeometry.h"
#include "../Model/Load/LoadManager.h"
#include "../Model/Model.h"

#include <QJsonArray>

namespace TSA::Api
{

using TSA::Model::SurfaceFormulation;
using TSA::Model::SurfaceTargetKind;

bool SurfaceCapabilities::supports(SurfaceFormulation f) const
{
    switch (f)
    {
    case SurfaceFormulation::Shell: return shell;
    case SurfaceFormulation::Plate: return plate;
    case SurfaceFormulation::Membrane: return membrane;
    default: return true;   // non structurale : jamais transmise, toujours acceptée
    }
}

namespace
{
std::string refOf(SurfaceTargetKind k, int id)
{
    return (k == SurfaceTargetKind::Slab ? "SL" : "W") + std::to_string(id);
}

std::string globalIdOf(const TSA::Model::Model& model, SurfaceTargetKind k, int id)
{
    const TSA::BIM::AnalyticalRef ref { k == SurfaceTargetKind::Slab ? TSA::Model::ElementKind::Slab : TSA::Model::ElementKind::Wall, id };
    const auto* p = model.bim().physicalOf(ref);
    return p ? p->globalId : std::string();
}

void fail(std::string* error, const std::string& msg)
{
    if (error) *error = msg;
}

QJsonArray polygonJson(const std::vector<std::pair<double, double>>& p)
{
    QJsonArray a;
    for (const auto& [u, v] : p) a.append(QJsonArray { u, v });
    return a;
}
} // namespace

std::vector<SurfaceInfo> SurfaceApi::list(const TSA::Model::Model& model)
{
    std::vector<SurfaceInfo> out;
    const auto& lm = model.loadManager();
    for (const auto& [id, s] : model.slabs())
    {
        SurfaceInfo i;
        i.kind = SurfaceTargetKind::Slab;
        i.id = id;
        i.ref = refOf(i.kind, id);
        i.globalId = globalIdOf(model, i.kind, id);
        i.name = s.name();
        i.formulation = s.formulation();
        i.thickness = s.thickness();
        i.material = s.material().name;
        i.grossArea = s.grossArea(model);
        i.netArea = s.area(model);
        i.openings = s.openings().size();
        i.edgeSupports = s.edgeSupports().size();
        i.loads = lm.surfaceLoadsForSurface(i.kind, id).size();
        out.push_back(i);
    }
    for (const auto& [id, w] : model.walls())
    {
        SurfaceInfo i;
        i.kind = SurfaceTargetKind::Wall;
        i.id = id;
        i.ref = refOf(i.kind, id);
        i.globalId = globalIdOf(model, i.kind, id);
        i.name = w.name();
        i.formulation = w.formulation();
        i.thickness = w.thickness();
        i.material = w.material().name;
        i.grossArea = w.grossArea(model);
        i.netArea = w.area(model);
        i.openings = w.openings().size();
        i.edgeSupports = w.edgeSupports().size();
        i.loads = lm.surfaceLoadsForSurface(i.kind, id).size();
        out.push_back(i);
    }
    return out;
}

SurfaceCapabilities SurfaceApi::capabilities(const TSA::Analysis::AnalysisEngine& engine)
{
    const auto c = engine.capabilities();
    SurfaceCapabilities s;
    s.engineId = engine.info().id;
    s.shell = c.supportsShell;
    s.plate = false;      // aucun moteur intégré : refusé avant calcul (OpenSeesMeshAdapter)
    s.membrane = false;   // éléments de membrane OpenSees à 2 DDL : incompatibles avec le modèle à 6 DDL
    s.modal = c.supportsShell && c.supportsModal;
    s.edgeSupports = c.supportsShell && c.supportsSprings;
    s.zoneLoads = c.supportsShell;
    s.pointLoads = c.supportsShell;
    s.thermal = false;
    return s;
}

QJsonObject SurfaceApi::toJson(const TSA::Model::Model& model)
{
    const auto geo = TSA::Mesh::MeshGeometry::extract(model);
    QJsonArray surfaces;
    for (const auto& i : list(model))
    {
        QJsonObject o { { "ref", QString::fromStdString(i.ref) },
                        { "id", i.id },
                        { "kind", i.kind == SurfaceTargetKind::Slab ? "slab" : "wall" },
                        { "globalId", QString::fromStdString(i.globalId) },
                        { "name", QString::fromStdString(i.name) },
                        { "formulation", TSA::Model::surfaceFormulationKey(i.formulation) },
                        { "thickness", i.thickness },
                        { "material", QString::fromStdString(i.material) },
                        { "grossArea", i.grossArea },
                        { "netArea", i.netArea } };
        if (const auto* sg = geo.surface(i.ref))
        {
            QJsonArray outer;
            for (const auto& p : sg->outer) outer.append(QJsonArray { p.x, p.y, p.z });
            o.insert("outer", outer);
            o.insert("normal", QJsonArray { sg->frame.N.x, sg->frame.N.y, sg->frame.N.z });
        }
        const std::vector<TSA::Model::SlabOpening>* openings = nullptr;
        const std::vector<TSA::Model::SurfaceEdgeSupport>* supports = nullptr;
        if (i.kind == SurfaceTargetKind::Slab)
        {
            const auto* s = model.getSlab(i.id);
            openings = &s->openings(), supports = &s->edgeSupports();
        }
        else
        {
            const auto* w = model.getWall(i.id);
            openings = &w->openings(), supports = &w->edgeSupports();
        }
        QJsonArray op, sp, ld;
        for (const auto& x : *openings) op.append(QJsonObject { { "name", QString::fromStdString(x.name) }, { "polygon", polygonJson(x.polygon) } });
        for (const auto& x : *supports)
        {
            QJsonArray fx, k;
            for (int d = 0; d < 6; ++d) fx.append(x.fixed[d]), k.append(x.stiffness[d]);
            sp.append(QJsonObject { { "edge", x.edge }, { "fixed", fx }, { "stiffness", k } });
        }
        for (const auto& l : model.loadManager().surfaceLoadsForSurface(i.kind, i.id))
            ld.append(QJsonObject { { "id", l.id },
                                    { "case", l.loadCaseId },
                                    { "distribution", l.distribution == TSA::Model::SurfaceLoadDistribution::Point ? "point" : "uniform" },
                                    { "direction", TSA::Model::surfaceLoadDirectionKey(l.direction) },
                                    { "value", l.value },
                                    { "gradU", l.gradU },
                                    { "gradV", l.gradV },
                                    { "zone", l.zone ? QJsonValue(QJsonArray { l.zoneU0, l.zoneV0, l.zoneU1, l.zoneV1 }) : QJsonValue() } });
        o.insert("openings", op);
        o.insert("edgeSupports", sp);
        o.insert("loads", ld);
        surfaces.append(o);
    }
    return QJsonObject { { "format", "tsa.surfaces/1" }, { "units", QJsonObject { { "length", "m" }, { "force", "kN" }, { "pressure", "kN/m2" } } },
                         { "surfaces", surfaces } };
}

bool SurfaceApi::addOpening(TSA::Model::Model& model, SurfaceTargetKind kind, int id, const std::vector<std::pair<double, double>>& polygon,
                            const std::string& name, std::string* error)
{
    TSA::Model::SlabOpening o;
    o.name = name;
    o.polygon = polygon;
    if (kind == SurfaceTargetKind::Slab)
    {
        auto* s = model.getSlab(id);
        if (!s) return fail(error, "dalle " + std::to_string(id) + " inexistante"), false;
        if (const auto msg = TSA::Mesh::validateSlabOpening(model, *s, polygon); !msg.empty()) return fail(error, msg), false;
        auto list = s->openings();
        list.push_back(o);
        s->setOpenings(list);
        return true;
    }
    auto* w = model.getWall(id);
    if (!w) return fail(error, "voile " + std::to_string(id) + " inexistant"), false;
    if (const auto msg = TSA::Mesh::validateWallOpening(model, *w, polygon); !msg.empty()) return fail(error, msg), false;
    auto list = w->openings();
    list.push_back(o);
    w->setOpenings(list);
    return true;
}

bool SurfaceApi::addEdgeSupport(TSA::Model::Model& model, SurfaceTargetKind kind, int id, const TSA::Model::SurfaceEdgeSupport& support, std::string* error)
{
    if (!support.any()) return fail(error, "appui sans DDL bloqué ni raideur"), false;
    for (int d = 0; d < 6; ++d)
        if (support.stiffness[d] < 0.0) return fail(error, "raideur négative"), false;
    if (kind == SurfaceTargetKind::Slab)
    {
        auto* s = model.getSlab(id);
        if (!s) return fail(error, "dalle " + std::to_string(id) + " inexistante"), false;
        if (support.edge < -1 || support.edge >= int(s->nodeIds().size())) return fail(error, "bord inexistant"), false;
        auto list = s->edgeSupports();
        list.push_back(support);
        s->setEdgeSupports(list);
        return true;
    }
    auto* w = model.getWall(id);
    if (!w) return fail(error, "voile " + std::to_string(id) + " inexistant"), false;
    if (support.edge < -1 || support.edge >= 4) return fail(error, "bord inexistant"), false;
    auto list = w->edgeSupports();
    list.push_back(support);
    w->setEdgeSupports(list);
    return true;
}

int SurfaceApi::addLoad(TSA::Model::Model& model, const TSA::Model::SurfaceLoad& load, std::string* error)
{
    const bool exists = load.targetKind == SurfaceTargetKind::Slab ? model.getSlab(load.surfaceId) != nullptr : model.getWall(load.surfaceId) != nullptr;
    if (!exists) return fail(error, "surface " + load.surfaceRef() + " inexistante"), 0;
    if (!model.loadManager().getLoadCase(load.loadCaseId)) return fail(error, "cas de charge " + std::to_string(load.loadCaseId) + " inexistant"), 0;
    if (load.zone && (load.zoneU1 <= load.zoneU0 || load.zoneV1 <= load.zoneV0)) return fail(error, "zone chargée vide"), 0;
    TSA::Model::SurfaceLoad l = load;
    l.id = 0;
    const int id = model.loadManager().addSurfaceLoad(l);
    model.notifySurfaceLoadsChanged();   // vues et résultats informés
    return id;
}

bool SurfaceApi::setFormulation(TSA::Model::Model& model, SurfaceTargetKind kind, int id, SurfaceFormulation f, std::string* error)
{
    if (kind == SurfaceTargetKind::Slab)
    {
        auto* s = model.getSlab(id);
        if (!s) return fail(error, "dalle " + std::to_string(id) + " inexistante"), false;
        s->setFormulation(f);
        return true;
    }
    auto* w = model.getWall(id);
    if (!w) return fail(error, "voile " + std::to_string(id) + " inexistant"), false;
    w->setFormulation(f);
    return true;
}

} // namespace TSA::Api
