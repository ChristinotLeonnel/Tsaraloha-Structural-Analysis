// Suite « modules » : architecture de modules (tests 274-279, docs/SDK.md). Validation du manifeste
// (structure, identifiants, chemins confinés, capacités), compatibilité d'hôte, dépendances (absentes,
// versions, facultatives, cycles, propagation), confiance (modules utilisateur à approuver, empreinte),
// activation ordonnée et arrêt inverse, convertisseur réel du module d'exemple en processus séparé.

#include "test_common.h"

#include "Core/AppPaths.h"
#include "IO/Tsa3d/Tsa3d.h"
#include "Modules/ModuleRegistry.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTemporaryDir>

using namespace TSA::Modules;

namespace
{
QJsonObject baseManifest(const QString& id, const QString& version = "1.0.0")
{
    return QJsonObject { { "schema", "tsa-module/1" },
                         { "id", id },
                         { "name", "Module " + id },
                         { "version", version },
                         { "hosts", QJsonArray { QJsonObject { { "application", "TSA" }, { "min", "0.1.0" } } } } };
}

/// Écrit <root>/<dossier>/module.json (et des fichiers vides éventuels) ; rend le dossier du module.
QString writeModule(const QString& root, const QString& folder, const QJsonObject& manifest, const QStringList& files = {})
{
    const QString dir = root + "/" + folder;
    QDir().mkpath(dir);
    for (const QString& f : files)
    {
        QDir().mkpath(QFileInfo(dir + "/" + f).absolutePath());
        QFile out(dir + "/" + f);
        (void)out.open(QIODevice::WriteOnly);
    }
    QFile f(dir + "/module.json");
    (void)f.open(QIODevice::WriteOnly | QIODevice::Truncate);
    f.write(QJsonDocument(manifest).toJson());
    return dir;
}

bool manifestErrors(const QJsonObject& j, const QString& dir, const QString& needle)
{
    QStringList errors;
    const bool ok = ModuleManifest::fromJson(j, dir, nullptr, &errors);
    return !ok && errors.join('\n').contains(needle);
}

ModuleState stateOf(const ModuleRegistry& r, const QString& id)
{
    const auto* m = r.module(id);
    return m ? m->state : ModuleState::Invalid;
}
} // namespace

bool runSuite_Modules(int& passed)
{
    // -------------------------------------------------------------------------
    // TEST 274 : manifeste — validation complète
    // -------------------------------------------------------------------------
    {
        QTemporaryDir tmp;
        const QString dir = writeModule(tmp.path(), "m", baseManifest("acme.tool"), { "bin/conv.exe", "tpl/a.tsatemplate", "plugin.dll" });
        QJsonObject j = baseManifest("acme.tool");
        QStringList errors;
        TEST_CHECK(ModuleManifest::fromJson(j, dir, nullptr, &errors), "Test 274: manifeste minimal valide (" << errors.join(" | ").toStdString() << ")");

        auto k = j;
        k["schema"] = "tsa-module/9";
        TEST_CHECK(manifestErrors(k, dir, "schema"), "Test 274: schéma inconnu refusé");
        k = j;
        k["id"] = "Acme Tool";
        TEST_CHECK(manifestErrors(k, dir, "id"), "Test 274: identifiant invalide refusé");
        k = j;
        k["version"] = "v1";
        TEST_CHECK(manifestErrors(k, dir, "version"), "Test 274: version invalide refusée");
        k = j;
        k["hosts"] = QJsonArray {};
        TEST_CHECK(manifestErrors(k, dir, "hosts"), "Test 274: hôte obligatoire");

        auto conv = [](const QString& id, const QString& cmd) {
            return QJsonObject { { "entryPoints", QJsonObject { { "importers", QJsonArray { QJsonObject {
                                                                     { "id", id }, { "extensions", QJsonArray { "csv" } }, { "command", cmd } } } } } } };
        };
        auto with = [&](QJsonObject extra) {
            auto m = j;
            for (auto it = extra.begin(); it != extra.end(); ++it) m[it.key()] = it.value();
            return m;
        };
        auto okConv = with(conv("acme.tool.csv", "bin/conv.exe"));
        okConv["capabilities"] = QJsonArray { "process" };
        TEST_CHECK(ModuleManifest::fromJson(okConv, dir, nullptr, &errors), "Test 274: convertisseur valide (" << errors.join(" | ").toStdString() << ")");
        auto bad = okConv;
        bad["entryPoints"] = conv("acme.tool.csv", "../outside.exe").value("entryPoints");
        TEST_CHECK(manifestErrors(bad, dir, "refusé"), "Test 274: chemin « .. » refusé (confinement au dossier du module)");
        bad["entryPoints"] = conv("acme.tool.csv", "C:/Windows/System32/cmd.exe").value("entryPoints");
        TEST_CHECK(manifestErrors(bad, dir, "refusé"), "Test 274: chemin absolu refusé");
        bad["entryPoints"] = conv("acme.tool.csv", "bin/absent.exe").value("entryPoints");
        TEST_CHECK(manifestErrors(bad, dir, "absent"), "Test 274: exécutable absent signalé");
        bad["entryPoints"] = conv("other.csv", "bin/conv.exe").value("entryPoints");
        TEST_CHECK(manifestErrors(bad, dir, "préfixé"), "Test 274: identifiant de convertisseur non préfixé refusé");
        TEST_CHECK(manifestErrors(with(conv("acme.tool.csv", "bin/conv.exe")), dir, "process"), "Test 274: capacité « process » non déclarée refusée");
        auto caps = j;
        caps["capabilities"] = QJsonArray { "network" };
        TEST_CHECK(manifestErrors(caps, dir, "inconnue"), "Test 274: capacité inconnue refusée");
        auto tpl = j;
        tpl["templates"] = QJsonArray { "tpl/a.tsatemplate" };
        TEST_CHECK(manifestErrors(tpl, dir, "templates"), "Test 274: capacité « templates » exigée");
        tpl["capabilities"] = QJsonArray { "templates" };
        TEST_CHECK(ModuleManifest::fromJson(tpl, dir, nullptr, &errors), "Test 274: paquet de templates déclaré valide");
        auto self = j;
        self["dependencies"] = QJsonArray { QJsonObject { { "id", "acme.tool" }, { "min", "1.0" } } };
        TEST_CHECK(manifestErrors(self, dir, "lui-même"), "Test 274: dépendance à soi-même refusée");
        std::cout << "[PASS] Test 274: Manifeste de module validé" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 275 : compatibilité d'hôte et manifeste illisible
    // -------------------------------------------------------------------------
    {
        QTemporaryDir tmp;
        const QString root = tmp.path() + "/shipped";
        auto future = baseManifest("acme.future");
        future["hosts"] = QJsonArray { QJsonObject { { "application", "TSA" }, { "min", "9.0" } } };
        writeModule(root, "a", future);
        auto lab = baseManifest("acme.labonly");
        lab["hosts"] = QJsonArray { QJsonObject { { "application", "TSALab" }, { "min", "0.1" } } };
        writeModule(root, "b", lab);
        auto old = baseManifest("acme.old");
        old["hosts"] = QJsonArray { QJsonObject { { "application", "TSA" }, { "min", "0.0.1" }, { "max", "0.0.9" } } };
        writeModule(root, "c", old);
        writeModule(root, "d", baseManifest("acme.ok"));
        QDir().mkpath(root + "/e");
        QFile broken(root + "/e/module.json");
        (void)broken.open(QIODevice::WriteOnly);
        broken.write("{ pas du json");
        broken.close();

        ModuleRegistry r("TSA", "0.1.0");
        r.setTrustFile(tmp.path() + "/trust.json");
        r.discover({ root }, {});
        TEST_CHECK(r.modules().size() == 5, "Test 275: 5 modules découverts");
        TEST_CHECK(stateOf(r, "acme.future") == ModuleState::Incompatible, "Test 275: version d'hôte trop ancienne → incompatible");
        TEST_CHECK(stateOf(r, "acme.labonly") == ModuleState::Incompatible, "Test 275: module TSALab seul → incompatible avec TSA");
        TEST_CHECK(stateOf(r, "acme.old") == ModuleState::Incompatible, "Test 275: borne max dépassée → incompatible");
        TEST_CHECK(stateOf(r, "acme.ok") == ModuleState::Ready, "Test 275: module livré compatible prêt");
        TEST_CHECK(stateOf(r, "e") == ModuleState::Invalid && r.module("e")->messages.join(" ").contains("illisible"),
                   "Test 275: manifeste illisible → invalide, message explicite");
        ModuleRegistry lr("TSALab", "0.1.0");
        lr.setTrustFile(tmp.path() + "/trust.json");
        lr.discover({ root }, {});
        TEST_CHECK(stateOf(lr, "acme.labonly") == ModuleState::Ready, "Test 275: même module prêt dans TSALab");
        std::cout << "[PASS] Test 275: Compatibilité d'hôte" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 276 : dépendances — absentes, versions, facultatives, cycles, propagation
    // -------------------------------------------------------------------------
    {
        QTemporaryDir tmp;
        const QString root = tmp.path() + "/shipped";
        auto dep = [](QJsonObject m, const QString& id, const QString& min, bool optional = false) {
            QJsonArray a = m.value("dependencies").toArray();
            a.append(QJsonObject { { "id", id }, { "min", min }, { "optional", optional } });
            m["dependencies"] = a;
            return m;
        };
        writeModule(root, "base", baseManifest("acme.base", "1.2.0"));
        writeModule(root, "user", dep(baseManifest("acme.user"), "acme.base", "1.0"));
        writeModule(root, "toonew", dep(baseManifest("acme.toonew"), "acme.base", "2.0"));
        writeModule(root, "missing", dep(baseManifest("acme.missing"), "acme.ghost", "1.0"));
        writeModule(root, "chain", dep(baseManifest("acme.chain"), "acme.missing", "1.0"));
        writeModule(root, "opt", dep(baseManifest("acme.opt"), "acme.ghost", "1.0", true));
        writeModule(root, "cyc1", dep(baseManifest("acme.cyc1"), "acme.cyc2", "1.0"));
        writeModule(root, "cyc2", dep(baseManifest("acme.cyc2"), "acme.cyc1", "1.0"));
        ModuleRegistry r("TSA", "0.1.0");
        r.setTrustFile(tmp.path() + "/trust.json");
        r.discover({ root }, {});
        TEST_CHECK(stateOf(r, "acme.user") == ModuleState::Ready, "Test 276: dépendance satisfaite");
        TEST_CHECK(stateOf(r, "acme.toonew") == ModuleState::MissingDependency && r.module("acme.toonew")->messages.join(" ").contains("2.0.0"),
                   "Test 276: version de dépendance insuffisante signalée");
        TEST_CHECK(stateOf(r, "acme.missing") == ModuleState::MissingDependency, "Test 276: dépendance absente");
        TEST_CHECK(stateOf(r, "acme.chain") == ModuleState::MissingDependency, "Test 276: propagation (dépendance elle-même inactive)");
        TEST_CHECK(stateOf(r, "acme.opt") == ModuleState::Ready, "Test 276: dépendance facultative absente tolérée");
        TEST_CHECK(stateOf(r, "acme.cyc1") == ModuleState::DependencyCycle && stateOf(r, "acme.cyc2") == ModuleState::DependencyCycle,
                   "Test 276: dépendances circulaires détectées");
        ModuleHostServices none;
        const int active = r.activate(none);
        TEST_CHECK(active == 3 && stateOf(r, "acme.cyc1") != ModuleState::Active, "Test 276: seuls les modules prêts sont activés (" << active << ")");
        std::cout << "[PASS] Test 276: Résolution des dépendances" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 277 : confiance — module utilisateur à approuver, empreinte, doublon
    // -------------------------------------------------------------------------
    {
        QTemporaryDir tmp;
        const QString shipped = tmp.path() + "/shipped", user = tmp.path() + "/user", trust = tmp.path() + "/cfg/trust.json";
        writeModule(shipped, "base", baseManifest("acme.base"));
        writeModule(user, "dup", baseManifest("acme.base"));
        const QString udir = writeModule(user, "ext", baseManifest("third.ext"));
        ModuleRegistry r("TSA", "0.1.0");
        r.setTrustFile(trust);
        r.discover({ shipped }, { user });
        TEST_CHECK(stateOf(r, "acme.base") == ModuleState::Ready, "Test 277: module livré approuvé d'office");
        TEST_CHECK(stateOf(r, "third.ext") == ModuleState::Untrusted, "Test 277: module utilisateur en attente d'approbation");
        bool dupInvalid = false;
        for (const auto& m : r.modules())
            if (m.dir.endsWith("/dup")) dupInvalid = m.state == ModuleState::Invalid && m.messages.join(" ").contains("déjà fourni");
        TEST_CHECK(dupInvalid, "Test 277: identifiant déjà fourni par un module livré refusé (pas de substitution)");
        ModuleHostServices none;
        r.activate(none);
        TEST_CHECK(stateOf(r, "third.ext") == ModuleState::Untrusted, "Test 277: module non approuvé jamais activé");
        QString err;
        TEST_CHECK(!r.approve("acme.base", &err), "Test 277: un module livré ne s'approuve pas");
        TEST_CHECK(r.approve("third.ext", &err) && stateOf(r, "third.ext") == ModuleState::Ready, "Test 277: approbation (" << err.toStdString() << ")");
        QFile tf(trust);
        TEST_CHECK(tf.open(QIODevice::ReadOnly) && QString::fromUtf8(tf.readAll()).contains("third.ext@" + r.module("third.ext")->manifestSha256),
                   "Test 277: empreinte du manifeste mémorisée");
        tf.close();
        r.activate(none);
        TEST_CHECK(stateOf(r, "third.ext") == ModuleState::Active, "Test 277: module approuvé activé");

        ModuleRegistry again("TSA", "0.1.0");
        again.setTrustFile(trust);
        again.discover({ shipped }, { user });
        TEST_CHECK(stateOf(again, "third.ext") == ModuleState::Ready, "Test 277: approbation conservée au lancement suivant");
        auto changed = baseManifest("third.ext", "1.0.1");
        writeModule(user, "ext", changed);
        again.discover({ shipped }, { user });
        TEST_CHECK(stateOf(again, "third.ext") == ModuleState::Untrusted, "Test 277: manifeste modifié → nouvelle approbation exigée");
        again.setEnabled("acme.base", false);
        again.discover({ shipped }, { user });
        TEST_CHECK(stateOf(again, "acme.base") == ModuleState::Disabled, "Test 277: module désactivé non activé");
        Q_UNUSED(udir);
        std::cout << "[PASS] Test 277: Confiance et approbation des modules" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 278 : activation dans l'ordre des dépendances, arrêt inverse, échec propre
    // -------------------------------------------------------------------------
    {
        QTemporaryDir tmp;
        const QString root = tmp.path() + "/shipped";
        auto withTpl = [](QJsonObject m) {
            m["templates"] = QJsonArray { "t.tsatemplate" };
            m["capabilities"] = QJsonArray { "templates" };
            return m;
        };
        auto top = withTpl(baseManifest("a.top"));   // nom avant « z.base » : l'ordre alphabétique ne suffit pas
        top["dependencies"] = QJsonArray { QJsonObject { { "id", "z.base" }, { "min", "1.0" } } };
        writeModule(root, "1top", top, { "t.tsatemplate" });
        writeModule(root, "2base", withTpl(baseManifest("z.base")), { "t.tsatemplate" });
        auto plug = baseManifest("p.plugin");
        plug["entryPoints"] = QJsonObject { { "plugin", "p.dll" } };
        plug["capabilities"] = QJsonArray { "plugin" };
        writeModule(root, "3plug", plug, { "p.dll" });

        QStringList events;
        ModuleHostServices s;
        s.registerTemplate = [&](const QString& path, const QString& id, QString*) {
            events << "reg:" + id;
            return QFileInfo::exists(path);
        };
        s.unregisterTemplates = [&](const QString& id) { events << "unreg:" + id; };
        s.loadPlugin = [&](const QString&, QString* e) {
            *e = "ABI incompatible";
            return false;
        };
        s.stopPlugin = [&](const QString&) { events << "stop"; };
        ModuleRegistry r("TSA", "0.1.0");
        r.setTrustFile(tmp.path() + "/trust.json");
        r.discover({ root }, {});
        const int active = r.activate(s);
        TEST_CHECK(active == 2 && events.indexOf("reg:z.base") < events.indexOf("reg:a.top") && events.indexOf("reg:z.base") >= 0,
                   "Test 278: dépendance activée avant le module qui en dépend (" << events.join(",").toStdString() << ")");
        TEST_CHECK(stateOf(r, "p.plugin") == ModuleState::Failed && r.module("p.plugin")->messages.join(" ").contains("ABI incompatible"),
                   "Test 278: plugin refusé → module en échec, raison conservée");
        TEST_CHECK(events.contains("unreg:p.plugin"), "Test 278: échec d'activation → contributions retirées");
        events.clear();
        r.shutdown(s);
        TEST_CHECK(events.indexOf("unreg:a.top") >= 0 && events.indexOf("unreg:a.top") < events.indexOf("unreg:z.base"),
                   "Test 278: arrêt dans l'ordre inverse des dépendances (" << events.join(",").toStdString() << ")");
        TEST_CHECK(stateOf(r, "a.top") == ModuleState::Ready && r.importers().empty(), "Test 278: modules arrêtés");
        std::cout << "[PASS] Test 278: Activation ordonnée et arrêt des modules" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 279 : module d'exemple — convertisseur réel en processus séparé → TSA3D validé
    // -------------------------------------------------------------------------
    {
        const QString sample = QStringLiteral(TSA_SAMPLE_MODULE_DIR);
        TEST_CHECK(QFileInfo::exists(sample + "/module.json") && QFileInfo::exists(sample + "/bin/sample_csv2tsa3d.exe"),
                   "Test 279: module d'exemple assemblé dans le dossier de compilation (" << sample.toStdString() << ")");
        TEST_CHECK(QFileInfo(TSA::Core::AppPaths::shippedModulesDir() + "/sample.csvimport/module.json").exists(),
                   "Test 279: AppPaths::shippedModulesDir trouve les modules livrés (" << TSA::Core::AppPaths::shippedModulesDir().toStdString() << ")");
        QTemporaryDir tmp;
        ModuleRegistry r("TSA", "0.1.0");
        r.setTrustFile(tmp.path() + "/trust.json");
        const QString modulesRoot = QFileInfo(sample).absolutePath();
        r.discover({}, { modulesRoot });   // installé comme module utilisateur : approbation exigée
        TEST_CHECK(stateOf(r, "sample.csvimport") == ModuleState::Untrusted,
                   "Test 279: module d'exemple valide (" << (r.module("sample.csvimport") ? r.module("sample.csvimport")->messages.join(" | ").toStdString() : "") << ")");
        ConverterSpec conv;
        for (const auto& m : r.modules())
            if (m.manifest.id == "sample.csvimport" && !m.manifest.importers.empty()) conv = m.manifest.importers.front();
        QString log;
        const QString out = tmp.filePath("out.tsa3d");
        TEST_CHECK(!r.runConverter(conv, sample + "/examples/portique.csv", out, &log) && !QFileInfo::exists(out),
                   "Test 279: convertisseur d'un module non approuvé jamais exécuté");
        ModuleHostServices none;
        r.approve("sample.csvimport");
        r.activate(none);
        TEST_CHECK(r.importers().size() == 1 && r.importers().front().extensions == QStringList { "csv" }, "Test 279: convertisseur d'import exposé");
        const bool ran = r.runConverter(r.importers().front(), sample + "/examples/portique.csv", out, &log);
        TEST_CHECK(ran, "Test 279: conversion réussie (" << log.toStdString() << ")");
        QJsonObject doc;
        TSA::IO::Tsa3d::Report rep;
        TEST_CHECK(TSA::IO::Tsa3d::readFile(out, &doc, &rep) && !TSA::IO::Tsa3d::validate(doc).hasErrors(),
                   "Test 279: TSA3D produit valide (" << TSA::IO::Tsa3d::validate(doc).lines().join(" | ").toStdString() << ")");
        Model m;
        const auto im = TSA::IO::Tsa3d::importDocument(doc, m);
        TEST_CHECK(im.ok && m.nodes().size() == 4 && im.members == 3 && im.loadCases == 2 && im.loads == 2,
                   "Test 279: import du résultat : 4 nœuds, 3 barres, 2 cas, 2 charges (" << im.report.lines().join(" | ").toStdString() << ")");

        QFile bad(tmp.filePath("bad.csv"));
        (void)bad.open(QIODevice::WriteOnly);
        bad.write("node,A,0,0,0\nnode,A,1,0,0\nmember,B1,beam,A\n");
        bad.close();
        const QString out2 = tmp.filePath("bad.tsa3d");
        TEST_CHECK(!r.runConverter(r.importers().front(), tmp.filePath("bad.csv"), out2, &log) && !QFileInfo::exists(out2) && log.contains("ligne 2"),
                   "Test 279: CSV invalide → échec, aucune sortie, erreur localisée (" << log.toStdString() << ")");
        std::cout << "[PASS] Test 279: Module d'exemple (convertisseur en processus séparé)" << std::endl;
        ++passed;
    }
    return true;
}
