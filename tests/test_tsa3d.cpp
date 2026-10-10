// Suite « tsa3d » : format d'échange TSA3D (tests 267-273). Aller-retour modèle → TSA3D → modèle → TSA3D,
// validation (versions, identifiants, références, unités, géométrie, maillage), conversion d'unités,
// conservation des données tierces (y compris à travers un .tsa), maillage et résultats d'un vrai calcul,
// exemples du SDK, import refusé sans effet.

#include "test_common.h"

#include "Analysis/Engine/AnalysisManager.h"
#include "IO/TSAFile.h"
#include "IO/Tsa3d/Tsa3d.h"

#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>

using namespace TSA::IO;
namespace T3 = TSA::IO::Tsa3d;

namespace
{
/// Modèle couvrant toutes les familles : portique, treillis, câble, dalle, voile, fondation, appuis
/// (dont ressort), charges (nodale, uniforme, trapézoïdale, ponctuelle), combinaison.
Model& richModel(Model& m)
{
    const int a = m.addNode(0, 0, 0), b = m.addNode(0, 0, 4), c = m.addNode(6, 0, 4), d = m.addNode(6, 0, 0), e = m.addNode(6, 5, 4),
              f = m.addNode(0, 5, 4), g = m.addNode(6, 5, 0);
    m.getNode(a)->setSupport(SupportDefinition::fixed());
    m.getNode(d)->setSupport(SupportDefinition::pinned());
    m.getNode(g)->setSupport(SupportDefinition::elastic(1e4, 2e4, 3e4, 0, 0, 0));
    m.getNode(b)->setName("Tête A");
    m.addColumn(a, b, Section::heb(200), Material::steelS235(), 15.0, "P1");
    m.addColumn(d, c, Section::heb(200), Material::steelS235());
    const int bm = m.addBar(b, c, Section::ipe(300), Material::steelS355(), BarRole::Beam, 0.0, "Traverse");
    m.getBeam(bm)->setEccentricity(BarEccentricity::TopFlange);
    EndRelease rel;
    rel.my = rel.mz = true;
    m.getBeam(bm)->setEndRelease(rel);
    m.addBar(c, e, Section::rectangular(0.25, 0.5), Material::concreteC25_30(), BarRole::Brace, 30.0);
    m.addTrussMember(a, c, 0.08, "Diag", TrussMemberRole::Diagonal);
    const int k = m.addCable(f, e, 0.03, "Hauban");
    m.getCable(k)->setInitialTension(25000.0);
    m.addSlab({ b, c, e, f }, 0.22, "Dalle");
    m.addWall(a, d, 4.0, 0.2, "Voile");
    m.addFoundation(g, 1.4, 1.6, 0.5, "Semelle");
    auto& lm = m.loadManager();
    const int g1 = lm.addLoadCase(LoadCase(0, "G", LoadCaseCategory::Dead, true, 1.0));
    const int q1 = lm.addLoadCase(LoadCase(0, "Q", LoadCaseCategory::Live));
    lm.addNodalLoad(NodalLoad(0, b, q1, 10.0, 0.0, -5.0, 0.0, 2.0, 0.0));
    lm.addMemberLoad(TSA::Model::MemberLoad::uniform(0, bm, g1, -8.0));
    lm.addMemberLoad(TSA::Model::MemberLoad::trapezoidal(0, bm, q1, -2.0, -6.0, 0.5, 4.5));
    lm.addMemberLoad(TSA::Model::MemberLoad::pointOnMember(0, bm, q1, -12.0, 3.0));
    lm.addCombination(LoadCombination(0, "ELU", LoadCombinationType::ULS_Fundamental, { { g1, 1.35 }, { q1, 1.5 } }));
    return m;
}

QJsonObject withoutVolatile(QJsonObject doc)
{
    QJsonObject meta = doc.value("metadata").toObject();
    meta.remove("modified");
    meta.remove("created");
    meta.remove("id");
    doc.insert("metadata", meta);
    return doc;
}

bool sameDouble(double a, double b) { return std::abs(a - b) <= 1e-9 * std::max(1.0, std::abs(a)); }
} // namespace

bool runSuite_Tsa3d(int& passed)
{
    // -------------------------------------------------------------------------
    // TEST 267 : aller-retour complet
    // -------------------------------------------------------------------------
    {
        Model m;
        richModel(m);
        const auto ex = T3::exportModel(m, { "Projet test", "", nullptr, false, false });
        const auto rep = T3::validate(ex.document);
        TEST_CHECK(!rep.hasErrors(), "Test 267: export valide (" << rep.lines().join(" | ").toStdString() << ")");
        Model back;
        const auto im = T3::importDocument(ex.document, back);
        TEST_CHECK(im.ok && im.nodes == 7 && im.members == 6 && im.surfaces == 2 && im.foundations == 1,
                   "Test 267: import complet (" << im.report.lines().join(" | ").toStdString() << ")");
        bool nodesOk = back.nodes().size() == m.nodes().size();
        for (const auto& [id, n] : m.nodes())
        {
            const auto* o = back.getNode(id);
            nodesOk &= o && o->x() == n.x() && o->y() == n.y() && o->z() == n.z() && o->name() == n.name() &&
                       o->support().tx() == n.support().tx() && o->support().rz() == n.support().rz() &&
                       sameDouble(o->support().ky(), n.support().ky());
        }
        TEST_CHECK(nodesOk, "Test 267: nœuds (identifiants, coordonnées, noms, appuis, ressorts) identiques");
        bool barsOk = back.beams().size() == m.beams().size() && back.columns().size() == m.columns().size();
        for (const auto& [id, b] : m.beams())
        {
            const auto* o = back.getBeam(id);
            barsOk &= o && o->startNodeId() == b.startNodeId() && o->endNodeId() == b.endNodeId() && o->role() == b.role() &&
                      o->rotation() == b.rotation() && o->eccentricity() == b.eccentricity() && o->endRelease().my == b.endRelease().my &&
                      o->section().shape == b.section().shape && sameDouble(o->section().height, b.section().height) &&
                      sameDouble(o->section().area(), b.section().area()) && o->material().name == b.material().name &&
                      sameDouble(o->material().mechanical.youngModulus, b.material().mechanical.youngModulus);
        }
        for (const auto& [id, c] : m.columns())
            barsOk &= back.getColumn(id) && back.getColumn(id)->rotation() == c.rotation() && back.getColumn(id)->name() == c.name();
        TEST_CHECK(barsOk, "Test 267: barres (connexions, rôles, rotations, excentrements, relâchements, sections, matériaux)");
        const auto* cable = back.getCable(back.cables().begin()->first);
        TEST_CHECK(cable && sameDouble(cable->initialTension(), 25000.0) && back.trussMembers().size() == 1 &&
                       back.slabs().begin()->second.nodeIds().size() == 4 && sameDouble(back.walls().begin()->second.height(), 4.0) &&
                       sameDouble(back.foundations().begin()->second.lengthB(), 1.6),
                   "Test 267: câble (tension N ↔ kN), treillis, dalle, voile, fondation");
        const auto a1 = m.loadManager().createSnapshot(), a2 = back.loadManager().createSnapshot();
        bool loadsOk = a1.loadCases.size() == a2.loadCases.size() && a1.nodalLoads.size() == a2.nodalLoads.size() &&
                       a1.memberLoads.size() == a2.memberLoads.size() && a1.combinations.size() == a2.combinations.size();
        for (const auto& [id, l] : a1.memberLoads)
        {
            const auto& o = a2.memberLoads.at(id);
            loadsOk &= o.type() == l.type() && sameDouble(o.q1(), l.q1()) && sameDouble(o.q2(), l.q2()) && sameDouble(o.x1(), l.x1()) &&
                       sameDouble(o.x2(), l.x2()) && o.elementId() == l.elementId() && o.targetType() == l.targetType();
        }
        for (const auto& [id, co] : a1.combinations) loadsOk &= a2.combinations.at(id).caseFactors() == co.caseFactors();
        TEST_CHECK(loadsOk, "Test 267: cas, charges (uniforme, trapézoïdale, ponctuelle, nodale) et combinaisons");
        const auto ex2 = T3::exportModel(back, { "Projet test", "", nullptr, false, false });
        TEST_CHECK(withoutVolatile(ex2.document) == withoutVolatile(ex.document), "Test 267: export → import → export identique");
        TEST_CHECK(!ex.report.issues.empty(), "Test 267: pertes signalées (détail des câbles)");
        std::cout << "[PASS] Test 267: TSA3D aller-retour complet" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 268 : validation — versions, identifiants, références, géométrie, maillage
    // -------------------------------------------------------------------------
    {
        auto base = [] {
            return QJsonDocument::fromJson(R"({"format":"TSA3D","version":"1.0",
              "materials":[{"id":"M","type":"steel","E":2.1e11}],"sections":[{"id":"S","shape":"rectangular","dimensions":{"b":0.2,"h":0.4}}],
              "nodes":[{"id":"N1","position":[0,0,0]},{"id":"N2","position":[3,0,0]}],
              "members":[{"id":"B1","type":"beam","nodes":["N1","N2"],"section":"S","material":"M"}]})").object();
        };
        TEST_CHECK(!T3::validate(base()).hasErrors(), "Test 268: document de base valide");
        auto expectError = [&](QJsonObject d, const char* needle) {
            const auto r = T3::validate(d);
            return r.hasErrors() && r.lines().join(" ").contains(QString::fromUtf8(needle));
        };
        QJsonObject d = base();
        d["version"] = "2.0";
        TEST_CHECK(expectError(d, "plus récente"), "Test 268: version majeure future refusée (aucune migration destructive)");
        d = base();
        d["version"] = "1.4";
        TEST_CHECK(!T3::validate(d).hasErrors() && T3::validate(d).count(T3::Severity::Warning) == 1, "Test 268: version mineure future acceptée avec avertissement");
        d = base();
        d["format"] = "AUTRE";
        TEST_CHECK(expectError(d, "TSA3D"), "Test 268: format inconnu");
        d = base();
        QJsonArray nodes = d["nodes"].toArray();
        nodes.append(QJsonObject { { "id", "N1" }, { "position", QJsonArray { 1, 1, 1 } } });
        d["nodes"] = nodes;
        TEST_CHECK(expectError(d, "déjà utilisé"), "Test 268: identifiant en double");
        d = base();
        QJsonArray members = d["members"].toArray();
        QJsonObject b1 = members[0].toObject();
        b1["nodes"] = QJsonArray { "N1", "N9" };
        members[0] = b1;
        d["members"] = members;
        TEST_CHECK(expectError(d, "N9"), "Test 268: référence inconnue");
        d = base();
        nodes = d["nodes"].toArray();
        QJsonObject n2 = nodes[1].toObject();
        n2["position"] = QJsonArray { 0, 0, 0 };
        nodes[1] = n2;
        d["nodes"] = nodes;
        TEST_CHECK(expectError(d, "longueur nulle"), "Test 268: barre de longueur nulle");
        d = base();
        QJsonArray secs = d["sections"].toArray();
        QJsonObject s = secs[0].toObject();
        s["dimensions"] = QJsonObject { { "b", 0.2 }, { "h", -1 } };
        secs[0] = s;
        d["sections"] = secs;
        TEST_CHECK(expectError(d, "positif"), "Test 268: dimension de section invalide");
        d = base();
        d["units"] = QJsonObject { { "length", "pouce" } };
        TEST_CHECK(expectError(d, "pouce"), "Test 268: unité inconnue");
        d = base();
        d["mesh"] = QJsonDocument::fromJson(R"({"nodes":[{"id":"m1","position":[0,0,0]}],"elements":[{"id":"e1","type":"line2","nodes":["m1","m9"]}]})").object();
        TEST_CHECK(expectError(d, "m9"), "Test 268: maillage : nœud inconnu");
        d = base();
        d["vendor.bloc"] = 3;
        TEST_CHECK(!T3::validate(d).hasErrors() && T3::validate(d).count(T3::Severity::Info) >= 1, "Test 268: bloc inconnu : information, conservé");
        T3::Report syntax;
        QJsonObject out;
        TEST_CHECK(!T3::read("{\n  \"format\": \"TSA3D\",\n  oops\n}", &out, &syntax) && syntax.lines().join(" ").contains("ligne 3"),
                   "Test 268: erreur de syntaxe JSON avec numéro de ligne");
        std::cout << "[PASS] Test 268: Validation TSA3D" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 269 : unités converties à l'import
    // -------------------------------------------------------------------------
    {
        QJsonObject doc;
        T3::Report r;
        TEST_CHECK(T3::readFile(QStringLiteral(TSA_SOURCE_DIR) + "/sdk/examples/tsa3d/materials-sections.tsa3d", &doc, &r), "Test 269: exemple lu");
        Model m;
        const auto im = T3::importDocument(doc, m);
        TEST_CHECK(im.ok, "Test 269: import (" << im.report.lines().join(" | ").toStdString() << ")");
        const auto* c = m.getNode(3);
        bool box = false;
        for (const auto& [id, b] : m.beams())
            if (b.section().shape == SectionShape::BoxHollow) box = sameDouble(b.section().height, 0.3) && sameDouble(b.material().mechanical.youngModulus, 210e9);
        TEST_CHECK(c && sameDouble(c->x(), 8.0) && box, "Test 269: mm → m, MPa → Pa");
        std::cout << "[PASS] Test 269: Unités TSA3D" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 270 : données tierces conservées (identifiants, propriétés, extensions), y compris via un .tsa
    // -------------------------------------------------------------------------
    {
        QJsonObject doc;
        T3::readFile(QStringLiteral(TSA_SOURCE_DIR) + "/sdk/examples/tsa3d/custom-properties.tsa3d", &doc, nullptr);
        Model m;
        TEST_CHECK(T3::importDocument(doc, m).ok, "Test 270: import");
        const std::string path = "test_tsa3d_ext.tsa";
        TSAFileWriter w;
        std::string err;
        TEST_CHECK(w.saveToFile(path, m, nullptr, "Ext", "Test", &err), "Test 270: enregistrement .tsa");
        Model loaded;
        TSAFileReader rd;
        TEST_CHECK(rd.loadFromFile(path, loaded, nullptr, "", nullptr, nullptr, nullptr, &err), "Test 270: relecture .tsa");
        std::filesystem::remove(path);
        std::cout << "  données d'échange : " << m.exchangeExtensionsJson().size() << " octets avant, " << loaded.exchangeExtensionsJson().size()
                  << " après relecture" << std::endl;
        const QJsonObject out = T3::exportModel(loaded).document;
        QStringList memberIds, nodeIds;
        for (const auto& v : out["members"].toArray()) memberIds << v.toObject()["id"].toString();
        for (const auto& v : out["nodes"].toArray()) nodeIds << v.toObject()["id"].toString();
        const QJsonObject beam = out["members"].toArray()[0].toObject();
        TEST_CHECK(memberIds == QStringList { "poutre-1" } && nodeIds.contains("pt-a") && nodeIds.contains("pt-b"),
                   "Test 270: identifiants d'origine conservés");
        TEST_CHECK(beam["properties"].toObject()["fournisseur"].toString() == "Atelier X" && beam["com.exemple.couleurPlan"].toString() == "#336699",
                   "Test 270: propriétés et champ inconnu de l'objet conservés");
        TEST_CHECK(out["groups"].toArray().size() == 1 && out["extensions"].toObject().contains("com.exemple.planning") &&
                       out["com.exemple.bloc"].toObject()["note"].toString().contains("conservé") &&
                       out["metadata"].toObject()["custom"].toObject()["chantier"].toString() == "Lot 3",
                   "Test 270: groupes, extensions, bloc inconnu, métadonnées personnalisées conservés");
        TEST_CHECK(!T3::validate(out).hasErrors(), "Test 270: export toujours valide");
        std::cout << "[PASS] Test 270: Conservation des données tierces" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 271 : maillage et résultats d'un vrai calcul ; résultats importés jamais validés
    // -------------------------------------------------------------------------
    {
        Model m;
        GridManager gm;
        const int a = m.addNode(0, 0, 0), b = m.addNode(0, 0, 3), c = m.addNode(5, 0, 3), d = m.addNode(5, 0, 0);
        m.getNode(a)->setSupport(SupportDefinition::fixed());
        m.getNode(d)->setSupport(SupportDefinition::fixed());
        m.addColumn(a, b, Section::heb(200), Material::steelS235());
        m.addBar(b, c, Section::ipe(300), Material::steelS235(), BarRole::Beam);
        m.addColumn(d, c, Section::heb(200), Material::steelS235());
        const int lc = m.loadManager().addLoadCase(LoadCase(0, "Q", LoadCaseCategory::Live));
        m.loadManager().addNodalLoad(NodalLoad(0, b, lc, 10.0, 0.0, -20.0));
        TSA::Analysis::AnalysisEngineRegistry reg;
        TSA::Analysis::registerBuiltInEngines(reg);
        TSA::Analysis::AnalysisManager mgr(reg);
        TSA::Analysis::AnalysisContext ctx;
        ctx.engineId = "opensees";
        ctx.common.includeSelfWeight = false;
        const auto run = mgr.run(ctx, mgr.prepare(m, &gm, ctx));
        TEST_CHECK(run.success, "Test 271: calcul OpenSees (" << run.message << ")");
        T3::ExportOptions opt;
        opt.results = &run.results;
        opt.includeResults = true;
        const auto ex = T3::exportModel(m, opt);
        const QJsonObject mesh = ex.document["mesh"].toObject();
        TEST_CHECK(mesh["elements"].toArray().size() == 3 && mesh["nodes"].toArray().size() == 4 && mesh["source"].toObject()["engine"] == "opensees",
                   "Test 271: maillage réel du solveur exporté");
        TEST_CHECK(ex.document["results"].toObject()["status"] == "computed-not-validated" &&
                       ex.document["results"].toObject()["displacements"].toArray().size() == 4,
                   "Test 271: résultats exportés, marqués non validés");
        TEST_CHECK(!T3::validate(ex.document).hasErrors(), "Test 271: document avec maillage et résultats valide");
        Model back;
        const auto im = T3::importDocument(ex.document, back);
        TEST_CHECK(im.ok && im.report.lines().join(" ").contains("non chargés comme résultats"), "Test 271: résultats importés non chargés comme résultats valides");
        std::cout << "[PASS] Test 271: Maillage et résultats TSA3D" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 272 : exemples du SDK et schéma
    // -------------------------------------------------------------------------
    {
        const QDir dir(QStringLiteral(TSA_SOURCE_DIR) + "/sdk/examples/tsa3d");
        const QStringList files = dir.entryList({ "*.tsa3d" }, QDir::Files);
        bool all = files.size() >= 6;
        QStringList bad;
        for (const QString& f : files)
        {
            QJsonObject doc;
            T3::Report r;
            Model m;
            const bool ok = T3::readFile(dir.filePath(f), &doc, &r) && !T3::validate(doc).hasErrors() && T3::importDocument(doc, m).ok;
            if (!ok) bad << f;
            all &= ok;
        }
        TEST_CHECK(all, "Test 272: les " << files.size() << " exemples sont valides et importables (" << bad.join(", ").toStdString() << ")");
        QFile schema(QStringLiteral(TSA_SOURCE_DIR) + "/docs/schemas/tsa3d-1.schema.json");
        QJsonParseError pe;
        const bool schemaOk = schema.open(QIODevice::ReadOnly) && !QJsonDocument::fromJson(schema.readAll(), &pe).isNull();
        TEST_CHECK(schemaOk, "Test 272: schéma JSON publié et lisible");
        std::cout << "[PASS] Test 272: Exemples TSA3D du SDK" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 273 : import refusé → modèle inchangé
    // -------------------------------------------------------------------------
    {
        Model m;
        m.addNode(1, 2, 3);
        const auto rev = m.revision();
        const auto im = T3::importDocument(QJsonDocument::fromJson(R"({"format":"TSA3D","version":"9.0"})").object(), m);
        TEST_CHECK(!im.ok && m.nodes().size() == 1 && m.revision() == rev, "Test 273: document refusé : modèle intact");
        std::cout << "[PASS] Test 273: Import refusé sans effet" << std::endl;
        ++passed;
    }
    return true;
}
