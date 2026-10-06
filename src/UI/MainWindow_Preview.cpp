// Aperçus « dernier état du modèle » affichés par le Start Center (AppShell).
//  - Les aperçus sont capturés dans le vrai viewport (OccView::captureViewImage, rendu OCCT hors
//    écran), après une pause d'activité (debounce), uniquement si la révision du modèle ou la
//    caméra ont changé. L'écriture disque se fait hors du thread UI.
//  - La caméra est mémorisée par projet et restaurée à la réouverture.

#include "MainWindow.h"

#include "../Project/ModelPreviewCache.h"
#include "../Project/ProjectManager.h"
#include "../Project/RecentProjects.h"
#include "../Platform/WindowsAssociation.h"
#include "../Viewer/OccView.h"
#include "../Model/Model.h"
#include "Dock/ResultsDockWidget.h"

#include <QFileInfo>
#include <QThread>
#include <QTimer>

namespace
{
constexpr int kPreviewDelayMs = 2500;
}

void MainWindow::createPreviewCapture()
{
    m_previewTimer = new QTimer(this);
    m_previewTimer->setSingleShot(true);
    connect(m_previewTimer, &QTimer::timeout, this, [this] { capturePreview(false); });
    // Caméra : rotation, zoom, vues standard, mode 2D…
    connect(m_occView, &OccView::viewCameraChanged, this, [this] { schedulePreviewCapture(); });
}

void MainWindow::schedulePreviewCapture(int delayMs)
{
    if (!m_previewTimer || !m_projectManager || !m_projectManager->hasFilePath()) return;
    m_previewTimer->start(delayMs > 0 ? delayMs : kPreviewDelayMs); // redémarre : capture après la dernière action
}

void MainWindow::onModelRevisionPolled()
{
    if (!m_model) return;
    const quint64 rev = m_model->revision();
    if (rev == m_lastPolledRevision) return;
    m_lastPolledRevision = rev;
    schedulePreviewCapture();
    if (m_resultsDock) m_resultsDock->updateNodeStats();
}

void MainWindow::capturePreview(bool synchronousWrite)
{
    if (m_previewTimer) m_previewTimer->stop();
    if (!m_occView || !m_model || !m_projectManager || !m_projectManager->hasFilePath()) return;
    if (!isVisible() || window()->isMinimized()) return; // le rendu hors écran exige un viewport affiché

    const QJsonObject camera = m_occView->cameraState();
    if (camera.isEmpty()) return;
    const QString path = m_projectManager->currentFilePath();
    TSA::Project::ModelPreviewCache cache;
    if (!TSA::Project::ModelPreviewCache::needsCapture(cache.metadata(path), m_model->revision(), camera)) return;

    // Rendu du vrai viewport (même renderer OCCT, mêmes calques, même caméra), basse résolution.
    const QImage image = m_occView->captureViewImage(TSA::Project::ModelPreviewCache::kWidth, TSA::Project::ModelPreviewCache::kHeight);
    if (image.isNull()) return;

    TSA::Project::ProjectPreviewMetadata meta;
    meta.projectPath = TSA::Project::RecentProjects::normalize(path);
    meta.fileLastModified = QFileInfo(path).lastModified();
    meta.capturedAt = QDateTime::currentDateTime();
    meta.modelRevision = m_model->revision();
    meta.cameraState = camera;
    meta.viewState = m_occView->viewState();
    meta.nodeCount = static_cast<int>(m_model->nodes().size());
    meta.elementCount = static_cast<int>(m_model->beams().size() + m_model->columns().size() + m_model->slabs().size()
                                         + m_model->walls().size() + m_model->foundations().size()
                                         + m_model->trussMembers().size() + m_model->cables().size());

    emit previewCaptured(path, image);
    if (synchronousWrite)
    {
        cache.store(image, meta);
        return;
    }
    QThread* worker = QThread::create([image, meta] { TSA::Project::ModelPreviewCache().store(image, meta); });
    connect(worker, &QThread::finished, worker, &QObject::deleteLater);
    worker->start(QThread::LowPriority);
}

void MainWindow::onProjectFileOpened(const QString& path)
{
    TSA::Project::RecentProjects().touch(path);
    m_lastPolledRevision = m_model ? m_model->revision() : 0;
    if (m_resultsDock) m_resultsDock->updateNodeStats();
    // Réouverture : dernière caméra connue, si le fichier n'a pas été modifié ailleurs depuis.
    TSA::Project::ModelPreviewCache cache;
    if (const auto meta = cache.metadata(path); meta && m_occView)
    {
        const bool fileNewer = QFileInfo(path).lastModified() > meta->capturedAt.addSecs(2);
        if (!fileNewer && m_occView->applyCameraState(meta->cameraState))
        {
            if (m_statusInfo) m_statusInfo->setText(tr("Dernière vue du projet restaurée"));
            return;
        }
    }
    schedulePreviewCapture(1500); // premier aperçu après le premier rendu
}

void MainWindow::onProjectFileSaved(const QString& path)
{
    TSA::Project::RecentProjects().touch(path);
    TSA::Platform::WindowsAssociation::notifyFileUpdated(path); // miniature Explorateur à jour
    capturePreview(false);
}
