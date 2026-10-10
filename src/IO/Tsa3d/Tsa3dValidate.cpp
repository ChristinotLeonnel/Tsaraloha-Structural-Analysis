#include "Tsa3dInternal.h"

#include <QJsonArray>
#include <QRegularExpression>
#include <QSet>

#include <cmath>
#include <map>

namespace TSA::IO::Tsa3d
{

using namespace detail;

namespace
{
struct Ctx
{
    Report& r;
    std::map<QString, QString> idPath;     ///< identifiant → chemin de sa déclaration
    std::map<QString, QString> idKind;     ///< identifiant → collection
    QHash<QString, std::array<double, 3>> nodePos;
};

bool isFiniteNumber(const QJsonValue& v)
{
    return v.isDouble() && std::isfinite(v.toDouble());
}

QJsonArray arrayOf(Ctx& c, const QJsonObject& doc, const QString& key)
{
    if (!doc.contains(key)) return {};
    if (!doc.value(key).isArray())
    {
        c.r.error(key, QStringLiteral("tableau attendu"));
        return {};
    }
    return doc.value(key).toArray();
}

/// Déclare l'identifiant de l'objet ; false s'il manque ou s'il est déjà utilisé.
bool declareId(Ctx& c, const QJsonObject& o, const QString& path, const QString& kind, QString* idOut = nullptr)
{
    const QJsonValue v = o.value("id");
    if (!v.isString() || v.toString().trimmed().isEmpty())
    {
        c.r.error(path + ".id", QStringLiteral("identifiant (chaîne non vide) obligatoire"));
        return false;
    }
    const QString id = v.toString();
    const auto it = c.idPath.find(id);
    if (it != c.idPath.end())
    {
        c.r.error(path + ".id", QStringLiteral("identifiant « %1 » déjà utilisé (%2)").arg(id, it->second));
        return false;
    }
    c.idPath[id] = path;
    c.idKind[id] = kind;
    if (idOut) *idOut = id;
    return true;
}

bool refOk(Ctx& c, const QJsonValue& v, const QString& path, const QStringList& kinds, bool required = true)
{
    if (v.isUndefined() || v.isNull())
    {
        if (required) c.r.error(path, QStringLiteral("référence obligatoire"));
        return !required;
    }
    if (!v.isString())
    {
        c.r.error(path, QStringLiteral("référence (identifiant) attendue"));
        return false;
    }
    const auto it = c.idKind.find(v.toString());
    if (it == c.idKind.end() || !kinds.contains(it->second))
    {
        c.r.error(path, QStringLiteral("« %1 » ne désigne aucun objet de type %2").arg(v.toString(), kinds.join(QStringLiteral(" / "))));
        return false;
    }
    return true;
}

void positive(Ctx& c, const QJsonObject& o, const QString& key, const QString& path, bool required = true)
{
    const QJsonValue v = o.value(key);
    if (v.isUndefined())
    {
        if (required) c.r.error(path + "." + key, QStringLiteral("valeur obligatoire"));
        return;
    }
    if (!isFiniteNumber(v) || v.toDouble() <= 0.0) c.r.error(path + "." + key, QStringLiteral("nombre strictement positif attendu"));
}

void enumField(Ctx& c, const QJsonObject& o, const QString& key, const QString& path, const QStringList& names, bool required = false)
{
    const QJsonValue v = o.value(key);
    if (v.isUndefined())
    {
        if (required) c.r.error(path + "." + key, QStringLiteral("valeur obligatoire (%1)").arg(names.join(", ")));
        return;
    }
    bool ok = false;
    for (const auto& n : names) ok |= n.compare(v.toString(), Qt::CaseInsensitive) == 0;
    if (!ok) c.r.error(path + "." + key, QStringLiteral("« %1 » inconnu (valeurs : %2)").arg(v.toString(), names.join(", ")));
}

void vec3(Ctx& c, const QJsonValue& v, const QString& path, bool required, std::array<double, 3>* out = nullptr)
{
    if (v.isUndefined())
    {
        if (required) c.r.error(path, QStringLiteral("[x, y, z] obligatoire"));
        return;
    }
    const QJsonArray a = v.toArray();
    if (!v.isArray() || a.size() != 3 || !isFiniteNumber(a[0]) || !isFiniteNumber(a[1]) || !isFiniteNumber(a[2]))
    {
        c.r.error(path, QStringLiteral("tableau de 3 nombres attendu"));
        return;
    }
    if (out) *out = { a[0].toDouble(), a[1].toDouble(), a[2].toDouble() };
}

void unknownKeys(Ctx& c, const QJsonObject& o, const QString& path, const QStringList& known)
{
    for (auto it = o.begin(); it != o.end(); ++it)
        if (!known.contains(it.key()))
            c.r.info(path + "." + it.key(), QStringLiteral("champ non interprété par TSA : conservé tel quel"));
    for (const char* k : { "extensions", "properties" })
        if (o.contains(k) && !o.value(k).isObject()) c.r.error(path + "." + k, QStringLiteral("objet attendu"));
}
} // namespace

Report validate(const QJsonObject& doc)
{
    Report report;
    Ctx c { report, {}, {}, {} };

    // --- Format et version ---------------------------------------------------------------------
    if (doc.value("format").toString() != QLatin1String(kFormatName))
        c.r.error("format", QStringLiteral("« %1 » attendu").arg(kFormatName));
    const QJsonValue vv = doc.value("version");
    int major = 0, minor = 0;
    if (vv.isString())
    {
        static const QRegularExpression re(QStringLiteral("^(\\d+)(?:\\.(\\d+))?(?:\\.\\d+)?$"));
        const auto m = re.match(vv.toString());
        if (m.hasMatch())
        {
            major = m.captured(1).toInt();
            minor = m.captured(2).toInt();
        }
    }
    else if (vv.isDouble())
        major = static_cast<int>(vv.toDouble());
    if (major == 0)
        c.r.error("version", QStringLiteral("version « majeur.mineur » obligatoire (ex. « %1 »)").arg(versionString()));
    else if (major > kVersionMajor)
        c.r.error("version", QStringLiteral("version %1 plus récente que celle prise en charge (%2) : import refusé, aucune "
                                            "conversion destructive n'est tentée").arg(vv.toVariant().toString(), versionString()));
    else if (major < kVersionMajor)
        c.r.error("version", QStringLiteral("version %1 non prise en charge").arg(vv.toVariant().toString()));
    else if (minor > kVersionMinor)
        c.r.warning("version", QStringLiteral("version mineure %1.%2 plus récente : champs inconnus conservés sans interprétation")
                                   .arg(major).arg(minor));
    if (c.r.hasErrors()) return report; // format ou version inexploitable : pas d'analyse du contenu

    readUnits(doc, &report);
    if (doc.contains("metadata") && !doc.value("metadata").isObject()) c.r.error("metadata", QStringLiteral("objet attendu"));
    if (doc.contains("extensions") && !doc.value("extensions").isObject()) c.r.error("extensions", QStringLiteral("objet attendu"));
    for (auto it = doc.begin(); it != doc.end(); ++it)
        if (!knownTopLevelKeys().contains(it.key()))
            c.r.info(it.key(), QStringLiteral("bloc non interprété par TSA : conservé tel quel"));

    // --- Déclarations (identifiants) avant les références -------------------------------------
    const QJsonArray materials = arrayOf(c, doc, "materials"), sections = arrayOf(c, doc, "sections"), levels = arrayOf(c, doc, "levels"),
                     nodes = arrayOf(c, doc, "nodes"), members = arrayOf(c, doc, "members"), surfaces = arrayOf(c, doc, "surfaces"),
                     foundations = arrayOf(c, doc, "foundations"), groups = arrayOf(c, doc, "groups");
    const QJsonObject loads = doc.value("loads").toObject();
    if (doc.contains("loads") && !doc.value("loads").isObject()) c.r.error("loads", QStringLiteral("objet attendu"));
    const QJsonArray cases = arrayOf(c, loads, "cases"), nodal = arrayOf(c, loads, "nodal"), memberLoads = arrayOf(c, loads, "member"),
                     combos = arrayOf(c, loads, "combinations");

    auto declareAll = [&](const QJsonArray& arr, const QString& coll, const QString& kind) {
        for (int i = 0; i < arr.size(); ++i)
        {
            if (!arr[i].isObject())
            {
                c.r.error(jsonPath(coll, i), QStringLiteral("objet attendu"));
                continue;
            }
            declareId(c, arr[i].toObject(), jsonPath(coll, i), kind);
        }
    };
    declareAll(materials, "materials", "material");
    declareAll(sections, "sections", "section");
    declareAll(levels, "levels", "level");
    declareAll(nodes, "nodes", "node");
    declareAll(members, "members", "member");
    declareAll(surfaces, "surfaces", "surface");
    declareAll(foundations, "foundations", "foundation");
    declareAll(cases, "loads.cases", "loadCase");
    declareAll(nodal, "loads.nodal", "nodalLoad");
    declareAll(memberLoads, "loads.member", "memberLoad");
    declareAll(combos, "loads.combinations", "combination");
    declareAll(groups, "groups", "group");

    // --- Matériaux, sections, niveaux -------------------------------------------------------------
    for (int i = 0; i < materials.size(); ++i)
    {
        const QJsonObject m = materials[i].toObject();
        const QString p = jsonPath("materials", i);
        enumField(c, m, "type", p, materialTypes());
        positive(c, m, "E", p);
        if (m.contains("nu") && (!isFiniteNumber(m.value("nu")) || m.value("nu").toDouble() <= -1.0 || m.value("nu").toDouble() >= 0.5))
            c.r.error(p + ".nu", QStringLiteral("coefficient de Poisson dans ]-1 ; 0,5[ attendu"));
        if (m.contains("density") && (!isFiniteNumber(m.value("density")) || m.value("density").toDouble() < 0.0))
            c.r.error(p + ".density", QStringLiteral("masse volumique positive attendue"));
        unknownKeys(c, m, p, knownKeys("materials"));
    }
    for (int i = 0; i < sections.size(); ++i)
    {
        const QJsonObject s = sections[i].toObject();
        const QString p = jsonPath("sections", i);
        enumField(c, s, "shape", p, sectionShapes(), true);
        const QJsonObject d = s.value("dimensions").toObject();
        const QString shape = s.value("shape").toString().toLower();
        QStringList need;
        if (shape == "rectangular") need = { "b", "h" };
        else if (shape == "circular") need = { "d" };
        else if (shape == "pipe") need = { "d", "tw" };
        else if (shape == "angle") need = { "b", "h", "tw" };
        else if (!shape.isEmpty()) need = { "b", "h", "tw", "tf" };
        if (!need.isEmpty() && !s.value("dimensions").isObject()) c.r.error(p + ".dimensions", QStringLiteral("objet de dimensions obligatoire"));
        else
            for (const auto& k : need) positive(c, d, k, p + ".dimensions");
        unknownKeys(c, s, p, knownKeys("sections"));
    }
    for (int i = 0; i < levels.size(); ++i)
    {
        const QJsonObject l = levels[i].toObject();
        if (!isFiniteNumber(l.value("elevation"))) c.r.error(jsonPath("levels", i, "elevation"), QStringLiteral("altitude (nombre) obligatoire"));
    }

    // --- Nœuds ----------------------------------------------------------------------------------------
    for (int i = 0; i < nodes.size(); ++i)
    {
        const QJsonObject n = nodes[i].toObject();
        const QString p = jsonPath("nodes", i);
        std::array<double, 3> pos {};
        vec3(c, n.value("position"), p + ".position", true, &pos);
        c.nodePos.insert(n.value("id").toString(), pos);
        if (n.contains("level")) refOk(c, n.value("level"), p + ".level", { "level" });
        if (n.contains("support"))
        {
            const QJsonObject s = n.value("support").toObject();
            const QString sp = p + ".support";
            if (!n.value("support").isObject()) c.r.error(sp, QStringLiteral("objet attendu"));
            for (const char* dof : { "tx", "ty", "tz", "rx", "ry", "rz" })
            {
                enumField(c, s, dof, sp, dofStates());
                if (s.value(dof).toString() == QLatin1String("spring"))
                {
                    const QString k = QStringLiteral("k%1").arg(QString::fromLatin1(dof).mid(0, 1) == "t" ? QString::fromLatin1(dof).mid(1) : QString::fromLatin1(dof));
                    if (!(s.value("stiffness").toObject().value(k).toDouble() > 0.0))
                        c.r.warning(sp + ".stiffness." + k, QStringLiteral("raideur positive attendue pour un DDL élastique"));
                }
            }
            if (s.contains("orientation")) enumField(c, s.value("orientation").toObject(), "type", sp + ".orientation", orientationTypes());
        }
        unknownKeys(c, n, p, knownKeys("nodes"));
    }

    // --- Barres -------------------------------------------------------------------------------------
    for (int i = 0; i < members.size(); ++i)
    {
        const QJsonObject m = members[i].toObject();
        const QString p = jsonPath("members", i);
        enumField(c, m, "type", p, memberTypes(), true);
        enumField(c, m, "role", p, barRoles());
        enumField(c, m, "eccentricity", p, eccentricities());
        const QJsonArray mn = m.value("nodes").toArray();
        if (!m.value("nodes").isArray() || mn.size() != 2)
            c.r.error(p + ".nodes", QStringLiteral("deux nœuds attendus [début, fin]"));
        else
        {
            const bool a = refOk(c, mn[0], p + ".nodes[0]", { "node" }), b = refOk(c, mn[1], p + ".nodes[1]", { "node" });
            if (a && b)
            {
                if (mn[0].toString() == mn[1].toString()) c.r.error(p + ".nodes", QStringLiteral("une barre relie deux nœuds distincts"));
                else
                {
                    const auto pa = c.nodePos.value(mn[0].toString()), pb = c.nodePos.value(mn[1].toString());
                    if (std::hypot(pa[0] - pb[0], pa[1] - pb[1], pa[2] - pb[2]) < 1e-9)
                        c.r.error(p + ".nodes", QStringLiteral("barre de longueur nulle (nœuds confondus)"));
                }
            }
        }
        if (m.contains("section")) refOk(c, m.value("section"), p + ".section", { "section" });
        else c.r.warning(p + ".section", QStringLiteral("section absente : section par défaut de TSA"));
        if (m.contains("material")) refOk(c, m.value("material"), p + ".material", { "material" });
        else c.r.warning(p + ".material", QStringLiteral("matériau absent : matériau par défaut de TSA"));
        if (m.contains("rotation") && !isFiniteNumber(m.value("rotation"))) c.r.error(p + ".rotation", QStringLiteral("nombre attendu"));
        if (m.contains("truss")) enumField(c, m.value("truss").toObject(), "role", p + ".truss", trussRoles());
        if (m.contains("cable"))
        {
            enumField(c, m.value("cable").toObject(), "type", p + ".cable", cableTypes());
            enumField(c, m.value("cable").toObject(), "geometry", p + ".cable", cableModes());
        }
        unknownKeys(c, m, p, knownKeys("members"));
    }

    // --- Surfaces, fondations -------------------------------------------------------------------------
    for (int i = 0; i < surfaces.size(); ++i)
    {
        const QJsonObject s = surfaces[i].toObject();
        const QString p = jsonPath("surfaces", i);
        enumField(c, s, "type", p, surfaceTypes(), true);
        const QJsonArray sn = s.value("nodes").toArray();
        const bool wall = s.value("type").toString() == QLatin1String("wall");
        if (wall ? sn.size() != 2 : sn.size() < 3)
            c.r.error(p + ".nodes", wall ? QStringLiteral("un voile est défini par deux nœuds (pied gauche, pied droit)")
                                         : QStringLiteral("une dalle a au moins trois nœuds"));
        QSet<QString> seen;
        for (int k = 0; k < sn.size(); ++k)
        {
            refOk(c, sn[k], QStringLiteral("%1.nodes[%2]").arg(p).arg(k), { "node" });
            if (seen.contains(sn[k].toString())) c.r.error(QStringLiteral("%1.nodes[%2]").arg(p).arg(k), QStringLiteral("nœud répété"));
            seen.insert(sn[k].toString());
        }
        positive(c, s, "thickness", p);
        if (wall) positive(c, s, "height", p);
        if (s.contains("material")) refOk(c, s.value("material"), p + ".material", { "material" });
        enumField(c, s, "slabType", p, slabTypes());
        unknownKeys(c, s, p, knownKeys("surfaces"));
    }
    for (int i = 0; i < foundations.size(); ++i)
    {
        const QJsonObject f = foundations[i].toObject();
        const QString p = jsonPath("foundations", i);
        enumField(c, f, "type", p, foundationTypes());
        refOk(c, f.value("node"), p + ".node", { "node" });
        const QJsonObject d = f.value("dimensions").toObject();
        for (const char* k : { "a", "b", "h" }) positive(c, d, k, p + ".dimensions");
        if (f.contains("material")) refOk(c, f.value("material"), p + ".material", { "material" });
        unknownKeys(c, f, p, knownKeys("foundations"));
    }

    // --- Charges ------------------------------------------------------------------------------------
    for (int i = 0; i < cases.size(); ++i)
    {
        const QJsonObject lc = cases[i].toObject();
        enumField(c, lc, "category", jsonPath("loads.cases", i), loadCaseCategories());
        unknownKeys(c, lc, jsonPath("loads.cases", i), knownKeys("loadCases"));
    }
    for (int i = 0; i < nodal.size(); ++i)
    {
        const QJsonObject l = nodal[i].toObject();
        const QString p = jsonPath("loads.nodal", i);
        refOk(c, l.value("node"), p + ".node", { "node" });
        refOk(c, l.value("case"), p + ".case", { "loadCase" });
        vec3(c, l.value("force"), p + ".force", false);
        vec3(c, l.value("moment"), p + ".moment", false);
        if (!l.contains("force") && !l.contains("moment")) c.r.error(p, QStringLiteral("force et / ou moment attendus"));
        enumField(c, l, "coordinateSystem", p, coordSystems());
        unknownKeys(c, l, p, knownKeys("nodalLoads"));
    }
    for (int i = 0; i < memberLoads.size(); ++i)
    {
        const QJsonObject l = memberLoads[i].toObject();
        const QString p = jsonPath("loads.member", i);
        refOk(c, l.value("member"), p + ".member", { "member" });
        refOk(c, l.value("case"), p + ".case", { "loadCase" });
        enumField(c, l, "kind", p, memberLoadKinds(), true);
        enumField(c, l, "direction", p, loadDirections());
        enumField(c, l, "coordinateSystem", p, coordSystems());
        if (!isFiniteNumber(l.value("q1"))) c.r.error(p + ".q1", QStringLiteral("intensité q1 (nombre) obligatoire"));
        const QString kind = l.value("kind").toString();
        if (kind == "trapezoidal" && !isFiniteNumber(l.value("q2"))) c.r.error(p + ".q2", QStringLiteral("intensité q2 obligatoire (trapézoïdale)"));
        if (kind == "point" && !isFiniteNumber(l.value("x1"))) c.r.error(p + ".x1", QStringLiteral("position x1 obligatoire (charge ponctuelle)"));
        if (l.contains("x1") && l.contains("x2") && l.value("x2").toDouble() < l.value("x1").toDouble())
            c.r.error(p + ".x2", QStringLiteral("x2 doit être supérieur ou égal à x1"));
        if (l.value("relative").toBool())
            for (const char* k : { "x1", "x2" })
                if (l.contains(k) && (l.value(k).toDouble() < 0.0 || l.value(k).toDouble() > 1.0))
                    c.r.error(p + "." + k, QStringLiteral("position relative dans [0 ; 1] attendue"));
        unknownKeys(c, l, p, knownKeys("memberLoads"));
    }
    for (int i = 0; i < combos.size(); ++i)
    {
        const QJsonObject co = combos[i].toObject();
        const QString p = jsonPath("loads.combinations", i);
        enumField(c, co, "type", p, combinationTypes());
        const QJsonArray f = co.value("factors").toArray();
        if (f.isEmpty()) c.r.warning(p + ".factors", QStringLiteral("combinaison sans facteur"));
        for (int k = 0; k < f.size(); ++k)
        {
            const QJsonObject fo = f[k].toObject();
            refOk(c, fo.value("case"), QStringLiteral("%1.factors[%2].case").arg(p).arg(k), { "loadCase" });
            if (!isFiniteNumber(fo.value("factor"))) c.r.error(QStringLiteral("%1.factors[%2].factor").arg(p).arg(k), QStringLiteral("nombre attendu"));
        }
        unknownKeys(c, co, p, knownKeys("combinations"));
    }
    if (!loads.value("surface").toArray().isEmpty())
        c.r.warning("loads.surface", QStringLiteral("charges surfaciques : non prises en charge par le modèle TSA actuel, conservées sans être appliquées"));

    // --- Groupes ------------------------------------------------------------------------------------
    for (int i = 0; i < groups.size(); ++i)
    {
        const QJsonArray objs = groups[i].toObject().value("objects").toArray();
        for (int k = 0; k < objs.size(); ++k)
        {
            const QString ref = objs[k].toString();
            if (!c.idKind.count(ref)) c.r.error(QStringLiteral("groups[%1].objects[%2]").arg(i).arg(k), QStringLiteral("« %1 » inconnu").arg(ref));
        }
    }

    // --- Maillage (informatif) -------------------------------------------------------------------------
    if (doc.contains("mesh"))
    {
        const QJsonObject mesh = doc.value("mesh").toObject();
        const QJsonArray mnodes = mesh.value("nodes").toArray(), melems = mesh.value("elements").toArray();
        QSet<QString> meshNodes;
        for (int i = 0; i < mnodes.size(); ++i)
        {
            const QJsonObject n = mnodes[i].toObject();
            const QString p = QStringLiteral("mesh.nodes[%1]").arg(i);
            if (declareId(c, n, p, "meshNode")) meshNodes.insert(n.value("id").toString());
            vec3(c, n.value("position"), p + ".position", true);
            if (n.contains("origin") && !n.value("origin").isNull()) refOk(c, n.value("origin"), p + ".origin", { "node" });
        }
        for (int i = 0; i < melems.size(); ++i)
        {
            const QJsonObject e = melems[i].toObject();
            const QString p = QStringLiteral("mesh.elements[%1]").arg(i);
            declareId(c, e, p, "meshElement");
            enumField(c, e, "type", p, meshCellTypes(), true);
            const QString t = e.value("type").toString();
            const int expected = t == "tri3" ? 3 : (t == "quad4" || t == "tet4") ? 4 : t == "hex8" ? 8 : 2;
            const QJsonArray en = e.value("nodes").toArray();
            if (en.size() != expected) c.r.error(p + ".nodes", QStringLiteral("%1 nœud(s) attendu(s) pour « %2 »").arg(expected).arg(t));
            for (int k = 0; k < en.size(); ++k)
                if (!meshNodes.contains(en[k].toString()))
                    c.r.error(QStringLiteral("%1.nodes[%2]").arg(p).arg(k), QStringLiteral("nœud de maillage « %1 » inconnu").arg(en[k].toString()));
            if (e.contains("origin") && !e.value("origin").isNull()) refOk(c, e.value("origin"), p + ".origin", { "member", "surface" }, false);
        }
        c.r.info("mesh", QStringLiteral("maillage : informatif, jamais appliqué au modèle (le moteur maille lui-même)"));
    }
    if (doc.contains("results"))
        c.r.warning("results", QStringLiteral("résultats présents : ils ne seront pas chargés comme résultats de calcul validés"));
    return report;
}

} // namespace TSA::IO::Tsa3d
