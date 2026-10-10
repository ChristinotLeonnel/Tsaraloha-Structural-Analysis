// Suite « help » : aide contextuelle vers la documentation en ligne (tests 214-218).
// Registre des identifiants, construction et validation des adresses (HTTPS, domaine officiel,
// aucune donnée de projet), repli des identifiants inconnus, boutons Aide des fenêtres.

#include "test_common.h"

#include "Help/HelpTopics.h"
#include "UI/HelpLauncher.h"
#include "UI/Dialogs/GridDialog.h"
#include "UI/Dialogs/LoadCaseDialog.h"
#include "UI/Dialogs/MemberLoadDialog.h"
#include "UI/Dialogs/NodalLoadDialog.h"

#include <ProductIdentity.h>

#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QSet>
#include <QUrlQuery>

namespace
{
const QString kBase = QStringLiteral("https://christinotleonnel.github.io/Tsaraloha-Web");

/// Clique le bouton « helpButton » de la fenêtre et rend l'adresse transmise au navigateur.
QUrl clickHelp(QWidget& dialog, bool& found)
{
    QUrl captured;
    TSA::UI::setHelpUrlOpenerForTesting([&captured](const QUrl& u) { captured = u; return true; });
    auto* button = dialog.findChild<QPushButton*>(QStringLiteral("helpButton"));
    found = button != nullptr;
    if (button) button->click();
    TSA::UI::setHelpUrlOpenerForTesting(nullptr);
    return captured;
}
} // namespace

bool runSuite_Help(int& passed)
{
    using namespace TSA::Help;

    // -------------------------------------------------------------------------
    // TEST 214 : registre — identifiants uniques et bien formés, chemins sous docs/, exemples requis
    // -------------------------------------------------------------------------
    {
        QSet<QString> ids;
        const QRegularExpression idRe(QStringLiteral("^[a-z0-9-]+(\\.[a-z0-9-]+)+$"));
        const QRegularExpression pathRe(QStringLiteral("^docs(/[a-z0-9-]+)*$"));
        for (const HelpTopic& t : topics())
        {
            const QString id = QString::fromLatin1(t.id);
            TEST_CHECK(!ids.contains(id), "Test 214: identifiant en double : " << t.id);
            ids.insert(id);
            TEST_CHECK(idRe.match(id).hasMatch(), "Test 214: identifiant mal formé : " << t.id);
            TEST_CHECK(pathRe.match(QString::fromLatin1(t.path)).hasMatch(), "Test 214: chemin invalide : " << t.path);
        }
        for (const char* required : { "general.overview", "project.create", "model.selection", "model.nodes", "model.members",
                                      "loading.distributed", "mesh.overview", "analysis.overview", "results.overview" })
            TEST_CHECK(isKnownTopic(QString::fromLatin1(required)), "Test 214: identifiant requis absent : " << required);
        TEST_CHECK(pathForTopic(QString::fromLatin1(kOverviewTopic)) == QStringLiteral("docs"), "Test 214: accueil = docs");
        TEST_CHECK(pathForTopic(QStringLiteral("loading.distributed")) == QStringLiteral("docs/loading/distributed-loads"),
                   "Test 214: loading.distributed → page des charges réparties");
        TEST_CHECK(pathForTopic(QStringLiteral("inconnu.xyz")).isEmpty(), "Test 214: identifiant inconnu → chemin vide");
        std::cout << "[PASS] Test 214: Registre des identifiants d'aide (" << ids.size() << " entrées)" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 215 : adresse construite — HTTPS, domaine officiel, seuls langue et version transmises
    // -------------------------------------------------------------------------
    {
        HelpUrl h = buildHelpUrl(kBase, QStringLiteral("model.nodes"), QStringLiteral("fr"), QStringLiteral("0.1.0"));
        TEST_CHECK(h.error.isEmpty() && h.knownTopic, "Test 215: adresse acceptée");
        TEST_CHECK(h.url.toString() == kBase + QStringLiteral("/docs/modeling/nodes?lang=fr&v=0.1.0"),
                   "Test 215: adresse exacte (" << h.url.toString().toStdString() << ")");
        h = buildHelpUrl(kBase + QStringLiteral("/"), QStringLiteral("model.nodes"), QStringLiteral("en"), QString());
        TEST_CHECK(h.url.toString() == kBase + QStringLiteral("/docs/modeling/nodes?lang=en"), "Test 215: barre oblique finale de la base");
        h = buildHelpUrl(kBase, QStringLiteral("model.nodes"), QStringLiteral("de&x=1"), QStringLiteral("0.1.0;evil"));
        TEST_CHECK(h.url.toString() == kBase + QStringLiteral("/docs/modeling/nodes"),
                   "Test 215: langue et version invalides ignorées (" << h.url.toString().toStdString() << ")");
        const HelpUrl official = officialHelpUrl(QStringLiteral("results.overview"));
        TEST_CHECK(official.error.isEmpty() && official.url.scheme() == QStringLiteral("https")
                       && official.url.host() == QUrl(QString::fromUtf8(TSA::Product::kDocsBaseUrl)).host()
                       && QUrlQuery(official.url).queryItemValue(QStringLiteral("v")) == QString::fromUtf8(TSA::Product::kVersion),
                   "Test 215: adresse officielle (ProductIdentity.h)");
        TEST_CHECK(isAllowedHelpUrl(official.url, QUrl(QString::fromUtf8(TSA::Product::kDocsBaseUrl))), "Test 215: adresse officielle autorisée");
        std::cout << "[PASS] Test 215: Construction des adresses d'aide" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 216 : identifiant inconnu ou vide → accueil de la documentation, signalé
    // -------------------------------------------------------------------------
    {
        for (const QString& id : { QStringLiteral("does.not.exist"), QString(), QStringLiteral("../../etc") })
        {
            const HelpUrl h = buildHelpUrl(kBase, id, QStringLiteral("fr"), QString());
            TEST_CHECK(h.error.isEmpty() && !h.knownTopic, "Test 216: identifiant inconnu signalé");
            TEST_CHECK(h.url.toString() == kBase + QStringLiteral("/docs?lang=fr"), "Test 216: repli sur /docs");
        }
        std::cout << "[PASS] Test 216: Repli des identifiants inconnus" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 217 : domaines et schémas refusés
    // -------------------------------------------------------------------------
    {
        TEST_CHECK(!buildHelpUrl(QString(), QStringLiteral("model.nodes"), {}, {}).error.isEmpty(), "Test 217: base vide refusée");
        TEST_CHECK(!buildHelpUrl(QStringLiteral("http://christinotleonnel.github.io/Tsaraloha-Web"), QStringLiteral("model.nodes"), {}, {}).error.isEmpty(),
                   "Test 217: HTTP refusé");
        TEST_CHECK(!buildHelpUrl(QStringLiteral("file:///C:/docs"), QStringLiteral("model.nodes"), {}, {}).error.isEmpty(), "Test 217: file:// refusé");
        TEST_CHECK(!buildHelpUrl(QStringLiteral("javascript:alert(1)"), QStringLiteral("model.nodes"), {}, {}).error.isEmpty(), "Test 217: javascript: refusé");
        TEST_CHECK(!buildHelpUrl(kBase + QStringLiteral("?x=1"), QStringLiteral("model.nodes"), {}, {}).error.isEmpty(), "Test 217: base avec requête refusée");
        TEST_CHECK(!buildHelpUrl(QStringLiteral("https://user:pw@christinotleonnel.github.io/Tsaraloha-Web"), QStringLiteral("model.nodes"), {}, {}).error.isEmpty(),
                   "Test 217: base avec identifiants refusée");

        const QUrl base(kBase);
        const auto allowed = [&base](const char* u) { return isAllowedHelpUrl(QUrl(QString::fromLatin1(u)), base); };
        TEST_CHECK(allowed("https://christinotleonnel.github.io/Tsaraloha-Web/docs/modeling/nodes?lang=fr&v=0.1.0"), "Test 217: page officielle acceptée");
        TEST_CHECK(allowed("https://CHRISTINOTLEONNEL.github.io/Tsaraloha-Web/docs"), "Test 217: hôte insensible à la casse");
        TEST_CHECK(!allowed("http://christinotleonnel.github.io/Tsaraloha-Web/docs"), "Test 217: HTTP refusé");
        TEST_CHECK(!allowed("https://evil.example/Tsaraloha-Web/docs"), "Test 217: autre domaine refusé");
        TEST_CHECK(!allowed("https://christinotleonnel.github.io.evil.example/Tsaraloha-Web/docs"), "Test 217: domaine suffixé refusé");
        TEST_CHECK(!allowed("https://christinotleonnel.github.io/Autre-Site/docs"), "Test 217: chemin hors de la base refusé");
        TEST_CHECK(!allowed("https://christinotleonnel.github.io/Tsaraloha-WebEvil/docs"), "Test 217: préfixe de chemin trompeur refusé");
        TEST_CHECK(!allowed("https://christinotleonnel.github.io/Tsaraloha-Web/docs/../../x"), "Test 217: remontée de chemin refusée");
        TEST_CHECK(!allowed("https://christinotleonnel.github.io:8443/Tsaraloha-Web/docs"), "Test 217: autre port refusé");
        TEST_CHECK(!allowed("https://u:p@christinotleonnel.github.io/Tsaraloha-Web/docs"), "Test 217: identifiants refusés");
        TEST_CHECK(!allowed("https://christinotleonnel.github.io/Tsaraloha-Web/docs?project=C:/secret.tsa"),
                   "Test 217: paramètre de projet refusé");
        TEST_CHECK(!allowed("https://christinotleonnel.github.io/Tsaraloha-Web/docs#x"), "Test 217: fragment refusé");
        std::cout << "[PASS] Test 217: Domaines, schémas et paramètres refusés" << std::endl;
        ++passed;
    }

    // -------------------------------------------------------------------------
    // TEST 218 : boutons Aide des fenêtres → page attendue ; navigateur indisponible → message
    // -------------------------------------------------------------------------
    {
        TEST_CHECK(TSA::UI::isOnlineHelpAvailable(), "Test 218: documentation en ligne configurée pour TSA");
        TSA::Model::Model m;
        struct Case { QWidget* dialog; const char* path; const char* name; };
        TSA::UI::MemberLoadDialog memberDlg(&m);
        TSA::UI::NodalLoadDialog nodalDlg(&m);
        TSA::UI::LoadCaseDialog caseDlg(&m);
        TSA::UI::GridDialog gridDlg;
        for (const Case& c : { Case{ &memberDlg, "/Tsaraloha-Web/docs/loading/distributed-loads", "charge sur barre" },
                               Case{ &nodalDlg, "/Tsaraloha-Web/docs/loading/nodal-loads", "charge nodale" },
                               Case{ &caseDlg, "/Tsaraloha-Web/docs/loading/load-cases-combinations", "cas de charge" },
                               Case{ &gridDlg, "/Tsaraloha-Web/docs/modeling/grids-workplanes", "grille" } })
        {
            bool found = false;
            const QUrl url = clickHelp(*c.dialog, found);
            TEST_CHECK(found, "Test 218: bouton Aide présent (" << c.name << ")");
            TEST_CHECK(url.scheme() == QStringLiteral("https") && url.path() == QString::fromLatin1(c.path),
                       "Test 218: page ouverte (" << c.name << ") : " << url.toString().toStdString());
            TEST_CHECK(isAllowedHelpUrl(url, QUrl(QString::fromUtf8(TSA::Product::kDocsBaseUrl))), "Test 218: adresse autorisée (" << c.name << ")");
        }

        // Navigateur indisponible : échec signalé par un message non bloquant (aucune attente).
        TSA::UI::setHelpUrlOpenerForTesting([](const QUrl&) { return false; });
        QWidget host;
        const auto result = TSA::UI::openHelpTopic(&host, QStringLiteral("model.nodes"));
        TSA::UI::setHelpUrlOpenerForTesting(nullptr);
        auto* box = host.findChild<QMessageBox*>();
        TEST_CHECK(result == TSA::UI::HelpOpenResult::Failed && box != nullptr
                       && box->text().contains(QStringLiteral("/docs/modeling/nodes")),
                   "Test 218: échec d'ouverture → message avec l'adresse");
        box->close();

        // Identifiant inconnu depuis l'interface : accueil ouvert.
        QUrl fallback;
        TSA::UI::setHelpUrlOpenerForTesting([&fallback](const QUrl& u) { fallback = u; return true; });
        const auto r2 = TSA::UI::openHelpTopic(&host, QStringLiteral("inconnu.bouton"));
        TSA::UI::setHelpUrlOpenerForTesting(nullptr);
        TEST_CHECK(r2 == TSA::UI::HelpOpenResult::Opened && fallback.path() == QStringLiteral("/Tsaraloha-Web/docs"),
                   "Test 218: identifiant inconnu → accueil de la documentation");
        std::cout << "[PASS] Test 218: Boutons Aide des fenêtres et gestion des erreurs" << std::endl;
        ++passed;
    }
    return true;
}
