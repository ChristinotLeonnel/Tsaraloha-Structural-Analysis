#include "StartPage.h"

#include "../../Project/ModelPreviewCache.h"
#include "../../Project/RecentProjects.h"

#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QGraphicsDropShadowEffect>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
#include <QSvgRenderer>
#include <QThread>
#include <QTimer>
#include <QVBoxLayout>

namespace TSA::UI
{

namespace
{
// Couleurs alignées sur le thème sombre de TSA (ThemeManager) ; la carte reste lisible en clair.
const char* kCardStyle =
    "TSA--UI--ProjectCard { background: palette(base); border: 1px solid palette(mid); border-radius: 6px; }"
    "TSA--UI--ProjectCard[hovered=\"true\"] { border: 1px solid #388BFD; }"
    "TSA--UI--ProjectCard[current=\"true\"] { border: 1px solid #3FB950; }";

QPixmap cropToFill(const QImage& img, int w, int h)
{
    // Remplit le cadre 16:9 sans déformer ; les miniatures carrées embarquées sont recadrées au centre.
    const QImage scaled = img.scaled(w, h, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    const int x = (scaled.width() - w) / 2, y = (scaled.height() - h) / 2;
    return QPixmap::fromImage(scaled.copy(x, y, w, h));
}
} // namespace

// -----------------------------------------------------------------------------
// ProjectCard
// -----------------------------------------------------------------------------

ProjectCard::ProjectCard(const QString& path, const QString& title, const QString& subtitle, const QString& format, QWidget* parent)
    : QFrame(parent)
    , m_path(path)
    , m_format(format)
{
    setObjectName("ProjectCard");
    setStyleSheet(kCardStyle);
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::StrongFocus);
    setFixedWidth(kImageWidth + 2);
    setToolTip(QDir::toNativeSeparators(path));
    setAccessibleName(title);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(1, 1, 1, 8);
    layout->setSpacing(6);

    m_image = new QLabel(this);
    m_image->setFixedSize(kImageWidth, kImageHeight);
    m_image->setStyleSheet("border-top-left-radius: 5px; border-top-right-radius: 5px; background: #161B22;");
    m_image->setPixmap(placeholder(format));
    layout->addWidget(m_image);

    // Badge de format : petit, dans le coin, ne masque pas le modèle.
    m_badge = new QLabel(format, m_image);
    m_badge->setStyleSheet("background: rgba(13,17,23,170); color: #C9D1D9; font-size: 10px; font-weight: 600;"
                           "padding: 1px 6px; border-radius: 3px;");
    m_badge->adjustSize();
    m_badge->move(kImageWidth - m_badge->width() - 8, kImageHeight - m_badge->height() - 8);

    m_openHint = new QLabel(tr("Ouvrir →"), m_image);
    m_openHint->setStyleSheet("background: #1F6FEB; color: white; font-weight: 600; padding: 3px 10px; border-radius: 3px;");
    m_openHint->adjustSize();
    m_openHint->move(8, kImageHeight - m_openHint->height() - 8);
    m_openHint->hide();

    auto* name = new QLabel(this);
    name->setStyleSheet("font-weight: 600; font-size: 13px; padding: 0 10px;");
    name->setText(name->fontMetrics().elidedText(title, Qt::ElideMiddle, kImageWidth - 20));
    layout->addWidget(name);
    auto* sub = new QLabel(subtitle, this);
    sub->setStyleSheet("color: palette(mid); font-size: 11px; padding: 0 10px;");
    layout->addWidget(sub);

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
        logo.render(&p, QRectF(kImageWidth / 2.0 - 28, kImageHeight / 2.0 - 44, 56, 56));
    p.setPen(QColor(0x8B, 0x94, 0x9E));
    p.drawText(QRect(0, kImageHeight / 2 + 20, kImageWidth, 24), Qt::AlignCenter,
               format == "TSA" ? QObject::tr("Aucun aperçu") : QObject::tr("Aperçu indisponible"));
    return pix;
}

void ProjectCard::setPreview(const QImage& image)
{
    if (image.isNull()) return;
    m_image->setPixmap(cropToFill(image, kImageWidth, kImageHeight));
}

void ProjectCard::setCurrent(bool current)
{
    setProperty("current", current);
    style()->unpolish(this);
    style()->polish(this);
}

void ProjectCard::setHovered(bool hovered)
{
    setProperty("hovered", hovered);
    style()->unpolish(this);
    style()->polish(this);
    m_openHint->setVisible(hovered);
    m_shadow->setBlurRadius(hovered ? 18 : 0);
    m_shadow->setOffset(0, hovered ? 3 : 0);
}

void ProjectCard::enterEvent(QEnterEvent* event)
{
    QFrame::enterEvent(event);
    setHovered(true);
}

void ProjectCard::leaveEvent(QEvent* event)
{
    QFrame::leaveEvent(event);
    setHovered(false);
}

void ProjectCard::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && rect().contains(event->pos())) emit openRequested(m_path);
    QFrame::mouseReleaseEvent(event);
}

void ProjectCard::contextMenuEvent(QContextMenuEvent* event)
{
    emit contextMenuRequested(m_path, event->globalPos());
}

void ProjectCard::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter || event->key() == Qt::Key_Space)
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
// StartPage
// -----------------------------------------------------------------------------

StartPage::StartPage(QWidget* parent)
    : QWidget(parent)
{
    setObjectName("StartPage");
    setAutoFillBackground(true);
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(40, 28, 40, 20);
    root->setSpacing(18);

    // En-tête : identité TSA + actions principales
    auto* header = new QHBoxLayout();
    auto* logo = new QLabel(this);
    logo->setPixmap(QIcon(":/icons/TSA.svg").pixmap(44, 44));
    header->addWidget(logo);
    auto* titles = new QVBoxLayout();
    titles->setSpacing(2);
    auto* title = new QLabel(tr("Projets récents"), this);
    title->setStyleSheet("font-size: 22px; font-weight: 600;");
    m_subtitle = new QLabel(tr("Reprenez un modèle là où vous l'avez laissé."), this);
    m_subtitle->setStyleSheet("color: palette(mid);");
    titles->addWidget(title);
    titles->addWidget(m_subtitle);
    header->addLayout(titles);
    header->addStretch();

    m_btnContinue = new QPushButton(tr("Revenir au modèle"), this);
    m_btnContinue->setToolTip(tr("Retourner au viewport du projet en cours (Échap)"));
    auto* btnNew = new QPushButton(QIcon(":/icons/file_new.svg"), tr("Nouveau projet"), this);
    auto* btnOpen = new QPushButton(QIcon(":/icons/file_open.svg"), tr("Ouvrir…"), this);
    btnNew->setDefault(true);
    for (auto* b : { m_btnContinue, btnNew, btnOpen }) { b->setMinimumHeight(32); header->addWidget(b); }
    root->addLayout(header);
    connect(m_btnContinue, &QPushButton::clicked, this, &StartPage::continueRequested);
    connect(btnNew, &QPushButton::clicked, this, &StartPage::newProjectRequested);
    connect(btnOpen, &QPushButton::clicked, this, &StartPage::openDialogRequested);

    auto* sep = new QFrame(this);
    sep->setFrameShape(QFrame::HLine);
    sep->setStyleSheet("color: palette(mid);");
    root->addWidget(sep);

    // Grille de cartes, responsive
    m_scroll = new QScrollArea(this);
    m_scroll->setWidgetResizable(true);
    m_scroll->setFrameShape(QFrame::NoFrame);
    m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_cardsHost = new QWidget(m_scroll);
    m_grid = new QGridLayout(m_cardsHost);
    m_grid->setContentsMargins(4, 4, 4, 4);
    m_grid->setHorizontalSpacing(22);
    m_grid->setVerticalSpacing(22);
    m_grid->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    m_scroll->setWidget(m_cardsHost);
    root->addWidget(m_scroll, 1);

    m_empty = new QLabel(tr("Aucun projet récent.\nCréez un nouveau projet ou ouvrez un fichier .tsa : son aperçu apparaîtra ici."), this);
    m_empty->setAlignment(Qt::AlignCenter);
    m_empty->setStyleSheet("color: palette(mid); font-size: 13px;");
    root->addWidget(m_empty, 1);
    m_empty->hide();
}

void StartPage::refresh(const QString& currentProjectPath, bool hasOpenModel)
{
    ++m_generation;
    for (auto& c : m_cards)
        if (c) c->deleteLater();
    m_cards.clear();
    m_columns = 0;

    m_btnContinue->setVisible(hasOpenModel);
    const auto recent = TSA::Project::RecentProjects().list(false);
    const QString current = currentProjectPath.isEmpty() ? QString() : TSA::Project::RecentProjects::normalize(currentProjectPath);
    for (const auto& r : recent)
    {
        const QFileInfo fi(r.path);
        const QDateTime modified = fi.lastModified();
        const QString subtitle = tr("Modifié %1  ·  %2")
                                     .arg(TSA::Project::RecentProjects::relativeTime(modified),
                                          fi.dir().dirName());
        auto* card = new ProjectCard(r.path, fi.fileName(), subtitle, fi.suffix().toUpper(), m_cardsHost);
        card->setCurrent(!current.isEmpty() && r.path.compare(current, Qt::CaseInsensitive) == 0);
        connect(card, &ProjectCard::openRequested, this, &StartPage::openRequested);
        connect(card, &ProjectCard::contextMenuRequested, this, &StartPage::cardContextMenuRequested);
        m_cards.push_back(card);
    }
    m_subtitle->setText(recent.isEmpty() ? tr("Commencez un nouveau modèle structural.")
                                         : tr("%1 projet(s) — reprenez un modèle là où vous l'avez laissé.").arg(recent.size()));
    m_empty->setVisible(recent.isEmpty());
    m_scroll->setVisible(!recent.isEmpty());
    relayoutCards();
    loadPreviewsAsync();
}

void StartPage::updatePreview(const QString& path, const QImage& image)
{
    const QString key = TSA::Project::RecentProjects::normalize(path);
    for (auto& c : m_cards)
        if (c && c->path().compare(key, Qt::CaseInsensitive) == 0) c->setPreview(image);
}

void StartPage::loadPreviewsAsync()
{
    QStringList paths;
    for (auto& c : m_cards)
        if (c) paths << c->path();
    if (paths.isEmpty()) return;

    // Lecture disque et décodage hors du thread UI ; chaque aperçu est publié dès qu'il est prêt.
    const quint64 generation = m_generation;
    QPointer<StartPage> self(this);
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

void StartPage::relayoutCards()
{
    const int available = m_scroll->viewport()->width() - 8;
    const int columns = std::max(1, (available + m_grid->horizontalSpacing()) / (ProjectCard::kImageWidth + 2 + m_grid->horizontalSpacing()));
    if (columns == m_columns && m_grid->count() == m_cards.size()) return;
    m_columns = columns;
    while (m_grid->count() > 0) m_grid->takeAt(0);
    int i = 0;
    for (auto& c : m_cards)
    {
        if (!c) continue;
        m_grid->addWidget(c, i / columns, i % columns);
        ++i;
    }
}

void StartPage::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    // La zone de défilement n'a sa taille définitive qu'après la mise en page des enfants.
    QTimer::singleShot(0, this, &StartPage::relayoutCards);
}

void StartPage::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    QTimer::singleShot(0, this, &StartPage::relayoutCards);
}

} // namespace TSA::UI
