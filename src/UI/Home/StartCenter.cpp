#include "StartCenter.h"

#include "../Theme/ThemeManager.h"
#include "../../Project/ModelPreviewCache.h"
#include "../../Project/RecentProjects.h"

#include <QApplication>
#include <QComboBox>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QGraphicsDropShadowEffect>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QLinearGradient>
#include <QLocale>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QProcess>
#include <QPushButton>
#include <QScrollArea>
#include <QStyle>
#include <QSvgRenderer>
#include <QThread>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>

namespace TSA::UI
{

namespace
{
constexpr int kContentMaxWidth = 1240;
constexpr int kCompactHeight = 760;

enum SortKey { SortLastOpened, SortName, SortModified };

const char* kCardStyle =
    "TSA--UI--ProjectCard { background: palette(base); border: 1px solid palette(mid); border-radius: 6px; }"
    "TSA--UI--ProjectCard[hovered=\"true\"] { border: 1px solid #388BFD; }"
    "TSA--UI--ProjectCard[selected=\"true\"] { border: 2px solid #1F6FEB; }";

QPixmap cropToFill(const QImage& img, int w, int h)
{
    // Remplit le cadre 16:9 sans déformer ; les miniatures carrées embarquées sont recadrées au centre.
    const QImage scaled = img.scaled(w, h, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    const int x = (scaled.width() - w) / 2, y = (scaled.height() - h) / 2;
    return QPixmap::fromImage(scaled.copy(x, y, w, h));
}

QLabel* elidedLabel(const QString& text, const QString& style, int width, Qt::TextElideMode mode, QWidget* parent)
{
    auto* label = new QLabel(parent);
    label->setStyleSheet(style);
    label->setText(label->fontMetrics().elidedText(text, mode, width));
    return label;
}
} // namespace

// -----------------------------------------------------------------------------
// ProjectCard
// -----------------------------------------------------------------------------

ProjectCard::ProjectCard(const QString& path, const QDateTime& lastOpened, QWidget* parent)
    : QFrame(parent)
    , m_path(path)
    , m_lastOpened(lastOpened)
{
    const QFileInfo fi(path);
    m_name = fi.completeBaseName();
    m_lastModified = fi.lastModified();

    setObjectName("ProjectCard");
    setStyleSheet(kCardStyle);
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::StrongFocus);
    setFixedWidth(kImageWidth + 4);
    setToolTip(tr("%1\nDouble-clic pour ouvrir").arg(QDir::toNativeSeparators(path)));
    setAccessibleName(m_name);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(2, 2, 2, 10);
    layout->setSpacing(4);

    m_image = new QLabel(this);
    m_image->setFixedSize(kImageWidth, kImageHeight);
    m_image->setStyleSheet("border-top-left-radius: 5px; border-top-right-radius: 5px; background: #161B22;");
    m_image->setPixmap(placeholder(fi.suffix().toUpper()));
    layout->addWidget(m_image);
    layout->addSpacing(4);

    const int textWidth = kImageWidth - 20;
    layout->addWidget(elidedLabel(m_name, "font-weight: 600; font-size: 13px; padding: 0 10px;", textWidth, Qt::ElideRight, this));
    layout->addWidget(elidedLabel(QDir::toNativeSeparators(fi.absolutePath()), "color: palette(mid); font-size: 11px; padding: 0 10px;",
                                  textWidth, Qt::ElideMiddle, this));
    const QString opened = lastOpened.isValid()
        ? tr("Ouvert %1  ·  %2").arg(TSA::Project::RecentProjects::relativeTime(lastOpened),
                                     QLocale().toString(lastOpened, QLocale::ShortFormat))
        : tr("Modifié le %1").arg(QLocale().toString(m_lastModified, QLocale::ShortFormat));
    layout->addWidget(elidedLabel(opened, "color: palette(mid); font-size: 11px; padding: 0 10px;", textWidth, Qt::ElideRight, this));

    m_shadow = new QGraphicsDropShadowEffect(this);
    m_shadow->setBlurRadius(0);
    m_shadow->setOffset(0, 0);
    m_shadow->setColor(QColor(0, 0, 0, 140));
    setGraphicsEffect(m_shadow);
}

QPixmap ProjectCard::placeholder(const QString& format)
{
    QPixmap pix(kImageWidth, kImageHeight);
    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);
    QLinearGradient g(0, 0, 0, kImageHeight);
    g.setColorAt(0, QColor(0x1C, 0x23, 0x2B));
    g.setColorAt(1, QColor(0x12, 0x17, 0x1D));
    p.fillRect(pix.rect(), g);
    QSvgRenderer logo(QStringLiteral(":/icons/TSA_glyph.svg"));
    if (logo.isValid())
        logo.render(&p, QRectF(kImageWidth / 2.0 - 26, kImageHeight / 2.0 - 40, 52, 52));
    p.setPen(QColor(0x8B, 0x94, 0x9E));
    p.drawText(QRect(0, kImageHeight / 2 + 18, kImageWidth, 24), Qt::AlignCenter,
               format == "TSA" ? QObject::tr("Aucun aperçu") : QObject::tr("Aperçu indisponible"));
    return pix;
}

void ProjectCard::setPreview(const QImage& image)
{
    if (image.isNull()) return;
    m_image->setPixmap(cropToFill(image, kImageWidth, kImageHeight));
}

void ProjectCard::setStateProperty(const char* name, bool value)
{
    setProperty(name, value);
    style()->unpolish(this);
    style()->polish(this);
}

void ProjectCard::setSelected(bool selected)
{
    setStateProperty("selected", selected);
}

bool ProjectCard::matches(const QString& filter) const
{
    return filter.isEmpty() || m_name.contains(filter, Qt::CaseInsensitive) || m_path.contains(filter, Qt::CaseInsensitive);
}

void ProjectCard::enterEvent(QEnterEvent* event)
{
    QFrame::enterEvent(event);
    setStateProperty("hovered", true);
    m_shadow->setBlurRadius(18);
    m_shadow->setOffset(0, 3);
}

void ProjectCard::leaveEvent(QEvent* event)
{
    QFrame::leaveEvent(event);
    setStateProperty("hovered", false);
    m_shadow->setBlurRadius(0);
    m_shadow->setOffset(0, 0);
}

void ProjectCard::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton || event->button() == Qt::RightButton)
    {
        setFocus(Qt::MouseFocusReason);
        emit clicked(this);
    }
    QFrame::mousePressEvent(event);
}

void ProjectCard::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton)
    {
        emit openRequested(m_path);
        return;
    }
    QFrame::mouseDoubleClickEvent(event);
}

void ProjectCard::contextMenuEvent(QContextMenuEvent* event)
{
    emit contextMenuRequested(m_path, event->globalPos());
}

void ProjectCard::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)
    {
        emit openRequested(m_path);
        return;
    }
    if (event->key() == Qt::Key_Menu)
    {
        emit contextMenuRequested(m_path, mapToGlobal(rect().center()));
        return;
    }
    QFrame::keyPressEvent(event);
}

// -----------------------------------------------------------------------------
// StartCenter
// -----------------------------------------------------------------------------

StartCenter::StartCenter(QWidget* parent)
    : QWidget(parent)
{
    setObjectName("StartCenter");
    setAutoFillBackground(true);

    // Contenu centré, largeur limitée : lisible sur grand écran comme en fenêtre réduite.
    auto* outer = new QHBoxLayout(this);
    outer->setContentsMargins(24, 0, 24, 0);
    auto* content = new QWidget(this);
    content->setMaximumWidth(kContentMaxWidth);
    outer->addStretch();
    outer->addWidget(content, 100);
    outer->addStretch();

    auto* root = new QVBoxLayout(content);
    root->setContentsMargins(0, 36, 0, 20);
    root->setSpacing(0);

    // Identité + actions principales
    m_logo = new QLabel(content);
    m_logo->setPixmap(QIcon(":/icons/TSA.svg").pixmap(72, 72));
    m_logo->setAlignment(Qt::AlignCenter);
    root->addWidget(m_logo);
    root->addSpacing(8);

    auto* title = new QLabel(QStringLiteral("TSA"), content);
    title->setObjectName("StartCenterTitle");
    title->setAlignment(Qt::AlignCenter);
    root->addWidget(title);
    auto* subtitle = new QLabel(tr("Structural Analysis"), content);
    subtitle->setObjectName("StartCenterSubtitle");
    subtitle->setAlignment(Qt::AlignCenter);
    root->addWidget(subtitle);
    root->addSpacing(22);

    m_btnNew = new QPushButton(QIcon(":/icons/file_new.svg"), tr("Nouveau projet"), content);
    m_btnNew->setObjectName("StartCenterPrimary");
    m_btnOpen = new QPushButton(QIcon(":/icons/file_open.svg"), tr("Ouvrir un projet"), content);
    m_btnOpen->setObjectName("StartCenterSecondary");
    m_btnNew->setToolTip(tr("Créer un projet TSA (Ctrl+N)"));
    m_btnOpen->setToolTip(tr("Ouvrir un fichier .tsa (Ctrl+O)"));
    for (auto* b : { m_btnNew, m_btnOpen })
    {
        b->setFixedSize(280, 40);
        b->setIconSize(QSize(18, 18));
        b->setCursor(Qt::PointingHandCursor);
        auto* row = new QHBoxLayout();
        row->addStretch();
        row->addWidget(b);
        row->addStretch();
        root->addLayout(row);
        root->addSpacing(8);
    }
    connect(m_btnNew, &QPushButton::clicked, this, &StartCenter::newProjectRequested);
    connect(m_btnOpen, &QPushButton::clicked, this, &StartCenter::openDialogRequested);
    root->addSpacing(22);

    // Projets récents : titre, recherche, tri
    auto* header = new QHBoxLayout();
    header->setSpacing(10);
    auto* recentTitle = new QLabel(tr("Projets récents"), content);
    recentTitle->setObjectName("StartCenterSection");
    header->addWidget(recentTitle);
    m_count = new QLabel(content);
    m_count->setStyleSheet("color: palette(mid);");
    header->addWidget(m_count);
    header->addStretch();

    m_search = new QLineEdit(content);
    m_search->setPlaceholderText(tr("Rechercher un projet (nom ou dossier)…"));
    m_search->setClearButtonEnabled(true);
    m_search->setFixedWidth(300);
    m_search->setMinimumHeight(28);
    header->addWidget(m_search);

    m_sort = new QComboBox(content);
    m_sort->addItem(tr("Dernière ouverture"), SortLastOpened);
    m_sort->addItem(tr("Nom"), SortName);
    m_sort->addItem(tr("Date de modification"), SortModified);
    m_sort->setMinimumHeight(28);
    m_sort->setToolTip(tr("Trier les projets récents"));
    header->addWidget(m_sort);
    root->addLayout(header);
    root->addSpacing(12);

    connect(m_search, &QLineEdit::textChanged, this, &StartCenter::applyFilter);
    connect(m_sort, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &StartCenter::applyFilter);

    // Grille de cartes, responsive
    m_scroll = new QScrollArea(content);
    m_scroll->setWidgetResizable(true);
    m_scroll->setFrameShape(QFrame::NoFrame);
    m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_cardsHost = new QWidget(m_scroll);
    m_grid = new QGridLayout(m_cardsHost);
    m_grid->setContentsMargins(4, 4, 4, 4);
    m_grid->setHorizontalSpacing(20);
    m_grid->setVerticalSpacing(20);
    m_grid->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    m_scroll->setWidget(m_cardsHost);
    root->addWidget(m_scroll, 1);

    m_empty = new QLabel(content);
    m_empty->setAlignment(Qt::AlignCenter);
    m_empty->setWordWrap(true);
    m_empty->setStyleSheet("color: palette(mid); font-size: 13px;");
    root->addWidget(m_empty, 1);
    m_empty->hide();

    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, &StartCenter::applyTheme);
    applyTheme(ThemeManager::instance().isDarkMode());
}

void StartCenter::applyTheme(bool dark)
{
    const QString accent = dark ? "#1F6FEB" : "#0969DA";
    setStyleSheet(QStringLiteral(
        "#StartCenterTitle { font-size: 34px; font-weight: 700; letter-spacing: 2px; }"
        "#StartCenterSubtitle { font-size: 14px; color: %1; }"
        "#StartCenterSection { font-size: 17px; font-weight: 600; }"
        "QPushButton#StartCenterPrimary { background: %2; color: white; border: none; border-radius: 4px;"
        "   font-size: 13px; font-weight: 600; }"
        "QPushButton#StartCenterPrimary:hover { background: %3; }"
        "QPushButton#StartCenterSecondary { background: transparent; border: 1px solid %4; border-radius: 4px;"
        "   font-size: 13px; font-weight: 600; }"
        "QPushButton#StartCenterSecondary:hover { border-color: %2; color: %2; }")
        .arg(dark ? "#8B949E" : "#57606A", accent, dark ? "#388BFD" : "#0550AE", dark ? "#30363D" : "#D0D7DE"));
}

void StartCenter::refresh()
{
    ++m_generation;
    for (auto& c : m_cards)
        if (c) c->deleteLater();
    m_cards.clear();
    m_visible.clear();
    m_selected = nullptr;
    m_columns = 0;

    for (const auto& r : TSA::Project::RecentProjects().list(false))
    {
        auto* card = new ProjectCard(r.path, r.lastOpened, m_cardsHost);
        connect(card, &ProjectCard::clicked, this, &StartCenter::selectCard);
        connect(card, &ProjectCard::openRequested, this, &StartCenter::openRequested);
        connect(card, &ProjectCard::contextMenuRequested, this, &StartCenter::showCardMenu);
        m_cards.push_back(card);
    }
    applyFilter();
    loadPreviewsAsync();
}

void StartCenter::applyFilter()
{
    const QString filter = m_search->text().trimmed();
    m_visible.clear();
    for (auto& c : m_cards)
    {
        if (!c) continue;
        const bool match = c->matches(filter);
        c->setVisible(match);
        if (match) m_visible.push_back(c);
    }

    const int key = m_sort->currentData().toInt();
    std::stable_sort(m_visible.begin(), m_visible.end(), [key](const QPointer<ProjectCard>& a, const QPointer<ProjectCard>& b) {
        if (key == SortName) return a->name().localeAwareCompare(b->name()) < 0;
        if (key == SortModified) return a->lastModified() > b->lastModified();
        return a->lastOpened() > b->lastOpened();
    });

    if (m_selected && !m_selected->isVisible())
    {
        m_selected->setSelected(false);
        m_selected = nullptr;
    }

    m_count->setText(m_cards.isEmpty() ? QString()
                     : filter.isEmpty() ? tr("(%1)").arg(m_cards.size())
                                        : tr("(%1 sur %2)").arg(m_visible.size()).arg(m_cards.size()));
    m_empty->setText(m_cards.isEmpty()
                         ? tr("Aucun projet récent.\nCréez un nouveau projet ou ouvrez un fichier .tsa : son aperçu apparaîtra ici.")
                         : tr("Aucun projet ne correspond à « %1 ».").arg(filter));
    m_empty->setVisible(m_visible.isEmpty());
    m_scroll->setVisible(!m_visible.isEmpty());
    m_search->setEnabled(!m_cards.isEmpty());
    m_sort->setEnabled(!m_cards.isEmpty());

    m_columns = 0; // force la remise en page (l'ordre ou le filtre ont changé)
    relayoutCards();
}

void StartCenter::selectCard(ProjectCard* card)
{
    if (m_selected == card) return;
    if (m_selected) m_selected->setSelected(false);
    m_selected = card;
    if (card) card->setSelected(true);
}

void StartCenter::updatePreview(const QString& path, const QImage& image)
{
    const QString key = TSA::Project::RecentProjects::normalize(path);
    for (auto& c : m_cards)
        if (c && c->path().compare(key, Qt::CaseInsensitive) == 0) c->setPreview(image);
}

void StartCenter::loadPreviewsAsync()
{
    QStringList paths;
    for (auto& c : m_cards)
        if (c) paths << c->path();
    if (paths.isEmpty()) return;

    // Lecture disque et décodage hors du thread UI ; chaque aperçu est publié dès qu'il est prêt.
    const quint64 generation = m_generation;
    QPointer<StartCenter> self(this);
    QThread* worker = QThread::create([self, paths, generation] {
        TSA::Project::ModelPreviewCache cache;
        for (const QString& path : paths)
        {
            const QImage img = cache.preview(path);
            if (img.isNull()) continue;
            QMetaObject::invokeMethod(qApp, [self, path, img, generation] {
                if (self && self->m_generation == generation) self->updatePreview(path, img);
            }, Qt::QueuedConnection);
        }
    });
    connect(worker, &QThread::finished, worker, &QObject::deleteLater);
    worker->start(QThread::LowPriority);
}

void StartCenter::relayoutCards()
{
    const int available = m_scroll->viewport()->width() - 8;
    const int columns = std::max(1, (available + m_grid->horizontalSpacing()) / (ProjectCard::kImageWidth + 4 + m_grid->horizontalSpacing()));
    if (columns == m_columns) return;
    m_columns = columns;
    while (m_grid->count() > 0) m_grid->takeAt(0);
    int i = 0;
    for (auto& c : m_visible)
    {
        if (!c) continue;
        m_grid->addWidget(c, i / columns, i % columns);
        ++i;
    }
}

void StartCenter::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    // Fenêtre basse : le logo cède sa place aux projets récents.
    m_logo->setVisible(height() >= kCompactHeight);
    // La zone de défilement n'a sa taille définitive qu'après la mise en page des enfants.
    QTimer::singleShot(0, this, &StartCenter::relayoutCards);
}

void StartCenter::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    QTimer::singleShot(0, this, &StartCenter::relayoutCards);
}

void StartCenter::showCardMenu(const QString& path, const QPoint& globalPos)
{
    const QFileInfo fi(path);
    TSA::Project::RecentProjects recent;
    TSA::Project::ModelPreviewCache cache;

    QMenu menu(this);
    QAction* actOpen = menu.addAction(QIcon(":/icons/file_open.svg"), tr("Ouvrir"));
    menu.setDefaultAction(actOpen);
    QAction* actFolder = menu.addAction(tr("Afficher dans l'explorateur"));
    QAction* actForget = menu.addAction(tr("Retirer des projets récents"));
    actForget->setToolTip(tr("Le fichier n'est pas supprimé"));
    menu.addSeparator();
    QAction* actRename = menu.addAction(tr("Renommer…"));
    QAction* actDuplicate = menu.addAction(tr("Dupliquer"));
    QAction* actRefresh = menu.addAction(tr("Actualiser l'aperçu"));
    QAction* actDelete = menu.addAction(tr("Supprimer le fichier (Corbeille)…"));
    menu.addSeparator();
    QAction* actProps = menu.addAction(tr("Propriétés"));

    QAction* chosen = menu.exec(globalPos);
    if (!chosen) return;

    if (chosen == actOpen)
    {
        emit openRequested(path);
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
    if (chosen == actForget)
    {
        // Retire uniquement l'entrée de la liste (et son aperçu en cache) : le fichier reste sur disque.
        recent.remove(path);
        cache.remove(path);
    }
    else if (chosen == actRefresh)
    {
        cache.remove(path); // repli sur la miniature embarquée dans le .tsa
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
    refresh();
}

} // namespace TSA::UI
