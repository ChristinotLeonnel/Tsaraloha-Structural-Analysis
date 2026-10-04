// Suite « preview » : projets récents et aperçus du dernier état du modèle (tests 124-126).

#include "test_common.h"

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
    return true;
}
