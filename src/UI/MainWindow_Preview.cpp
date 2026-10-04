// Aperçus « dernier état du modèle » et page d'accueil « Projets récents ».
//  - La zone centrale est un QStackedWidget : page d'accueil OU l'unique viewport (jamais deux).
//  - Les aperçus sont capturés dans le vrai viewport (OccView::captureViewImage, rendu OCCT hors
//    écran), après une pause d'activité (debounce), uniquement si la révision du modèle ou la
//    caméra ont changé. L'écriture disque se fait hors du thread UI.
//  - La caméra est mémorisée par projet et restaurée à la réouverture.

#include "MainWindow.h"

#include "Home/StartPage.h"
#include "Ruler/ViewportContainer.h"
#include "../Project/ModelPreviewCache.h"
#include "../Project/ProjectManager.h"
#include "../Project/RecentProjects.h"
#include "../Viewer/OccView.h"
#include "../Model/Model.h"

#include <QAction>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QInputDialog>
#include <QLocale>
#include <QMenu>
#include <QMessageBox>
#include <QProcess>
#include <QShortcut>
#include <QStackedWidget>
#include <QThread>
#include <QTimer>
#include <QUrl>

namespace
{
constexpr int kPreviewDelayMs = 2500;
}

void MainWindow::createStartPage()
{
    m_centralStack = new QStackedWidget(this);
    m_startPage = new TSA::UI::StartPage(m_centralStack);
    m_centralStack->addWidget(m_startPage);
    m_centralStack->addWidget(m_viewportContainer);
    setCentralWidget(m_centralStack);
    m_centralStack->setCurrentWidget(m_startPage);

    connect(m_startPage, &TSA::UI::StartPage::openRequested, this, [this](const QString& path) {
        if (m_projectManager && m_projectManager->hasFilePath()
            && TSA::Project::RecentProjects::normalize(m_projectManager->currentFilePath()).compare(path, Qt::CaseInsensitive) == 0)
        {
            showViewport(); // projet déjà ouvert
            return;
        }
        if (!maybeSave()) return;
        loadFile(path);
    });
    connect(m_startPage, &TSA::UI::StartPage::newProjectRequested, this, [this] { onActionNew(); showViewport(); });
    connect(m_startPage, &TSA::UI::StartPage::openDialogRequested, this, &MainWindow::onActionOpen);
    connect(m_startPage, &TSA::UI::StartPage::continueRequested, this, &MainWindow::showViewport);
    connect(m_startPage, &TSA::UI::StartPage::cardContextMenuRequested, this, &MainWindow::showRecentProjectMenu);
    auto* esc = new QShortcut(QKeySequence(Qt::Key_Escape), m_startPage);
    connect(esc, &QShortcut::activated, this, [this] { if (m_model && m_model->revision() > 0) showViewport(); });

    m_previewTimer = new QTimer(this);
    m_previewTimer->setSingleShot(true);
    connect(m_previewTimer, &QTimer::timeout, this, [this] { capturePreview(false); });

    m_actionStartPage = new QAction(QIcon(":/icons/file_tsa.svg"), tr("Projets récents"), this);
    m_actionStartPage->setToolTip(tr("Page d'accueil : projets récents avec l'aperçu du dernier état de chaque modèle"));
    connect(m_actionStartPage, &QAction::triggered, this, &MainWindow::showStartPage);

    m_startPage->refresh();
}

void MainWindow::connectPreviewTriggers()
{
    // Caméra : rotation, zoom, vues standard, mode 2D…
    if (m_occView)
        connect(m_occView, &OccView::viewCameraChanged, this, [this] { schedulePreviewCapture(); });
}

void MainWindow::showStartPage()
{
    if (!m_centralStack) return;
    // Dernier état du projet en cours avant de quitter le viewport (pour que sa carte soit à jour).
    capturePreview(false);
    const QString current = m_projectManager && m_projectManager->hasFilePath() ? m_projectManager->currentFilePath() : QString();
    m_startPage->refresh(current, m_model && (m_model->revision() > 0 || !current.isEmpty()));
    m_centralStack->setCurrentWidget(m_startPage);
}

void MainWindow::showViewport()
{
    if (m_centralStack && m_centralStack->currentWidget() != m_viewportContainer)
        m_centralStack->setCurrentWidget(m_viewportContainer); // initialise OCCT au premier affichage
}

bool MainWindow::isViewportShown() const
{
    return !m_centralStack || m_centralStack->currentWidget() == m_viewportContainer;
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
    // Une modification du modèle depuis l'accueil (commande, Undo…) ramène au viewport.
    if (!isViewportShown()) showViewport();
}

void MainWindow::capturePreview(bool synchronousWrite)
{
    if (m_previewTimer) m_previewTimer->stop();
    if (!m_occView || !m_model || !m_projectManager || !m_projectManager->hasFilePath()) return;
    if (!isViewportShown() || isMinimized()) return; // le rendu hors écran exige un viewport initialisé

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

    if (m_startPage) m_startPage->updatePreview(path, image);
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
    capturePreview(false);
}

void MainWindow::showRecentProjectMenu(const QString& path, const QPoint& globalPos)
{
    const QFileInfo fi(path);
    const bool isCurrent = m_projectManager && m_projectManager->hasFilePath()
                        && TSA::Project::RecentProjects::normalize(m_projectManager->currentFilePath()).compare(path, Qt::CaseInsensitive) == 0;
    TSA::Project::RecentProjects recent;
    TSA::Project::ModelPreviewCache cache;

    QMenu menu(this);
    QAction* actOpen = menu.addAction(QIcon(":/icons/file_open.svg"), isCurrent ? tr("Revenir au modèle") : tr("Ouvrir"));
    menu.setDefaultAction(actOpen);
    menu.addSeparator();
    QAction* actRename = menu.addAction(tr("Renommer…"));
    QAction* actDuplicate = menu.addAction(tr("Dupliquer"));
    QAction* actFolder = menu.addAction(tr("Afficher dans le dossier"));
    QAction* actRefresh = menu.addAction(tr("Actualiser l'aperçu"));
    menu.addSeparator();
    QAction* actForget = menu.addAction(tr("Retirer de la liste"));
    QAction* actDelete = menu.addAction(tr("Supprimer (Corbeille)…"));
    menu.addSeparator();
    QAction* actProps = menu.addAction(tr("Propriétés"));
    actRename->setEnabled(!isCurrent);
    actDelete->setEnabled(!isCurrent);
    if (isCurrent)
    {
        actRename->setToolTip(tr("Fermez ou changez de projet avant de le renommer"));
        actDelete->setToolTip(tr("Impossible de supprimer le projet ouvert"));
    }

    QAction* chosen = menu.exec(globalPos);
    if (!chosen) return;

    if (chosen == actOpen)
    {
        if (isCurrent) showViewport();
        else if (maybeSave()) loadFile(path);
        return;
    }
    if (chosen == actFolder)
    {
#ifdef _WIN32
        QProcess::startDetached("explorer.exe", { "/select,", QDir::toNativeSeparators(path) });
#else
        QDesktopServices::openUrl(QUrl::fromLocalFile(fi.absolutePath()));
#endif
        return;
    }
    if (chosen == actRefresh)
    {
        cache.remove(path); // repli sur la miniature embarquée ou nouvelle capture
        if (isCurrent && isViewportShown()) capturePreview(true);
        m_startPage->refresh(isCurrent ? path : (m_projectManager ? m_projectManager->currentFilePath() : QString()),
                             m_model && m_model->revision() > 0);
        return;
    }
    if (chosen == actForget)
    {
        recent.remove(path);
        cache.remove(path);
    }
    else if (chosen == actRename)
    {
        bool ok = false;
        QString name = QInputDialog::getText(this, tr("Renommer le projet"), tr("Nouveau nom :"), QLineEdit::Normal,
                                             fi.completeBaseName(), &ok).trimmed();
        if (!ok || name.isEmpty()) return;
        if (!name.endsWith(".tsa", Qt::CaseInsensitive)) name += ".tsa";
        const QString target = fi.absoluteDir().filePath(name);
        if (QFileInfo::exists(target) || !QFile::rename(path, target))
        {
            QMessageBox::warning(this, tr("Renommer"), tr("Impossible de renommer en « %1 » (nom déjà utilisé ou fichier verrouillé).").arg(name));
            return;
        }
        recent.rename(path, target);
        cache.rename(path, target);
    }
    else if (chosen == actDuplicate)
    {
        QString target = fi.absoluteDir().filePath(fi.completeBaseName() + tr(" - copie.tsa"));
        for (int i = 2; QFileInfo::exists(target); ++i)
            target = fi.absoluteDir().filePath(fi.completeBaseName() + tr(" - copie (%1).tsa").arg(i));
        if (!QFile::copy(path, target))
        {
            QMessageBox::warning(this, tr("Dupliquer"), tr("Copie impossible vers %1").arg(target));
            return;
        }
        // L'aperçu du dernier état vaut aussi pour la copie.
        if (auto meta = cache.metadata(path))
        {
            const QImage img(meta->previewPath);
            meta->projectPath = target;
            meta->capturedAt = QDateTime::currentDateTime();
            cache.store(img, *meta);
        }
        recent.touch(target);
    }
    else if (chosen == actDelete)
    {
        if (QMessageBox::question(this, tr("Supprimer le projet"),
                                  tr("Envoyer « %1 » à la Corbeille ?").arg(fi.fileName())) != QMessageBox::Yes)
            return;
        if (!QFile::moveToTrash(path))
        {
            QMessageBox::warning(this, tr("Supprimer"), tr("Impossible de déplacer le fichier dans la Corbeille."));
            return;
        }
        recent.remove(path);
        cache.remove(path);
    }
    else if (chosen == actProps)
    {
        const auto meta = cache.metadata(path);
        TSA::Project::PreviewSource source = TSA::Project::PreviewSource::None;
        cache.preview(path, &source);
        const QLocale loc;
        QString text = tr("<b>%1</b><br>%2<br><br>Taille : %3<br>Modifié : %4<br>")
                           .arg(fi.fileName().toHtmlEscaped(), QDir::toNativeSeparators(fi.absolutePath()).toHtmlEscaped(),
                                loc.formattedDataSize(fi.size()), loc.toString(fi.lastModified(), QLocale::ShortFormat));
        if (meta)
            text += tr("<br><b>Dernier état capturé</b> : %1<br>%2 nœuds · %3 éléments<br>Vue : %4%5")
                        .arg(loc.toString(meta->capturedAt, QLocale::ShortFormat))
                        .arg(meta->nodeCount).arg(meta->elementCount)
                        .arg(meta->cameraState["projection"].toString() == "orthographic" ? tr("orthographique") : tr("perspective"),
                             meta->viewState["mode2D"].toBool() ? tr(" · plan 2D") : QString());
        text += tr("<br>Source de l'aperçu : %1")
                    .arg(source == TSA::Project::PreviewSource::Cache ? tr("dernier état capturé dans TSA")
                         : source == TSA::Project::PreviewSource::Embedded ? tr("miniature enregistrée dans le fichier")
                                                                           : tr("aucune"));
        QMessageBox::information(this, tr("Propriétés du projet"), text);
        return;
    }
    m_startPage->refresh(m_projectManager ? m_projectManager->currentFilePath() : QString(), m_model && m_model->revision() > 0);
}
