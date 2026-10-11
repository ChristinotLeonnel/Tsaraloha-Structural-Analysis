#include "NDCProjectNote.h"

#include "App/ProductInfo.h"
#include "NormativeReferenceDetector.h"
#include "ResultAnalyzer.h"
#include "../Analysis/OpenSeesManager.h"
#include "../Analysis/ResultsModel.h"
#include "../Geometry/DiagramGeometry.h"
#include "../Model/Beam.h"
#include "../Model/Cable/Cable.h"
#include "../Model/Column.h"
#include "../Model/Load/LoadManager.h"
#include "../Model/Model.h"
#include "../Model/TrussMember.h"
#include "../Standards/Design/ConcreteDesignEC2.h"
#include "../Standards/Design/SteelDesignEC3.h"

#include <QBuffer>
#include <QDateTime>
#include <QRegularExpression>

#include <algorithm>
#include <cmath>
#include <functional>
#include <queue>
#include <set>
#include <tuple>

namespace TSA::NDC
{

using TSA::Analysis::StructuralElementKind;

namespace
{
constexpr double kGravity = 9.81;

// --- Barres transmises au calcul ------------------------------------------------------------------------------
struct Member
{
    StructuralElementKind kind;
    int id = 0, n1 = 0, n2 = 0;
    const TSA::Model::Section* section = nullptr;
    const TSA::Model::Material* material = nullptr;
    QString name, family;
    double length = 0.0;
};

std::vector<Member> membersOf(const TSA::Model::Model& m)
{
    std::vector<Member> out;
    auto len = [&](int a, int b) {
        const auto* p = m.getNode(a);
        const auto* q = m.getNode(b);
        return p && q ? std::sqrt(std::pow(q->x() - p->x(), 2) + std::pow(q->y() - p->y(), 2) + std::pow(q->z() - p->z(), 2)) : 0.0;
    };
    for (const auto& [id, b] : m.beams())
        out.push_back({ StructuralElementKind::Beam, id, b.startNodeId(), b.endNodeId(), &b.section(), &b.material(), QString::fromStdString(b.name()),
                        QStringLiteral("Poutre / barre"), len(b.startNodeId(), b.endNodeId()) });
    for (const auto& [id, c] : m.columns())
        out.push_back({ StructuralElementKind::Column, id, c.startNodeId(), c.endNodeId(), &c.section(), &c.material(), QString::fromStdString(c.name()),
                        QStringLiteral("Poteau"), len(c.startNodeId(), c.endNodeId()) });
    for (const auto& [id, t] : m.trussMembers())
        out.push_back({ StructuralElementKind::Truss, id, t.startNodeId(), t.endNodeId(), &t.section(), &t.material(), QString::fromStdString(t.name()),
                        QStringLiteral("Treillis"), len(t.startNodeId(), t.endNodeId()) });
    for (const auto& [id, c] : m.cables())
        out.push_back({ StructuralElementKind::Cable, id, c.startNodeId(), c.endNodeId(), &c.section(), &c.material(), QString::fromStdString(c.name()),
                        QStringLiteral("Câble"), len(c.startNodeId(), c.endNodeId()) });
    return out;
}

QString kindName(StructuralElementKind k)
{
    switch (k)
    {
    case StructuralElementKind::Column: return QStringLiteral("Poteau");
    case StructuralElementKind::Truss: return QStringLiteral("Treillis");
    case StructuralElementKind::Cable: return QStringLiteral("Câble");
    default: return QStringLiteral("Poutre");
    }
}

// --- Mise en forme -------------------------------------------------------------------------------------------
QString f(double v, int digits = 2) { return QString::number(v, 'f', digits); }
QString ok(const QString& t = QStringLiteral("OK")) { return QStringLiteral("<span class=\"badge-success\">%1</span>").arg(t); }
QString bad(const QString& t) { return QStringLiteral("<span class=\"badge-danger\">%1</span>").arg(t); }
QString warn(const QString& t) { return QStringLiteral("<span class=\"badge-warning\">%1</span>").arg(t); }
QString notComputed() { return QStringLiteral("<span class=\"notcomputed\">non calculée</span>"); }
QString formula(const QString& t) { return QStringLiteral("<span class=\"formula\">%1</span>").arg(t); }

QString imageUri(const QImage& img)
{
    if (img.isNull()) return {};
    QByteArray ba;
    QBuffer buf(&ba);
    buf.open(QIODevice::WriteOnly);
    img.save(&buf, "JPG", 88); // vues 3D : JPEG, PDF nettement plus léger qu'en PNG
    return QStringLiteral("data:image/jpeg;base64,") + QString::fromLatin1(ba.toBase64());
}

/// Contexte de construction : compteurs de tableaux et de figures, mode détaillé.
struct Ctx
{
    const TSA::Model::Model& model;
    const std::shared_ptr<TSA::Analysis::ResultsModel>& results;
    const ReportConfiguration& config;
    const NDCGenerator::SnapshotProvider& snapshots;
    bool detailed = false;
    int chapter = 1, table = 1, figure = 1;
    std::vector<Member> members;
    /// Résultats statiques (déplacements, efforts, réactions) ; une analyse modale n'en fournit pas.
    bool hasResults() const { return results && results->isValid() && results->analysisType() != TSA::Analysis::AnalysisType::Modal; }
    bool hasModal() const { return results && results->isValid() && !results->modal().empty(); }
    int rowCap() const { return detailed ? 30 : 10; }

    NDCTable makeTable(const QString& caption, const QStringList& headers, const std::vector<QString>& align)
    {
        NDCTable t;
        t.number = table++;
        t.caption = caption;
        t.headers = headers;
        t.columnAlignments = align;
        return t;
    }
    NDCChapter makeChapter(const QString& title)
    {
        NDCChapter ch;
        ch.number = chapter++;
        ch.title = title;
        return ch;
    }
};

/// Texte des chapitres de résultats statiques en l'absence de tels résultats.
QString noStaticResults(const Ctx& x)
{
    return x.hasModal() ? QStringLiteral("Aucun résultat statique : le dernier calcul est une analyse modale (voir le chapitre « Analyse modale »).")
                        : QStringLiteral("Aucun résultat de calcul.");
}

/// Libellé du cas ou de la combinaison du calcul (métadonnées du solveur : « Combinaison #id », « Cas de charge #id »).
QString analysedLoading(const Ctx& x, int* combId = nullptr, int* caseId = nullptr)
{
    if (!x.hasResults()) return x.hasModal() ? QStringLiteral("aucun (analyse modale)") : QStringLiteral("aucun calcul disponible");
    const QString meta = QString::fromStdString(x.results->executionMetadata().loadCombinationType);
    const auto& lm = x.model.loadManager();
    const QRegularExpressionMatch m = QRegularExpression(QStringLiteral("#(\\d+)")).match(meta);
    if (m.hasMatch() && meta.startsWith(QStringLiteral("Combinaison")))
    {
        const int id = m.captured(1).toInt();
        if (combId) *combId = id;
        if (const auto* c = lm.getCombination(id))
            return QStringLiteral("combinaison « %1 » : %2").arg(QString::fromStdString(c->name()), QString::fromStdString(c->formula(lm.loadCases())));
    }
    if (m.hasMatch() && meta.startsWith(QStringLiteral("Cas")))
    {
        const int id = m.captured(1).toInt();
        if (caseId) *caseId = id;
        if (const auto* c = lm.getLoadCase(id)) return QStringLiteral("cas de charge « %1 »").arg(QString::fromStdString(c->name()));
    }
    return QStringLiteral("tous les cas actifs, non pondérés (%1)").arg(meta);
}

/// Somme des charges d'un cas (kN) : charges linéiques uniformes verticales Σ q·L et charges nodales verticales.
struct CaseLoads
{
    std::size_t memberLoads = 0, nodalLoads = 0, otherLoads = 0;
    double lineSum = 0.0, nodalFz = 0.0, nodalFh = 0.0;
};

std::map<int, CaseLoads> loadsByCase(const TSA::Model::Model& m)
{
    std::map<int, CaseLoads> out;
    for (const auto& [id, l] : m.loadManager().memberLoads())
    {
        auto& c = out[l.loadCaseId()];
        const auto* b = m.getBeam(l.elementId());
        if (b && l.type() == TSA::Model::LoadType::MemberUniform && l.direction() == TSA::Model::LoadDirection::Gravity)
        {
            ++c.memberLoads;
            c.lineSum += l.q1() * b->length(m);
        }
        else
            ++c.otherLoads;
    }
    for (const auto& [id, l] : m.loadManager().nodalLoads())
    {
        auto& c = out[l.loadCaseId()];
        ++c.nodalLoads;
        c.nodalFz += -l.fz(); // charge descendante comptée positivement
        c.nodalFh += std::hypot(l.fx(), l.fy());
    }
    return out;
}

/// Flèche relative à la corde (m) d'une barre : écart maximal des stations au segment joignant les extrémités.
double relativeDeflection(const TSA::Analysis::ElementResults& r, double* xAt)
{
    std::vector<TSA::Analysis::StationForces> st;
    st.push_back(r.startForces);
    st.insert(st.end(), r.intermediateStations.begin(), r.intermediateStations.end());
    st.push_back(r.endForces);
    const double L = std::max(r.length, st.back().position);
    if (L <= 0.0 || st.size() < 3) return 0.0;
    double best = 0.0;
    for (const auto& s : st)
    {
        const double t = std::clamp(s.position / L, 0.0, 1.0);
        const double dy = s.uy - ((1 - t) * r.startForces.uy + t * r.endForces.uy);
        const double dz = s.uz - ((1 - t) * r.startForces.uz + t * r.endForces.uz);
        const double d = std::hypot(dy, dz);
        if (d > best)
        {
            best = d;
            if (xAt) *xAt = s.position;
        }
    }
    return best;
}

double maxNormalStress(const TSA::Analysis::ElementResults& r, const TSA::Model::Section& s)
{
    double best = 0.0;
    auto test = [&](const TSA::Analysis::StationForces& st) {
        const double v = TSA::Geometry::DiagramGeometry::normalStressMPa(st, s.area(), s.wy(), s.wz());
        if (std::abs(v) > std::abs(best)) best = v;
    };
    test(r.startForces);
    test(r.endForces);
    for (const auto& st : r.intermediateStations) test(st);
    return best;
}

double absMax(const TSA::Analysis::ElementResults& r, double TSA::Analysis::StationForces::*field)
{
    double v = std::max(std::abs(r.startForces.*field), std::abs(r.endForces.*field));
    for (const auto& st : r.intermediateStations) v = std::max(v, std::abs(st.*field));
    return v;
}

/// Surfaces calculées en coques (présentes dans les résultats) : référence (SL2, W5).
std::set<std::string> computedSurfaces(const Ctx& x)
{
    std::set<std::string> out;
    if (x.hasResults())
        for (const auto& [id, r] : x.results->shellResults()) out.insert(r.source);
    return out;
}

/// Poids propre (kN) des surfaces calculées : ρ · g · e · aire nette.
double surfaceSelfWeightKN(const Ctx& x)
{
    double w = 0.0;
    const auto refs = computedSurfaces(x);
    for (const auto& [id, s] : x.model.slabs())
        if (refs.count("SL" + std::to_string(id))) w += s.material().mechanical.density * kGravity * s.thickness() * s.area(x.model) / 1000.0;
    for (const auto& [id, s] : x.model.walls())
        if (refs.count("W" + std::to_string(id))) w += s.material().mechanical.density * kGravity * s.thickness() * s.area(x.model) / 1000.0;
    return w;
}

bool isSteel(const TSA::Model::Material& m)
{
    return m.type == TSA::Model::MaterialType::Steel || m.type == TSA::Model::MaterialType::GalvanizedSteel;
}
bool isConcrete(const TSA::Model::Material& m)
{
    return m.type == TSA::Model::MaterialType::Concrete || m.type == TSA::Model::MaterialType::ReinforcedConcrete;
}

// =============================================================================================================
// Chapitres
// =============================================================================================================

void chapterPresentation(Ctx& x, NDCDocument& doc)
{
    auto ch = x.makeChapter(QStringLiteral("Présentation du projet"));
    NDCSection s;
    s.title = QStringLiteral("Objet de la note");
    if (!x.config.projectDescription.isEmpty()) s.paragraphs.push_back(x.config.projectDescription);
    s.paragraphs.push_back(QStringLiteral("La présente note %1 rassemble les données du modèle, les actions, les résultats du calcul et les "
                                          "vérifications effectuées par %2. Toutes les valeurs proviennent du modèle et du calcul réellement "
                                          "exécuté ; les vérifications non implémentées sont signalées « non calculée ».")
                               .arg(x.detailed ? QStringLiteral("détaillée") : QStringLiteral("simplifiée"), TSA::Product::name()));
    s.keyValues.push_back({ QStringLiteral("Projet"), x.config.projectTitle });
    s.keyValues.push_back({ QStringLiteral("Statut du document"), x.config.documentStatus });
    s.keyValues.push_back({ QStringLiteral("Logiciel"), doc.softwareVersion });
    s.keyValues.push_back({ QStringLiteral("Chargement calculé"), analysedLoading(x) });
    if (x.hasResults())
    {
        const auto& meta = x.results->executionMetadata();
        s.keyValues.push_back({ QStringLiteral("Date du calcul"), QString::fromStdString(meta.executionTimestamp.empty() ? x.results->timestamp() : meta.executionTimestamp) });
        s.keyValues.push_back({ QStringLiteral("Moteur"), QString::fromStdString(meta.solverEngine + " " + meta.solverVersion) });
    }
    s.keyValues.push_back({ QStringLiteral("Note produite le"), QDateTime::currentDateTime().toString(QStringLiteral("dd/MM/yyyy HH:mm")) });
    ch.sections.push_back(s);
    doc.addChapter(ch);
}

void chapterStandards(Ctx& x, NDCDocument& doc)
{
    auto ch = x.makeChapter(QStringLiteral("Normes et références"));
    NDCSection s;
    s.paragraphs.push_back(QStringLiteral("Référentiels associés aux éléments, matériaux et actions du modèle. Leur citation n'implique pas que "
                                          "toutes leurs clauses soient vérifiées : seules les vérifications du chapitre « Vérifications "
                                          "réglementaires » marquées comme calculées l'ont été."));
    auto t = x.makeTable(QStringLiteral("Référentiels associés au modèle"), { QStringLiteral("Norme"), QStringLiteral("Domaine"), QStringLiteral("Titre et motif") },
                         { QStringLiteral("left"), QStringLiteral("left"), QStringLiteral("left") });
    for (const auto& r : NormativeReferenceDetector::detectApplicableStandards(x.model))
        t.rows.push_back({ r.code, r.domain, QStringLiteral("%1<br><small>%2</small>").arg(r.title, r.justification) });
    s.tables.push_back(t);
    ch.sections.push_back(s);
    doc.addChapter(ch);
}

QStringList hypotheses(const Ctx& x)
{
    const auto& m = x.model;
    QStringList h;
    h << QStringLiteral("Unités : longueurs en m, forces en kN, moments en kN·m, contraintes en MPa (sauf mention).");
    h << QStringLiteral("Modèle filaire tridimensionnel, 6 degrés de liberté par nœud. Poutres et poteaux : éléments de poutre élastiques "
                        "(Navier-Bernoulli) ; treillis : barres articulées ; câbles : barres à formulation corotationnelle.");
    h << QStringLiteral("Analyse statique linéaire : petits déplacements, matériaux élastiques linéaires, sans effet du second ordre (P-Δ) "
                        "ni analyse modale ou dynamique.");
    const std::size_t slabs = m.slabs().size(), walls = m.walls().size(), fnd = m.foundations().size();
    const auto computed = computedSurfaces(x);
    if (!computed.empty())
        h << QStringLiteral("Surfaces calculées en coques : %1 surface(s) maillée(s) (Gmsh), éléments ShellMITC4 (quadrangles, Mindlin-Reissner) et "
                            "ShellDKGT (triangles, Kirchhoff discret), section élastique membrane + plaque (E, ν, épaisseur). Nœuds partagés avec les "
                            "barres situées sur leurs arêtes ; aucune liaison rigide ni excentricité ajoutée (plans moyens confondus avec les axes "
                            "des barres). Poids propre (ρ·g·e) et charges surfaciques convertis en charges nodales cohérentes (∫ Nᵢ p dA).")
                 .arg(computed.size());
    const std::size_t notComputed = slabs + walls - computed.size();
    if (notComputed + fnd > 0)
        h << QStringLiteral("Éléments modélisés mais non transmis au calcul : %1 dalle(s) ou voile(s) (non maillés, non structuraux ou maillage non "
                            "transmis), %2 semelle(s). Ils servent à la géométrie, au rendu et au maillage ; leurs charges éventuelles sont reportées "
                            "sur les poutres en charges linéiques (libellés des charges) et leur rigidité n'est pas prise en compte.")
                 .arg(notComputed)
                 .arg(fnd);
    if (!m.cables().empty())
        h << QStringLiteral("Câbles : tension initiale T0 imposée par une déformation initiale ε0 = T0 / (E·A) ; matériau élastique, algorithme "
                            "linéaire : ni comportement « tension seule » itératif ni non-linéarité géométrique. La tension de chaque câble "
                            "est contrôlée a posteriori (chapitre « Câbles et haubans »). Pas de réglage itératif des tensions.");
    h << QStringLiteral("Appuis idéalisés (encastrements, articulations) ; aucune interaction sol-structure.");
    h << QStringLiteral("Poids propre calculé par le moteur : ρ · g · A par unité de longueur (g = 9,81 m/s²), pour les cas qui l'incluent.");
    return h;
}

void chapterHypotheses(Ctx& x, NDCDocument& doc)
{
    auto ch = x.makeChapter(QStringLiteral("Hypothèses de modélisation"));
    NDCSection s;
    for (const auto& p : hypotheses(x)) s.paragraphs.push_back(QStringLiteral("• ") + p);
    ch.sections.push_back(s);
    doc.addChapter(ch);
}

void chapterGeometry(Ctx& x, NDCDocument& doc)
{
    const auto& m = x.model;
    auto ch = x.makeChapter(QStringLiteral("Géométrie du modèle"));
    NDCSection s;
    s.title = QStringLiteral("Inventaire");
    double x0 = 1e30, y0 = 1e30, z0 = 1e30, x1 = -1e30, y1 = -1e30, z1 = -1e30;
    for (const auto& [id, n] : m.nodes())
    {
        x0 = std::min(x0, n.x()), y0 = std::min(y0, n.y()), z0 = std::min(z0, n.z());
        x1 = std::max(x1, n.x()), y1 = std::max(y1, n.y()), z1 = std::max(z1, n.z());
    }
    s.keyValues.push_back({ QStringLiteral("Nœuds"), QString::number(m.nodes().size()) });
    s.keyValues.push_back({ QStringLiteral("Poutres, poteaux, treillis, câbles"),
                            QStringLiteral("%1, %2, %3, %4").arg(m.beams().size()).arg(m.columns().size()).arg(m.trussMembers().size()).arg(m.cables().size()) });
    s.keyValues.push_back({ QStringLiteral("Dalles, voiles, semelles (non transmis)"),
                            QStringLiteral("%1, %2, %3").arg(m.slabs().size()).arg(m.walls().size()).arg(m.foundations().size()) });
    if (!m.nodes().empty())
        s.keyValues.push_back({ QStringLiteral("Encombrement X × Y × Z"), QStringLiteral("%1 × %2 × %3 m").arg(f(x1 - x0, 1), f(y1 - y0, 1), f(z1 - z0, 1)) });
    ch.sections.push_back(s);

    // Regroupement par nom d'élément : nombre, sections, longueur cumulée
    struct G
    {
        QString family;
        int count = 0;
        double length = 0.0;
        std::set<QString> sections;
    };
    std::map<QString, G> groups;
    for (const auto& mb : x.members)
    {
        auto& g = groups[mb.name.isEmpty() ? QStringLiteral("(sans nom)") : mb.name];
        g.family = mb.family;
        ++g.count;
        g.length += mb.length;
        g.sections.insert(QString::fromStdString(mb.section->name));
    }
    NDCSection sg;
    sg.title = QStringLiteral("Éléments transmis au calcul, par désignation");
    auto t = x.makeTable(QStringLiteral("Barres du modèle de calcul regroupées par désignation"),
                         { QStringLiteral("Désignation"), QStringLiteral("Famille"), QStringLiteral("Nombre"), QStringLiteral("Section(s)"), QStringLiteral("Longueur cumulée (m)") },
                         { QStringLiteral("left"), QStringLiteral("left"), QStringLiteral("right"), QStringLiteral("left"), QStringLiteral("right") });
    for (const auto& [name, g] : groups)
    {
        QStringList secs(g.sections.begin(), g.sections.end());
        t.rows.push_back({ name, g.family, QString::number(g.count), secs.join(QStringLiteral(", ")), f(g.length, 1) });
    }
    sg.tables.push_back(t);
    ch.sections.push_back(sg);
    doc.addChapter(ch);
}

void chapterMaterials(Ctx& x, NDCDocument& doc)
{
    auto ch = x.makeChapter(QStringLiteral("Matériaux"));
    std::map<std::string, const TSA::Model::Material*> mats;
    for (const auto& mb : x.members) mats[mb.material->name] = mb.material;
    NDCSection s;
    s.paragraphs.push_back(QStringLiteral("Propriétés mécaniques utilisées par le calcul. Les finitions d'aspect (couleur, texture) sont des propriétés "
                                          "de rendu indépendantes : elles n'interviennent pas dans le calcul."));
    auto t = x.makeTable(QStringLiteral("Propriétés mécaniques des matériaux"),
                         { QStringLiteral("Matériau"), QStringLiteral("E (GPa)"), QStringLiteral("ν"), QStringLiteral("ρ (kg/m³)"), QStringLiteral("f<sub>k</sub> / f<sub>y</sub> (MPa)") },
                         { QStringLiteral("left"), QStringLiteral("right"), QStringLiteral("right"), QStringLiteral("right"), QStringLiteral("right") });
    for (const auto& [n, mt] : mats) t.rows.push_back({ QString::fromStdString(n), f(mt->E / 1e9, 1), f(mt->nu, 2), f(mt->density, 0), f(mt->fk / 1e6, 0) });
    s.tables.push_back(t);
    ch.sections.push_back(s);
    doc.addChapter(ch);
}

void chapterSections(Ctx& x, NDCDocument& doc)
{
    auto ch = x.makeChapter(QStringLiteral("Sections"));
    std::map<std::string, std::pair<const TSA::Model::Section*, int>> secs;
    for (const auto& mb : x.members)
    {
        auto& e = secs[mb.section->name];
        e.first = mb.section;
        ++e.second;
    }
    NDCSection s;
    auto t = x.makeTable(QStringLiteral("Caractéristiques des sections utilisées"),
                         { QStringLiteral("Section"), QStringLiteral("Barres"), QStringLiteral("A (cm²)"), QStringLiteral("I<sub>y</sub> (cm⁴)"), QStringLiteral("I<sub>z</sub> (cm⁴)"),
                           QStringLiteral("W<sub>el,y</sub> (cm³)"), QStringLiteral("W<sub>el,z</sub> (cm³)") },
                         { QStringLiteral("left"), QStringLiteral("right"), QStringLiteral("right"), QStringLiteral("right"), QStringLiteral("right"), QStringLiteral("right"),
                           QStringLiteral("right") });
    for (const auto& [n, e] : secs)
    {
        const auto* sc = e.first;
        t.rows.push_back({ QString::fromStdString(n), QString::number(e.second), f(sc->area() * 1e4, 1), f(sc->iy() * 1e8, 0), f(sc->iz() * 1e8, 0),
                           f(sc->wy() * 1e6, 0), f(sc->wz() * 1e6, 0) });
    }
    s.tables.push_back(t);
    ch.sections.push_back(s);
    doc.addChapter(ch);
}

void chapterSupports(Ctx& x, NDCDocument& doc)
{
    auto ch = x.makeChapter(QStringLiteral("Appuis et liaisons"));
    std::map<QString, int> byType;
    std::vector<int> ids;
    for (const auto& [id, n] : x.model.nodes())
        if (n.support().isSupported())
        {
            ++byType[QString::fromStdString(n.support().typeName())];
            ids.push_back(id);
        }
    NDCSection s;
    for (const auto& [t, c] : byType) s.keyValues.push_back({ t, QStringLiteral("%1 nœud(s)").arg(c) });
    if (x.detailed)
    {
        auto t = x.makeTable(QStringLiteral("Nœuds d'appui"), { QStringLiteral("Nœud"), QStringLiteral("X (m)"), QStringLiteral("Y (m)"), QStringLiteral("Z (m)"), QStringLiteral("Type"),
                                                                 QStringLiteral("Degrés de liberté bloqués") },
                             { QStringLiteral("center"), QStringLiteral("right"), QStringLiteral("right"), QStringLiteral("right"), QStringLiteral("left"), QStringLiteral("left") });
        for (int id : ids)
        {
            const auto* n = x.model.getNode(id);
            t.rows.push_back({ QString::number(id), f(n->x()), f(n->y()), f(n->z()), QString::fromStdString(n->support().typeName()),
                               QString::fromStdString(n->support().dofSummary()) });
        }
        s.tables.push_back(t);
    }
    ch.sections.push_back(s);
    doc.addChapter(ch);
}

void chapterActions(Ctx& x, NDCDocument& doc)
{
    const auto& lm = x.model.loadManager();
    auto ch = x.makeChapter(QStringLiteral("Actions et chargements"));
    const auto sums = loadsByCase(x.model);
    const double sw = selfWeightKN(x.model);
    NDCSection s;
    s.title = QStringLiteral("Cas de charge");
    auto t = x.makeTable(QStringLiteral("Cas de charge et résultantes verticales des charges appliquées"),
                         { QStringLiteral("Cas"), QStringLiteral("Nature"), QStringLiteral("Poids propre"), QStringLiteral("Charges linéiques"), QStringLiteral("Σ q·L (kN)"),
                           QStringLiteral("Charges nodales"), QStringLiteral("Σ F<sub>z</sub> / Σ F<sub>h</sub> (kN)") },
                         { QStringLiteral("left"), QStringLiteral("left"), QStringLiteral("right"), QStringLiteral("right"), QStringLiteral("right"), QStringLiteral("right"),
                           QStringLiteral("right") });
    for (const auto& [id, lc] : lm.loadCases())
    {
        const auto it = sums.find(id);
        const CaseLoads c = it != sums.end() ? it->second : CaseLoads {};
        t.rows.push_back({ QString::fromStdString(lc.name()) + (lc.description().empty() ? QString() : QStringLiteral("<br><small>%1</small>").arg(QString::fromStdString(lc.description()))),
                           QString::fromStdString(TSA::Model::loadCategoryToString(lc.category())),
                           lc.isSelfWeightIncluded() ? QStringLiteral("%1 kN").arg(f(sw * lc.selfWeightFactor(), 0)) : QStringLiteral("—"),
                           QString::number(c.memberLoads) + (c.otherLoads ? QStringLiteral(" (+%1 non sommées)").arg(c.otherLoads) : QString()), f(c.lineSum, 1),
                           QString::number(c.nodalLoads), c.nodalLoads ? QStringLiteral("%1 / %2").arg(f(c.nodalFz, 1), f(c.nodalFh, 1)) : QStringLiteral("—") });
    }
    s.tables.push_back(t);
    s.paragraphs.push_back(QStringLiteral("Poids propre des barres transmises au calcul : %1 kN (Σ ρ·g·A·L, g = 9,81 m/s²).").arg(f(sw, 1)));
    ch.sections.push_back(s);

    // Charges par libellé : le libellé porte l'hypothèse (charge surfacique × largeur de reprise)
    struct L
    {
        int caseId = 0, count = 0;
        double q = 0.0, total = 0.0;
        bool uniformQ = true;
    };
    std::map<std::pair<int, QString>, L> labels;
    for (const auto& [id, l] : lm.memberLoads())
    {
        const auto* b = x.model.getBeam(l.elementId());
        auto& e = labels[{ l.loadCaseId(), QString::fromStdString(l.name()) }];
        if (e.count && std::abs(e.q - l.q1()) > 1e-9) e.uniformQ = false;
        e.caseId = l.loadCaseId();
        e.q = l.q1();
        ++e.count;
        if (b) e.total += l.q1() * b->length(x.model);
    }
    NDCSection sl;
    sl.title = QStringLiteral("Charges linéiques par hypothèse");
    auto tl = x.makeTable(QStringLiteral("Charges linéiques appliquées, regroupées par libellé"),
                          { QStringLiteral("Cas"), QStringLiteral("Libellé (hypothèse)"), QStringLiteral("Barres"), QStringLiteral("q (kN/m)"), QStringLiteral("Σ q·L (kN)") },
                          { QStringLiteral("left"), QStringLiteral("left"), QStringLiteral("right"), QStringLiteral("right"), QStringLiteral("right") });
    for (const auto& [key, e] : labels)
    {
        const auto* lc = lm.getLoadCase(e.caseId);
        tl.rows.push_back({ lc ? QString::fromStdString(lc->name()) : QString::number(e.caseId), key.second.isEmpty() ? QStringLiteral("—") : key.second,
                            QString::number(e.count), e.uniformQ ? f(e.q, 2) : QStringLiteral("variable"), f(e.total, 1) });
    }
    if (!tl.rows.empty())
    {
        sl.tables.push_back(tl);
        ch.sections.push_back(sl);
    }
    if (!lm.surfaceLoads().empty())
    {
        NDCSection ss;
        ss.title = QStringLiteral("Charges surfaciques");
        ss.paragraphs.push_back(QStringLiteral("Charges des dalles et voiles, converties au calcul en charges nodales cohérentes sur le maillage des surfaces "
                                               "(résultante et moment contrôlés, ouvertures déduites)."));
        auto ts = x.makeTable(QStringLiteral("Charges surfaciques"), { QStringLiteral("Surface"), QStringLiteral("Cas"), QStringLiteral("Libellé"), QStringLiteral("Charge") },
                              { QStringLiteral("left"), QStringLiteral("left"), QStringLiteral("left"), QStringLiteral("left") });
        auto dirName = [](TSA::Model::SurfaceLoadDirection d) {
            switch (d)
            {
            case TSA::Model::SurfaceLoadDirection::GlobalX: return QStringLiteral("X global");
            case TSA::Model::SurfaceLoadDirection::GlobalY: return QStringLiteral("Y global");
            case TSA::Model::SurfaceLoadDirection::GlobalZ: return QStringLiteral("Z global");
            case TSA::Model::SurfaceLoadDirection::LocalNormal: return QStringLiteral("normale locale");
            default: return QStringLiteral("gravité");
            }
        };
        for (const auto& [id, l] : lm.surfaceLoads())
        {
            const auto* lc = lm.getLoadCase(l.loadCaseId);
            const bool pt = l.distribution == TSA::Model::SurfaceLoadDistribution::Point;
            ts.rows.push_back({ QString::fromStdString(l.surfaceRef()), lc ? QString::fromStdString(lc->name()) : QString::number(l.loadCaseId),
                                l.name.empty() ? QStringLiteral("—") : QString::fromStdString(l.name),
                                QStringLiteral("%1 %2, %3%4").arg(f(l.value, 2), pt ? QStringLiteral("kN") : QStringLiteral("kN/m²"), dirName(l.direction),
                                                                  pt ? QStringLiteral(" en (%1 ; %2) m").arg(f(l.u), f(l.v)) : (l.projected ? QStringLiteral(", surface projetée") : QString())) });
        }
        ss.tables.push_back(ts);
        ch.sections.push_back(ss);
    }
    doc.addChapter(ch);
}

void chapterCombinations(Ctx& x, NDCDocument& doc)
{
    const auto& lm = x.model.loadManager();
    auto ch = x.makeChapter(QStringLiteral("Combinaisons d'actions"));
    NDCSection s;
    s.paragraphs.push_back(QStringLiteral("Combinaisons définies dans le projet (aucune génération automatique exhaustive). Formes de référence (EN 1990) :"));
    s.paragraphs.push_back(formula(QStringLiteral("ELU fondamental (6.10) : E<sub>d</sub> = Σ γ<sub>G,j</sub>·G<sub>k,j</sub> + γ<sub>Q,1</sub>·Q<sub>k,1</sub> + Σ γ<sub>Q,i</sub>·ψ<sub>0,i</sub>·Q<sub>k,i</sub>")));
    s.paragraphs.push_back(formula(QStringLiteral("ELS caractéristique : E<sub>d</sub> = Σ G<sub>k,j</sub> + Q<sub>k,1</sub> + Σ ψ<sub>0,i</sub>·Q<sub>k,i</sub>")));
    auto t = x.makeTable(QStringLiteral("Combinaisons du projet"), { QStringLiteral("Combinaison"), QStringLiteral("Formule") },
                         { QStringLiteral("left"), QStringLiteral("left") });
    for (const auto& [id, c] : lm.loadCombinations()) t.rows.push_back({ QString::fromStdString(c.name()), QString::fromStdString(c.formula(lm.loadCases())) });
    s.tables.push_back(t);
    s.paragraphs.push_back(QStringLiteral("Chargement des résultats de la présente note : %1.").arg(analysedLoading(x)));
    ch.sections.push_back(s);
    doc.addChapter(ch);
}

void chapterMethod(Ctx& x, NDCDocument& doc)
{
    auto ch = x.makeChapter(QStringLiteral("Méthode de calcul et formules"));
    NDCSection s;
    s.paragraphs.push_back(QStringLiteral("Méthode des déplacements : assemblage de la matrice de rigidité globale K et résolution du système linéaire "
                                          "pour les déplacements nodaux U sous le vecteur des charges F (moteur OpenSees, algorithme linéaire) :"));
    s.paragraphs.push_back(formula(QStringLiteral("K · U = F")));
    s.paragraphs.push_back(QStringLiteral("Rigidités élémentaires d'une poutre de longueur L (repère local) : axiale E·A/L, flexion 12·E·I/L³, 6·E·I/L², "
                                          "4·E·I/L et 2·E·I/L, torsion G·I<sub>t</sub>/L. Les charges réparties sont transformées en forces nodales équivalentes."));
    s.paragraphs.push_back(QStringLiteral("Efforts internes : repère local de la barre, N > 0 en traction. Contrainte normale de fibre extrême (théorie des poutres, "
                                          "sans cisaillement ni torsion) :"));
    s.paragraphs.push_back(formula(QStringLiteral("σ = N / A ± |M<sub>y</sub>| / W<sub>el,y</sub> ± |M<sub>z</sub>| / W<sub>el,z</sub>")));
    s.paragraphs.push_back(QStringLiteral("Équilibre global : résultante des charges appliquées et des réactions ; résidu relatif :"));
    s.paragraphs.push_back(formula(QStringLiteral("r = |Σ F + Σ R| / |Σ F| ≤ 10<sup>-3</sup>")));
    s.paragraphs.push_back(QStringLiteral("Flèche relative d'une barre : écart maximal de la déformée à la corde joignant ses extrémités, comparé à L/250."));
    if (!x.model.cables().empty())
    {
        s.paragraphs.push_back(QStringLiteral("Câbles : déformation initiale et effort normal (E module du câble, A section métallique) :"));
        s.paragraphs.push_back(formula(QStringLiteral("ε<sub>0</sub> = T<sub>0</sub> / (E · A) ;  N = E · A · (ε + ε<sub>0</sub>)")));
    }
    s.paragraphs.push_back(QStringLiteral("Flambement par flexion des barres acier comprimées (EN 1993-1-1 §6.3.1) :"));
    s.paragraphs.push_back(formula(QStringLiteral("N<sub>b,Rd</sub> = χ · A · f<sub>y</sub> / γ<sub>M1</sub> ;  χ = 1 / (Φ + √(Φ² − λ̄²)) ;  Φ = 0,5·[1 + α·(λ̄ − 0,2) + λ̄²]")));
    s.paragraphs.push_back(QStringLiteral("Flexion simple des poutres en béton armé (EN 1992-1-1, diagramme rectangulaire simplifié) :"));
    s.paragraphs.push_back(formula(QStringLiteral("μ<sub>cu</sub> = M<sub>Ed</sub> / (b · d² · f<sub>cd</sub>) ;  A<sub>s</sub> = M<sub>Ed</sub> / (z · f<sub>yd</sub>)")));
    ch.sections.push_back(s);
    doc.addChapter(ch);
}

void chapterChecks(Ctx& x, NDCDocument& doc, int& issues)
{
    auto ch = x.makeChapter(x.detailed ? QStringLiteral("Contrôle du modèle et équilibre") : QStringLiteral("Contrôle du modèle et équilibre global"));
    const ModelAudit a = auditModel(x.model);
    NDCSection s;
    s.title = QStringLiteral("Topologie du modèle de calcul");
    auto t = x.makeTable(QStringLiteral("Contrôles topologiques (valeurs calculées sur le modèle)"),
                         { QStringLiteral("Contrôle"), QStringLiteral("Valeur"), QStringLiteral("Attendu"), QStringLiteral("Statut") },
                         { QStringLiteral("left"), QStringLiteral("right"), QStringLiteral("center"), QStringLiteral("center") });
    auto row = [&](const QString& name, std::size_t v, bool good, const QString& expect) {
        t.rows.push_back({ name, QString::number(v), expect, good ? ok() : bad(QStringLiteral("À CORRIGER")) });
        if (!good) ++issues;
    };
    row(QStringLiteral("Nœuds isolés (sans barre)"), a.isolatedNodes, a.isolatedNodes == 0, QStringLiteral("0"));
    row(QStringLiteral("Barres de longueur nulle"), a.zeroLength, a.zeroLength == 0, QStringLiteral("0"));
    row(QStringLiteral("Nœuds confondus"), a.duplicateNodes, a.duplicateNodes == 0, QStringLiteral("0"));
    row(QStringLiteral("Barres en double"), a.duplicateMembers, a.duplicateMembers == 0, QStringLiteral("0"));
    row(QStringLiteral("Nœuds sur une barre sans y être connectés"), a.nodesInsideMembers, a.nodesInsideMembers == 0, QStringLiteral("0"));
    row(QStringLiteral("Parties disjointes de la structure"), a.components, a.components == 1, QStringLiteral("1"));
    row(QStringLiteral("Nœuds d'appui"), a.supportedNodes, a.supportedNodes >= 1, QStringLiteral("≥ 1"));
    s.tables.push_back(t);
    ch.sections.push_back(s);

    NDCSection se;
    se.title = QStringLiteral("Équilibre global");
    if (!x.hasResults())
        se.paragraphs.push_back(QStringLiteral("Aucun résultat de calcul : équilibre non contrôlé."));
    else
    {
        const auto eq = x.results->equilibrium();
        const double scale = std::max({ std::abs(eq.appliedFx), std::abs(eq.appliedFy), std::abs(eq.appliedFz), 1e-9 });
        const double rel = eq.maxError() / scale;
        auto te = x.makeTable(QStringLiteral("Résultantes des charges et des réactions (kN)"),
                              { QStringLiteral("Direction"), QStringLiteral("Σ charges"), QStringLiteral("Σ réactions"), QStringLiteral("Écart") },
                              { QStringLiteral("left"), QStringLiteral("right"), QStringLiteral("right"), QStringLiteral("right") });
        te.rows.push_back({ QStringLiteral("X"), f(eq.appliedFx), f(eq.reactionFx), f(eq.errorFx(), 4) });
        te.rows.push_back({ QStringLiteral("Y"), f(eq.appliedFy), f(eq.reactionFy), f(eq.errorFy(), 4) });
        te.rows.push_back({ QStringLiteral("Z"), f(eq.appliedFz), f(eq.reactionFz), f(eq.errorFz(), 4) });
        se.tables.push_back(te);
        const bool balanced = eq.isBalanced(1e-3);
        if (!balanced) ++issues;
        se.paragraphs.push_back(QStringLiteral("Résidu relatif des forces r = %1 ; résidu relatif des moments = %2 → %3.")
                                    .arg(QString::number(rel, 'e', 2), QString::number(eq.relativeMomentResidual(), 'e', 2),
                                         balanced ? ok(QStringLiteral("ÉQUILIBRE VÉRIFIÉ")) : bad(QStringLiteral("ÉQUILIBRE NON VÉRIFIÉ"))));

        // Contrôle analytique indépendant : charge verticale attendue d'après le modèle (poids propre et charges)
        int combId = 0, caseId = 0;
        analysedLoading(x, &combId, &caseId);
        const auto& lm = x.model.loadManager();
        const auto sums = loadsByCase(x.model);
        const double sw = selfWeightKN(x.model) + surfaceSelfWeightKN(x);
        double surfaceLoads = 0.0;   // charges surfaciques verticales (uniformes : q × aire nette ; ponctuelles : P)
        std::map<int, double> surfaceByCase;
        for (const auto& [id, l] : x.model.loadManager().surfaceLoads())
        {
            double v = 0.0;
            if (l.direction == TSA::Model::SurfaceLoadDirection::Gravity)
            {
                double area = 0.0;
                if (l.targetKind == TSA::Model::SurfaceTargetKind::Slab)
                {
                    if (const auto* s = x.model.getSlab(l.surfaceId)) area = s->area(x.model);
                }
                else if (const auto* w = x.model.getWall(l.surfaceId))
                    area = w->area(x.model);
                v = l.distribution == TSA::Model::SurfaceLoadDistribution::Point ? l.value : l.value * area;
            }
            else
                surfaceLoads += 1.0;   // charge non verticale ou normale : composante verticale non sommée ici
            surfaceByCase[l.loadCaseId] += v;
        }
        std::map<int, double> factors;
        if (const auto* c = combId ? lm.getCombination(combId) : nullptr) factors = c->caseFactors();
        else if (caseId) factors[caseId] = 1.0;
        if (!factors.empty())
        {
            double expected = 0.0;
            bool complete = true;
            for (const auto& [cid, fac] : factors)
            {
                const auto* lc = lm.getLoadCase(cid);
                if (!lc) continue;
                const auto it = sums.find(cid);
                if (it != sums.end())
                {
                    expected += fac * (it->second.lineSum + it->second.nodalFz);
                    complete = complete && it->second.otherLoads == 0;
                }
                if (lc->isSelfWeightIncluded()) expected += fac * lc->selfWeightFactor() * sw;
                expected += fac * surfaceByCase[cid];
            }
            complete = complete && surfaceLoads == 0.0;
            const double computed = -eq.appliedFz;
            const double gap = std::abs(computed - expected) / std::max(std::abs(expected), 1e-9);
            se.paragraphs.push_back(QStringLiteral("Contrôle indépendant de la charge verticale : Σ facteurs × (poids propre + Σ q·L + Σ F<sub>z</sub> + charges surfaciques) = %1 kN "
                                                   "d'après le modèle ; charge appliquée par le moteur = %2 kN ; écart %3 % → %4.")
                                        .arg(f(expected, 1), f(computed, 1), f(gap * 100.0, 3),
                                             !complete ? warn(QStringLiteral("charges non sommées présentes"))
                                                       : gap <= 5e-3 ? ok(QStringLiteral("CONCORDANT")) : warn(QStringLiteral("ÉCART À EXAMINER"))));
        }
    }
    ch.sections.push_back(se);
    doc.addChapter(ch);
}

void chapterViews(Ctx& x, NDCDocument& doc)
{
    auto ch = x.makeChapter(QStringLiteral("Vues du modèle"));
    NDCSection s;
    if (!x.snapshots)
        s.paragraphs.push_back(QStringLiteral("Aucune vue : la note a été produite sans fenêtre 3D."));
    else
    {
        auto add = [&](const QString& type, const QString& caption) {
            const QImage img = x.snapshots(type, x.config.snapshotWidth, x.config.snapshotHeight);
            if (img.isNull()) return;
            NDCFigure fig;
            fig.number = x.figure++;
            fig.caption = caption;
            fig.imageBase64 = imageUri(img);
            fig.widthPercent = 100;
            s.figures.push_back(fig);
        };
        add(QStringLiteral("model"), QStringLiteral("Vue axonométrique du modèle (capture de la vue 3D de %1)").arg(TSA::Product::name()));
        if (x.detailed && x.hasResults()) add(QStringLiteral("deformed"), QStringLiteral("Déformée sous le chargement calculé (amplification graphique)"));
    }
    ch.sections.push_back(s);
    doc.addChapter(ch);
}

void chapterDisplacements(Ctx& x, NDCDocument& doc, int& issues)
{
    auto ch = x.makeChapter(QStringLiteral("Déplacements et flèches"));
    NDCSection s;
    if (!x.hasResults())
    {
        s.paragraphs.push_back(noStaticResults(x));
        ch.sections.push_back(s);
        doc.addChapter(ch);
        return;
    }
    const auto sum = x.results->summary();
    const auto* n = x.model.getNode(sum.maxDisplacementNodeId);
    s.keyValues.push_back({ QStringLiteral("Déplacement maximal"), QStringLiteral("%1 mm au nœud %2%3")
                                                                       .arg(f(sum.maxDisplacement * 1000.0, 2))
                                                                       .arg(sum.maxDisplacementNodeId)
                                                                       .arg(n ? QStringLiteral(" (%1 ; %2 ; %3)").arg(f(n->x()), f(n->y()), f(n->z())) : QString()) });
    std::vector<std::pair<double, int>> nodes;
    for (const auto& [id, d] : x.results->allDisplacements()) nodes.push_back({ d.translationMagnitude(), id });
    std::sort(nodes.rbegin(), nodes.rend());
    auto t = x.makeTable(QStringLiteral("Nœuds les plus déplacés (mm)"), { QStringLiteral("Nœud"), QStringLiteral("U<sub>x</sub>"), QStringLiteral("U<sub>y</sub>"), QStringLiteral("U<sub>z</sub>"), QStringLiteral("|U|") },
                         { QStringLiteral("center"), QStringLiteral("right"), QStringLiteral("right"), QStringLiteral("right"), QStringLiteral("right") });
    for (std::size_t i = 0; i < nodes.size() && int(i) < (x.detailed ? 15 : 5); ++i)
    {
        const auto* d = x.results->getNodeDisplacement(nodes[i].second);
        t.rows.push_back({ QString::number(nodes[i].second), f(d->ux * 1000), f(d->uy * 1000), f(d->uz * 1000), f(nodes[i].first * 1000) });
    }
    s.tables.push_back(t);
    ch.sections.push_back(s);

    // Flèches relatives des poutres et poteaux
    struct D
    {
        double ratio, f, L, xAt;
        Member m;
    };
    std::vector<D> defl;
    for (const auto& mb : x.members)
    {
        if (mb.kind != StructuralElementKind::Beam && mb.kind != StructuralElementKind::Column) continue;
        const auto* r = x.results->getElementResults(mb.kind, mb.id);
        if (!r || mb.length <= 0.0) continue;
        double xAt = 0.0;
        const double d = relativeDeflection(*r, &xAt);
        defl.push_back({ d / mb.length, d, mb.length, xAt, mb });
    }
    std::sort(defl.begin(), defl.end(), [](const D& a, const D& b) { return a.ratio > b.ratio; });
    int over = 0;
    for (const auto& d : defl) over += d.f > d.L / 250.0;
    NDCSection sd;
    sd.title = QStringLiteral("Flèches relatives des barres");
    sd.paragraphs.push_back(QStringLiteral("Écart maximal de la déformée à la corde, sous le chargement calculé (%1). La limite L/250 est une limite "
                                           "d'aptitude au service : la comparaison n'est réglementaire que pour une combinaison ELS ; sous une "
                                           "combinaison ELU elle est indicative et majorante.")
                                .arg(analysedLoading(x)));
    auto td = x.makeTable(QStringLiteral("Barres de plus grande flèche relative"),
                          { QStringLiteral("Barre"), QStringLiteral("Désignation"), QStringLiteral("L (m)"), QStringLiteral("f (mm)"), QStringLiteral("x (m)"), QStringLiteral("L / f"), QStringLiteral("L/250") },
                          { QStringLiteral("left"), QStringLiteral("left"), QStringLiteral("right"), QStringLiteral("right"), QStringLiteral("right"), QStringLiteral("right"), QStringLiteral("center") });
    for (std::size_t i = 0; i < defl.size() && int(i) < x.rowCap(); ++i)
    {
        const auto& d = defl[i];
        td.rows.push_back({ QStringLiteral("%1 %2").arg(kindName(d.m.kind)).arg(d.m.id), d.m.name, f(d.L), f(d.f * 1000), f(d.xAt),
                            d.f > 1e-9 ? QString::number(qRound(d.L / d.f)) : QStringLiteral("—"), d.f <= d.L / 250.0 ? ok() : warn(QStringLiteral("DÉPASSÉE")) });
    }
    sd.tables.push_back(td);
    sd.paragraphs.push_back(QStringLiteral("%1 barre(s) sur %2 dépassent L/250 sous ce chargement. Les barres étant découpées aux nœuds, cette flèche est "
                                           "locale (entre deux nœuds) ; la flèche d'ensemble d'une travée se lit sur les déplacements nodaux ci-dessus "
                                           "et n'est pas comparée automatiquement à une limite.")
                                .arg(over)
                                .arg(defl.size()));
    ch.sections.push_back(sd);
    doc.addChapter(ch);
}

void chapterReactions(Ctx& x, NDCDocument& doc)
{
    auto ch = x.makeChapter(QStringLiteral("Réactions d'appui"));
    NDCSection s;
    if (!x.hasResults())
    {
        s.paragraphs.push_back(noStaticResults(x));
        ch.sections.push_back(s);
        doc.addChapter(ch);
        return;
    }
    double sx = 0, sy = 0, sz = 0, maxR = 0;
    int maxId = 0;
    std::vector<std::pair<double, int>> order;
    for (const auto& [id, r] : x.results->allReactions())
    {
        sx += r.rx, sy += r.ry, sz += r.rz;
        if (r.forceMagnitude() > maxR) maxR = r.forceMagnitude(), maxId = id;
        order.push_back({ r.rz, id });
    }
    s.keyValues.push_back({ QStringLiteral("Σ R<sub>x</sub> ; Σ R<sub>y</sub> ; Σ R<sub>z</sub>"), QStringLiteral("%1 ; %2 ; %3 kN").arg(f(sx, 1), f(sy, 1), f(sz, 1)) });
    s.keyValues.push_back({ QStringLiteral("Réaction maximale"), QStringLiteral("%1 kN au nœud %2").arg(f(maxR, 1)).arg(maxId) });
    if (x.detailed)
    {
        std::sort(order.rbegin(), order.rend());
        auto t = x.makeTable(QStringLiteral("Réactions d'appui (kN, kN·m), par réaction verticale décroissante"),
                             { QStringLiteral("Nœud"), QStringLiteral("R<sub>x</sub>"), QStringLiteral("R<sub>y</sub>"), QStringLiteral("R<sub>z</sub>"), QStringLiteral("M<sub>x</sub>"),
                               QStringLiteral("M<sub>y</sub>"), QStringLiteral("M<sub>z</sub>") },
                             { QStringLiteral("center"), QStringLiteral("right"), QStringLiteral("right"), QStringLiteral("right"), QStringLiteral("right"), QStringLiteral("right"),
                               QStringLiteral("right") });
        for (std::size_t i = 0; i < order.size() && i < 60; ++i)
        {
            const auto* r = x.results->getNodeReaction(order[i].second);
            t.rows.push_back({ QString::number(order[i].second), f(r->rx, 1), f(r->ry, 1), f(r->rz, 1), f(r->mx, 1), f(r->my, 1), f(r->mz, 1) });
        }
        if (order.size() > 60) t.caption += QStringLiteral(" — 60 premières sur %1").arg(order.size());
        s.tables.push_back(t);
    }
    ch.sections.push_back(s);
    doc.addChapter(ch);
}

void chapterForces(Ctx& x, NDCDocument& doc)
{
    auto ch = x.makeChapter(QStringLiteral("Efforts internes et contraintes"));
    NDCSection s;
    if (!x.hasResults())
    {
        s.paragraphs.push_back(noStaticResults(x));
        ch.sections.push_back(s);
        doc.addChapter(ch);
        return;
    }
    s.paragraphs.push_back(QStringLiteral("Convention : repère local de chaque barre, N > 0 en traction, valeurs extrêmes sur les stations de calcul."));
    const auto ex = ResultAnalyzer::analyzeExtrema(x.model, x.results);
    auto t = x.makeTable(QStringLiteral("Valeurs extrêmes et localisation"), { QStringLiteral("Grandeur"), QStringLiteral("Valeur"), QStringLiteral("Élément"), QStringLiteral("Position (X ; Y ; Z)") },
                         { QStringLiteral("left"), QStringLiteral("right"), QStringLiteral("left"), QStringLiteral("center") });
    for (const auto* p : { &ex.maxTensionN, &ex.maxCompressionN, &ex.absMaxBendingMz, &ex.absMaxBendingMy, &ex.absMaxShearVz, &ex.absMaxShearVy })
    {
        if (p->elementId <= 0) continue;
        t.rows.push_back({ p->quantityName, QStringLiteral("%1 %2").arg(f(p->value, 1), p->unit), QStringLiteral("%1 %2").arg(p->elementType).arg(p->elementId),
                           QStringLiteral("%1 ; %2 ; %3").arg(f(p->globalCoords.X()), f(p->globalCoords.Y()), f(p->globalCoords.Z())) });
    }
    s.tables.push_back(t);
    ch.sections.push_back(s);

    struct R
    {
        double sigma;
        Member m;
        double N, Vy, Vz, My, Mz, Mx;
    };
    std::vector<R> rows;
    for (const auto& mb : x.members)
    {
        if (mb.kind == StructuralElementKind::Cable) continue;
        const auto* r = x.results->getElementResults(mb.kind, mb.id);
        if (!r) continue;
        rows.push_back({ maxNormalStress(*r, *mb.section), mb, std::max(std::abs(r->minNormalForce()), std::abs(r->maxNormalForce())) * (std::abs(r->minNormalForce()) > std::abs(r->maxNormalForce()) ? -1.0 : 1.0),
                         absMax(*r, &TSA::Analysis::StationForces::Vy), absMax(*r, &TSA::Analysis::StationForces::Vz), absMax(*r, &TSA::Analysis::StationForces::My),
                         absMax(*r, &TSA::Analysis::StationForces::Mz), absMax(*r, &TSA::Analysis::StationForces::Mx) });
    }
    std::sort(rows.begin(), rows.end(), [](const R& a, const R& b) { return std::abs(a.sigma) > std::abs(b.sigma); });
    NDCSection ss;
    ss.title = QStringLiteral("Barres les plus sollicitées");
    auto ts = x.makeTable(QStringLiteral("Barres classées par contrainte normale de fibre extrême σ (MPa) ; efforts en kN et kN·m (valeurs absolues maximales, N signé)"),
                          { QStringLiteral("Barre"), QStringLiteral("Désignation / section"), QStringLiteral("N"), QStringLiteral("V<sub>y</sub>"), QStringLiteral("V<sub>z</sub>"),
                            QStringLiteral("M<sub>y</sub>"), QStringLiteral("M<sub>z</sub>"), QStringLiteral("M<sub>x</sub>"), QStringLiteral("σ") },
                          { QStringLiteral("left"), QStringLiteral("left"), QStringLiteral("right"), QStringLiteral("right"), QStringLiteral("right"), QStringLiteral("right"),
                            QStringLiteral("right"), QStringLiteral("right"), QStringLiteral("right") });
    for (std::size_t i = 0; i < rows.size() && int(i) < x.rowCap(); ++i)
    {
        const auto& r = rows[i];
        ts.rows.push_back({ QStringLiteral("%1 %2").arg(kindName(r.m.kind)).arg(r.m.id), QStringLiteral("%1<br><small>%2</small>").arg(r.m.name, QString::fromStdString(r.m.section->name)),
                            f(r.N, 1), f(r.Vy, 1), f(r.Vz, 1), f(r.My, 1), f(r.Mz, 1), f(r.Mx, 2), f(r.sigma, 1) });
    }
    ss.tables.push_back(ts);
    ch.sections.push_back(ss);

    if (!x.results->shellResults().empty())
    {
        struct S
        {
            int elements = 0;
            double m = 0, n = 0, v = 0;
        };
        std::map<std::string, S> per;
        auto principal = [](double a, double b, double c) {
            const double mid = 0.5 * (a + b), r = std::sqrt(0.25 * (a - b) * (a - b) + c * c);
            return std::max(std::abs(mid + r), std::abs(mid - r));
        };
        for (const auto& [id, r] : x.results->shellResults())
        {
            auto& s = per[r.source];
            ++s.elements;
            const auto& m = r.mean;
            s.m = std::max(s.m, principal(m[3], m[4], m[5]));
            s.n = std::max(s.n, principal(m[0], m[1], m[2]));
            s.v = std::max(s.v, std::hypot(m[6], m[7]));
        }
        int node = 0;
        const double wmax = x.results->maxSurfaceDisplacement(&node);
        NDCSection sc;
        sc.title = QStringLiteral("Résultantes des coques (dalles, voiles)");
        sc.paragraphs.push_back(QStringLiteral("Résultantes par unité de longueur, moyenne des 4 points d'intégration de chaque élément, valeurs principales "
                                               "(indépendantes du repère local de l'élément) : moment |M|, effort dans le plan |N|, effort tranchant transverse "
                                               "|V| = √(Vxz² + Vyz²). Déplacement maximal des surfaces : %1 mm (nœud %2 du maillage).")
                                    .arg(f(wmax * 1000.0, 2))
                                    .arg(node));
        auto tc = x.makeTable(QStringLiteral("Résultantes maximales par surface"),
                              { QStringLiteral("Surface"), QStringLiteral("Éléments"), QStringLiteral("|M| max (kN·m/m)"), QStringLiteral("|N| max (kN/m)"), QStringLiteral("|V| max (kN/m)") },
                              { QStringLiteral("left"), QStringLiteral("right"), QStringLiteral("right"), QStringLiteral("right"), QStringLiteral("right") });
        for (const auto& [ref, s] : per) tc.rows.push_back({ QString::fromStdString(ref), QString::number(s.elements), f(s.m, 2), f(s.n, 2), f(s.v, 2) });
        sc.tables.push_back(tc);
        ch.sections.push_back(sc);
    }
    doc.addChapter(ch);
}

void chapterModal(Ctx& x, NDCDocument& doc)
{
    const auto& md = x.results->modal();
    auto ch = x.makeChapter(QStringLiteral("Analyse modale"));
    const bool kN = x.results->units().force == "kN";
    const QString mu = kN ? QStringLiteral("t") : QStringLiteral("kg");
    NDCSection sm;
    sm.title = QStringLiteral("Méthode");
    sm.paragraphs.push_back(QStringLiteral("Modes propres non amortis : valeurs et vecteurs propres du problème généralisé (matrice de rigidité K, matrice de "
                                           "masse M concentrée aux nœuds), résolu par OpenSees (commande eigen ; LAPACK dense jusqu'à 3000 DDL libres, Arpack "
                                           "au-delà) :"));
    sm.paragraphs.push_back(formula(QStringLiteral("(K − ω² · M) · φ = 0 ;  f = ω / (2π) ;  T = 1 / f")));
    sm.paragraphs.push_back(QStringLiteral("Masses : barres m = ρ · A par unité de longueur (elasticBeamColumn, truss, corotTruss), coques m = ρ · e par unité "
                                           "d'aire (section ElasticMembranePlateSection), translations seulement. Origine : %1. Conversion d'une charge en masse "
                                           "(cas choisis explicitement) :")
                                .arg(QString::fromStdString(md.massSource)));
    sm.paragraphs.push_back(formula(QStringLiteral("m = F<sub>z,descendante</sub> / g  (g = 9,81 m/s², appliquée sur X, Y et Z)")));
    sm.paragraphs.push_back(QStringLiteral("Participation (OpenSees modalProperties) : facteur Γ<sub>i</sub> = φ<sub>i</sub>ᵀ · M · r / (φ<sub>i</sub>ᵀ · M · φ<sub>i</sub>) "
                                           "et masse effective Γ<sub>i</sub>² · φ<sub>i</sub>ᵀ · M · φ<sub>i</sub>, rapportée à la masse libre (hors DDL bloqués). "
                                           "Les formes propres sont affichées normalisées à une translation maximale unitaire (amplitude sans unité)."));
    ch.sections.push_back(sm);

    NDCSection st;
    st.title = QStringLiteral("Masses");
    auto tm = x.makeTable(QStringLiteral("Masses du modèle de calcul (%1)").arg(mu),
                          { QStringLiteral("Grandeur"), QStringLiteral("X"), QStringLiteral("Y"), QStringLiteral("Z") },
                          { QStringLiteral("left"), QStringLiteral("right"), QStringLiteral("right"), QStringLiteral("right") });
    tm.rows.push_back({ QStringLiteral("Masse totale (y compris nœuds bloqués)"), f(md.totalMass[0], 3), f(md.totalMass[1], 3), f(md.totalMass[2], 3) });
    tm.rows.push_back({ QStringLiteral("Masse libre (participante)"), f(md.totalFreeMass[0], 3), f(md.totalFreeMass[1], 3), f(md.totalFreeMass[2], 3) });
    st.tables.push_back(tm);
    // Contrôle indépendant : masses des barres (ρ·A·L) et des surfaces en coque (ρ·e·aire nette) d'après le modèle
    double members = 0.0, surfaces = 0.0;
    for (const auto& mb : x.members) members += mb.material->density * mb.section->area() * mb.length;
    for (const auto& [id, sl] : x.model.slabs())
        if (sl.formulation() == TSA::Model::SurfaceFormulation::Shell) surfaces += sl.material().mechanical.density * sl.thickness() * sl.area(x.model);
    for (const auto& [id, w] : x.model.walls())
        if (w.formulation() == TSA::Model::SurfaceFormulation::Shell) surfaces += w.material().mechanical.density * w.thickness() * w.area(x.model);
    const double k = kN ? 1e-3 : 1.0;
    st.paragraphs.push_back(QStringLiteral("Contrôle d'après le modèle : barres Σ ρ·A·L = %1 %3 ; surfaces en coque Σ ρ·e·A<sub>nette</sub> = %2 %3 (surfaces maillées "
                                           "et transmises au calcul seulement ; les charges converties s'y ajoutent).")
                                .arg(f(members * k, 3), f(surfaces * k, 3), mu));
    ch.sections.push_back(st);

    NDCSection sr;
    sr.title = QStringLiteral("Fréquences et participation");
    auto tr = x.makeTable(QStringLiteral("Modes propres calculés (%1)").arg(md.modes.size()),
                          { QStringLiteral("Mode"), QStringLiteral("f (Hz)"), QStringLiteral("T (s)"), QStringLiteral("MX (%)"), QStringLiteral("MY (%)"),
                            QStringLiteral("MZ (%)"), QStringLiteral("Σ MX (%)"), QStringLiteral("Σ MY (%)"), QStringLiteral("Σ MZ (%)") },
                          { QStringLiteral("right"), QStringLiteral("right"), QStringLiteral("right"), QStringLiteral("right"), QStringLiteral("right"),
                            QStringLiteral("right"), QStringLiteral("right"), QStringLiteral("right"), QStringLiteral("right") });
    for (const auto& m : md.modes)
        tr.rows.push_back({ QString::number(m.index), f(m.frequency, 3), f(m.period, 4), f(m.massRatio[0], 1), f(m.massRatio[1], 1), f(m.massRatio[2], 1),
                            f(m.cumulativeRatio[0], 1), f(m.cumulativeRatio[1], 1), f(m.cumulativeRatio[2], 1) });
    sr.tables.push_back(tr);
    const auto& last = md.modes.back().cumulativeRatio;
    sr.paragraphs.push_back(QStringLiteral("Masse cumulée des %1 modes : X %2 %, Y %3 %, Z %4 %. Les règles réglementaires (par exemple 90 % de la masse en "
                                           "analyse sismique, EN 1998-1 §4.3.3.3) ne sont pas vérifiées automatiquement ; le nombre de modes est un choix de "
                                           "l'utilisateur. Aucun calcul de réponse (spectre, temporel) n'est effectué.")
                                .arg(md.modes.size())
                                .arg(f(last[0], 1), f(last[1], 1), f(last[2], 1)));
    ch.sections.push_back(sr);
    doc.addChapter(ch);
}

void chapterCables(Ctx& x, NDCDocument& doc, int& issues)
{
    auto ch = x.makeChapter(QStringLiteral("Câbles et haubans"));
    NDCSection s;
    if (x.model.cables().empty())
    {
        s.paragraphs.push_back(QStringLiteral("Sans objet : le modèle ne comporte aucun câble."));
        ch.sections.push_back(s);
        doc.addChapter(ch);
        return;
    }
    // Définitions utilisées
    std::map<std::string, const TSA::Model::Cable*> defs;
    for (const auto& [id, c] : x.model.cables()) defs[c.definition().name()] = &c;
    auto td = x.makeTable(QStringLiteral("Câbles utilisés"), { QStringLiteral("Définition"), QStringLiteral("A (mm²)"), QStringLiteral("E (GPa)"), QStringLiteral("F<sub>pk</sub> (kN)"), QStringLiteral("Nombre") },
                          { QStringLiteral("left"), QStringLiteral("right"), QStringLiteral("right"), QStringLiteral("right"), QStringLiteral("right") });
    for (const auto& [n, c] : defs)
    {
        int count = 0;
        for (const auto& [id, o] : x.model.cables()) count += o.definition().name() == n;
        td.rows.push_back({ QString::fromStdString(n), f(c->definition().area() * 1e6, 0), f(c->definition().elasticModulus() / 1e9, 0),
                            f(c->definition().minimumBreakingForce() / 1e3, 0), QString::number(count) });
    }
    s.tables.push_back(td);
    ch.sections.push_back(s);

    NDCSection sr;
    sr.title = QStringLiteral("Efforts dans les câbles");
    if (!x.hasResults())
        sr.paragraphs.push_back(noStaticResults(x));
    else
    {
        struct C
        {
            int id;
            QString name;
            double L, T0, N, fpk;
        };
        std::vector<C> list;
        int missing = 0;
        for (const auto& mb : x.members)
        {
            if (mb.kind != StructuralElementKind::Cable) continue;
            const auto* c = x.model.getCable(mb.id);
            const auto* r = x.results->getElementResults(StructuralElementKind::Cable, mb.id);
            if (!r)
            {
                ++missing;
                continue;
            }
            list.push_back({ mb.id, mb.name, mb.length, c->initialTension() / 1e3, std::min(r->startForces.N, r->endForces.N), c->definition().minimumBreakingForce() / 1e3 });
        }
        double nMin = 1e30, nMax = -1e30, ratioMax = 0;
        int compressed = 0;
        for (const auto& c : list)
        {
            nMin = std::min(nMin, c.N), nMax = std::max(nMax, c.N);
            if (c.fpk > 0) ratioMax = std::max(ratioMax, c.N / c.fpk);
            compressed += c.N <= 0.0;
        }
        if (compressed || missing) ++issues;
        sr.keyValues.push_back({ QStringLiteral("Câbles avec résultat"), QStringLiteral("%1 sur %2").arg(list.size()).arg(x.model.cables().size()) });
        if (!list.empty())
        {
            sr.keyValues.push_back({ QStringLiteral("Effort normal minimal / maximal"), QStringLiteral("%1 / %2 kN").arg(f(nMin, 0), f(nMax, 0)) });
            sr.keyValues.push_back({ QStringLiteral("Câbles comprimés ou détendus (N ≤ 0)"),
                                     compressed ? bad(QString::number(compressed)) : ok(QStringLiteral("aucun : tous les câbles restent tendus")) });
            sr.keyValues.push_back({ QStringLiteral("Rapport maximal N / F<sub>pk</sub>"), f(ratioMax, 3) });
        }
        sr.paragraphs.push_back(QStringLiteral("Le modèle linéaire ne relâche pas un câble qui se détendrait : le contrôle N > 0 ci-dessus valide a posteriori "
                                               "l'hypothèse d'un câble tendu. Le rapport N / F<sub>pk</sub> est indicatif ; la vérification EN 1993-1-11 "
                                               "complète (résistance avec γ<sub>R</sub>, fatigue, ancrages, déviateurs) n'est pas calculée."));
        if (x.detailed && !list.empty())
        {
            std::sort(list.begin(), list.end(), [](const C& a, const C& b) { return a.N / a.fpk > b.N / b.fpk; });
            auto tc = x.makeTable(QStringLiteral("Effort normal des câbles (kN), par rapport N / F<sub>pk</sub> décroissant"),
                                  { QStringLiteral("Câble"), QStringLiteral("L (m)"), QStringLiteral("T<sub>0</sub>"), QStringLiteral("N calculé"), QStringLiteral("N / F<sub>pk</sub>"),
                                    QStringLiteral("Tendu") },
                                  { QStringLiteral("left"), QStringLiteral("right"), QStringLiteral("right"), QStringLiteral("right"), QStringLiteral("right"), QStringLiteral("center") });
            for (const auto& c : list)
                tc.rows.push_back({ QStringLiteral("%1 %2").arg(c.name).arg(c.id), f(c.L, 1), f(c.T0, 0), f(c.N, 0), f(c.fpk > 0 ? c.N / c.fpk : 0.0, 3),
                                    c.N > 0 ? ok(QStringLiteral("oui")) : bad(QStringLiteral("non")) });
            sr.tables.push_back(tc);
        }
    }
    ch.sections.push_back(sr);
    doc.addChapter(ch);
}

void chapterDesign(Ctx& x, NDCDocument& doc, int& computedChecks, int& failedChecks)
{
    auto ch = x.makeChapter(QStringLiteral("Vérifications réglementaires"));
    NDCSection intro;
    intro.paragraphs.push_back(QStringLiteral("Vérifications réellement calculées par %1 sur les résultats de la présente note (%2). Les autres vérifications "
                                              "nécessaires à un dimensionnement complet sont listées en fin de chapitre et marquées « non calculée ».")
                                   .arg(TSA::Product::name(), analysedLoading(x)));
    ch.sections.push_back(intro);
    if (!x.hasResults())
    {
        doc.addChapter(ch);
        return;
    }

    // 1. Béton armé : flexion simple des poutres rectangulaires (EN 1992-1-1)
    struct Row
    {
        double ratio;
        QStringList cells;
    };
    std::vector<Row> ec2;
    for (const auto& mb : x.members)
    {
        if (mb.kind != StructuralElementKind::Beam || !isConcrete(*mb.material) || mb.section->shape != TSA::Model::SectionShape::Rectangular) continue;
        const auto* r = x.results->getElementResults(mb.kind, mb.id);
        if (!r) continue;
        const double med = std::max(absMax(*r, &TSA::Analysis::StationForces::My), absMax(*r, &TSA::Analysis::StationForces::Mz));
        const auto c = TSA::Standards::Design::ConcreteDesignEC2::calculateFromModel(*mb.section, *mb.material, std::max(med, 0.01) * 1000.0);
        const bool pass = c.valid && c.utilizationRatio <= 1.0;
        ++computedChecks;
        failedChecks += !pass;
        ec2.push_back({ c.utilizationRatio, { QStringLiteral("Poutre %1").arg(mb.id), QStringLiteral("%1<br><small>%2</small>").arg(mb.name, QString::fromStdString(mb.section->name)),
                                              f(med, 1), f(c.As_provided * 1e4, 2), f(c.utilizationRatio * 100.0, 1) + QStringLiteral(" %"), pass ? ok() : bad(QStringLiteral("NON CONFORME")) } });
    }
    if (!ec2.empty())
    {
        std::sort(ec2.begin(), ec2.end(), [](const Row& a, const Row& b) { return a.ratio > b.ratio; });
        NDCSection s;
        s.title = QStringLiteral("Flexion simple des poutres en béton armé (EN 1992-1-1) — calculée");
        s.paragraphs.push_back(QStringLiteral("M<sub>Ed</sub> = moment maximal (|M<sub>y</sub>| ou |M<sub>z</sub>|) de la barre ; acier B500, enrobage 5 cm ; "
                                              "ratio = μ<sub>cu</sub> / μ<sub>lu</sub>. %1 poutre(s) vérifiée(s).")
                               .arg(ec2.size()));
        auto t = x.makeTable(QStringLiteral("Poutres en béton armé, par ratio décroissant"),
                             { QStringLiteral("Barre"), QStringLiteral("Désignation / section"), QStringLiteral("M<sub>Ed</sub> (kN·m)"), QStringLiteral("A<sub>s</sub> (cm²)"), QStringLiteral("Ratio"),
                               QStringLiteral("Statut") },
                             { QStringLiteral("left"), QStringLiteral("left"), QStringLiteral("right"), QStringLiteral("right"), QStringLiteral("right"), QStringLiteral("center") });
        for (std::size_t i = 0; i < ec2.size() && int(i) < x.rowCap(); ++i) t.rows.push_back(ec2[i].cells);
        s.tables.push_back(t);
        ch.sections.push_back(s);
    }

    // 2. Acier : flambement par flexion des barres comprimées (EN 1993-1-1 §6.3.1), axe faible, L = longueur de barre
    std::vector<Row> ec3;
    for (const auto& mb : x.members)
    {
        if (mb.kind == StructuralElementKind::Cable || !isSteel(*mb.material)) continue;
        const auto* r = x.results->getElementResults(mb.kind, mb.id);
        if (!r || r->minNormalForce() >= -1e-3) continue;
        const double ned = -r->minNormalForce();
        const auto c = TSA::Standards::Design::SteelDesignEC3::calculateFromBar(*mb.section, *mb.material, mb.length, 1.0, ned * 1000.0, false);
        const bool pass = c.pass && c.utilizationRatio <= 1.0;
        ++computedChecks;
        failedChecks += !pass;
        ec3.push_back({ c.utilizationRatio, { QStringLiteral("%1 %2").arg(kindName(mb.kind)).arg(mb.id), QStringLiteral("%1<br><small>%2</small>").arg(mb.name, QString::fromStdString(mb.section->name)),
                                              f(ned, 1), f(mb.length, 2), f(c.reducedSlenderness, 2), f(c.chi, 3), f(c.Nb_Rd / 1000.0, 0),
                                              f(c.utilizationRatio * 100.0, 1) + QStringLiteral(" %"), pass ? ok() : bad(QStringLiteral("NON CONFORME")) } });
    }
    if (!ec3.empty())
    {
        std::sort(ec3.begin(), ec3.end(), [](const Row& a, const Row& b) { return a.ratio > b.ratio; });
        NDCSection s;
        s.title = QStringLiteral("Flambement par flexion des barres acier comprimées (EN 1993-1-1 §6.3.1) — calculée");
        s.paragraphs.push_back(QStringLiteral("Effort de compression seul (sans interaction avec la flexion), axe faible, longueur de flambement = longueur de la barre "
                                              "entre nœuds (β = 1). %1 barre(s) vérifiée(s).")
                               .arg(ec3.size()));
        auto t = x.makeTable(QStringLiteral("Barres acier comprimées, par taux de travail décroissant"),
                             { QStringLiteral("Barre"), QStringLiteral("Désignation / section"), QStringLiteral("N<sub>Ed</sub> (kN)"), QStringLiteral("L (m)"), QStringLiteral("λ̄"), QStringLiteral("χ"),
                               QStringLiteral("N<sub>b,Rd</sub> (kN)"), QStringLiteral("η"), QStringLiteral("Statut") },
                             { QStringLiteral("left"), QStringLiteral("left"), QStringLiteral("right"), QStringLiteral("right"), QStringLiteral("right"), QStringLiteral("right"),
                               QStringLiteral("right"), QStringLiteral("right"), QStringLiteral("center") });
        for (std::size_t i = 0; i < ec3.size() && int(i) < x.rowCap(); ++i) t.rows.push_back(ec3[i].cells);
        s.tables.push_back(t);
        ch.sections.push_back(s);
    }

    // 3. Indicateur élastique σ ≤ f_y pour l'acier (pas une vérification EC3 de section)
    std::vector<Row> sig;
    for (const auto& mb : x.members)
    {
        if (mb.kind == StructuralElementKind::Cable || !isSteel(*mb.material)) continue;
        const auto* r = x.results->getElementResults(mb.kind, mb.id);
        if (!r) continue;
        const double s = std::abs(maxNormalStress(*r, *mb.section)), fy = mb.material->fk / 1e6;
        if (fy <= 0) continue;
        sig.push_back({ s / fy, { QStringLiteral("%1 %2").arg(kindName(mb.kind)).arg(mb.id), QStringLiteral("%1<br><small>%2</small>").arg(mb.name, QString::fromStdString(mb.section->name)),
                                  f(s, 1), f(fy, 0), f(s / fy * 100.0, 1) + QStringLiteral(" %"), s <= fy ? ok() : warn(QStringLiteral("σ > f<sub>y</sub>")) } });
    }
    if (!sig.empty())
    {
        std::sort(sig.begin(), sig.end(), [](const Row& a, const Row& b) { return a.ratio > b.ratio; });
        int above = 0;
        for (const auto& r : sig) above += r.ratio > 1.0;
        NDCSection s;
        s.title = QStringLiteral("Contrainte normale élastique des barres acier — indicateur");
        s.paragraphs.push_back(QStringLiteral("σ = N/A ± |M<sub>y</sub>|/W<sub>el,y</sub> ± |M<sub>z</sub>|/W<sub>el,z</sub> comparée à f<sub>y</sub>. Indicateur élastique "
                                              "(sans γ<sub>M0</sub>, sans classe de section ni interaction réglementaire) : ce n'est pas la vérification EC3 §6.2. "
                                              "%1 barre(s) sur %2 dépassent f<sub>y</sub>.")
                               .arg(above)
                               .arg(sig.size()));
        auto t = x.makeTable(QStringLiteral("Barres acier, par rapport σ / f<sub>y</sub> décroissant"),
                             { QStringLiteral("Barre"), QStringLiteral("Désignation / section"), QStringLiteral("|σ| (MPa)"), QStringLiteral("f<sub>y</sub> (MPa)"), QStringLiteral("σ / f<sub>y</sub>"), QStringLiteral("") },
                             { QStringLiteral("left"), QStringLiteral("left"), QStringLiteral("right"), QStringLiteral("right"), QStringLiteral("right"), QStringLiteral("center") });
        for (std::size_t i = 0; i < sig.size() && int(i) < x.rowCap(); ++i) t.rows.push_back(sig[i].cells);
        s.tables.push_back(t);
        ch.sections.push_back(s);
    }

    // 4. Vérifications non calculées
    NDCSection nc;
    nc.title = QStringLiteral("Vérifications non calculées");
    auto t = x.makeTable(QStringLiteral("Vérifications nécessaires non effectuées par la présente note"), { QStringLiteral("Vérification"), QStringLiteral("Référence"), QStringLiteral("Statut") },
                         { QStringLiteral("left"), QStringLiteral("left"), QStringLiteral("center") });
    const bool concrete = std::any_of(x.members.begin(), x.members.end(), [](const Member& m) { return isConcrete(*m.material); });
    const bool steel = std::any_of(x.members.begin(), x.members.end(), [](const Member& m) { return isSteel(*m.material); });
    auto add = [&](const QString& what, const QString& ref) { t.rows.push_back({ what, ref, notComputed() }); };
    if (concrete)
    {
        add(QStringLiteral("Effort tranchant et armatures transversales des poutres"), QStringLiteral("EN 1992-1-1 §6.2"));
        add(QStringLiteral("Poteaux en béton armé : flexion composée, élancement, second ordre"), QStringLiteral("EN 1992-1-1 §5.8, §6.1"));
        add(QStringLiteral("Torsion des sections en béton armé"), QStringLiteral("EN 1992-1-1 §6.3"));
        add(QStringLiteral("États limites de service : fissuration, limitation des contraintes"), QStringLiteral("EN 1992-1-1 §7"));
        add(QStringLiteral("Dispositions constructives et ferraillage minimal détaillé"), QStringLiteral("EN 1992-1-1 §8, §9"));
    }
    if (steel)
    {
        add(QStringLiteral("Résistance des sections acier (classe, flexion composée, cisaillement)"), QStringLiteral("EN 1993-1-1 §5.5, §6.2"));
        add(QStringLiteral("Déversement des poutres acier"), QStringLiteral("EN 1993-1-1 §6.3.2"));
        add(QStringLiteral("Interaction flexion-compression des barres acier"), QStringLiteral("EN 1993-1-1 §6.3.3"));
        add(QStringLiteral("Assemblages (boulonnés, soudés), pieds de poteaux"), QStringLiteral("EN 1993-1-8"));
    }
    if (!x.model.slabs().empty() || !x.model.walls().empty())
        add(computedSurfaces(x).empty() ? QStringLiteral("Dalles et voiles (non transmis au calcul) : flexion, poinçonnement, contreventement")
                                        : QStringLiteral("Dalles et voiles calculés en coques : ferraillage de flexion et d'effort tranchant, poinçonnement, "
                                                         "voiles (flexion composée, cisaillement), ouvertures (renforts)"),
            QStringLiteral("EN 1992-1-1"));
    if (!x.model.foundations().empty()) add(QStringLiteral("Fondations : portance du sol, tassements, ferraillage des semelles"), QStringLiteral("EN 1997-1, EN 1992-1-1"));
    if (!x.model.cables().empty())
    {
        add(QStringLiteral("Câbles : résistance avec γ<sub>R</sub>, fatigue, ancrages et déviateurs"), QStringLiteral("EN 1993-1-11"));
        add(QStringLiteral("Réglage des tensions et phasage de construction"), QStringLiteral("—"));
    }
    add(QStringLiteral("Effets du second ordre (P-Δ) et imperfections globales"), QStringLiteral("EN 1993-1-1 §5.2, EN 1992-1-1 §5.8"));
    add(QStringLiteral("Analyse modale, actions dynamiques et sismiques"), QStringLiteral("EN 1998-1"));
    add(QStringLiteral("Vent (pressions réglementaires, effets dynamiques) et neige"), QStringLiteral("EN 1991-1-3, EN 1991-1-4"));
    add(QStringLiteral("Résistance au feu"), QStringLiteral("EN 1992-1-2, EN 1993-1-2"));
    add(QStringLiteral("Enveloppe de toutes les combinaisons (seul le chargement indiqué est exploité)"), QStringLiteral("EN 1990"));
    nc.tables.push_back(t);
    ch.sections.push_back(nc);
    doc.addChapter(ch);
}

void chapterConclusion(Ctx& x, NDCDocument& doc, int issues, int computedChecks, int failedChecks)
{
    auto ch = x.makeChapter(x.detailed ? QStringLiteral("Conclusion, limites et références") : QStringLiteral("Conclusion et limites"));
    NDCSection s;
    if (!x.hasResults())
        s.paragraphs.push_back(QStringLiteral("Aucun résultat de calcul n'est disponible : seules les données du modèle sont présentées."));
    else
    {
        const auto eq = x.results->equilibrium();
        s.paragraphs.push_back(QStringLiteral("Calcul %1 réussi pour le chargement « %2 ». Équilibre global %3. %4 contrôle(s) du modèle ou des câbles signalent un problème.")
                                   .arg(QString::fromStdString(x.results->executionMetadata().solverEngine), analysedLoading(x),
                                        eq.isBalanced(1e-3) ? QStringLiteral("vérifié") : QStringLiteral("NON vérifié"))
                                   .arg(issues));
        s.paragraphs.push_back(QStringLiteral("%1 vérification(s) réglementaire(s) calculée(s), dont %2 non conforme(s). Les vérifications listées comme "
                                              "« non calculée » restent à effectuer : la présente note ne constitue pas une justification complète de l'ouvrage.")
                                   .arg(computedChecks)
                                   .arg(failedChecks));
    }
    s.paragraphs.push_back(QStringLiteral("Limites : %1").arg(hypotheses(x).mid(2).join(QStringLiteral(" "))));
    ch.sections.push_back(s);
    if (x.detailed)
    {
        NDCSection sb;
        sb.title = QStringLiteral("Références");
        auto t = x.makeTable(QStringLiteral("Références citées"), { QStringLiteral("Réf."), QStringLiteral("Auteur / organisme"), QStringLiteral("Titre"), QStringLiteral("Année") },
                             { QStringLiteral("center"), QStringLiteral("left"), QStringLiteral("left"), QStringLiteral("center") });
        for (const auto& b : NormativeReferenceDetector::generateBibliography(x.model)) t.rows.push_back({ b.citationKey, b.authors, b.title, QString::number(b.year) });
        sb.tables.push_back(t);
        ch.sections.push_back(sb);
    }
    doc.addChapter(ch);
}
} // namespace

// =============================================================================================================

ModelAudit auditModel(const TSA::Model::Model& m)
{
    ModelAudit a;
    const auto members = membersOf(m);
    a.nodes = m.nodes().size();
    a.members = members.size();
    std::map<std::tuple<long long, long long, long long>, int> coords;
    for (const auto& [id, n] : m.nodes())
    {
        if (++coords[{ std::llround(n.x() * 1000), std::llround(n.y() * 1000), std::llround(n.z() * 1000) }] > 1) ++a.duplicateNodes;
        if (n.support().isSupported()) ++a.supportedNodes;
    }
    std::set<std::pair<int, int>> pairs;
    std::map<int, std::vector<int>> adj;
    for (const auto& mb : members)
    {
        if (!pairs.insert({ std::min(mb.n1, mb.n2), std::max(mb.n1, mb.n2) }).second) ++a.duplicateMembers;
        a.totalLength += mb.length;
        if (mb.length < 1e-6)
        {
            ++a.zeroLength;
            continue;
        }
        adj[mb.n1].push_back(mb.n2);
        adj[mb.n2].push_back(mb.n1);
        const auto* p = m.getNode(mb.n1);
        const auto* q = m.getNode(mb.n2);
        if (!p || !q) continue;
        const double ax = p->x(), ay = p->y(), az = p->z(), bx = q->x() - ax, by = q->y() - ay, bz = q->z() - az, L2 = bx * bx + by * by + bz * bz;
        for (const auto& [id, n] : m.nodes())
        {
            if (id == mb.n1 || id == mb.n2) continue;
            const double px = n.x() - ax, py = n.y() - ay, pz = n.z() - az;
            const double t = (px * bx + py * by + pz * bz) / L2;
            if (t <= 1e-9 || t >= 1 - 1e-9) continue;
            const double cx = py * bz - pz * by, cy = pz * bx - px * bz, cz = px * by - py * bx;
            if (std::sqrt((cx * cx + cy * cy + cz * cz) / L2) < 1e-4) ++a.nodesInsideMembers;
        }
    }
    std::set<int> seen;
    for (const auto& [id, n] : m.nodes())
    {
        if (!adj.count(id))
        {
            ++a.isolatedNodes;
            continue;
        }
        if (seen.count(id)) continue;
        ++a.components;
        std::queue<int> q;
        q.push(id);
        seen.insert(id);
        while (!q.empty())
        {
            const int u = q.front();
            q.pop();
            for (int v : adj[u])
                if (seen.insert(v).second) q.push(v);
        }
    }
    return a;
}

double selfWeightKN(const TSA::Model::Model& m)
{
    double w = 0.0;
    for (const auto& mb : membersOf(m)) w += mb.material->density * kGravity * mb.section->area() * mb.length / 1000.0;
    return w;
}

NDCDocument NDCProjectNote::build(const TSA::Model::Model& model, const std::shared_ptr<TSA::Analysis::ResultsModel>& results, const ReportConfiguration& config,
                                  const NDCGenerator::SnapshotProvider& snapshotProvider)
{
    NDCDocument doc;
    doc.config = config;
    if (results && results->isValid() && !results->executionMetadata().solverEngine.empty())
        doc.softwareVersion = QStringLiteral("%1 v%2 — moteur %3 %4").arg(TSA::Product::name(), QString::fromLatin1(TSA::Product::kReportVersion),
                                                                         QString::fromStdString(results->executionMetadata().solverEngine),
                                                                         QString::fromStdString(results->executionMetadata().solverVersion));
    Ctx x { model, results, config, snapshotProvider };
    x.detailed = config.noteLayout == NoteLayout::Detailed;
    x.members = membersOf(model);
    int issues = 0, computed = 0, failed = 0;
    if (x.detailed)
    {
        chapterPresentation(x, doc);
        chapterStandards(x, doc);
        chapterHypotheses(x, doc);
        chapterGeometry(x, doc);
        chapterMaterials(x, doc);
        chapterSections(x, doc);
        chapterSupports(x, doc);
        chapterActions(x, doc);
        chapterCombinations(x, doc);
        chapterMethod(x, doc);
        chapterChecks(x, doc, issues);
        chapterViews(x, doc);
        chapterDisplacements(x, doc, issues);
        chapterReactions(x, doc);
        chapterForces(x, doc);
        if (x.hasModal()) chapterModal(x, doc);
        chapterCables(x, doc, issues);
        chapterDesign(x, doc, computed, failed);
        chapterConclusion(x, doc, issues, computed, failed);
    }
    else
    {
        chapterPresentation(x, doc);
        chapterHypotheses(x, doc);
        chapterActions(x, doc);
        chapterChecks(x, doc, issues);
        chapterViews(x, doc);
        chapterDisplacements(x, doc, issues);
        chapterReactions(x, doc);
        if (x.hasModal()) chapterModal(x, doc);
        if (!model.cables().empty()) chapterCables(x, doc, issues);
        chapterDesign(x, doc, computed, failed);
        chapterConclusion(x, doc, issues, computed, failed);
    }
    return doc;
}

} // namespace TSA::NDC
