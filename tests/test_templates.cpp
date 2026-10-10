// Suite « templates » : documents produits par templates (tests 280-285, docs/TEMPLATES.md). Moteur
// (syntaxe, échappement, sections, partiels, diagnostics, limites), paquets .tsatemplate (validation,
// sécurité, aller-retour, options, données obligatoires), équivalence octet par octet du template
// « tsa.ndc.standard » avec l'ancien générateur NDCDocument::toHtml, dépôt (origines, import, versions,
// personnalisation, modules), rapports structurés sur un vrai calcul, rendu HTML/PDF et repli.

#include "test_common.h"

#include "Analysis/Engine/AnalysisManager.h"
#include "NDC/NDCExporter.h"
#include "NDC/NDCGenerator.h"
#include "NDC/NDCTemplateData.h"
#include "Reports/DocumentRenderer.h"
#include "Reports/ReportDataBuilder.h"
#include "Templates/TemplateRepository.h"

#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTemporaryDir>

using namespace TSA::Templates;

namespace
{
QJsonObject obj(const char* json) { return QJsonDocument::fromJson(json).object(); }

RenderResult R(const QString& t, const char* json, const TemplateEngine::PartialResolver& p = {}) { return TemplateEngine::render(t, obj(json), p); }

TemplatePackage minimalPackage(const QString& mainHtml)
{
    TemplatePackage p;
    auto& m = p.manifest();
    m.id = "acme.test";
    m.name = "Test";
    m.version = "1.0";
    m.dataSchema = "tsa-report-data/1";
    m.reports.push_back({ "main", "Principal", "", "main.html", { "project.title" } });
    p.setFile("main.html", mainHtml.toUtf8());
    return p;
}

bool refuses(const QString& html, const QString& needle)
{
    const QStringList e = minimalPackage(html).validate();
    return !e.isEmpty() && e.join('\n').contains(needle);
}

QString firstDifference(const QString& a, const QString& b)
{
    qsizetype i = 0;
    while (i < a.size() && i < b.size() && a[i] == b[i]) ++i;
    if (i == a.size() && i == b.size()) return {};
    return QStringLiteral("position %1 : attendu « %2 » / obtenu « %3 »").arg(i).arg(a.mid(std::max<qsizetype>(0, i - 40), 90), b.mid(std::max<qsizetype>(0, i - 40), 90));
}

/// Note de calcul couvrant toutes les constructions (couverture, sommaire, LOF/LOT, paragraphes HTML, paires,
/// tableaux alignés, figures embarquées et locales, sections sans titre).
TSA::NDC::NDCDocument richNdc()
{
    using namespace TSA::NDC;
    NDCDocument d;
    d.config.projectTitle = "Bâtiment <A> & annexes";
    d.config.emissionDate = "01/02/2026";
    d.config.marginMmLeft = 12.5;
    NDCChapter c1 { 1, "Introduction", {} };
    NDCSection s1;
    s1.title = "Objet";
    s1.paragraphs = { "Premier paragraphe avec <span class=\"badge badge-success\">OK</span>.", "Second « paragraphe »." };
    s1.keyValues = { { "Norme", "EN 1990" }, { "Classe", "CC2" } };
    NDCTable t;
    t.number = 1;
    t.caption = "Matériaux";
    t.headers = { "Nom", "E (GPa)", "fk" };
    t.columnAlignments = { "left", "right" };
    t.rows = { { "S235", "210", "235" }, { "C25/30", "31", "25" } };
    s1.tables.push_back(t);
    NDCFigure f;
    f.number = 1;
    f.caption = "Vue 3D";
    f.imageBase64 = "data:image/png;base64,iVBORw0KGgo=";
    f.elementRef = "B12";
    f.loadCaseOrCombo = "ELU-01";
    f.widthPercent = 75;
    s1.figures.push_back(f);
    NDCFigure g;
    g.number = 2;
    g.caption = "Plan";
    g.localFilePath = "C:/inexistant/plan.png";
    s1.figures.push_back(g);
    NDCSection s2;
    s2.paragraphs = { "Section sans titre." };
    c1.sections = { s1, s2 };
    NDCChapter c2 { 2, "Résultats", {} };
    NDCSection s3;
    s3.title = "Synthèse";
    NDCTable t2;
    t2.number = 2;
    t2.headers = { "A" };
    t2.rows = { { "1" } };
    s3.tables.push_back(t2);
    c2.sections = { s3 };
    d.chapters = { c1, c2 };
    return d;
}

Model& portal(Model& m)
{
    const int a = m.addNode(0, 0, 0), b = m.addNode(0, 0, 3), c = m.addNode(5, 0, 3), d = m.addNode(5, 0, 0);
    m.getNode(a)->setSupport(SupportDefinition::fixed());
    m.getNode(d)->setSupport(SupportDefinition::fixed());
    m.addColumn(a, b, Section::heb(200), Material::steelS235());
    m.addBar(b, c, Section::ipe(300), Material::steelS235(), BarRole::Beam);
    m.addColumn(d, c, Section::heb(200), Material::steelS235());
    const int lc = m.loadManager().addLoadCase(LoadCase(0, "Q", LoadCaseCategory::Live));
    m.loadManager().addNodalLoad(NodalLoad(0, b, lc, 10.0, 0.0, -20.0));
    return m;
}
} // namespace

bool runSuite_Templates(int& passed)
{
    // -------------------------------------------------------------------------
    // TEST 280 : moteur de templates
    // -------------------------------------------------------------------------
    {
        TEST_CHECK(R("Bonjour {{nom}} !", R"({"nom":"<Tsa> & \"co\""})").output == "Bonjour &lt;Tsa&gt; &amp; &quot;co&quot; !", "Test 280: variable échappée");
        TEST_CHECK(R("{{{h}}}|{{&h}}", R"({"h":"<b>x</b>"})").output == "<b>x</b>|<b>x</b>", "Test 280: valeurs brutes {{{ }}} et {{& }}");
        TEST_CHECK(R("{{a.b.c}}/{{n}}/{{d}}/{{t}}", R"({"a":{"b":{"c":"ok"}},"n":12,"d":1.5,"t":true})").output == "ok/12/1.5/oui", "Test 280: noms pointés et scalaires");
        TEST_CHECK(R("{{#l}}[{{.}}]{{/l}}", R"({"l":[1,2,3]})").output == "[1][2][3]", "Test 280: section liste, élément courant");
        TEST_CHECK(R("{{#o}}{{x}}-{{y}}{{/o}}", R"({"o":{"x":1},"y":"parent"})").output == "1-parent", "Test 280: section objet, contexte parent");
        TEST_CHECK(R("{{#f}}A{{/f}}{{^f}}B{{/f}}{{#z}}C{{/z}}{{^z}}D{{/z}}{{^e}}E{{/e}}{{^s}}S{{/s}}{{^m}}M{{/m}}", R"({"f":false,"z":0,"e":[],"s":""})").output == "BDESM",
                   "Test 280: valeurs fausses (faux, 0, liste vide, chaîne vide, absente)");
        TEST_CHECK(R("a{{! commentaire }}b", "{}").output == "ab", "Test 280: commentaire");
        const auto standalone = R("<ul>\n  {{#l}}\n  <li>{{.}}</li>\n  {{/l}}\n</ul>\n", R"({"l":["x","y"]})");
        TEST_CHECK(standalone.output == "<ul>\n  <li>x</li>\n  <li>y</li>\n</ul>\n", "Test 280: balises seules sur leur ligne supprimées (" << standalone.output.toStdString() << ")");
        TEST_CHECK(R("{{#a}}\nx\n{{/a}}\n", R"({"a":true})").output == "x\n", "Test 280: balise seule en tout début de texte");
        auto partials = [](const QString& n) -> std::optional<QString> {
            if (n == "p") return QString("<{{v}}>");
            if (n == "loop") return QString("{{> loop}}");
            return std::nullopt;
        };
        TEST_CHECK(R("{{> p}}", R"({"v":"ok"})", partials).output == "<ok>", "Test 280: partiel");
        const auto missing = R("[{{absent}}]{{#absent2}}x{{/absent2}}", "{}");
        TEST_CHECK(missing.ok() && missing.output == "[]" && missing.missingVariables == QStringList { "absent" }, "Test 280: variable absente rendue vide ET signalée");
        TEST_CHECK(!R("{{#a}}x", "{}").ok() && R("{{#a}}x", "{}").errors.join(" ").contains("non fermée"), "Test 280: section non fermée");
        TEST_CHECK(!R("{{#a}}x{{/b}}{{/a}}", "{}").ok(), "Test 280: fermeture mal imbriquée");
        TEST_CHECK(!R("{{=<% %>=}}", "{}").ok(), "Test 280: changement de délimiteurs refusé");
        TEST_CHECK(!R("{{l}}", R"({"l":[1]})").ok(), "Test 280: liste affichée comme valeur refusée");
        TEST_CHECK(!R("{{> inconnu}}", "{}", partials).ok(), "Test 280: partiel introuvable");
        const auto loop = R("{{> loop}}", "{}", partials);
        TEST_CHECK(!loop.ok() && loop.errors.join(" ").contains("imbriquées"), "Test 280: inclusion récursive arrêtée");
        TEST_CHECK(TemplateEngine::referencedNames("{{a}}{{#b}}{{c.d}}{{/b}}") == QStringList({ "a", "b", "c.d" }), "Test 280: noms référencés");
        std::cout << "[PASS] Test 280: Moteur de templates" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 281 : paquets .tsatemplate — validation, sécurité, aller-retour, options, données requises
    // -------------------------------------------------------------------------
    {
        const QString src = QStringLiteral(TSA_SOURCE_DIR) + "/resources/templates/builtin";
        QStringList errs;
        TemplatePackage ndc, base;
        TEST_CHECK(TemplatePackage::loadDirectory(src + "/tsa.ndc.standard", &ndc, &errs), "Test 281: paquet tsa.ndc.standard valide (" << errs.join(" | ").toStdString() << ")");
        TEST_CHECK(TemplatePackage::loadDirectory(src + "/tsa.reports.base", &base, &errs), "Test 281: paquet tsa.reports.base valide (" << errs.join(" | ").toStdString() << ")");
        TEST_CHECK(base.manifest().reports.size() == 10, "Test 281: 10 rapports de base");

        TEST_CHECK(refuses("<script>alert(1)</script>", "script"), "Test 281: <script> refusé");
        TEST_CHECK(refuses("<img src=\"x.png\" onerror=\"x()\">", "événement"), "Test 281: gestionnaire d'événement refusé");
        TEST_CHECK(refuses("<a href=\"javascript:x()\">a</a>", "javascript"), "Test 281: URL javascript: refusée");
        TEST_CHECK(refuses("<iframe></iframe>", "embarqué"), "Test 281: iframe refusé");
        TEST_CHECK(refuses("<img src=\"https://exemple.org/a.png\">", "distante"), "Test 281: image distante refusée (document autonome)");
        TEST_CHECK(refuses("<style>@import 'x.css';</style>", "@import"), "Test 281: @import refusé");
        TEST_CHECK(refuses("<div style=\"background:url(http://x/y.png)\"></div>", "distante"), "Test 281: url() distante refusée");
        TEST_CHECK(refuses("{{#a}}", "non fermée"), "Test 281: syntaxe invalide refusée");
        TEST_CHECK(refuses("{{> absent}}", "absent du paquet"), "Test 281: partiel absent refusé");
        auto p = minimalPackage("<p>{{project.title}}</p>");
        TEST_CHECK(p.validate().isEmpty(), "Test 281: paquet minimal valide (" << p.validate().join(" | ").toStdString() << ")");
        auto bad = p;
        bad.setFile("../evil.html", "x");
        TEST_CHECK(bad.validate().join(" ").contains("refusé"), "Test 281: chemin « .. » refusé");
        bad = p;
        bad.setFile("run.exe", "MZ");
        TEST_CHECK(bad.validate().join(" ").contains("non autorisé"), "Test 281: type de fichier non autorisé");
        bad = p;
        bad.manifest().engine = "tsa-template/2";
        TEST_CHECK(bad.validate().join(" ").contains("plus récent"), "Test 281: moteur futur refusé");
        bad = p;
        bad.manifest().dataSchema = "autre/1";
        TEST_CHECK(!bad.validate().isEmpty(), "Test 281: schéma de données inconnu refusé");
        bad = p;
        bad.manifest().id = "Mauvais Id";
        TEST_CHECK(!bad.validate().isEmpty(), "Test 281: identifiant invalide refusé");

        // Aller-retour fichier (texte et binaire), champs inconnus du manifeste conservés.
        p.setFile("logo.png", QByteArray("\x89PNG\r\n\x1a\n\x00\x01", 10));
        p.manifest().raw.insert("x-editeur", "conservé");
        p.manifest().options.push_back({ "color", "Couleur", "color", QJsonValue("#112233"), {} });
        QTemporaryDir tmp;
        QString err;
        TEST_CHECK(p.saveFile(tmp.filePath("p.tsatemplate"), &err), "Test 281: écriture du paquet");
        TemplatePackage back;
        TEST_CHECK(TemplatePackage::loadFile(tmp.filePath("p.tsatemplate"), &back, &errs) && back.contentHash() == p.contentHash() &&
                       back.fileBytes("logo.png") == p.fileBytes("logo.png") && back.manifest().raw.value("x-editeur").toString() == "conservé",
                   "Test 281: aller-retour fichier identique (empreinte, binaire, champs inconnus)");
        const QJsonObject eff = p.effectiveOptions(QJsonObject { { "color", "rouge" } }, &errs);
        TEST_CHECK(eff.value("color").toString() == "#112233" && errs.size() == 1, "Test 281: option invalide → valeur par défaut, signalée");
        const auto refused = p.render("main", QJsonObject {});
        TEST_CHECK(!refused.ok() && refused.errors.join(" ").contains("project.title"), "Test 281: données obligatoires absentes → rendu refusé");
        p.setFile("main.html", "<p>{{project.title}}</p><img src=\"{{{assets.logo_png}}}\">");
        const auto ok = p.render("main", QJsonObject { { "project", QJsonObject { { "title", "T" } } } });
        TEST_CHECK(ok.ok() && ok.output.contains("data:image/png;base64,") && ok.output.startsWith("<p>T</p>"), "Test 281: images du paquet incorporées (URI data:)");
        std::cout << "[PASS] Test 281: Paquets de templates" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 282 : équivalence du template standard avec l'ancien générateur (octet par octet)
    // -------------------------------------------------------------------------
    {
        TemplatePackage ndc;
        QStringList errs;
        TEST_CHECK(TemplatePackage::loadDirectory(QStringLiteral(TSA_SOURCE_DIR) + "/resources/templates/builtin/tsa.ndc.standard", &ndc, &errs), "Test 282: paquet NDC");
        auto same = [&](const TSA::NDC::NDCDocument& d, const char* what) {
            const auto r = ndc.render("ndc", TSA::NDC::ndcTemplateData(d, { false }));
            const QString diff = r.ok() ? firstDifference(d.toHtml(), r.output) : r.errors.join(" | ");
            if (!diff.isEmpty() || !r.missingVariables.isEmpty())
                std::cerr << "  [" << what << "] " << diff.toStdString() << " " << r.missingVariables.join(",").toStdString() << std::endl;
            return diff.isEmpty() && r.missingVariables.isEmpty();
        };
        auto d = richNdc();
        TEST_CHECK(same(d, "complet"), "Test 282: note complète identique à toHtml");
        d.config.includeCoverPage = false;
        d.config.pageOrientation = TSA::NDC::PageOrientation::Landscape;
        d.config.pageFormat = TSA::NDC::PageFormat::A3;
        TEST_CHECK(same(d, "sans couverture, A3 paysage"), "Test 282: sans couverture, A3 paysage");
        d = richNdc();
        d.config.includeLof = d.config.includeLot = false;
        d.config.showTsaLogo = false;
        d.config.projectDescription.clear();
        TEST_CHECK(same(d, "sans listes ni logo"), "Test 282: sans listes, logo ni description");
        d.config.includeToc = false;
        TEST_CHECK(same(d, "sans sommaire"), "Test 282: sans sommaire");
        TEST_CHECK(same(TSA::NDC::NDCDocument(), "vide"), "Test 282: note vide");
        Model m;
        GridManager gm;
        portal(m);
        const auto gen = TSA::NDC::NDCGenerator::generate(m, nullptr, TSA::NDC::ReportConfiguration {});
        TEST_CHECK(!gen.chapters.empty() && same(gen, "générateur réel"), "Test 282: note produite par NDCGenerator identique (" << gen.chapters.size() << " chapitres)");
        std::cout << "[PASS] Test 282: Template NDC standard équivalent au générateur historique" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 283 : dépôt — intégrés (ressources), import, versions, personnalisation, modules, problèmes
    // -------------------------------------------------------------------------
    {
        QTemporaryDir tmp;
        TemplateRepository repo;
        repo.setUserRoot(tmp.path());
        repo.reload();
        const auto* ndc = repo.effective("tsa.ndc.standard");
        const auto* base = repo.effective("tsa.reports.base");
        TEST_CHECK(ndc && base && ndc->origin == TemplateOrigin::BuiltIn && repo.problems().empty(),
                   "Test 283: templates intégrés chargés depuis les ressources (" << repo.problems().size() << " problème(s))");
        // Les ressources compilées correspondent aux sources (resources/templates.qrc à jour).
        for (const QString& id : { QString("tsa.ndc.standard"), QString("tsa.reports.base") })
        {
            TemplatePackage fromSource;
            QStringList e;
            TemplatePackage::loadDirectory(QStringLiteral(TSA_SOURCE_DIR) + "/resources/templates/builtin/" + id, &fromSource, &e);
            TEST_CHECK(repo.effective(id)->package.contentHash() == fromSource.contentHash(),
                       "Test 283: ressources " << id.toStdString() << " à jour (python tools/update_templates_qrc.py)");
        }

        auto pkg = minimalPackage("<p>{{project.title}}</p>");
        QString err, key;
        QStringList errs;
        pkg.saveFile(tmp.filePath("src/v1.tsatemplate"), &err);
        TEST_CHECK(repo.importPackage(tmp.filePath("src/v1.tsatemplate"), &key, &errs), "Test 283: import (" << errs.join(" | ").toStdString() << ")");
        const auto* imp = repo.entryByKey(key);
        TEST_CHECK(imp && imp->origin == TemplateOrigin::Imported && !imp->modified && repo.effective("acme.test") == imp, "Test 283: origine « importé », non modifié, effectif");
        pkg.manifest().version = "1.1";
        pkg.saveFile(tmp.filePath("src/v2.tsatemplate"), &err);
        TEST_CHECK(repo.importPackage(tmp.filePath("src/v2.tsatemplate"), &key, &errs), "Test 283: import d'une nouvelle version");
        int archived = 0, imported = 0;
        for (const auto& e : repo.entries())
            if (e.package.manifest().id == "acme.test") archived += e.origin == TemplateOrigin::Archived, imported += e.origin == TemplateOrigin::Imported;
        TEST_CHECK(imported == 1 && archived == 1 && repo.effective("acme.test")->package.manifest().version == "1.1",
                   "Test 283: version précédente archivée, jamais écrasée");

        auto evil = minimalPackage("<script>x</script>");
        QFile ef(tmp.filePath("src/evil.tsatemplate"));
        (void)ef.open(QIODevice::WriteOnly);
        ef.write(QJsonDocument(evil.toJson()).toJson());
        ef.close();
        TEST_CHECK(!repo.importPackage(tmp.filePath("src/evil.tsatemplate"), &key, &errs) && errs.join(" ").contains("script"), "Test 283: import d'un template dangereux refusé");
        QFile::copy(tmp.filePath("src/evil.tsatemplate"), tmp.filePath("evil.tsatemplate"));
        repo.reload();
        TEST_CHECK(repo.problems().size() == 1 && !repo.effective("acme.test")->path.endsWith("evil.tsatemplate"), "Test 283: template invalide déposé : listé en problème, non chargé");
        QFile::remove(tmp.filePath("evil.tsatemplate"));

        const QString baseKey = repo.effective("tsa.reports.base")->key();
        TemplatePackage edited = repo.effective("tsa.reports.base")->package;
        for (auto& o : edited.manifest().options)
            if (o.id == "primaryColor") o.defaultValue = "#aa0000";
        TEST_CHECK(repo.saveCustomized(baseKey, edited, &key, &errs), "Test 283: personnalisation (" << errs.join(" | ").toStdString() << ")");
        const auto* cust = repo.entryByKey(key);
        TEST_CHECK(cust && cust->origin == TemplateOrigin::Customized && cust->modified && cust->package.manifest().basedOn == "tsa.reports.base@1.0.0" &&
                       repo.effective("tsa.reports.base") == cust,
                   "Test 283: copie personnalisée effective, marquée modifiée, origine conservée");
        TEST_CHECK(repo.effective("tsa.reports.base") != nullptr && repo.entryByKey(baseKey) && repo.entryByKey(baseKey)->origin == TemplateOrigin::BuiltIn,
                   "Test 283: l'original intégré reste disponible");
        edited.setFile("partials/footer.html", "<script>x</script>");
        TEST_CHECK(!repo.saveCustomized(baseKey, edited, &key, &errs), "Test 283: personnalisation dangereuse refusée");
        TEST_CHECK(repo.remove(cust->key(), &err) && repo.effective("tsa.reports.base")->origin == TemplateOrigin::BuiltIn, "Test 283: suppression de la copie → retour à l'intégré");
        TEST_CHECK(!repo.remove(baseKey, &err), "Test 283: un template intégré ne se supprime pas");

        TEST_CHECK(repo.registerModulePackage(tmp.filePath("src/v2.tsatemplate"), "acme.module", &err), "Test 283: template fourni par un module");
        bool fromModule = false;
        for (const auto& e : repo.entries()) fromModule |= e.origin == TemplateOrigin::Module && e.moduleId == "acme.module";
        repo.unregisterModule("acme.module");
        bool still = false;
        for (const auto& e : repo.entries()) still |= e.origin == TemplateOrigin::Module;
        TEST_CHECK(fromModule && !still, "Test 283: templates du module retirés à son arrêt");
        TEST_CHECK(repo.exportPackage(repo.effective("tsa.ndc.standard")->key(), tmp.filePath("out/ndc.tsatemplate"), &err), "Test 283: export d'un template intégré");
        TemplatePackage exported;
        TEST_CHECK(TemplatePackage::loadFile(tmp.filePath("out/ndc.tsatemplate"), &exported, &errs) && exported.contentHash() == repo.effective("tsa.ndc.standard")->package.contentHash(),
                   "Test 283: template exporté réimportable à l'identique");
        std::cout << "[PASS] Test 283: Dépôt de templates" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 284 : rapports structurés — sans résultats (indisponibles explicites) puis sur un vrai calcul
    // -------------------------------------------------------------------------
    {
        TemplateRepository repo;
        QTemporaryDir tmp;
        repo.setUserRoot(tmp.path());
        repo.reload();
        const auto& base = repo.effective("tsa.reports.base")->package;
        Model m;
        GridManager gm;
        portal(m);
        TSA::Reports::ReportProjectInfo info { "Portique test" };
        const QJsonObject noResults = TSA::Reports::ReportDataBuilder::build(m, nullptr, info);
        bool allOk = true;
        QStringList issues;
        for (const auto& r : base.manifest().reports)
        {
            const auto out = base.render(r.id, noResults);
            if (!out.ok() || !out.missingVariables.isEmpty()) issues << r.id + " : " + (out.errors + out.missingVariables).join(", ");
            allOk &= out.ok() && out.missingVariables.isEmpty();
        }
        TEST_CHECK(allOk, "Test 284: les 10 rapports se rendent sans résultats, sans donnée manquante (" << issues.join(" | ").toStdString() << ")");
        const QString general = base.render("general", noResults).output;
        TEST_CHECK(general.contains("Résultats non disponibles") && general.contains("Analyse modale") && !general.contains("Déplacement maximal"),
                   "Test 284: résultats absents signalés, aucun résultat inventé");
        TEST_CHECK(base.render("modal", noResults).output.contains("Analyse modale non disponible"), "Test 284: rapport modal : indisponibilité explicite");
        TEST_CHECK(base.render("plane-2d", noResults).output.contains("plan XZ"), "Test 284: structure plane reconnue (plan XZ)");

        TSA::Analysis::AnalysisEngineRegistry reg;
        TSA::Analysis::registerBuiltInEngines(reg);
        TSA::Analysis::AnalysisManager mgr(reg);
        TSA::Analysis::AnalysisContext ctx;
        ctx.engineId = "opensees";
        ctx.common.includeSelfWeight = false;
        const auto run = mgr.run(ctx, mgr.prepare(m, &gm, ctx));
        TEST_CHECK(run.success, "Test 284: calcul OpenSees (" << run.message << ")");
        const QJsonObject withResults = TSA::Reports::ReportDataBuilder::build(m, &run.results, info);
        issues.clear();
        allOk = true;
        for (const auto& r : base.manifest().reports)
        {
            const auto out = base.render(r.id, withResults);
            if (!out.ok() || !out.missingVariables.isEmpty()) issues << r.id + " : " + (out.errors + out.missingVariables).join(", ");
            allOk &= out.ok() && out.missingVariables.isEmpty() && TSA::Reports::DocumentRenderer::externalReferences(out.output).isEmpty();
        }
        TEST_CHECK(allOk, "Test 284: les 10 rapports se rendent avec résultats, documents autonomes (" << issues.join(" | ").toStdString() << ")");
        const QString df = base.render("displacements-forces", withResults).output;
        const QString maxNode = withResults["results"].toObject()["displacements"].toObject()["max"].toObject()["node"].toString();
        TEST_CHECK(df.contains("Déplacement maximal") && !maxNode.isEmpty() && df.contains(maxNode) && df.contains("non validés"),
                   "Test 284: déplacements, réactions et efforts du calcul présentés, statut « non validés »");
        TEST_CHECK(withResults["results"].toObject()["reactions"].toObject()["rows"].toArray().size() == 2 &&
                       std::abs(withResults["results"].toObject()["reactions"].toObject()["sum"].toObject()["fz"].toString().toDouble() - 20.0) < 0.05,
                   "Test 284: somme des réactions verticales = charge appliquée (20 kN)");
        const QString beam = base.render("beam", withResults).output;
        TEST_CHECK(beam.contains("poutre B") && beam.contains("|My|") && beam.contains("IPE"), "Test 284: fiche de poutre avec efforts");
        TemplatePackage sample;
        QStringList sampleErrors;
        TEST_CHECK(TemplatePackage::loadDirectory(QStringLiteral(TSA_SOURCE_DIR) + "/sdk/examples/templates/societe.rapport-simple", &sample, &sampleErrors),
                   "Test 284: exemple de template du SDK valide (" << sampleErrors.join(" | ").toStdString() << ")");
        const auto s1 = sample.render("synthese", noResults), s2 = sample.render("synthese", withResults);
        TEST_CHECK(s1.ok() && s2.ok() && s1.missingVariables.isEmpty() && s2.missingVariables.isEmpty() && s1.output.contains("non disponibles") &&
                       s2.output.contains("<table>"),
                   "Test 284: exemple du SDK rendu avec et sans résultats");
        const auto custom = base.render("general", withResults, QJsonObject { { "documentTitle", "Mon titre <b>" }, { "primaryColor", "#123456" }, { "showFooter", false } });
        TEST_CHECK(custom.ok() && custom.output.contains("Mon titre &lt;b&gt;") && custom.output.contains("#123456") && !custom.output.contains("class=\"footer\""),
                   "Test 284: options appliquées (titre échappé, couleur, pied de page masqué)");
        std::cout << "[PASS] Test 284: Rapports structurés" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 285 : rendu HTML/PDF, note de calcul par le dépôt, repli explicite
    // -------------------------------------------------------------------------
    {
        QTemporaryDir tmp;
        auto d = richNdc();
        QStringList diag;
        QString used;
        const QString html = TSA::NDC::NDCExporter::renderHtml(d, &diag, &used);
        TEST_CHECK(used.startsWith("tsa.ndc.standard"), "Test 285: note rendue par le template du dépôt (" << used.toStdString() << ")");
        TEST_CHECK(html.contains("Bâtiment <A> & annexes") && html.contains("file:///C:/inexistant/plan.png") && html.contains("data:image/png;base64,iVBORw0KGgo="),
                   "Test 285: image locale introuvable conservée en lien (aucune donnée inventée)");
        d.config.templateId = "inexistant.template";
        const QString fallback = TSA::NDC::NDCExporter::renderHtml(d, &diag, &used);
        TEST_CHECK(fallback == d.toHtml() && used == "générateur historique" && diag.join(" ").contains("introuvable"), "Test 285: repli explicite sur le générateur historique");
        QString err;
        TEST_CHECK(TSA::Reports::DocumentRenderer::writeHtml(html, tmp.filePath("note.html"), &err), "Test 285: export HTML");
        QFile hf(tmp.filePath("note.html"));
        TEST_CHECK(hf.open(QIODevice::ReadOnly) && QString::fromUtf8(hf.readAll()) == html, "Test 285: HTML écrit en UTF-8 sans altération");
        TEST_CHECK(TSA::Reports::DocumentRenderer::writePdf(html, tmp.filePath("note.pdf"), { "A3", true, 10.0 }, &err), "Test 285: export PDF (" << err.toStdString() << ")");
        QFile pf(tmp.filePath("note.pdf"));
        TEST_CHECK(pf.open(QIODevice::ReadOnly) && pf.read(5) == "%PDF-" && pf.size() > 1000, "Test 285: fichier PDF valide");
        TEST_CHECK(TSA::Reports::DocumentRenderer::externalReferences("<img src=\"file:///x.png\"><img src=\"data:image/png;base64,AA\">") == QStringList { "file:///x.png" },
                   "Test 285: détection des ressources externes");
        std::cout << "[PASS] Test 285: Rendu des documents" << std::endl;
        ++passed;
    }
    return true;
}
