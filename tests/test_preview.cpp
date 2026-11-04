// Suite « preview » : projets récents et aperçus du dernier état du modèle (tests 124-126).

#include "test_common.h"
#include "App/ProductInfo.h"
#include "Help/HelpTopics.h"
#include "IO/TSAFile.h"
#include "Project/ProjectFileSummary.h"
#include "UI/Common/ProjectCardGrid.h"
#include "UI/Common/RecentFilesView.h"
#include <QTableWidget>
#include <QToolButton>
#ifdef TSA_HAVE_QTTEST
#include <QTest>
#endif
#include <QFile>
#include <QTemporaryDir>

#include "IO/TSAFile.h"
#include "Model/Model.h"
#include "Project/ModelPreviewCache.h"
#include "Project/RecentProjects.h"

#include <QJsonArray>
#include <QPainter>
#include <QTemporaryDir>

using TSA::Project::ModelPreviewCache;
using TSA::Project::PreviewSource;
using TSA::Project::ProjectPreviewMetadata;
using TSA::Project::RecentProjects;

namespace
{
QImage makeImage(int w, int h, QColor c)
{
    QImage img(w, h, QImage::Format_RGB888);
    img.fill(c);
    return img;
}

QString touchFile(const QTemporaryDir& dir, const QString& name)
{
    const QString path = dir.filePath(name);
    QFile f(path);
    if (f.open(QIODevice::WriteOnly)) f.write("tsa");
    return path;
}

QJsonObject camera(double eyeX)
{
    return QJsonObject{ { "eye", QJsonArray{ eyeX, -10.0, 8.0 } }, { "center", QJsonArray{ 0.0, 0.0, 0.0 } },
                        { "up", QJsonArray{ 0.0, 0.0, 1.0 } }, { "scale", 25.0 }, { "projection", "orthographic" } };
}
} // namespace

bool runSuite_Preview(int& passed)
{
    // TEST 124 : projets récents (ordre, doublons, renommage, plafond, temps relatif)
    {
        QTemporaryDir dir;
        RecentProjects recent(QStringLiteral("RecentProjectsTest"));
        recent.clear();
        const QString a = touchFile(dir, "A.tsa"), b = touchFile(dir, "B.tsa");
        recent.touch(a);
        recent.touch(b);
        recent.touch(a); // rouvert : en tête, sans doublon
        auto list = recent.list();
        TEST_CHECK(list.size() == 2 && list[0].path == RecentProjects::normalize(a), "Test 124: dernier ouvert en tête, sans doublon");
        const QString c = dir.filePath("C.tsa");
        QFile::rename(b, c);
        recent.rename(b, c);
        TEST_CHECK(recent.list()[1].path == RecentProjects::normalize(c), "Test 124: renommage suivi");
        QFile::remove(a);
        TEST_CHECK(recent.list().size() == 1 && recent.list(true).size() == 2, "Test 124: fichiers disparus masqués");
        for (int i = 0; i < 20; ++i) recent.touch(touchFile(dir, QString("P%1.tsa").arg(i)));
        TEST_CHECK(recent.list(true).size() == RecentProjects::kMaxEntries, "Test 124: liste plafonnée");
        recent.clear();
        const QDateTime now(QDate(2026, 10, 4), QTime(15, 0));
        TEST_CHECK(RecentProjects::relativeTime(now.addSecs(-20), now) == "à l'instant", "Test 124: à l'instant");
        TEST_CHECK(RecentProjects::relativeTime(now.addSecs(-300), now) == "il y a 5 min", "Test 124: minutes");
        TEST_CHECK(RecentProjects::relativeTime(now.addSecs(-7200), now) == "il y a 2 h", "Test 124: heures");
        TEST_CHECK(RecentProjects::relativeTime(now.addDays(-1), now).startsWith("hier"), "Test 124: hier");
        TEST_CHECK(RecentProjects::relativeTime(now.addDays(-30), now) == "04/09/2026", "Test 124: date");
        std::cout << "[PASS] Test 124: Projets récents" << std::endl;
        ++passed;
    }

    // TEST 125 : cache d'aperçus (métadonnées, caméra, invalidation, recapture)
    {
        QTemporaryDir dir, cacheDir;
        ModelPreviewCache cache(cacheDir.path());
        const QString project = touchFile(dir, "Building.tsa");
        TEST_CHECK(cache.preview(project).isNull(), "Test 125: aucun aperçu au départ");
        TEST_CHECK(ModelPreviewCache::needsCapture(cache.metadata(project), 1, camera(10)), "Test 125: capture initiale nécessaire");

        ProjectPreviewMetadata meta;
        meta.projectPath = project;
        meta.capturedAt = QDateTime::currentDateTime().addSecs(5);
        meta.modelRevision = 42;
        meta.cameraState = camera(10);
        meta.viewState = QJsonObject{ { "mode2D", true } };
        meta.nodeCount = 12;
        TEST_CHECK(cache.store(makeImage(1280, 720, Qt::blue), meta), "Test 125: écriture");
        const auto read = cache.metadata(project);
        TEST_CHECK(read && read->modelRevision == 42 && read->viewState["mode2D"].toBool() && read->nodeCount == 12, "Test 125: métadonnées relues");
        PreviewSource src = PreviewSource::None;
        const QImage img = cache.preview(project, &src);
        TEST_CHECK(src == PreviewSource::Cache && img.width() == ModelPreviewCache::kWidth && img.height() == ModelPreviewCache::kHeight,
                   "Test 125: aperçu basse résolution (480×270)");
        TEST_CHECK(!ModelPreviewCache::needsCapture(read, 42, camera(10)), "Test 125: rien n'a changé → pas de capture");
        TEST_CHECK(ModelPreviewCache::needsCapture(read, 43, camera(10)), "Test 125: modèle modifié → capture");
        TEST_CHECK(ModelPreviewCache::needsCapture(read, 42, camera(12)), "Test 125: caméra déplacée → capture");

        // Fichier modifié ailleurs après la capture : le cache est ignoré (pas de miniature embarquée ici).
        QFile f(project);
        TEST_CHECK(f.open(QIODevice::ReadWrite), "Test 125: ouverture");
        f.setFileTime(QDateTime::currentDateTime().addSecs(120), QFileDevice::FileModificationTime);
        f.close();
        TEST_CHECK(cache.preview(project, &src).isNull() && src == PreviewSource::None, "Test 125: cache périmé ignoré");

        const QString renamed = dir.filePath("Building_R+5.tsa");
        QFile::rename(project, renamed);
        cache.rename(project, renamed);
        TEST_CHECK(!cache.metadata(project) && cache.metadata(renamed), "Test 125: aperçu suivi au renommage");
        cache.remove(renamed);
        TEST_CHECK(!cache.metadata(renamed), "Test 125: suppression");
        std::cout << "[PASS] Test 125: Cache d'aperçus" << std::endl;
        ++passed;
    }

    // TEST 126 : repli sur la miniature embarquée dans le .tsa (chunk THMB)
    {
        QTemporaryDir dir, cacheDir;
        TSA::Model::Model m;
        const int a = m.addNode(0, 0, 0), b = m.addNode(5, 0, 0);
        m.addBeam(a, b);
        const QString path = dir.filePath("Embedded.tsa");
        TEST_CHECK(TSA::IO::TSAProjectIO::saveProject(path, m, nullptr, "Embedded", "test", true, makeImage(512, 512, Qt::red)),
                   "Test 126: enregistrement avec miniature");
        ModelPreviewCache cache(cacheDir.path());
        PreviewSource src = PreviewSource::None;
        const QImage img = cache.preview(path, &src);
        TEST_CHECK(src == PreviewSource::Embedded && !img.isNull() && img.pixelColor(10, 10).red() > 200,
                   "Test 126: projet jamais capturé → miniature du fichier");
        std::cout << "[PASS] Test 126: Repli sur la miniature embarquée" << std::endl;
        ++passed;
    }
    // TEST 602 : accueil — résumé d'un fichier projet (en-tête seul), ressources réellement disponibles
    {
        using TSA::Project::readProjectFileSummary;
        QTemporaryDir dir;
        TSA::Model::Model m;
        m.addNode(0, 0, 0);
        const QString plain = dir.filePath("Pont.tsa"), locked = dir.filePath("Tour.tsa"), junk = dir.filePath("Autre.tsa");
        TEST_CHECK(TSA::IO::TSAProjectIO::saveProject(plain, m, nullptr, QStringLiteral("Pont de la Betsiboka"), QStringLiteral("Bureau")),
                   "Test 602: projet enregistré");
        const auto a = readProjectFileSummary(plain);
        TEST_CHECK(a.valid && a.projectName == QStringLiteral("Pont de la Betsiboka") && !a.passwordProtected
                       && a.formatMajor == TSA::IO::TSA_FORMAT_VERSION_MAJOR && a.formatMinor == TSA::IO::TSA_FORMAT_VERSION_MINOR,
                   "Test 602: nom et format lus dans l'en-tête");
        TEST_CHECK(TSA::IO::TSAProjectIO::saveProtectedProject(locked, m, nullptr, QStringLiteral("Secret"), QString(), QStringLiteral("motdepasse")),
                   "Test 602: projet protégé enregistré");
        const auto b = readProjectFileSummary(locked);
        TEST_CHECK(b.valid && b.passwordProtected && b.projectName.isEmpty(), "Test 602: projet protégé — reconnu, aucun nom en clair");
        QFile f(junk);
        TEST_CHECK(f.open(QIODevice::WriteOnly), "ouverture de " << f.fileName().toStdString());
        f.write("ceci n'est pas un projet");
        f.close();
        TEST_CHECK(!readProjectFileSummary(junk).valid && !readProjectFileSummary(dir.filePath("Absent.tsa")).valid,
                   "Test 602: fichier illisible ou absent — sans erreur");

        const QString docs = QStringLiteral("https://exemple.org/site"), repo = QStringLiteral("https://github.com/exemple/depot");
        const auto all = TSA::Help::homeResources(docs, repo);
        QStringList ids;
        for (const auto& r : all)
        {
            ids << r.id;
            TEST_CHECK(r.topic.isEmpty() != r.url.isEmpty(), "Test 602: une seule destination pour « " << r.id.toStdString() << " »");
            TEST_CHECK(r.topic.isEmpty() || TSA::Help::isKnownTopic(r.topic), "Test 602: page de documentation connue");
            TEST_CHECK(r.url.isEmpty() || r.url.scheme() == QLatin1String("https"), "Test 602: adresse HTTPS");
        }
        TEST_CHECK(ids == QStringList({ "documentation", "tutorials", "guide", "website", "github", "technical" }), "Test 602: six ressources");
        TEST_CHECK(all.last().url.toString() == repo + QStringLiteral("/tree/main/docs"), "Test 602: documentation technique dans le dépôt");
        TEST_CHECK(TSA::Help::homeResources(QString(), repo).size() == 2 && TSA::Help::homeResources(docs, QString()).size() == 4
                       && TSA::Help::homeResources(QStringLiteral("http://exemple.org"), QString()).isEmpty(),
                   "Test 602: ressource sans destination valide jamais proposée");
        const auto official = TSA::Help::homeResources(QString::fromUtf8(TSA::Product::kDocsBaseUrl), QString::fromUtf8(TSA::Product::kSourceRepositoryUrl));
        TEST_CHECK(official.size() == 6, "Test 602: six ressources configurées pour ce produit (" << official.size() << ")");
        std::cout << "[PASS] Test 602: Accueil — résumé des fichiers projet, ressources" << std::endl;
        ++passed;
    }

    // TEST 603 : accueil — grille de cartes et liste des récents : actions, filtre, tri, limite, avec de vrais événements souris
    {
#ifdef TSA_HAVE_QTTEST
        using TSA::UI::ProjectCard;
        using TSA::UI::ProjectCardGrid;
        using TSA::UI::RecentFilesView;
        using TSA::UI::ProjectSortKey;
        QTemporaryDir dir;
        QVector<ProjectCardGrid::Entry> entries;
        const QDateTime now = QDateTime::currentDateTime();
        int age = 0;
        for (const QString name : { QStringLiteral("Alpha.tsa"), QStringLiteral("Bravo.tsa"), QStringLiteral("Charlie.tsa") })
        {
            QFile f(dir.filePath(name));
            TEST_CHECK(f.open(QIODevice::WriteOnly), "ouverture de " << f.fileName().toStdString());
            f.write("x");
            f.close();
            entries.push_back({ TSA::Project::RecentProjects::normalize(dir.filePath(name)), now.addSecs(-60 * age++) });
        }

        ProjectCardGrid grid;
        grid.resize(1000, 700);
        QStringList opened, removed;
        QObject::connect(&grid, &ProjectCardGrid::openRequested, [&](const QString& p) { opened << p; });
        QObject::connect(&grid, &ProjectCardGrid::removeRequested, [&](const QString& p) { removed << p; });
        grid.setProjects(entries, true);
        grid.show();
        QCoreApplication::processEvents();
        auto card = [&](const QString& name) -> ProjectCard* {
            for (auto* c : grid.findChildren<ProjectCard*>())
                if (c->name() == name) return c;
            return nullptr;
        };
        TEST_CHECK(grid.count() == 3 && grid.visibleCount() == 3 && card("Alpha"), "Test 603: trois cartes");
        QTest::mouseDClick(card("Bravo"), Qt::LeftButton);
        TEST_CHECK(opened == QStringList({ entries[1].path }), "Test 603: double-clic sur une carte — projet ouvert");
        opened.clear();
        QTest::mouseClick(card("Charlie")->findChild<QToolButton*>("ProjectCardOpen"), Qt::LeftButton);
        TEST_CHECK(opened == QStringList({ entries[2].path }), "Test 603: bouton Ouvrir de la carte");
        QTest::mouseClick(card("Alpha")->findChild<QToolButton*>("ProjectCardRemove"), Qt::LeftButton);
        TEST_CHECK(removed == QStringList({ entries[0].path }) && QFile::exists(entries[0].path), "Test 603: bouton Retirer — demandé, fichier intact");
        card("Bravo")->setFocus();
        opened.clear();
        QTest::keyClick(card("Bravo"), Qt::Key_Return);
        TEST_CHECK(opened == QStringList({ entries[1].path }), "Test 603: Entrée sur une carte — projet ouvert");

        TSA::Project::ProjectFileSummary named;
        named.valid = true;
        named.projectName = QStringLiteral("Pont de la Betsiboka");
        named.formatMajor = 3;
        named.formatMinor = 2;
        grid.updateSummary(entries[1].path, named);
        TEST_CHECK(card("Pont de la Betsiboka") != nullptr, "Test 603: nom enregistré affiché à la place du nom de fichier");
        named.passwordProtected = true;
        TEST_CHECK(TSA::UI::projectTypeText(named).contains(QStringLiteral("3.2")) && TSA::UI::projectTypeText(named).contains(QStringLiteral("protégé")),
                   "Test 603: type du projet, protection annoncée");
        grid.setFilter(QStringLiteral("betsiboka"));
        TEST_CHECK(grid.visibleCount() == 1, "Test 603: filtre sur le nom enregistré");
        grid.setFilter(QString());
        grid.setSortKey(ProjectSortKey::Name);
        grid.setProjects(entries, false, 2);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);   // anciennes cartes (deleteLater)
        QCoreApplication::processEvents();
        TEST_CHECK(grid.count() == 3 && grid.visibleCount() == 2 && !grid.findChild<QToolButton*>("ProjectCardRemove"),
                   "Test 603: limite d'affichage ; dossier de projets : pas de bouton Retirer");

        RecentFilesView view;
        view.resize(1000, 400);
        opened.clear();
        removed.clear();
        QObject::connect(&view, &RecentFilesView::openRequested, [&](const QString& p) { opened << p; });
        QObject::connect(&view, &RecentFilesView::removeRequested, [&](const QString& p) { removed << p; });
        view.setProjects(entries);
        view.show();
        QCoreApplication::processEvents();
        auto* table = view.findChild<QTableWidget*>("RecentFilesTable");
        TEST_CHECK(table && table->rowCount() == 3 && table->item(0, 1)->text() == QStringLiteral("Alpha.tsa"), "Test 603: liste triée par dernière ouverture");
        // Séquence d'un vrai double-clic sur une vue d'éléments : appui préalable, puis double-clic
        const QPoint rowCenter = table->visualItemRect(table->item(1, 0)).center();
        QTest::mouseClick(table->viewport(), Qt::LeftButton, Qt::NoModifier, rowCenter);
        QTest::mouseDClick(table->viewport(), Qt::LeftButton, Qt::NoModifier, rowCenter);
        TEST_CHECK(opened == QStringList({ entries[1].path }), "Test 603: double-clic sur une ligne — projet ouvert");
        auto buttons = table->cellWidget(2, 5)->findChildren<QToolButton*>();
        TEST_CHECK(buttons.size() == 2, "Test 603: boutons Ouvrir et Retirer sur chaque ligne");
        QTest::mouseClick(buttons[1], Qt::LeftButton);
        TEST_CHECK(removed == QStringList({ entries[2].path }), "Test 603: bouton Retirer de la ligne");
        TEST_CHECK(buttons[0]->width() >= buttons[0]->sizeHint().width(), "Test 603: bouton Ouvrir entier (" << buttons[0]->width() << " px)");
        view.setFilter(QStringLiteral("bravo"));
        TEST_CHECK(view.visibleCount() == 1 && view.count() == 3, "Test 603: filtre de la liste");
        view.setFilter(QString());
        view.setSortKey(ProjectSortKey::Name);
        TEST_CHECK(table->item(0, 1)->text() == QStringLiteral("Alpha.tsa") && table->item(2, 1)->text() == QStringLiteral("Charlie.tsa"), "Test 603: tri par nom");
        std::cout << "[PASS] Test 603: Accueil — cartes et liste des récents (souris, clavier, filtre, tri)" << std::endl;
        ++passed;
#else
        std::cout << "[SKIP] Test 603: accueil — NON EXÉCUTÉ (Qt Test absent)" << std::endl;
#endif
    }

    return true;
}
