#pragma once

// =====================================================================================================================
// Filet de caractérisation Annuler / Rétablir (TSA Core, préparation de l'étape 2, 2026-10-26 ; suite « undo »,
// tests/test_undo_characterization.cpp). Même convention que tests/migration_dump.h.
//
// Outils de caractérisation Annuler / Rétablir, à figer AVANT la migration des appels Model::pushUndoState vers
// EditTransaction (migration non faite) :
//   - RecordingObserver : enregistre CHAQUE notification de TSA::Model::IModelObserver (39 méthodes, Model.h:51-110),
//     avec son identifiant, son ordre et la révision du modèle au moment de l'appel ;
//   - UndoProbe        : mesure d'une opération (entrées Annuler / Rétablir en delta SIGNÉ, révision, isModified,
//     nom de la dernière entrée, enregistrements EditRecord de l'entrée au sommet) ;
//   - fingerprint()    : empreinte textuelle déterministe du modèle (tri par identifiant, réels en %a, aucune adresse) ;
//   - CoalesceClock    : garde temporelle pour les vérifications de coalescence (fenêtre de 2 s non injectable) ;
//   - measureTransition: opération → Annuler → Rétablir (révisions, ModelEdited, entrées, EditRecord, empreintes).
// Corrige les trois faiblesses connues de l'ébauche précédente (agents/a6/characterization_draft.h) :
//   observateur incomplet (13 méthodes sur 39), coalescence dépendante de l'horloge sans garde, soustraction size_t
//   `undoCount() - undo0` qui déborde quand l'historique est tronqué (maxSteps 50, budget mémoire).
// =====================================================================================================================

#include "Model/Model.h"
#include "Model/ModelDiff.h"
#include "Grid/GridManager.h"
#include "UndoRedo/UndoManager.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace UndoChar
{

// ---------------------------------------------------------------------------------------------------------------------
// 1. Observateur enregistreur (toutes les méthodes de IModelObserver, Model.h:51-110)
// ---------------------------------------------------------------------------------------------------------------------
struct Event
{
    std::string kind;          ///< nom de la méthode sans « on » : "NodeModified", "ModelEdited", "ModelDiffApplied"…
    int id = -1;               ///< identifiant de l'objet, -1 si sans objet
    std::string payload;       ///< résumé du différentiel (ModelDiffApplied) ou des ids (DimensionsChanged)
    std::uint64_t revision = 0;///< Model::revision() au moment de l'appel (ordre relatif à bumpRevision)
};

inline std::string joinIds(const std::vector<int>& v)
{
    std::string s;
    for (size_t i = 0; i < v.size(); ++i) s += (i ? "," : "") + std::to_string(v[i]);
    return s;
}

/// Résumé canonique d'un ModelDiff : catégories non vides seulement, ids dans l'ordre fourni par ModelDiff::compute.
inline std::string summarize(const TSA::Model::ModelDiff& d)
{
    std::string s;
    auto cat = [&s](const char* name, const std::vector<int>& c, const std::vector<int>& m, const std::vector<int>& r) {
        if (c.empty() && m.empty() && r.empty()) return;
        s += std::string(name) + "{c:" + joinIds(c) + ";m:" + joinIds(m) + ";d:" + joinIds(r) + "}";
    };
    cat("N", d.createdNodeIds, d.modifiedNodeIds, d.deletedNodeIds);
    cat("B", d.createdBeamIds, d.modifiedBeamIds, d.deletedBeamIds);
    cat("C", d.createdColumnIds, d.modifiedColumnIds, d.deletedColumnIds);
    cat("S", d.createdSlabIds, d.modifiedSlabIds, d.deletedSlabIds);
    cat("W", d.createdWallIds, d.modifiedWallIds, d.deletedWallIds);
    cat("F", d.createdFoundationIds, d.modifiedFoundationIds, d.deletedFoundationIds);
    cat("T", d.createdTrussMemberIds, d.modifiedTrussMemberIds, d.deletedTrussMemberIds);
    cat("K", d.createdCableIds, d.modifiedCableIds, d.deletedCableIds);
    if (d.dimensionsChanged) s += "Dims";
    return s.empty() ? std::string("<vide>") : s;
}

class RecordingObserver : public TSA::Model::IModelObserver
{
public:
    explicit RecordingObserver(TSA::Model::Model* model = nullptr) : m_model(model)
    {
        if (m_model) m_model->addObserver(this);
    }
    ~RecordingObserver() override
    {
        if (m_model) m_model->removeObserver(this);
    }
    RecordingObserver(const RecordingObserver&) = delete;
    RecordingObserver& operator=(const RecordingObserver&) = delete;

    std::vector<Event> events;
    /// Injection d'exception (caractérisation du comportement actuel en cas d'échec) : levée au premier événement
    /// de ce type (et de cet id si throwOnId >= 0), une seule fois.
    std::string throwOnKind;
    int throwOnId = -1;

    void clear() { events.clear(); }

    int count(const std::string& kind) const
    {
        int n = 0;
        for (const auto& e : events) n += e.kind == kind ? 1 : 0;
        return n;
    }
    int count(const std::string& kind, int id) const
    {
        int n = 0;
        for (const auto& e : events) n += (e.kind == kind && e.id == id) ? 1 : 0;
        return n;
    }
    /// Séquence des notifications SPÉCIFIQUES (sans ModelEdited, dont la position change si pushUndoState devient une
    /// transaction : émise au push aujourd'hui, au commit après migration), ex. "NodeModified(3) NodeModified(4)".
    std::string specificSequence() const
    {
        std::string s;
        for (const auto& e : events)
        {
            if (e.kind == "ModelEdited") continue;
            if (!s.empty()) s += ' ';
            s += e.kind;
            if (e.id >= 0) s += "(" + std::to_string(e.id) + ")";
            if (!e.payload.empty()) s += "[" + e.payload + "]";
        }
        return s;
    }
    /// Séquence complète, ModelEdited compris (comportement actuel ; ordre NON contractuel, voir rapport §C1).
    std::string fullSequence() const
    {
        std::string s;
        for (const auto& e : events)
        {
            if (!s.empty()) s += ' ';
            s += e.kind;
            if (e.id >= 0) s += "(" + std::to_string(e.id) + ")";
        }
        return s;
    }

    // --- IModelObserver : 39 méthodes ---------------------------------------------------------------------------------
    void onNodeAdded(const TSA::Model::Node& n) override { rec("NodeAdded", n.id()); }
    void onNodeModified(const TSA::Model::Node& n) override { rec("NodeModified", n.id()); }
    void onNodeRemoved(int id) override { rec("NodeRemoved", id); }
    void onBeamAdded(const TSA::Model::Beam& b) override { rec("BeamAdded", b.id()); }
    void onBeamModified(const TSA::Model::Beam& b) override { rec("BeamModified", b.id()); }
    void onBeamRemoved(int id) override { rec("BeamRemoved", id); }
    void onColumnAdded(const TSA::Model::Column& c) override { rec("ColumnAdded", c.id()); }
    void onColumnModified(const TSA::Model::Column& c) override { rec("ColumnModified", c.id()); }
    void onColumnRemoved(int id) override { rec("ColumnRemoved", id); }
    void onSlabAdded(const TSA::Model::Slab& s) override { rec("SlabAdded", s.id()); }
    void onSlabModified(const TSA::Model::Slab& s) override { rec("SlabModified", s.id()); }
    void onSlabRemoved(int id) override { rec("SlabRemoved", id); }
    void onWallAdded(const TSA::Model::Wall& w) override { rec("WallAdded", w.id()); }
    void onWallModified(const TSA::Model::Wall& w) override { rec("WallModified", w.id()); }
    void onWallRemoved(int id) override { rec("WallRemoved", id); }
    void onFoundationAdded(const TSA::Model::Foundation& f) override { rec("FoundationAdded", f.id()); }
    void onFoundationModified(const TSA::Model::Foundation& f) override { rec("FoundationModified", f.id()); }
    void onFoundationRemoved(int id) override { rec("FoundationRemoved", id); }
    void onTrussMemberAdded(const TSA::Model::TrussMember& t) override { rec("TrussMemberAdded", t.id()); }
    void onTrussMemberModified(const TSA::Model::TrussMember& t) override { rec("TrussMemberModified", t.id()); }
    void onTrussMemberRemoved(int id) override { rec("TrussMemberRemoved", id); }
    void onCableAdded(const TSA::Model::Cable& c) override { rec("CableAdded", c.id()); }
    void onCableModified(const TSA::Model::Cable& c) override { rec("CableModified", c.id()); }
    void onCableRemoved(int id) override { rec("CableRemoved", id); }
    void onNodalLoadAdded(int id) override { rec("NodalLoadAdded", id); }
    void onNodalLoadModified(int id) override { rec("NodalLoadModified", id); }
    void onNodalLoadRemoved(int id) override { rec("NodalLoadRemoved", id); }
    void onMemberLoadAdded(int id) override { rec("MemberLoadAdded", id); }
    void onMemberLoadModified(int id) override { rec("MemberLoadModified", id); }
    void onMemberLoadRemoved(int id) override { rec("MemberLoadRemoved", id); }
    void onLoadAdded(int id) override { rec("LoadAdded", id); }
    void onLoadModified(int id) override { rec("LoadModified", id); }
    void onLoadRemoved(int id) override { rec("LoadRemoved", id); }
    void onLoadCaseChanged(int id) override { rec("LoadCaseChanged", id); }
    void onModelDiffApplied(const TSA::Model::ModelDiff& d) override { rec("ModelDiffApplied", -1, summarize(d)); }
    void onDimensionsChanged(const std::vector<int>& ids) override { rec("DimensionsChanged", -1, joinIds(ids)); }
    void onModelCleared() override { rec("ModelCleared", -1); }
    void onModelEdited() override { rec("ModelEdited", -1); }
    void onModelDestroyed() override
    {
        rec("ModelDestroyed", -1);
        m_model = nullptr; // contrat Model.h:106-110 : ne plus appeler removeObserver
    }

private:
    void rec(const char* kind, int id, std::string payload = {})
    {
        events.push_back(Event{ kind, id, std::move(payload), m_model ? m_model->revision() : 0 });
        if (!throwOnKind.empty() && throwOnKind == kind && (throwOnId < 0 || throwOnId == id))
        {
            throwOnKind.clear(); // une seule fois : les opérations suivantes (Undo) ne sont pas perturbées
            throw std::runtime_error(std::string("UndoChar : exception injectée sur ") + kind);
        }
    }
    TSA::Model::Model* m_model = nullptr;
};

// ---------------------------------------------------------------------------------------------------------------------
// 2. Sonde d'opération
// ---------------------------------------------------------------------------------------------------------------------
struct UndoProbe
{
    TSA::Model::Model& m;
    TSA::UndoRedo::UndoManager& um;
    RecordingObserver obs;
    long long undo0, redo0;
    std::uint64_t rev0;
    bool modified0;
    std::string top0;

    explicit UndoProbe(TSA::Model::Model& model)
        : m(model), um(*model.undoManager()), obs(&model),
          undo0(static_cast<long long>(um.undoCount())), redo0(static_cast<long long>(um.redoCount())),
          rev0(model.revision()), modified0(model.isModified()), top0(model.lastUndoActionName())
    {
        // Historique non tronqué pendant la mesure (sinon un delta nul peut cacher une entrée créée) : le banc
        // impose maxSteps élevé (voir prepareModel) ; une sonde ouverte près de la limite est une erreur du test.
        if (um.undoCount() + 8 >= um.maxSteps())
            throw std::logic_error("UndoProbe : historique proche de maxSteps, delta non fiable");
    }

    long long undoDelta() const { return static_cast<long long>(um.undoCount()) - undo0; } // signé : jamais de débordement
    long long redoDelta() const { return static_cast<long long>(um.redoCount()) - redo0; }
    long long revDelta() const { return static_cast<long long>(m.revision() - rev0); }       // révision jamais décrémentée
    std::string topName() const { return m.lastUndoActionName(); }
    size_t topRecords() const
    {
        const auto h = um.undoHistory();
        return h.empty() ? 0 : h.back().records.size();
    }
};

/// Modèle de test : historique large (aucune troncature pendant un test), grilles rattachées si fournies.
inline void prepareModel(TSA::Model::Model& m, TSA::Grid::GridManager* gm = nullptr)
{
    if (gm) m.setGridManager(gm);
    m.undoManager()->setMaxSteps(1000);
    m.undoManager()->setMemoryBudgetBytes(size_t(1) << 40);
    m.clearUndoRedo();
}

// ---------------------------------------------------------------------------------------------------------------------
// 3. Empreinte canonique
// ---------------------------------------------------------------------------------------------------------------------
struct FingerprintOptions
{
    /// false : identifiants volatils normalisés ou exclus (id de grille fondé sur l'horloge, GridDefinition.cpp:22 et
    /// GridManager.cpp:97/118 ; GlobalId BIM aléatoires) — pour comparer deux modèles construits séparément.
    bool includeVolatileIds = true;
    /// Couche BIM : bim() synchronise paresseusement le cache (Model_Snapshots.cpp:276-280) ; désactivée par défaut
    /// pour que la prise d'empreinte n'ait aucun effet observable.
    bool includeBim = false;
    /// État d'affichage des grilles (visible, libellés, intersections, active) : volontairement NON restauré par
    /// Annuler (Model_Snapshots.cpp:139-163) ; exclu par défaut pour comparer avant / après Annuler.
    bool includeGridDisplay = false;
    /// Compteurs d'identifiants (nextNodeId…) : restaurés par Annuler (Model_Snapshots.cpp:164-171).
    bool includeNextIds = true;
};

namespace detail
{
inline std::string d(double v)
{
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%a", v); // exact au bit près, indépendant de la locale
    return buf;
}
inline std::string s(const std::string& v)
{
    std::string out;
    for (unsigned char ch : v)
    {
        if (ch == '|' || ch == ';' || ch == '=' || ch == '\\' || ch < 0x20)
        {
            char buf[8];
            std::snprintf(buf, sizeof(buf), "\\x%02X", ch);
            out += buf;
        }
        else out += static_cast<char>(ch);
    }
    return out;
}
template <class E> int e(E v) { return static_cast<int>(v); }

struct Line
{
    std::ostringstream o;
    explicit Line(const std::string& head) { o << head; }
    Line& f(const char* k, const std::string& v) { o << '|' << k << '=' << s(v); return *this; }
    Line& f(const char* k, double v) { o << '|' << k << '=' << d(v); return *this; }
    Line& i(const char* k, long long v) { o << '|' << k << '=' << v; return *this; }
    Line& b(const char* k, bool v) { o << '|' << k << '=' << (v ? 1 : 0); return *this; }
    std::string str() const { return o.str() + "\n"; }
};

inline void section(Line& l, const char* p, const TSA::Model::Section& x)
{
    std::string k(p);
    l.i((k + "id").c_str(), x.id).f((k + "name").c_str(), x.name).i((k + "shape").c_str(), e(x.shape))
     .f((k + "w").c_str(), x.width).f((k + "h").c_str(), x.height).f((k + "D").c_str(), x.diameter)
     .f((k + "tw").c_str(), x.tw).f((k + "tf").c_str(), x.tf)
     .f((k + "r").c_str(), x.filletRadius ? *x.filletRadius : -1.0).b((k + "rSet").c_str(), x.filletRadius.has_value());
}
inline void material(Line& l, const char* p, const TSA::Model::Material& x)
{
    std::string k(p);
    l.i((k + "id").c_str(), x.id).f((k + "name").c_str(), x.name).i((k + "type").c_str(), e(x.type))
     .f((k + "E").c_str(), x.E).f((k + "nu").c_str(), x.nu).f((k + "rho").c_str(), x.density).f((k + "fk").c_str(), x.fk)
     .f((k + "alpha").c_str(), x.thermalCoeff)
     .f((k + "mE").c_str(), x.mechanical.youngModulus).f((k + "mNu").c_str(), x.mechanical.poissonRatio)
     .f((k + "mRho").c_str(), x.mechanical.density).f((k + "mFk").c_str(), x.mechanical.yieldStrength)
     .f((k + "mAlpha").c_str(), x.mechanical.thermalCoeff)
     .f((k + "vColor").c_str(), x.visual.baseColor).f((k + "vRough").c_str(), x.visual.roughness)
     .f((k + "vMetal").c_str(), x.visual.metallic).f((k + "vTransp").c_str(), x.visual.transparency)
     .f((k + "vShine").c_str(), x.visual.shininess).f((k + "vTex").c_str(), x.visual.textureName)
     .f((k + "vTexPath").c_str(), x.visual.texturePath).f((k + "vTexU").c_str(), x.visual.textureScaleU)
     .f((k + "vTexV").c_str(), x.visual.textureScaleV);
}
inline void support(Line& l, const TSA::Model::SupportDefinition& x)
{
    l.i("tx", e(x.tx())).i("ty", e(x.ty())).i("tz", e(x.tz())).i("rx", e(x.rx())).i("ry", e(x.ry())).i("rz", e(x.rz()))
     .f("kx", x.kx()).f("ky", x.ky()).f("kz", x.kz()).f("krx", x.krx()).f("kry", x.kry()).f("krz", x.krz())
     .i("orient", e(x.orientationType())).f("dx", x.customDirX()).f("dy", x.customDirY()).f("dz", x.customDirZ())
     .i("refElem", x.referenceElementId());
}
inline void openings(Line& l, const std::vector<TSA::Model::SlabOpening>& v)
{
    l.i("nOpen", static_cast<long long>(v.size()));
    for (size_t a = 0; a < v.size(); ++a)
    {
        std::string poly;
        for (const auto& [u, w] : v[a].polygon) poly += d(u) + "," + d(w) + " ";
        l.f(("open" + std::to_string(a)).c_str(), v[a].name + "/" + v[a].shape + "/" + poly);
    }
}
inline void edges(Line& l, const std::vector<TSA::Model::SurfaceEdgeSupport>& v)
{
    l.i("nEdge", static_cast<long long>(v.size()));
    for (size_t a = 0; a < v.size(); ++a)
    {
        std::string t = std::to_string(v[a].edge) + "/" + v[a].name + "/";
        for (int k = 0; k < 6; ++k) t += std::string(v[a].fixed[static_cast<size_t>(k)] ? "1" : "0") + ":" + d(v[a].stiffness[static_cast<size_t>(k)]) + " ";
        l.f(("edge" + std::to_string(a)).c_str(), t);
    }
}

/// Grilles : JSON de GridManager::serializeToJson (QJsonObject à clés triées, réels relus à l'identique,
/// GridManager.cpp:204-212), affichage retiré, ids normalisés si demandé.
inline std::string grids(const TSA::Grid::GridManager& gm, const FingerprintOptions& o)
{
    const QJsonObject root = QJsonDocument::fromJson(QByteArray::fromStdString(gm.serializeToJson())).object();
    QJsonArray out;
    std::map<QString, QString> rename;
    int index = 0;
    for (const auto& v : root.value("grids").toArray())
    {
        QJsonObject g = v.toObject();
        const QString id = g.value("id").toString();
        const QString canon = o.includeVolatileIds ? id : QStringLiteral("G%1").arg(index);
        rename[id] = canon;
        g["id"] = canon;
        if (!o.includeGridDisplay)
            for (const char* k : { "isVisible", "showLabels", "showIntersections", "isActive" }) g.remove(QLatin1String(k));
        out.append(g);
        ++index;
    }
    QJsonObject r{ { "grids", out } };
    if (o.includeGridDisplay)
    {
        const QString active = root.value("activeGridId").toString();
        r["activeGridId"] = rename.count(active) ? rename[active] : active;
    }
    return QJsonDocument(r).toJson(QJsonDocument::Compact).toStdString();
}
} // namespace detail

/// Empreinte textuelle déterministe : une ligne par objet, triée par identifiant (std::map), réels en %a.
/// Couverture et exclusions : voir rapport §C2. NE PRÉTEND PAS à l'égalité complète de deux modèles :
/// champs privés sans accesseur et réglages JSON hors historique exclus.
inline std::string fingerprint(const TSA::Model::Model& m, const TSA::Grid::GridManager* gm = nullptr,
                               const FingerprintOptions& o = {})
{
    using namespace detail;
    std::string out;
    for (const auto& [id, n] : m.nodes())
    {
        Line l("node|" + std::to_string(id));
        l.f("name", n.name()).f("x", n.x()).f("y", n.y()).f("z", n.z()).f("level", n.levelId()).f("color", n.color());
        support(l, n.support());
        out += l.str();
    }
    for (const auto& [id, b] : m.beams())
    {
        Line l("beam|" + std::to_string(id));
        l.i("n1", b.startNodeId()).i("n2", b.endNodeId()).f("name", b.name()).i("role", e(b.role())).f("rot", b.rotation())
         .i("ecc", e(b.eccentricity())).f("color", b.color());
        const auto& r1 = b.startRelease(); const auto& r2 = b.endRelease();
        l.f("rel", std::string() + char('0' + r1.fx) + char('0' + r1.fy) + char('0' + r1.fz) + char('0' + r1.mx) + char('0' + r1.my) + char('0' + r1.mz)
                   + "/" + char('0' + r2.fx) + char('0' + r2.fy) + char('0' + r2.fz) + char('0' + r2.mx) + char('0' + r2.my) + char('0' + r2.mz));
        section(l, "s.", b.section());
        material(l, "m.", b.material());
        out += l.str();
    }
    for (const auto& [id, c] : m.columns())
    {
        Line l("column|" + std::to_string(id));
        l.i("n1", c.startNodeId()).i("n2", c.endNodeId()).f("name", c.name()).f("rot", c.rotation()).f("color", c.color());
        section(l, "s.", c.section());
        material(l, "m.", c.material());
        out += l.str();
    }
    for (const auto& [id, sl] : m.slabs())
    {
        Line l("slab|" + std::to_string(id));
        l.f("nodes", joinIds(sl.nodeIds())).f("t", sl.thickness()).i("type", e(sl.slabType())).i("form", e(sl.formulation()))
         .f("name", sl.name()).f("color", sl.color());
        openings(l, sl.openings());
        edges(l, sl.edgeSupports());
        material(l, "m.", sl.material());
        out += l.str();
    }
    for (const auto& [id, w] : m.walls())
    {
        Line l("wall|" + std::to_string(id));
        l.i("n1", w.startNodeId()).i("n2", w.endNodeId()).f("h", w.height()).f("t", w.thickness()).f("off", w.offset())
         .i("form", e(w.formulation())).f("name", w.name()).f("color", w.color());
        openings(l, w.openings());
        edges(l, w.edgeSupports());
        material(l, "m.", w.material());
        out += l.str();
    }
    for (const auto& [id, f] : m.foundations())
    {
        Line l("foundation|" + std::to_string(id));
        l.i("node", f.nodeId()).i("type", e(f.foundationType())).f("A", f.widthA()).f("B", f.lengthB()).f("H", f.heightH())
         .f("qa", f.soilBearingCapacity()).f("name", f.name()).f("color", f.color());
        material(l, "m.", f.material());
        out += l.str();
    }
    for (const auto& [id, t] : m.trussMembers())
    {
        Line l("truss|" + std::to_string(id));
        l.i("n1", t.startNodeId()).i("n2", t.endNodeId()).i("role", e(t.role())).f("name", t.name()).f("color", t.color());
        section(l, "s.", t.section());
        material(l, "m.", t.material());
        out += l.str();
    }
    for (const auto& [id, k] : m.cables())
    {
        Line l("cable|" + std::to_string(id));
        const auto& def = k.definition();
        const auto& ap = k.analysisProperties();
        const auto& ps = k.prestress();
        l.i("n1", k.startNodeId()).i("n2", k.endNodeId()).i("type", e(k.type())).i("mode", e(k.geometryMode()))
         .f("sag", k.sag()).f("diam", k.diameter()).f("name", k.name()).f("color", k.color())
         .f("dName", def.name()).f("dStd", def.standardName()).f("dGrade", def.grade()).f("dDiam", def.nominalDiameter())
         .f("dArea", def.area()).f("dE", def.elasticModulus()).f("dT0", def.initialTension())
         .f("pT0", ps.initialTension).f("pEps0", ps.initialStrain).f("pSig", ps.stressAtTransfer).f("pSlip", ps.anchorageSlip)
         .f("pMu", ps.frictionCoeff).f("pK", ps.wobbleCoeff).f("pRelax", ps.relaxationLossRatio)
         .f("pEl", ps.elasticShorteningLoss).f("pTime", ps.timeDependentLosses)
         .b("aTens", ap.tensionOnly).b("aLarge", ap.largeDisplacement).b("aGeom", ap.geometricNonlinearity)
         .b("aSag", ap.sagEffectEnabled).f("aMinT", ap.minTensionThreshold)
         .i("anc1", e(k.startAnchor().type())).i("anc2", e(k.endAnchor().type()))
         .i("nPts", static_cast<long long>(k.intermediatePoints().size()));
        section(l, "s.", k.section());
        material(l, "m.", k.material());
        out += l.str();
    }

    // Charges (LoadManager::createSnapshot est const et sans effet)
    const auto ls = m.loadManager().createSnapshot();
    for (const auto& [id, c] : ls.loadCases)
        out += Line("case|" + std::to_string(id)).f("name", c.name()).i("cat", e(c.category())).f("desc", c.description())
                   .b("sw", c.isSelfWeightIncluded()).f("swF", c.selfWeightFactor()).i("tag", c.patternTag()).str();
    for (const auto& [id, c] : ls.combinations)
    {
        std::string factors;
        for (const auto& [caseId, factor] : c.caseFactors()) factors += std::to_string(caseId) + ":" + detail::d(factor) + " ";
        out += Line("combo|" + std::to_string(id)).f("name", c.name()).i("type", e(c.type())).f("factors", factors).str();
    }
    for (const auto& [id, n] : ls.nodalLoads)
        out += Line("nload|" + std::to_string(id)).i("node", n.nodeId()).i("case", n.loadCaseId()).f("name", n.name())
                   .f("fx", n.fx()).f("fy", n.fy()).f("fz", n.fz()).f("mx", n.mx()).f("my", n.my()).f("mz", n.mz())
                   .i("cs", e(n.coordSystem())).str();
    for (const auto& [id, ml] : ls.memberLoads)
        out += Line("mload|" + std::to_string(id)).i("elem", ml.elementId()).i("target", e(ml.targetType())).i("case", ml.loadCaseId())
                   .f("name", ml.name()).i("type", e(ml.type())).i("dir", e(ml.direction())).i("cs", e(ml.coordSystem()))
                   .f("q1", ml.q1()).f("q2", ml.q2()).f("x1", ml.x1()).f("x2", ml.x2()).b("rel", ml.isRelativePosition()).str();
    for (const auto& [id, sl] : ls.surfaceLoads)
        out += Line("sload|" + std::to_string(id)).i("kind", e(sl.targetKind)).i("surf", sl.surfaceId).i("case", sl.loadCaseId)
                   .f("name", sl.name).i("dist", e(sl.distribution)).i("dir", e(sl.direction)).f("v", sl.value).b("proj", sl.projected)
                   .f("u", sl.u).f("vv", sl.v).f("gU", sl.gradU).f("gV", sl.gradV).b("zone", sl.zone)
                   .f("zu0", sl.zoneU0).f("zv0", sl.zoneV0).f("zu1", sl.zoneU1).f("zv1", sl.zoneV1).str();
    out += Line("loads").i("active", ls.activeLoadCaseId).i("nextCase", ls.nextLoadCaseId).i("nextCombo", ls.nextCombinationId)
               .i("nextNodal", ls.nextNodalLoadId).i("nextMember", ls.nextMemberLoadId).i("nextSurface", ls.nextSurfaceLoadId).str();

    // Repère (axes, niveaux) : JSON de CoordinateSystem::serializeToJson (QJsonObject, clés triées) — ids de niveau
    // « lvl_N » issus d'un compteur (LevelManager.cpp:37) : déterministes à construction identique.
    if (m.coordinateSystem()) out += "coords|" + detail::s(m.coordinateSystem()->serializeToJson()) + "\n";
    if (gm) out += "grids|" + detail::s(detail::grids(*gm, o)) + "\n";

    // Annotations et calcul figé (hors F1–F6, inclus pour détecter un effet de bord)
    out += "dims|" + detail::s(m.dimensions().toJson()) + "\n";
    for (const auto& [key, ref] : m.calculationSnapshots()) out += "calcsnap|" + detail::s(key) + "\n";

    if (o.includeNextIds)
        out += Line("next").i("node", m.nextNodeId()).i("beam", m.nextBeamId()).i("column", m.nextColumnId()).i("slab", m.nextSlabId())
                   .i("wall", m.nextWallId()).i("foundation", m.nextFoundationId()).i("truss", m.nextTrussMemberId())
                   .i("cable", m.nextCableId()).str();
    if (o.includeBim && o.includeVolatileIds)
        out += "bim|" + detail::s(QJsonDocument(m.bim().toJson()).toJson(QJsonDocument::Compact).toStdString()) + "\n";
    return out;
}

/// Première ligne différente (diagnostic lisible dans un message [FAIL]).
inline std::string firstDifference(const std::string& a, const std::string& b)
{
    std::istringstream sa(a), sb(b);
    std::string la, lb;
    for (int line = 1;; ++line)
    {
        const bool ea = !std::getline(sa, la), eb = !std::getline(sb, lb);
        if (ea && eb) return {};
        if (ea || eb || la != lb)
            return "ligne " + std::to_string(line) + " : « " + (ea ? "<fin>" : la) + " » ≠ « " + (eb ? "<fin>" : lb) + " »";
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// 4. Garde temporelle de coalescence (UndoManager::COALESCE_WINDOW = 2 s, steady_clock, aucun point d'injection)
// ---------------------------------------------------------------------------------------------------------------------
struct CoalesceClock
{
    using Clock = std::chrono::steady_clock;
    Clock::time_point start = Clock::now();
    /// Vrai si l'intervalle mesuré reste nettement sous la fenêtre (marge 500 ms) : la vérification « coalescée »
    /// est alors déterministe ; sinon le test l'annonce [SKIP] au lieu d'échouer (poste surchargé).
    bool safelyInside() const
    {
        return Clock::now() - start < TSA::UndoRedo::UndoManager::COALESCE_WINDOW - std::chrono::milliseconds(500);
    }
};

// ---------------------------------------------------------------------------------------------------------------------
// 5. Transition opération → Annuler → Rétablir : révision, ModelEdited, entrées, EditRecord, empreintes
//    Contrat écrit (Model.h, revision()) : « augmente à chaque modification notifiée, jamais décrémentée, y compris par
//    Undo ». bumpRevision émet ModelEdited (Model.cpp:35-42) ; Annuler / Rétablir = restore → notifyModelDiffApplied
//    (UndoManager.cpp:99-108, Model_Snapshots.cpp:193-200) ; EditRecord recopiés entre les piles (UndoManager.cpp:117-143).
// ---------------------------------------------------------------------------------------------------------------------
struct Transition
{
    std::uint64_t r0 = 0, rOp = 0, rUndo = 0, rRedo = 0;
    long long entries = 0;                           ///< entrées Annuler créées par l'opération (delta signé)
    int editedOp = 0, editedUndo = 0, editedRedo = 0; ///< ModelEdited reçus pendant chaque étape
    std::string specificOp;                          ///< notifications spécifiques de l'opération
    std::vector<std::string> recordsOp, recordsRedo; ///< EditRecord::toText() de l'entrée au sommet
    std::string f0, fOp, fUndo, fRedo;
    bool undone = false, redone = false;
};

inline std::vector<std::string> topRecordTexts(const TSA::UndoRedo::UndoManager& um)
{
    std::vector<std::string> out;
    const auto h = um.undoHistory();
    if (!h.empty())
        for (const auto& r : h.back().records) out.push_back(r.toText());
    return out;
}

/// Mesure une opération ; si elle a créé une entrée Annuler, mesure aussi Annuler puis Rétablir.
template <class Op>
Transition measureTransition(TSA::Model::Model& m, const TSA::Grid::GridManager* gm, Op&& op)
{
    auto& um = *m.undoManager();
    Transition t;
    t.f0 = fingerprint(m, gm);
    t.r0 = m.revision();
    {
        RecordingObserver obs(&m);
        const long long u0 = static_cast<long long>(um.undoCount());
        op();
        t.entries = static_cast<long long>(um.undoCount()) - u0;
        t.editedOp = obs.count("ModelEdited");
        t.specificOp = obs.specificSequence();
    }
    t.rOp = m.revision();
    t.fOp = fingerprint(m, gm);
    if (t.entries <= 0) return t;
    t.recordsOp = topRecordTexts(um);
    {
        RecordingObserver obs(&m);
        t.undone = m.undo();
        t.editedUndo = obs.count("ModelEdited");
    }
    t.rUndo = m.revision();
    t.fUndo = fingerprint(m, gm);
    {
        RecordingObserver obs(&m);
        t.redone = m.redo();
        t.editedRedo = obs.count("ModelEdited");
    }
    t.rRedo = m.revision();
    t.fRedo = fingerprint(m, gm);
    t.recordsRedo = topRecordTexts(um);
    return t;
}

} // namespace UndoChar
