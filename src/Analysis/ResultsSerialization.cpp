// Enregistrement des résultats de calcul dans le .tsa (bloc RSLT, docs/PROJECT_STATE_PLAN.md §3.3, BUG-013).
//
// Disposition (petit-boutiste, comme le reste du .tsa) :
//   u32 magic 'TSAR' | u32 layoutVersion (1)
//   puis des sections { u32 tag, u64 longueur, octets } : une section inconnue (version future) est sautée.
// Tous les membres de ResultsModel sont écrits, sauf les données avancées (matrices de rigidité, vecteurs K·U = F :
// volumineuses et recalculables, décision D4) et le résumé (recalculé par computeSummary à la lecture : jamais une
// valeur figée qui contredirait les résultats relus).

#include "ResultsModel.h"

#include <cstring>
#include <limits>
#include <type_traits>

namespace TSA::Analysis
{
namespace
{
constexpr std::uint32_t kMagic = 0x52415354;   // 'TSAR'
constexpr std::uint32_t kLayoutVersion = 1;

constexpr std::uint32_t tag(char a, char b, char c, char d)
{
    return std::uint32_t(std::uint8_t(a)) | (std::uint32_t(std::uint8_t(b)) << 8) | (std::uint32_t(std::uint8_t(c)) << 16) |
           (std::uint32_t(std::uint8_t(d)) << 24);
}
constexpr std::uint32_t kMeta = tag('M', 'E', 'T', 'A'), kCurr = tag('C', 'U', 'R', 'R'), kFinl = tag('F', 'I', 'N', 'L'),
                        kStep = tag('S', 'T', 'E', 'P'), kShel = tag('S', 'H', 'E', 'L'), kModl = tag('M', 'O', 'D', 'L'),
                        kSurf = tag('S', 'U', 'R', 'F'), kMesh = tag('M', 'E', 'S', 'H'), kPlan = tag('P', 'L', 'A', 'N'),
                        kTabl = tag('T', 'A', 'B', 'L');

struct Writer
{
    std::string out;

    template <typename T> void pod(T v)
    {
        static_assert(std::is_trivially_copyable_v<T>);
        out.append(reinterpret_cast<const char*>(&v), sizeof(T));
    }
    void u8(bool v) { pod<std::uint8_t>(v ? 1 : 0); }
    void i32(int v) { pod<std::int32_t>(v); }
    void u32(std::size_t v) { pod<std::uint32_t>(static_cast<std::uint32_t>(v)); }
    void f64(double v) { pod<double>(v); }
    void str(const std::string& s)
    {
        u32(s.size());
        out.append(s);
    }
    void strings(const std::vector<std::string>& v)
    {
        u32(v.size());
        for (const auto& s : v) str(s);
    }
    void doubles(const double* p, std::size_t n)
    {
        for (std::size_t i = 0; i < n; ++i) f64(p[i]);
    }
    void key(const ElementKey& k)
    {
        i32(static_cast<int>(k.kind));
        i32(k.id);
    }
    /// Section : étiquette, longueur, contenu écrit par fill.
    template <typename F> void section(std::uint32_t t, F&& fill)
    {
        Writer inner;
        fill(inner);
        u32(t);
        pod<std::uint64_t>(inner.out.size());
        out += inner.out;
    }
};

struct Reader
{
    const char* p = nullptr;
    const char* end = nullptr;
    bool ok = true;

    std::size_t left() const { return ok ? std::size_t(end - p) : 0; }
    template <typename T> T pod()
    {
        T v {};
        if (!ok || left() < sizeof(T))
        {
            ok = false;
            return v;
        }
        std::memcpy(&v, p, sizeof(T));
        p += sizeof(T);
        return v;
    }
    bool u8() { return pod<std::uint8_t>() != 0; }
    int i32() { return pod<std::int32_t>(); }
    double f64() { return pod<double>(); }
    /// Nombre d'éléments, borné par les octets restants (minBytes par élément) : un fichier corrompu ne réserve jamais
    /// une mémoire démesurée.
    std::size_t count(std::size_t minBytes)
    {
        const std::size_t n = pod<std::uint32_t>();
        if (ok && minBytes > 0 && n > left() / minBytes) ok = false;
        return ok ? n : 0;
    }
    std::string str()
    {
        const std::size_t n = count(1);
        std::string s(ok ? n : 0, '\0');
        if (ok && n > 0)
        {
            std::memcpy(s.data(), p, n);
            p += n;
        }
        return s;
    }
    std::vector<std::string> strings()
    {
        std::vector<std::string> v(count(4));
        for (auto& s : v) s = str();
        return v;
    }
    void doubles(double* q, std::size_t n)
    {
        for (std::size_t i = 0; i < n; ++i) q[i] = f64();
    }
    ElementKey key()
    {
        ElementKey k;
        const int kind = i32();
        if (kind < 0 || kind > static_cast<int>(StructuralElementKind::Cable)) ok = false;
        k.kind = static_cast<StructuralElementKind>(kind);
        k.id = i32();
        return k;
    }
};

// --- Éléments de base --------------------------------------------------------------------------------------------
void put(Writer& w, const NodeDisplacement& d) { w.f64(d.ux), w.f64(d.uy), w.f64(d.uz), w.f64(d.rx), w.f64(d.ry), w.f64(d.rz); }
void get(Reader& r, NodeDisplacement& d) { d.ux = r.f64(), d.uy = r.f64(), d.uz = r.f64(), d.rx = r.f64(), d.ry = r.f64(), d.rz = r.f64(); }
void put(Writer& w, const NodeReaction& d) { w.f64(d.rx), w.f64(d.ry), w.f64(d.rz), w.f64(d.mx), w.f64(d.my), w.f64(d.mz); }
void get(Reader& r, NodeReaction& d) { d.rx = r.f64(), d.ry = r.f64(), d.rz = r.f64(), d.mx = r.f64(), d.my = r.f64(), d.mz = r.f64(); }

void put(Writer& w, const StationForces& s)
{
    for (double v : { s.position, s.N, s.Vy, s.Vz, s.Mx, s.My, s.Mz, s.ux, s.uy, s.uz, s.rx, s.ry, s.rz }) w.f64(v);
}
void get(Reader& r, StationForces& s)
{
    for (double* v : { &s.position, &s.N, &s.Vy, &s.Vz, &s.Mx, &s.My, &s.Mz, &s.ux, &s.uy, &s.uz, &s.rx, &s.ry, &s.rz }) *v = r.f64();
}
void put(Writer& w, const ElementResults& e)
{
    w.i32(e.elementId);
    w.i32(static_cast<int>(e.kind));
    w.i32(e.opsTag);
    w.f64(e.length);
    put(w, e.startForces);
    put(w, e.endForces);
    w.u32(e.intermediateStations.size());
    for (const auto& s : e.intermediateStations) put(w, s);
}
void get(Reader& r, ElementResults& e)
{
    e.elementId = r.i32();
    const int kind = r.i32();
    if (kind < 0 || kind > static_cast<int>(StructuralElementKind::Cable)) r.ok = false;
    e.kind = static_cast<StructuralElementKind>(kind);
    e.opsTag = r.i32();
    e.length = r.f64();
    get(r, e.startForces);
    get(r, e.endForces);
    e.intermediateStations.resize(r.count(13 * sizeof(double)));
    for (auto& s : e.intermediateStations) get(r, s);
}

template <typename V> void putIntMap(Writer& w, const std::map<int, V>& m)
{
    w.u32(m.size());
    for (const auto& [id, v] : m)
    {
        w.i32(id);
        put(w, v);
    }
}
template <typename V> void getIntMap(Reader& r, std::map<int, V>& m, std::size_t minBytes)
{
    m.clear();
    const std::size_t n = r.count(minBytes);
    for (std::size_t i = 0; i < n && r.ok; ++i)
    {
        const int id = r.i32();
        get(r, m[id]);
    }
}
void putElements(Writer& w, const std::map<ElementKey, ElementResults>& m)
{
    w.u32(m.size());
    for (const auto& [k, e] : m)
    {
        w.key(k);
        put(w, e);
    }
}
void getElements(Reader& r, std::map<ElementKey, ElementResults>& m)
{
    m.clear();
    const std::size_t n = r.count(8 + 20 + 26 * sizeof(double));
    for (std::size_t i = 0; i < n && r.ok; ++i)
    {
        const ElementKey k = r.key();
        get(r, m[k]);
    }
}

void put(Writer& w, const StepResults& s)
{
    w.i32(s.stepNumber);
    w.f64(s.factorOrTime);
    putIntMap(w, s.displacements);
    putIntMap(w, s.reactions);
    putElements(w, s.elementResults);
    putIntMap(w, s.surfaceDisplacements);
}
void get(Reader& r, StepResults& s)
{
    s.stepNumber = r.i32();
    s.factorOrTime = r.f64();
    getIntMap(r, s.displacements, 52);
    getIntMap(r, s.reactions, 52);
    getElements(r, s.elementResults);
    getIntMap(r, s.surfaceDisplacements, 52);
}

void put(Writer& w, const ShellElementResult& s)
{
    w.i32(s.tag);
    w.i32(s.meshElement);
    w.str(s.source);
    w.u32(s.gauss.size());
    for (const auto& g : s.gauss) w.doubles(g.data(), g.size());
    w.doubles(s.mean.data(), s.mean.size());
}
void get(Reader& r, ShellElementResult& s)
{
    s.tag = r.i32();
    s.meshElement = r.i32();
    s.source = r.str();
    s.gauss.resize(r.count(8 * sizeof(double)));
    for (auto& g : s.gauss) r.doubles(g.data(), g.size());
    r.doubles(s.mean.data(), s.mean.size());
}

void put(Writer& w, const PlanarMemberCurves& c)
{
    w.key(c.key);
    w.f64(c.length);
    w.u32(c.points.size());
    for (const auto& p : c.points)
        for (double v : { p.x, p.N, p.V, p.M, p.u, p.v }) w.f64(v);
    for (double v : { c.Mi, c.Mj }) w.f64(v);
    w.u8(c.hasSpanExtremum);
    for (double v : { c.xSpanExtremum, c.MSpanExtremum, c.Mmax, c.xMmax, c.Mmin, c.xMmin }) w.f64(v);
    w.u32(c.momentZeros.size());
    w.doubles(c.momentZeros.data(), c.momentZeros.size());
    for (double v : { c.Vi, c.Vj, c.Nmin, c.Nmax, c.deflectionMax, c.xDeflectionMax, c.rotationI, c.rotationJ }) w.f64(v);
}
void get(Reader& r, PlanarMemberCurves& c)
{
    c.key = r.key();
    c.length = r.f64();
    c.points.resize(r.count(6 * sizeof(double)));
    for (auto& p : c.points)
        for (double* v : { &p.x, &p.N, &p.V, &p.M, &p.u, &p.v }) *v = r.f64();
    for (double* v : { &c.Mi, &c.Mj }) *v = r.f64();
    c.hasSpanExtremum = r.u8();
    for (double* v : { &c.xSpanExtremum, &c.MSpanExtremum, &c.Mmax, &c.xMmax, &c.Mmin, &c.xMmin }) *v = r.f64();
    c.momentZeros.resize(r.count(sizeof(double)));
    r.doubles(c.momentZeros.data(), c.momentZeros.size());
    for (double* v : { &c.Vi, &c.Vj, &c.Nmin, &c.Nmax, &c.deflectionMax, &c.xDeflectionMax, &c.rotationI, &c.rotationJ }) *v = r.f64();
}

void put(Writer& w, const AnalysisExecutionMetadata& m)
{
    for (const std::string* s : { &m.solverEngine, &m.solverVersion, &m.engineId, &m.analysisScope, &m.analysisDimension,
                                  &m.calculationMethod, &m.nationalAnnex, &m.normativeFramework, &m.loadCombinationType })
        w.str(*s);
    w.f64(m.globalEquilibriumTolerance);
    w.f64(m.maxResidualForce);
    w.u8(m.isEquilibriumVerified);
    w.str(m.executionTimestamp);
    w.i32(m.totalNodes);
    w.i32(m.totalElements);
    for (double v : { m.executionDurationMs, m.scriptDurationMs, m.solveDurationMs, m.readDurationMs }) w.f64(v);
    for (const std::string* s : { &m.modelBuilder, &m.systemSolver, &m.constraintHandler, &m.numberer, &m.geomTransf }) w.str(*s);
    w.f64(m.relativeEquilibriumResidual);
    w.f64(m.relativeMomentResidual);
    w.i32(static_cast<int>(m.extractionLevel));
    w.strings(m.warnings);
    w.strings(m.preparationMessages);
}
void get(Reader& r, AnalysisExecutionMetadata& m)
{
    for (std::string* s : { &m.solverEngine, &m.solverVersion, &m.engineId, &m.analysisScope, &m.analysisDimension,
                            &m.calculationMethod, &m.nationalAnnex, &m.normativeFramework, &m.loadCombinationType })
        *s = r.str();
    m.globalEquilibriumTolerance = r.f64();
    m.maxResidualForce = r.f64();
    m.isEquilibriumVerified = r.u8();
    m.executionTimestamp = r.str();
    m.totalNodes = r.i32();
    m.totalElements = r.i32();
    for (double* v : { &m.executionDurationMs, &m.scriptDurationMs, &m.solveDurationMs, &m.readDurationMs }) *v = r.f64();
    for (std::string* s : { &m.modelBuilder, &m.systemSolver, &m.constraintHandler, &m.numberer, &m.geomTransf }) *s = r.str();
    m.relativeEquilibriumResidual = r.f64();
    m.relativeMomentResidual = r.f64();
    const int level = r.i32();
    // Les données avancées ne sont pas enregistrées (D4) : des résultats relus sont toujours du niveau « léger ».
    m.extractionLevel = ExtractionLevel::Light;
    (void)level;
    m.warnings = r.strings();
    m.preparationMessages = r.strings();
}

void put(Writer& w, const GlobalEquilibrium& e)
{
    for (double v : { e.appliedFx, e.appliedFy, e.appliedFz, e.reactionFx, e.reactionFy, e.reactionFz, e.appliedMx, e.appliedMy,
                      e.appliedMz, e.reactionMx, e.reactionMy, e.reactionMz, e.momentScale })
        w.f64(v);
}
void get(Reader& r, GlobalEquilibrium& e)
{
    for (double* v : { &e.appliedFx, &e.appliedFy, &e.appliedFz, &e.reactionFx, &e.reactionFy, &e.reactionFz, &e.appliedMx,
                       &e.appliedMy, &e.appliedMz, &e.reactionMx, &e.reactionMy, &e.reactionMz, &e.momentScale })
        *v = r.f64();
}
} // namespace

std::string ResultsModel::toBinary() const
{
    Writer w;
    w.pod<std::uint32_t>(kMagic);
    w.pod<std::uint32_t>(kLayoutVersion);
    w.section(kMeta, [&](Writer& s) {
        s.u8(m_isValid);
        s.i32(static_cast<int>(m_analysisType));
        s.str(m_caseOrComboName);
        s.str(m_timestamp);
        s.str(m_journalLog);
        s.i32(m_activeStep);
        for (const std::string* u : { &m_units.force, &m_units.length, &m_units.moment, &m_units.translationalStiffness,
                                      &m_units.rotationalStiffness, &m_units.stress })
            s.str(*u);
        // Disponibilité : les données avancées ne sont pas enregistrées (D4), elles ne sont donc jamais annoncées.
        for (bool b : { m_availability.displacements, m_availability.reactions, m_availability.elementForces, false, false,
                        false, m_availability.modes })
            s.u8(b);
        put(s, m_executionMetadata);
        put(s, m_equilibrium);
    });
    w.section(kCurr, [&](Writer& s) {
        putIntMap(s, m_displacements);
        putIntMap(s, m_reactions);
        putElements(s, m_elementResults);
    });
    w.section(kFinl, [&](Writer& s) {
        putIntMap(s, m_finalDisplacements);
        putIntMap(s, m_finalReactions);
        putElements(s, m_finalElementResults);
    });
    w.section(kStep, [&](Writer& s) {
        s.u32(m_stepResults.size());
        for (const auto& step : m_stepResults) put(s, step);
    });
    w.section(kShel, [&](Writer& s) {
        s.u32(m_shellResults.size());
        for (const auto& [id, sh] : m_shellResults)
        {
            s.i32(id);
            put(s, sh);
        }
    });
    w.section(kModl, [&](Writer& s) {
        s.u32(m_modal.modes.size());
        for (const auto& m : m_modal.modes)
        {
            s.i32(m.index);
            for (double v : { m.eigenvalue, m.omega, m.frequency, m.period }) s.f64(v);
            s.doubles(m.massRatio.data(), 6);
            s.doubles(m.cumulativeRatio.data(), 6);
        }
        s.doubles(m_modal.totalMass.data(), 6);
        s.doubles(m_modal.totalFreeMass.data(), 6);
        s.str(m_modal.massSource);
    });
    w.section(kSurf, [&](Writer& s) {
        putIntMap(s, m_surfaceNodeReactions);
        putIntMap(s, m_surfaceNodeDisplacements);
        putIntMap(s, m_finalSurfaceNodeDisplacements);
        s.str(m_surfaceMeshStamp);
        for (std::size_t v : { m_surfaceMeshSummary.quads, m_surfaceMeshSummary.triangles, m_surfaceMeshSummary.nodes })
            s.pod<std::uint64_t>(v);
        for (double v : { m_surfaceMeshSummary.minEdge, m_surfaceMeshSummary.maxEdge, m_surfaceMeshSummary.maxAspect,
                          m_surfaceMeshSummary.minAngle })
            s.f64(v);
    });
    w.section(kMesh, [&](Writer& s) {
        s.str(m_solverMesh.engineId);
        s.str(m_solverMesh.description);
        s.u32(m_solverMesh.nodes.size());
        for (const auto& [t, n] : m_solverMesh.nodes)
        {
            s.i32(t);
            s.i32(n.tag);
            s.f64(n.x), s.f64(n.y), s.f64(n.z);
            s.i32(n.tsaNodeId);
            s.str(n.role);
        }
        s.u32(m_solverMesh.cells.size());
        for (const auto& c : m_solverMesh.cells)
        {
            s.i32(c.tag);
            s.i32(static_cast<int>(c.type));
            s.str(c.solverClass);
            s.u32(c.nodes.size());
            for (int n : c.nodes) s.i32(n);
            s.u8(c.source.has_value());
            if (c.source) s.key(*c.source);
        }
    });
    w.section(kPlan, [&](Writer& s) {
        s.u32(m_planarCurves.size());
        for (const auto& [k, c] : m_planarCurves) put(s, c);
    });
    w.section(kTabl, [&](Writer& s) {
        s.u32(m_engineTables.size());
        for (const auto& t : m_engineTables)
        {
            s.str(t.engineId);
            s.str(t.title);
            s.strings(t.columns);
            s.u32(t.rows.size());
            for (const auto& row : t.rows) s.strings(row);
        }
    });
    return w.out;
}

bool ResultsModel::fromBinary(const std::string& bytes, std::string* error)
{
    auto fail = [&](const std::string& why) {
        if (error) *error = why;
        return false;
    };
    Reader head { bytes.data(), bytes.data() + bytes.size() };
    if (head.pod<std::uint32_t>() != kMagic || !head.ok)
        return fail("résultats enregistrés : signature inconnue");
    const std::uint32_t version = head.pod<std::uint32_t>();
    if (!head.ok || version == 0 || version > kLayoutVersion)
        return fail("résultats enregistrés : version de disposition " + std::to_string(version) + " inconnue");

    ResultsModel r;   // rempli à part : *this reste inchangé si la lecture échoue
    bool sawMeta = false;
    while (head.ok && head.left() > 0)
    {
        const std::uint32_t t = head.pod<std::uint32_t>();
        const std::uint64_t len = head.pod<std::uint64_t>();
        if (!head.ok || len > head.left())
            return fail("résultats enregistrés : section tronquée");
        Reader s { head.p, head.p + len };
        head.p += len;
        if (t == kMeta)
        {
            sawMeta = true;
            r.m_isValid = s.u8();
            const int type = s.i32();
            if (type < 0 || type > static_cast<int>(AnalysisType::Modal)) s.ok = false;
            r.m_analysisType = static_cast<AnalysisType>(type);
            r.m_caseOrComboName = s.str();
            r.m_timestamp = s.str();
            r.m_journalLog = s.str();
            r.m_activeStep = s.i32();
            for (std::string* u : { &r.m_units.force, &r.m_units.length, &r.m_units.moment, &r.m_units.translationalStiffness,
                                    &r.m_units.rotationalStiffness, &r.m_units.stress })
                *u = s.str();
            for (bool* b : { &r.m_availability.displacements, &r.m_availability.reactions, &r.m_availability.elementForces,
                             &r.m_availability.elementStiffness, &r.m_availability.globalStiffness, &r.m_availability.dofMapping,
                             &r.m_availability.modes })
                *b = s.u8();
            get(s, r.m_executionMetadata);
            get(s, r.m_equilibrium);
        }
        else if (t == kCurr)
        {
            getIntMap(s, r.m_displacements, 52);
            getIntMap(s, r.m_reactions, 52);
            getElements(s, r.m_elementResults);
        }
        else if (t == kFinl)
        {
            getIntMap(s, r.m_finalDisplacements, 52);
            getIntMap(s, r.m_finalReactions, 52);
            getElements(s, r.m_finalElementResults);
        }
        else if (t == kStep)
        {
            r.m_stepResults.resize(s.count(28));
            for (auto& step : r.m_stepResults) get(s, step);
        }
        else if (t == kShel)
        {
            const std::size_t n = s.count(16);
            for (std::size_t i = 0; i < n && s.ok; ++i)
            {
                const int id = s.i32();
                get(s, r.m_shellResults[id]);
            }
        }
        else if (t == kModl)
        {
            r.m_modal.modes.resize(s.count(4 + 16 * sizeof(double)));
            for (auto& m : r.m_modal.modes)
            {
                m.index = s.i32();
                for (double* v : { &m.eigenvalue, &m.omega, &m.frequency, &m.period }) *v = s.f64();
                s.doubles(m.massRatio.data(), 6);
                s.doubles(m.cumulativeRatio.data(), 6);
            }
            s.doubles(r.m_modal.totalMass.data(), 6);
            s.doubles(r.m_modal.totalFreeMass.data(), 6);
            r.m_modal.massSource = s.str();
        }
        else if (t == kSurf)
        {
            getIntMap(s, r.m_surfaceNodeReactions, 52);
            getIntMap(s, r.m_surfaceNodeDisplacements, 52);
            getIntMap(s, r.m_finalSurfaceNodeDisplacements, 52);
            r.m_surfaceMeshStamp = s.str();
            for (std::size_t* v : { &r.m_surfaceMeshSummary.quads, &r.m_surfaceMeshSummary.triangles, &r.m_surfaceMeshSummary.nodes })
                *v = static_cast<std::size_t>(s.pod<std::uint64_t>());
            for (double* v : { &r.m_surfaceMeshSummary.minEdge, &r.m_surfaceMeshSummary.maxEdge, &r.m_surfaceMeshSummary.maxAspect,
                               &r.m_surfaceMeshSummary.minAngle })
                *v = s.f64();
        }
        else if (t == kMesh)
        {
            r.m_solverMesh.engineId = s.str();
            r.m_solverMesh.description = s.str();
            const std::size_t nodes = s.count(36);
            for (std::size_t i = 0; i < nodes && s.ok; ++i)
            {
                const int key = s.i32();
                SolverMeshNode& n = r.m_solverMesh.nodes[key];
                n.tag = s.i32();
                n.x = s.f64(), n.y = s.f64(), n.z = s.f64();
                n.tsaNodeId = s.i32();
                n.role = s.str();
            }
            r.m_solverMesh.cells.resize(s.count(17));
            for (auto& c : r.m_solverMesh.cells)
            {
                c.tag = s.i32();
                const int type = s.i32();
                if (type < 0 || type > static_cast<int>(SolverCellType::Hexahedron)) s.ok = false;
                c.type = static_cast<SolverCellType>(type);
                c.solverClass = s.str();
                c.nodes.resize(s.count(4));
                for (int& n : c.nodes) n = s.i32();
                if (s.u8()) c.source = s.key();
            }
        }
        else if (t == kPlan)
        {
            const std::size_t n = s.count(8);
            for (std::size_t i = 0; i < n && s.ok; ++i)
            {
                PlanarMemberCurves c;
                get(s, c);
                r.m_planarCurves[c.key] = std::move(c);
            }
        }
        else if (t == kTabl)
        {
            r.m_engineTables.resize(s.count(16));
            for (auto& table : r.m_engineTables)
            {
                table.engineId = s.str();
                table.title = s.str();
                table.columns = s.strings();
                table.rows.resize(s.count(4));
                for (auto& row : table.rows) row = s.strings();
            }
        }
        // Section inconnue (version future du même layout) : sautée grâce à sa longueur.
        if (!s.ok)
            return fail("résultats enregistrés : section illisible");
    }
    if (!head.ok || !sawMeta)
        return fail("résultats enregistrés : contenu incomplet");
    r.computeSummary();
    *this = std::move(r);
    return true;
}

namespace
{
constexpr std::uint32_t kBlockMagic = 0x42525354;   // 'TSRB' : bloc RSLT (empreinte + résultats)
constexpr std::uint32_t kBlockVersion = 1;
} // namespace

std::string encodeStoredResults(const ResultsModel& results, const std::string& modelFingerprint)
{
    Writer w;
    w.pod<std::uint32_t>(kBlockMagic);
    w.pod<std::uint32_t>(kBlockVersion);
    w.str(modelFingerprint);
    w.out += results.toBinary();
    return w.out;
}

bool decodeStoredResults(const std::string& block, ResultsModel& results, std::string& modelFingerprint, std::string* error)
{
    Reader r { block.data(), block.data() + block.size() };
    const std::uint32_t magic = r.pod<std::uint32_t>();
    const std::uint32_t version = r.pod<std::uint32_t>();
    const std::string fingerprint = r.str();
    if (!r.ok || magic != kBlockMagic || version == 0 || version > kBlockVersion)
    {
        if (error) *error = "bloc de résultats : signature ou version inconnue";
        return false;
    }
    ResultsModel decoded;
    if (!decoded.fromBinary(std::string(r.p, r.end), error))
        return false;
    results = std::move(decoded);
    modelFingerprint = fingerprint;
    return true;
}

} // namespace TSA::Analysis
