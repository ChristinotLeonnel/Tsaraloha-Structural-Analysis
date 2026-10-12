#include "BenchmarkSuite.h"

#include "../Analysis/Engine/AnalysisEngineRegistry.h"
#include "../Analysis/Engine/AnalysisManager.h"
#include "../Analysis/OpenSeesManager.h"
#include "../App/ProductInfo.h"
#include "../Grid/GridManager.h"
#include "../Mesh/MeshGenerator.h"
#include "../Mesh/MeshGeometry.h"
#include "../Mesh/Solvers/OpenSeesMeshAdapter.h"
#include "../Model/Load/LoadManager.h"
#include "../Model/Model.h"

#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSysInfo>
#include <QTextStream>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <functional>
#include <limits>
#include <sstream>

namespace TSA::Validation
{

// =====================================================================================================================
// Expressions
// =====================================================================================================================
namespace
{
class Parser
{
public:
    Parser(const std::string& s, const std::map<std::string, double>& p) : m_s(s), m_p(p) {}
    double parse()
    {
        const double v = sum();
        skip();
        if (m_i != m_s.size()) fail("caractère inattendu « " + std::string(1, m_s[m_i]) + " »");
        return v;
    }
    std::string error;

private:
    void fail(const std::string& e)
    {
        if (error.empty()) error = e;
        m_i = m_s.size();
    }
    void skip()
    {
        while (m_i < m_s.size() && std::isspace(static_cast<unsigned char>(m_s[m_i]))) ++m_i;
    }
    bool eat(char c)
    {
        skip();
        if (m_i < m_s.size() && m_s[m_i] == c) return ++m_i, true;
        return false;
    }
    double sum()
    {
        double v = product();
        for (;;)
        {
            if (eat('+')) v += product();
            else if (eat('-')) v -= product();
            else return v;
        }
    }
    double product()
    {
        double v = unary();
        for (;;)
        {
            if (eat('*')) v *= unary();
            else if (eat('/')) v /= unary();
            else return v;
        }
    }
    double unary()
    {
        if (eat('-')) return -unary();
        if (eat('+')) return unary();
        return power();
    }
    double power()
    {
        const double b = atom();
        if (eat('^')) return std::pow(b, unary());   // associatif à droite, -x^2 = -(x^2)
        return b;
    }
    double atom()
    {
        skip();
        if (eat('('))
        {
            const double v = sum();
            if (!eat(')')) fail("parenthèse fermante attendue");
            return v;
        }
        if (m_i < m_s.size() && (std::isdigit(static_cast<unsigned char>(m_s[m_i])) || m_s[m_i] == '.'))
        {
            std::size_t used = 0;
            const double v = std::stod(m_s.substr(m_i), &used);
            m_i += used;
            return v;
        }
        std::string name;
        while (m_i < m_s.size() && (std::isalnum(static_cast<unsigned char>(m_s[m_i])) || m_s[m_i] == '_')) name += m_s[m_i++];
        if (name.empty())
        {
            fail("opérande attendu");
            return 0.0;
        }
        if (eat('('))
        {
            const double a = sum();
            if (!eat(')')) fail("parenthèse fermante attendue");
            static const std::map<std::string, std::function<double(double)>> fns = {
                { "sqrt", [](double x) { return std::sqrt(x); } }, { "abs", [](double x) { return std::abs(x); } },
                { "sin", [](double x) { return std::sin(x); } },   { "cos", [](double x) { return std::cos(x); } },
                { "tan", [](double x) { return std::tan(x); } },   { "ln", [](double x) { return std::log(x); } },
                { "exp", [](double x) { return std::exp(x); } },     { "tanh", [](double x) { return std::tanh(x); } },
            };
            auto it = fns.find(name);
            if (it == fns.end()) fail("fonction inconnue « " + name + " »");
            return it == fns.end() ? 0.0 : it->second(a);
        }
        if (name == "pi") return 3.14159265358979323846;
        auto it = m_p.find(name);
        if (it == m_p.end())
        {
            fail("paramètre inconnu « " + name + " »");
            return 0.0;
        }
        return it->second;
    }
    const std::string& m_s;
    const std::map<std::string, double>& m_p;
    std::size_t m_i = 0;
};
} // namespace

double evaluateExpression(const std::string& expression, const std::map<std::string, double>& parameters, std::string* error)
{
    Parser p(expression, parameters);
    const double v = p.parse();
    if (error) *error = p.error;
    return p.error.empty() ? v : std::numeric_limits<double>::quiet_NaN();
}

bool Tolerance::accepts(double value, double reference) const
{
    return std::isfinite(value) && std::abs(value - reference) <= abs + rel * std::abs(reference);
}

bool CaseOutcome::passed() const
{
    if (!ran) return false;
    return std::all_of(checks.begin(), checks.end(), [](const CheckOutcome& c) { return c.passed; });
}

int BenchmarkReport::checksPassed() const
{
    int n = 0;
    for (const auto& c : cases)
        for (const auto& k : c.checks) n += k.passed;
    return n;
}
int BenchmarkReport::checksFailed() const
{
    int n = 0;
    for (const auto& c : cases)
        for (const auto& k : c.checks) n += k.available && !k.passed;
    return n;
}
int BenchmarkReport::checksUnavailable() const
{
    int n = 0;
    for (const auto& c : cases)
        for (const auto& k : c.checks) n += !k.available;
    return n;
}
int BenchmarkReport::casesSkipped() const
{
    int n = 0;
    for (const auto& c : cases) n += c.skipped;
    return n;
}

// =====================================================================================================================
// Lecture
// =====================================================================================================================
namespace
{
std::string str(const QJsonValue& v) { return v.toString().toStdString(); }

std::vector<std::string> strings(const QJsonValue& v)
{
    std::vector<std::string> out;
    for (const auto& x : v.toArray()) out.push_back(str(x));
    return out;
}

bool knownQuantity(const std::string& q)
{
    static const char* prefixes[] = { "node:", "reaction:", "reactionsum:", "member:", "surface:", "modal:" };
    return std::any_of(std::begin(prefixes), std::end(prefixes), [&](const char* p) { return q.rfind(p, 0) == 0; });
}

Tolerance readTolerance(const QJsonValue& v, Tolerance def)
{
    const QJsonObject o = v.toObject();
    if (o.contains("rel")) def.rel = o.value("rel").toDouble();
    if (o.contains("abs")) def.abs = o.value("abs").toDouble();
    return def;
}
} // namespace

bool parseBenchmark(const QJsonObject& json, BenchmarkCase* out, std::vector<std::string>* errors)
{
    std::vector<std::string> err;
    BenchmarkCase c;
    if (json.value("format").toString() != "tsa.benchmark/1") err.push_back("format attendu « tsa.benchmark/1 »");
    c.id = str(json.value("id"));
    c.title = str(json.value("title"));
    c.category = str(json.value("category"));
    c.description = str(json.value("description"));
    c.sources = strings(json.value("sources"));
    c.hypotheses = strings(json.value("hypotheses"));
    c.engines = strings(json.value("engines"));
    if (c.engines.empty()) c.engines = { "opensees" };
    if (c.id.empty()) err.push_back("champ « id » obligatoire");
    if (c.title.empty()) err.push_back(c.id + " : champ « title » obligatoire");
    if (c.sources.empty()) err.push_back(c.id + " : au moins une source documentaire (« sources ») est obligatoire");
    const QJsonObject params = json.value("parameters").toObject();
    for (auto it = params.begin(); it != params.end(); ++it)
    {
        if (it.value().isDouble()) c.parameters[it.key().toStdString()] = it.value().toDouble();
        else if (it.value().isString())
        {
            // Paramètre dérivé (ex. I = b*h^3/12) : évalué dans l'ordre alphabétique puis réévalué jusqu'à stabilité
            continue;
        }
        else err.push_back(c.id + " : paramètre « " + it.key().toStdString() + " » numérique ou formule attendu");
    }
    for (int pass = 0; pass < 8; ++pass)
        for (auto it = params.begin(); it != params.end(); ++it)
            if (it.value().isString())
            {
                std::string e;
                const double v = evaluateExpression(str(it.value()), c.parameters, &e);
                if (e.empty()) c.parameters[it.key().toStdString()] = v;
                else if (pass == 7) err.push_back(c.id + " : paramètre « " + it.key().toStdString() + " » : " + e);
            }
    c.model = json.value("model").toObject();
    c.analysis = json.value("analysis").toObject();
    if (c.model.isEmpty()) err.push_back(c.id + " : « model » obligatoire");
    const Tolerance def = readTolerance(json.value("tolerance"), Tolerance {});
    for (const auto& v : json.value("checks").toArray())
    {
        const QJsonObject o = v.toObject();
        BenchmarkCheck k;
        k.quantity = str(o.value("quantity"));
        k.label = str(o.value("label"));
        k.unit = str(o.value("unit"));
        k.tolerance = readTolerance(o.value("tolerance"), def);
        k.engines = strings(o.value("engines"));
        if (!knownQuantity(k.quantity)) err.push_back(c.id + " : grandeur inconnue « " + k.quantity + " »");
        const QJsonValue e = o.value("expected");
        if (e.isDouble()) k.expected = e.toDouble();
        else
        {
            k.formula = str(e);
            std::string pe;
            k.expected = evaluateExpression(k.formula, c.parameters, &pe);
            if (!pe.empty()) err.push_back(c.id + " : formule « " + k.formula + " » : " + pe);
        }
        c.checks.push_back(k);
    }
    if (c.checks.empty()) err.push_back(c.id + " : aucune vérification (« checks »)");
    if (errors) errors->insert(errors->end(), err.begin(), err.end());
    if (out) *out = c;
    return err.empty();
}

bool loadBenchmarkFile(const QString& path, BenchmarkCase* out, std::vector<std::string>* errors)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
    {
        if (errors) errors->push_back("fichier illisible : " + path.toStdString());
        return false;
    }
    QJsonParseError pe;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &pe);
    if (pe.error != QJsonParseError::NoError)
    {
        if (errors) errors->push_back(path.toStdString() + " : JSON invalide (" + pe.errorString().toStdString() + ", position " + std::to_string(pe.offset) + ")");
        return false;
    }
    const bool ok = parseBenchmark(doc.object(), out, errors);
    if (out) out->file = path.toStdString();
    return ok;
}

std::vector<QString> benchmarkFiles(const QString& directory)
{
    std::vector<QString> out;
    QDirIterator it(directory, { "*.json" }, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) out.push_back(it.next());
    std::sort(out.begin(), out.end());
    return out;
}

std::vector<ExternalReference> loadExternalReferences(const QString& path, std::vector<std::string>* errors)
{
    std::vector<ExternalReference> out;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        if (errors) errors->push_back("références illisibles : " + path.toStdString());
        return out;
    }
    if (path.endsWith(".json", Qt::CaseInsensitive))
    {
        const QJsonObject root = QJsonDocument::fromJson(f.readAll()).object();
        if (root.value("format").toString() != "tsa.reference/1")
        {
            if (errors) errors->push_back(path.toStdString() + " : format attendu « tsa.reference/1 »");
            return out;
        }
        for (const auto& v : root.value("references").toArray())
        {
            const QJsonObject o = v.toObject();
            ExternalReference r;
            r.caseId = str(o.value("case")), r.quantity = str(o.value("quantity")), r.unit = str(o.value("unit"));
            r.source = o.contains("source") ? str(o.value("source")) : str(root.value("source"));
            r.value = o.value("value").toDouble(std::numeric_limits<double>::quiet_NaN());
            if (o.contains("tolerance")) r.tolerance = readTolerance(o.value("tolerance"), Tolerance {}), r.hasTolerance = true;
            if (r.caseId.empty() || !knownQuantity(r.quantity) || !std::isfinite(r.value))
            {
                if (errors) errors->push_back(path.toStdString() + " : référence incomplète (case, quantity, value)");
                continue;
            }
            out.push_back(r);
        }
        return out;
    }
    QTextStream ts(&f);
    const QStringList header = ts.readLine().split(',');
    auto col = [&](const char* name) { return int(header.indexOf(QString::fromLatin1(name))); };
    const int cCase = col("case"), cQ = col("quantity"), cV = col("value"), cU = col("unit"), cS = col("source"), cR = col("rel_tol"), cA = col("abs_tol");
    if (cCase < 0 || cQ < 0 || cV < 0)
    {
        if (errors) errors->push_back(path.toStdString() + " : en-tête CSV « case,quantity,value » obligatoire");
        return out;
    }
    int line = 1;
    while (!ts.atEnd())
    {
        ++line;
        const QString l = ts.readLine().trimmed();
        if (l.isEmpty() || l.startsWith('#')) continue;
        const QStringList c = l.split(',');
        auto at = [&](int i) { return i >= 0 && i < c.size() ? c[i].trimmed() : QString(); };
        bool ok = false;
        ExternalReference r;
        r.caseId = at(cCase).toStdString(), r.quantity = at(cQ).toStdString(), r.unit = at(cU).toStdString(), r.source = at(cS).toStdString();
        r.value = at(cV).toDouble(&ok);
        if (!ok || r.caseId.empty() || !knownQuantity(r.quantity))
        {
            if (errors) errors->push_back(path.toStdString() + " ligne " + std::to_string(line) + " : référence invalide");
            continue;
        }
        if (!at(cR).isEmpty() || !at(cA).isEmpty())
        {
            r.hasTolerance = true;
            r.tolerance.rel = at(cR).toDouble();
            r.tolerance.abs = at(cA).toDouble();
        }
        out.push_back(r);
    }
    return out;
}

// =====================================================================================================================
// Construction du modèle et calcul
// =====================================================================================================================
namespace
{
using TSA::Model::Model;

struct Built
{
    std::map<int, int> nodes;   ///< id JSON → id du modèle
    std::map<int, TSA::Analysis::ElementKey> members;
    std::map<int, int> slabs;
    std::map<int, int> cases;   ///< id JSON → id du cas dans le modèle (un modèle neuf contient déjà les cas par défaut)
    std::map<int, TSA::Model::Section> memberSections;   ///< id JSON → section (contraintes σ = N/A + M/W)
    std::string error;
};

TSA::Model::Section sectionOf(const QJsonObject& o, std::string* err)
{
    using TSA::Model::Section;
    const std::string shape = str(o.value("shape"));
    auto d = [&](const char* k) { return o.value(k).toDouble(); };
    if (shape == "rectangular") return Section::rectangular(d("b"), d("h"));
    if (shape == "circular") return Section::circular(d("d"));
    if (shape == "pipe") return Section::pipe(d("d"), d("t"));
    if (shape == "box") return Section::boxHollow(d("b"), d("h"), d("t"));
    if (shape == "ipe") return Section::ipe(o.value("n").toInt());
    if (shape == "hea") return Section::hea(o.value("n").toInt());
    if (shape == "heb") return Section::heb(o.value("n").toInt());
    if (err) *err = "forme de section inconnue « " + shape + " »";
    return Section::rectangular(0.1, 0.1);
}

TSA::Model::SupportDefinition supportOf(const QJsonValue& v, const QJsonValue& springs)
{
    using TSA::Model::DOFState;
    using TSA::Model::SupportDefinition;
    if (v.isString())
    {
        const QString s = v.toString();
        if (s == "fixed") return SupportDefinition::fixed();
        if (s == "pinned") return SupportDefinition::pinned();
        return SupportDefinition::free();
    }
    const QJsonArray a = v.toArray(), k = springs.toArray();
    DOFState st[6];
    for (int i = 0; i < 6; ++i)
        st[i] = a.at(i).toInt() ? DOFState::Fixed : (k.at(i).toDouble() > 0 ? DOFState::Spring : DOFState::Free);
    SupportDefinition def = SupportDefinition::custom(st[0], st[1], st[2], st[3], st[4], st[5]);
    if (!k.isEmpty()) def.setStiffnesses(k.at(0).toDouble(), k.at(1).toDouble(), k.at(2).toDouble(), k.at(3).toDouble(), k.at(4).toDouble(), k.at(5).toDouble());
    return def;
}

TSA::Model::LoadDirection directionOf(const QString& d)
{
    using TSA::Model::LoadDirection;
    if (d == "globalX") return LoadDirection::GlobalX;
    if (d == "globalY") return LoadDirection::GlobalY;
    if (d == "globalZ") return LoadDirection::GlobalZ;
    if (d == "localX") return LoadDirection::LocalX;
    if (d == "localY") return LoadDirection::LocalY;
    if (d == "localZ") return LoadDirection::LocalZ;
    return LoadDirection::Gravity;
}

Built buildModel(const QJsonObject& m, Model& model)
{
    Built b;
    std::map<std::string, TSA::Model::Material> mats;
    std::map<std::string, TSA::Model::Section> secs;
    const QJsonObject jm = m.value("materials").toObject();
    for (auto it = jm.begin(); it != jm.end(); ++it)
    {
        const QJsonObject o = it.value().toObject();
        TSA::Model::Material mat = TSA::Model::Material::steelS235();
        mat.name = it.key().toStdString();
        mat.E = o.value("E").toDouble() * 1e3;   // kPa → Pa
        mat.nu = o.value("nu").toDouble(0.3);
        mat.density = o.value("rho").toDouble(0.0);
        if (o.contains("fk")) mat.fk = o.value("fk").toDouble() * 1e3;
        mat.syncMechanical();
        mats[mat.name] = mat;
    }
    const QJsonObject js = m.value("sections").toObject();
    for (auto it = js.begin(); it != js.end(); ++it)
    {
        std::string e;
        secs[it.key().toStdString()] = sectionOf(it.value().toObject(), &e);
        if (!e.empty()) b.error = e;
    }
    for (const auto& v : m.value("nodes").toArray())
    {
        const QJsonObject o = v.toObject();
        const int id = model.addNode(o.value("x").toDouble(), o.value("y").toDouble(), o.value("z").toDouble());
        if (o.contains("support")) model.getNode(id)->setSupport(supportOf(o.value("support"), o.value("springs")));
        b.nodes[o.value("id").toInt()] = id;
    }
    auto node = [&](const QJsonValue& v) {
        auto it = b.nodes.find(v.toInt());
        if (it == b.nodes.end()) b.error = "nœud " + std::to_string(v.toInt()) + " inconnu";
        return it == b.nodes.end() ? 0 : it->second;
    };
    for (const auto& v : m.value("members").toArray())
    {
        const QJsonObject o = v.toObject();
        const std::string type = o.contains("type") ? str(o.value("type")) : std::string("beam");
        const auto sec = secs.count(str(o.value("section"))) ? secs.at(str(o.value("section"))) : TSA::Model::Section::rectangular(0.1, 0.1);
        const auto mat = mats.count(str(o.value("material"))) ? mats.at(str(o.value("material"))) : TSA::Model::Material::steelS235();
        if (!secs.count(str(o.value("section"))) || !mats.count(str(o.value("material")))) b.error = "section ou matériau inconnu (barre " + std::to_string(o.value("id").toInt()) + ")";
        const int i = node(o.value("i")), j = node(o.value("j"));
        const double rot = o.value("rotation").toDouble();
        TSA::Analysis::ElementKey key;
        if (type == "truss")
        {
            const int id = model.addTrussMember(i, j, 0.1);
            if (auto* t = model.getTrussMember(id))
            {
                t->setSection(sec);
                t->setMaterial(mat);
            }
            key = { TSA::Analysis::StructuralElementKind::Truss, id };
        }
        else if (type == "column")
            key = { TSA::Analysis::StructuralElementKind::Column, model.addColumn(i, j, sec, mat, rot) };
        else
        {
            const int id = model.addBar(i, j, sec, mat, TSA::Model::BarRole::Beam, rot);
            key = { TSA::Analysis::StructuralElementKind::Beam, id };
            auto release = [](const QJsonValue& r) {
                TSA::Model::EndRelease e;
                const QString s = r.toString();
                e.my = s.contains("my"), e.mz = s.contains("mz");
                return e;
            };
            if (auto* bm = model.getBeam(id))
            {
                bm->setStartRelease(release(o.value("release_i")));
                bm->setEndRelease(release(o.value("release_j")));
            }
        }
        b.members[o.value("id").toInt()] = key;
        b.memberSections[o.value("id").toInt()] = sec;
    }
    for (const auto& v : m.value("slabs").toArray())
    {
        const QJsonObject o = v.toObject();
        std::vector<int> ids;
        for (const auto& n : o.value("nodes").toArray()) ids.push_back(node(n));
        const int s = model.addSlab(ids, o.value("thickness").toDouble(0.2), "Dalle");
        auto* slab = model.getSlab(s);
        if (!slab) continue;
        if (mats.count(str(o.value("material")))) slab->setMaterial(mats.at(str(o.value("material"))));
        std::vector<TSA::Model::SurfaceEdgeSupport> sups;
        for (const auto& e : o.value("edgeSupports").toArray())
        {
            const QJsonObject eo = e.toObject();
            TSA::Model::SurfaceEdgeSupport sp;
            sp.edge = eo.value("edge").toInt(-1);
            for (int d = 0; d < 6; ++d) sp.fixed[d] = eo.value("fixed").toArray().at(d).toInt() != 0, sp.stiffness[d] = eo.value("stiffness").toArray().at(d).toDouble();
            sups.push_back(sp);
        }
        slab->setEdgeSupports(sups);
        std::vector<TSA::Model::SlabOpening> openings;
        for (const auto& op : o.value("openings").toArray())
        {
            TSA::Model::SlabOpening so;
            for (const auto& pt : op.toArray()) so.polygon.emplace_back(pt.toArray().at(0).toDouble(), pt.toArray().at(1).toDouble());
            openings.push_back(so);
        }
        slab->setOpenings(openings);
        b.slabs[o.value("id").toInt()] = s;
    }
    auto& lm = model.loadManager();
    auto& cases = b.cases;
    for (const auto& v : m.value("cases").toArray())
    {
        const QJsonObject o = v.toObject();
        cases[o.value("id").toInt()] = lm.addLoadCase(TSA::Model::LoadCase(0, str(o.value("name")), TSA::Model::LoadCaseCategory::Live));
    }
    auto caseOf = [&](const QJsonValue& v) {
        auto it = cases.find(v.toInt(1));
        if (it == cases.end()) b.error = "cas de charge " + std::to_string(v.toInt(1)) + " inconnu";
        return it == cases.end() ? 0 : it->second;
    };
    for (const auto& v : m.value("nodalLoads").toArray())
    {
        const QJsonObject o = v.toObject();
        auto d = [&](const char* k) { return o.value(k).toDouble(); };
        lm.addNodalLoad(TSA::Model::NodalLoad(0, node(o.value("node")), caseOf(o.value("case")), d("fx"), d("fy"), d("fz"), d("mx"), d("my"), d("mz")));
    }
    for (const auto& v : m.value("memberLoads").toArray())
    {
        const QJsonObject o = v.toObject();
        auto it = b.members.find(o.value("member").toInt());
        if (it == b.members.end())
        {
            b.error = "barre " + std::to_string(o.value("member").toInt()) + " inconnue (charge)";
            continue;
        }
        const auto dir = directionOf(o.value("direction").toString());
        const auto sys = (dir == TSA::Model::LoadDirection::LocalX || dir == TSA::Model::LoadDirection::LocalY || dir == TSA::Model::LoadDirection::LocalZ)
                             ? TSA::Model::LoadCoordSystem::Local
                             : TSA::Model::LoadCoordSystem::Global;
        const QString type = o.value("type").toString("uniform");
        const bool rel = o.value("relative").toBool(true);
        TSA::Model::MemberLoad ml;
        if (type == "point") ml = TSA::Model::MemberLoad::pointOnMember(it->second.id, caseOf(o.value("case")), o.value("p").toDouble(), o.value("x").toDouble(), dir, sys, rel);
        else if (type == "trapezoidal")
            ml = TSA::Model::MemberLoad::trapezoidal(it->second.id, caseOf(o.value("case")), o.value("q1").toDouble(), o.value("q2").toDouble(), o.value("x1").toDouble(0.0),
                                                     o.value("x2").toDouble(1.0), dir, sys, rel);
        else ml = TSA::Model::MemberLoad::uniform(it->second.id, caseOf(o.value("case")), o.value("q").toDouble(), dir, sys);
        using K = TSA::Analysis::StructuralElementKind;
        ml.setTargetType(it->second.kind == K::Column ? TSA::Model::MemberTargetType::Column
                                                      : it->second.kind == K::Truss ? TSA::Model::MemberTargetType::Truss : TSA::Model::MemberTargetType::Beam);
        lm.addMemberLoad(ml);
    }
    for (const auto& v : m.value("surfaceLoads").toArray())
    {
        const QJsonObject o = v.toObject();
        TSA::Model::SurfaceLoad sl;
        sl.surfaceId = b.slabs.count(o.value("slab").toInt()) ? b.slabs.at(o.value("slab").toInt()) : 0;
        sl.loadCaseId = caseOf(o.value("case"));
        sl.value = o.value("q").toDouble();
        lm.addSurfaceLoad(sl);
    }
    return b;
}

TSA::Mesh::MeshSettingsSet meshSettings(double h, bool quads)
{
    TSA::Mesh::MeshSettingsSet s;
    s.global.globalSize = h;
    s.global.mesh3D = false;
    s.global.recombine = quads;
    if (!quads) s.global.preferStructured = false;   // un rectangle serait sinon maillé en quadrangles transfinis
    s.global.nearSupports.enabled = s.global.nearPointLoads.enabled = s.global.nearOpenings.enabled = s.global.nearConnections.enabled =
        s.global.nearSectionChanges.enabled = false;
    s.global.globalSize *= h / s.global.effectiveSize();
    return s;
}

/// Valeur d'une grandeur dans les résultats ; nullopt si absente. interpolated : valeur interpolée entre stations.
bool extract(const std::string& q, const TSA::Analysis::ResultsModel& r, const Built& b, double* value, bool* interpolated, std::string* note)
{
    *interpolated = false;
    std::vector<std::string> p;
    std::stringstream ss(q);
    for (std::string t; std::getline(ss, t, ':');) p.push_back(t);
    auto comp6 = [](const std::string& c, const double v[6]) -> double {
        static const char* names[6] = { "ux", "uy", "uz", "rx", "ry", "rz" };
        for (int i = 0; i < 6; ++i)
            if (c == names[i]) return v[i];
        return std::numeric_limits<double>::quiet_NaN();
    };
    if (p[0] == "node" && p.size() == 3)
    {
        auto it = b.nodes.find(std::stoi(p[1]));
        if (it == b.nodes.end() || !r.getNodeDisplacement(it->second)) return false;
        const auto d = r.nodeDisplacement(it->second);
        const double v[6] = { d.ux, d.uy, d.uz, d.rx, d.ry, d.rz };
        *value = comp6(p[2], v);
        return std::isfinite(*value);
    }
    if (p[0] == "reaction" && p.size() == 3)
    {
        auto it = b.nodes.find(std::stoi(p[1]));
        if (it == b.nodes.end() || !r.getNodeReaction(it->second)) return false;
        const auto d = r.nodeReaction(it->second);
        const std::map<std::string, double> m = { { "fx", d.rx }, { "fy", d.ry }, { "fz", d.rz }, { "mx", d.mx }, { "my", d.my }, { "mz", d.mz } };
        if (!m.count(p[2])) return false;
        *value = m.at(p[2]);
        return true;
    }
    if (p[0] == "reactionsum" && p.size() == 2)
    {
        const auto eq = r.equilibrium();
        if (p[1] == "fx") *value = eq.reactionFx;
        else if (p[1] == "fy") *value = eq.reactionFy;
        else if (p[1] == "fz") *value = eq.reactionFz;
        else return false;
        return true;
    }
    if (p[0] == "member" && p.size() == 4)
    {
        auto it = b.members.find(std::stoi(p[1]));
        const auto* er = it == b.members.end() ? nullptr : r.getElementResults(it->second);
        if (!er) return false;
        std::vector<TSA::Analysis::StationForces> st { er->startForces };
        st.insert(st.end(), er->intermediateStations.begin(), er->intermediateStations.end());
        st.push_back(er->endForces);
        std::sort(st.begin(), st.end(), [](const auto& a, const auto& c) { return a.position < c.position; });
        const auto& sec = b.memberSections.at(std::stoi(p[1]));
        auto field = [&](const TSA::Analysis::StationForces& s) -> double {
            const std::string& f = p[2];
            if (f == "V") return std::hypot(s.Vy, s.Vz);   // indépendants de l'orientation des axes locaux
            if (f == "M") return std::hypot(s.My, s.Mz);
            if (f == "sigma") return std::abs(s.N) / sec.area() + std::abs(s.My) / sec.wy() + std::abs(s.Mz) / sec.wz();   // fibre extrême, enveloppe (kPa)
            if (f == "N") return s.N;
            if (f == "Vy") return s.Vy;
            if (f == "Vz") return s.Vz;
            if (f == "T") return s.Mx;
            if (f == "My") return s.My;
            if (f == "Mz") return s.Mz;
            if (f == "ux") return s.ux;
            if (f == "uy") return s.uy;
            if (f == "uz") return s.uz;
            return std::numeric_limits<double>::quiet_NaN();
        };
        const std::string& sel = p[3];
        if (sel == "start") *value = field(er->startForces);
        else if (sel == "end") *value = field(er->endForces);
        else if (sel == "absmax" || sel == "max" || sel == "min")
        {
            double best = sel == "min" ? 1e300 : -1e300, bestAbs = -1;
            for (const auto& s : st)
            {
                const double v = field(s);
                if (sel == "absmax" && std::abs(v) > bestAbs) bestAbs = std::abs(v), best = v;
                if (sel == "max") best = std::max(best, v);
                if (sel == "min") best = std::min(best, v);
            }
            *value = best;
            if (note) *note = std::to_string(st.size()) + " stations du moteur";
        }
        else if (sel.rfind("at=", 0) == 0)
        {
            const double x = std::stod(sel.substr(3)) * er->length;
            for (std::size_t k = 0; k + 1 < st.size(); ++k)
                if (x >= st[k].position - 1e-9 && x <= st[k + 1].position + 1e-9)
                {
                    if (std::abs(x - st[k].position) < 1e-9) *value = field(st[k]);
                    else if (std::abs(x - st[k + 1].position) < 1e-9) *value = field(st[k + 1]);
                    else
                    {
                        const double t = (x - st[k].position) / (st[k + 1].position - st[k].position);
                        *value = field(st[k]) + t * (field(st[k + 1]) - field(st[k]));
                        *interpolated = true;
                    }
                    return std::isfinite(*value);
                }
            return false;
        }
        else return false;
        return std::isfinite(*value);
    }
    if (p[0] == "surface" && p.size() == 3 && p[2] == "absmax")
    {
        if (r.surfaceNodeDisplacements().empty()) return false;
        double best = 0;
        for (const auto& [id, d] : r.surfaceNodeDisplacements())
        {
            const double v[6] = { d.ux, d.uy, d.uz, d.rx, d.ry, d.rz };
            const double c = comp6(p[1], v);
            if (std::abs(c) > std::abs(best)) best = c;
        }
        *value = best;
        return true;
    }
    if (p[0] == "modal")
    {
        const auto& md = r.modal();
        if (md.empty()) return false;
        if (p.size() == 2 && (p[1][0] == 'f' || p[1][0] == 'T'))
        {
            const int k = std::stoi(p[1].substr(1));
            if (k < 1 || k > int(md.modes.size())) return false;
            *value = p[1][0] == 'f' ? md.modes[k - 1].frequency : md.modes[k - 1].period;
            return true;
        }
        if (p.size() == 3 && p[1] == "mass")
        {
            const int i = p[2] == "x" ? 0 : p[2] == "y" ? 1 : 2;
            *value = md.totalMass[i];
            return true;
        }
    }
    return false;
}

std::string engineVersion(const std::string& engine)
{
    if (engine == "opensees")
    {
        // Version lue sur l'exécutable ; jamais une valeur par défaut présentée comme constatée
        const auto v = TSA::Analysis::OpenSeesManager::instance().versionInfo().versionString.toStdString();
        return "OpenSees " + (v.empty() ? std::string("(version non lue)") : v);
    }
    return engine == "custom2d" ? "Custom2D (TSALab MetDeDeplacement)" : engine;
}

struct RunResult
{
    bool ok = false;
    std::string message;
    TSA::Analysis::ResultsModel results;
    Built built;
    std::string meshInfo;   ///< composition du maillage transmis (quadrangles, triangles)
};

RunResult runOnce(const BenchmarkCase& c, const std::string& engine, double meshSize)
{
    RunResult rr;
    Model model;
    rr.built = buildModel(c.model, model);
    if (!rr.built.error.empty())
    {
        rr.message = "modèle invalide : " + rr.built.error;
        return rr;
    }
    TSA::Analysis::AnalysisEngineRegistry reg;
    TSA::Analysis::registerBuiltInEngines(reg);
    auto* eng = reg.engine(engine);
    if (!eng)
    {
        rr.message = "moteur « " + engine + " » inconnu";
        return rr;
    }
    if (!eng->availability().available)
    {
        rr.message = "moteur « " + engine + " » indisponible sur ce poste";
        return rr;
    }
    TSA::Analysis::AnalysisContext ctx;
    ctx.engineId = engine;
    ctx.common.includeSelfWeight = c.analysis.value("selfWeight").toBool(false);
    if (engine == "custom2d")
    {
        ctx.dimension = TSA::Analysis::AnalysisDimension::Plane2D;
        ctx.scope.type = TSA::Analysis::ScopeType::ModelPlane;
    }
    if (c.analysis.value("type").toString() == "nonlinear_static") ctx.type = TSA::Analysis::AnalysisType::NonLinearStatic;
    // Réglages propres au moteur (ex. transformation géométrique P-Delta, nombre de pas) transmis tels quels
    if (const QJsonObject es = c.analysis.value("engineSettings").toObject().value(QString::fromStdString(engine)).toObject(); !es.isEmpty())
        ctx.engineSettings[engine] = es;
    if (c.analysis.value("type").toString() == "modal")
    {
        ctx.type = TSA::Analysis::AnalysisType::Modal;
        ctx.common.modeCount = c.analysis.value("modes").toInt(3);
    }
    if (c.analysis.contains("case"))
    {
        auto it = rr.built.cases.find(c.analysis.value("case").toInt());
        if (it == rr.built.cases.end())
        {
            rr.message = "cas de charge " + std::to_string(c.analysis.value("case").toInt()) + " inconnu";
            return rr;
        }
        ctx.loadCaseIds = { it->second };
    }
    // Barres subdivisées (masses réparties en modal, stations de résultats) : n sous-éléments égaux
    if (const int n = c.analysis.value("subdivide").toInt(0); n > 1)
        for (const auto& [jid, key] : rr.built.members)
        {
            std::vector<double> pos;
            for (int k = 1; k < n; ++k) pos.push_back(double(k) / n);
            ctx.memberSubdivisions[key] = pos;
        }
    if (meshSize > 0)
    {
        const auto g = TSA::Mesh::MeshGeometry::extract(model);
        const auto mesh = TSA::Mesh::MeshGenerator::generate(g, meshSettings(meshSize, c.analysis.value("quads").toBool(true)));
        const auto compat = TSA::Mesh::OpenSeesMeshAdapter::check(mesh.mesh, g, "OpenSees", eng->capabilities(), 3000000, &model);
        if (compat.blocking())
        {
            for (const auto& i : compat.items)
                if (i.level == TSA::Mesh::SolverCheckItem::Level::Blocking) rr.message += i.message + " ; ";
            rr.message = "maillage non transmissible : " + rr.message;
            return rr;
        }
        for (const auto& [k, v] : compat.memberSubdivisions)
            if (!ctx.memberSubdivisions.count(k)) ctx.memberSubdivisions[k] = v;
        ctx.surfaces = compat.surfaces;
        rr.meshInfo = std::to_string(compat.surfaces.mesh.quads) + " quadrangles, " + std::to_string(compat.surfaces.mesh.triangles) + " triangles";
        ctx.surfaces.meshStamp = "benchmark h = " + std::to_string(meshSize);
    }
    TSA::Grid::GridManager gm;
    TSA::Analysis::AnalysisManager mgr(reg);
    const auto prepared = mgr.prepare(model, &gm, ctx);
    if (!prepared.canRun())
    {
        for (const auto& e : prepared.validation.texts(TSA::Analysis::ValidationSeverity::Error)) rr.message += e + " ; ";
        rr.message = "calcul refusé : " + rr.message;
        return rr;
    }
    auto run = mgr.run(ctx, prepared);
    if (!run.success)
    {
        rr.message = "échec du calcul : " + run.message;
        return rr;
    }
    rr.ok = true;
    rr.results = run.results;
    return rr;
}

CheckOutcome compare(const std::string& caseId, const std::string& engine, const std::string& quantity, double reference, const Tolerance& tol,
                     const RunResult& rr)
{
    CheckOutcome o;
    o.caseId = caseId, o.engine = engine, o.quantity = quantity, o.reference = reference, o.tolerance = tol;
    std::string note;
    o.available = rr.ok && extract(quantity, rr.results, rr.built, &o.value, &o.interpolated, &note);
    o.note = note;
    if (!o.available)
    {
        o.relError = std::numeric_limits<double>::quiet_NaN();
        o.note = "grandeur absente des résultats du moteur";
        return o;
    }
    o.absError = std::abs(o.value - reference);
    o.relError = std::abs(reference) > 1e-12 ? o.absError / std::abs(reference) : std::numeric_limits<double>::quiet_NaN();
    o.passed = tol.accepts(o.value, reference);
    if (o.interpolated) o.note += (o.note.empty() ? "" : " ; ") + std::string("valeur interpolée entre deux stations");
    return o;
}
} // namespace

std::vector<CaseOutcome> runBenchmark(const BenchmarkCase& c, const RunOptions& options)
{
    std::vector<CaseOutcome> out;
    std::vector<double> meshSizes;
    for (const auto& v : c.analysis.value("meshSizes").toArray()) meshSizes.push_back(v.toDouble());
    if (meshSizes.empty() && c.analysis.contains("meshSize")) meshSizes.push_back(c.analysis.value("meshSize").toDouble());
    if (meshSizes.empty()) meshSizes.push_back(0.0);
    for (const auto& engine : c.engines)
    {
        if (!options.engines.empty() && std::find(options.engines.begin(), options.engines.end(), engine) == options.engines.end()) continue;
        CaseOutcome co;
        co.caseId = c.id, co.title = c.title, co.category = c.category, co.engine = engineVersion(engine);
        QElapsedTimer timer;
        timer.start();
        // Étude de convergence : un calcul par taille de maille, vérifications sur la plus fine (dernière)
        std::vector<RunResult> runs;
        for (double h : meshSizes) runs.push_back(runOnce(c, engine, h));
        co.seconds = timer.elapsed() / 1000.0;
        const RunResult& last = runs.back();
        co.ran = last.ok;
        co.message = last.message;
        if (!last.ok && last.message.find("indisponible") != std::string::npos) co.skipped = true;
        for (const auto& k : c.checks)
        {
            if (!k.engines.empty() && std::find(k.engines.begin(), k.engines.end(), engine) == k.engines.end()) continue;
            CheckOutcome o = compare(c.id, engine, k.quantity, k.expected, k.tolerance, last);
            o.label = k.label, o.formula = k.formula, o.unit = k.unit, o.source = "analytique";
            if (meshSizes.size() > 1)
            {
                std::ostringstream conv;
                conv.precision(6);
                conv << "convergence :";
                for (std::size_t i = 0; i < runs.size(); ++i)
                {
                    ConvergencePoint p;
                    bool interp = false;
                    p.meshSize = meshSizes[i];
                    p.meshInfo = runs[i].meshInfo;
                    p.available = runs[i].ok && extract(k.quantity, runs[i].results, runs[i].built, &p.value, &interp, nullptr);
                    conv << " h=" << p.meshSize << " (" << p.meshInfo << ") → " << (p.available ? std::to_string(p.value) : std::string("—"));
                    o.convergence.push_back(p);
                }
                o.note += (o.note.empty() ? "" : " ; ") + conv.str();
            }
            co.checks.push_back(o);
        }
        for (const auto& ref : options.external)
        {
            if (ref.caseId != c.id) continue;
            CheckOutcome o = compare(c.id, engine, ref.quantity, ref.value, ref.hasTolerance ? ref.tolerance : options.externalDefault, last);
            o.source = ref.source.empty() ? "référence externe" : ref.source;
            o.unit = ref.unit;
            o.label = "comparaison externe";
            co.checks.push_back(o);
        }
        out.push_back(co);
    }
    return out;
}

// =====================================================================================================================
// Rapports
// =====================================================================================================================
namespace
{
QString num(double v)
{
    if (!std::isfinite(v)) return QStringLiteral("—");
    return QString::number(v, 'g', 8);
}
QString pct(double v)
{
    if (!std::isfinite(v)) return QStringLiteral("—");
    return QString::number(v * 100.0, 'g', 3) + QStringLiteral(" %");
}
QString tolText(const Tolerance& t) { return QStringLiteral("%1 + %2·|réf|").arg(t.abs, 0, 'g', 3).arg(t.rel, 0, 'g', 3); }
QString status(const CheckOutcome& c) { return !c.available ? QStringLiteral("NON DISPONIBLE") : (c.passed ? QStringLiteral("RÉUSSI") : QStringLiteral("ÉCHEC")); }
} // namespace

QString reportMarkdown(const BenchmarkReport& r)
{
    QString s;
    QTextStream o(&s);
    o << "# Rapport de validation TSA\n\n";
    o << "- Version de TSA : " << QString::fromStdString(r.tsaVersion) << "\n";
    o << "- Moteurs : " << QString::fromStdString(r.engineVersions) << "\n";
    o << "- Date : " << QString::fromStdString(r.timestamp) << "\n";
    o << "- Poste : " << QString::fromStdString(r.machine) << "\n";
    o << "- Vérifications : " << r.checksPassed() << " réussies, " << r.checksFailed() << " en échec, " << r.checksUnavailable() << " non disponibles ; "
      << r.casesSkipped() << " cas ignoré(s) (moteur indisponible)\n\n";
    o << "Tolérance mixte : |x_TSA − x_réf| ≤ ε_abs + ε_rel·|x_réf|. Une valeur interpolée entre stations est signalée.\n\n";
    for (const auto& c : r.cases)
    {
        o << "## " << QString::fromStdString(c.caseId) << " — " << QString::fromStdString(c.title) << " (" << QString::fromStdString(c.engine) << ")\n\n";
        if (!c.ran) o << "**Non exécuté : " << QString::fromStdString(c.message) << "**\n\n";
        o << "| Grandeur | Formule | Référence | TSA | Écart abs. | Écart rel. | Tolérance | Statut | Source | Remarque |\n";
        o << "|---|---|---|---|---|---|---|---|---|---|\n";
        for (const auto& k : c.checks)
            o << "| " << QString::fromStdString(k.label.empty() ? k.quantity : k.label + " (" + k.quantity + ")") << " | " << QString::fromStdString(k.formula) << " | "
              << num(k.reference) << " " << QString::fromStdString(k.unit) << " | " << (k.available ? num(k.value) : QStringLiteral("—")) << " | " << num(k.absError) << " | "
              << pct(k.relError) << " | " << tolText(k.tolerance) << " | " << status(k) << " | " << QString::fromStdString(k.source) << " | "
              << QString::fromStdString(k.note) << " |\n";
        o << "\nDurée : " << QString::number(c.seconds, 'f', 2) << " s\n\n";
    }
    return s;
}

QString reportCsv(const BenchmarkReport& r)
{
    QString s = QStringLiteral("case,engine,quantity,label,formula,reference,value,abs_error,rel_error,abs_tol,rel_tol,status,source,interpolated,note\n");
    auto q = [](const std::string& x) { return QStringLiteral("\"") + QString::fromStdString(x).replace('"', "'") + QStringLiteral("\""); };
    for (const auto& c : r.cases)
        for (const auto& k : c.checks)
            s += QStringList { QString::fromStdString(k.caseId), q(c.engine), QString::fromStdString(k.quantity), q(k.label), q(k.formula), num(k.reference),
                               k.available ? num(k.value) : QString(), num(k.absError), std::isfinite(k.relError) ? QString::number(k.relError, 'g', 6) : QString(),
                               QString::number(k.tolerance.abs, 'g', 6), QString::number(k.tolerance.rel, 'g', 6), status(k), q(k.source),
                               k.interpolated ? QStringLiteral("1") : QStringLiteral("0"), q(k.note) }
                     .join(',') +
                 QStringLiteral("\n");
    return s;
}

QJsonObject reportJson(const BenchmarkReport& r)
{
    QJsonArray cases;
    for (const auto& c : r.cases)
    {
        QJsonArray checks;
        for (const auto& k : c.checks)
        {
            QJsonArray conv;
            for (const auto& p : k.convergence)
                conv.append(QJsonObject { { "meshSize", p.meshSize },
                                          { "mesh", QString::fromStdString(p.meshInfo) },
                                          { "value", p.available ? QJsonValue(p.value) : QJsonValue() } });
            QJsonObject check { { "quantity", QString::fromStdString(k.quantity) },
                                { "label", QString::fromStdString(k.label) },
                                { "formula", QString::fromStdString(k.formula) },
                                { "reference", k.reference },
                                { "value", k.available ? QJsonValue(k.value) : QJsonValue() },
                                { "absError", k.available ? QJsonValue(k.absError) : QJsonValue() },
                                { "relError", std::isfinite(k.relError) ? QJsonValue(k.relError) : QJsonValue() },
                                { "tolerance", QJsonObject { { "rel", k.tolerance.rel }, { "abs", k.tolerance.abs } } },
                                { "status", status(k) },
                                { "source", QString::fromStdString(k.source) },
                                { "interpolated", k.interpolated },
                                { "note", QString::fromStdString(k.note) } };
            if (!conv.isEmpty()) check.insert("convergence", conv);
            checks.append(check);
        }
        cases.append(QJsonObject { { "id", QString::fromStdString(c.caseId) },
                                   { "title", QString::fromStdString(c.title) },
                                   { "category", QString::fromStdString(c.category) },
                                   { "engine", QString::fromStdString(c.engine) },
                                   { "ran", c.ran },
                                   { "skipped", c.skipped },
                                   { "passed", c.passed() },
                                   { "message", QString::fromStdString(c.message) },
                                   { "seconds", c.seconds },
                                   { "checks", checks } });
    }
    return QJsonObject { { "format", "tsa.validation-report/1" },
                         { "tsaVersion", QString::fromStdString(r.tsaVersion) },
                         { "engines", QString::fromStdString(r.engineVersions) },
                         { "timestamp", QString::fromStdString(r.timestamp) },
                         { "machine", QString::fromStdString(r.machine) },
                         { "summary", QJsonObject { { "passed", r.checksPassed() }, { "failed", r.checksFailed() }, { "unavailable", r.checksUnavailable() },
                                                    { "skippedCases", r.casesSkipped() } } },
                         { "cases", cases } };
}

bool writeReport(const BenchmarkReport& r, const QString& directory, QString* error)
{
    if (!QDir().mkpath(directory))
    {
        if (error) *error = QStringLiteral("dossier non créé : %1").arg(directory);
        return false;
    }
    const QString stamp = QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss");
    auto put = [&](const QString& name, const QByteArray& data) {
        QFile f(QDir(directory).filePath(name));
        if (!f.open(QIODevice::WriteOnly)) return false;
        return f.write(data) == data.size();
    };
    const QByteArray json = QJsonDocument(reportJson(r)).toJson(QJsonDocument::Indented);
    bool ok = put("report-" + stamp + ".md", reportMarkdown(r).toUtf8()) && put("report-" + stamp + ".csv", reportCsv(r).toUtf8()) &&
              put("report-" + stamp + ".json", json) && put("latest.json", json) && put("latest.md", reportMarkdown(r).toUtf8());
    if (!ok && error) *error = QStringLiteral("écriture impossible dans %1").arg(directory);
    return ok;
}

} // namespace TSA::Validation
