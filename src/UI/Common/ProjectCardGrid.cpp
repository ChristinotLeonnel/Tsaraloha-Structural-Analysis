#include "ProjectCardGrid.h"
#include "UI/Theme/ThemedIcon.h"
#include "App/ProductInfo.h"

#include "../../Project/RecentProjects.h"

#include <QDir>
#include <QFileInfo>
#include <QGraphicsDropShadowEffect>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLinearGradient>
#include <QLocale>
#include <QMouseEvent>
#include <QPainter>
#include <QScrollArea>
#include <QStyle>
#include <QSvgRenderer>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>

namespace TSA::UI
{
QPixmap cropToFill(const QImage& img, int w, int h)
{
    // Remplit le cadre 16:9 sans déformer ; les miniatures carrées embarquées sont recadrées au centre.
    const QImage scaled = img.scaled(w, h, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    const int x = (scaled.width() - w) / 2, y = (scaled.height() - h) / 2;
    return QPixmap::fromImage(scaled.copy(x, y, w, h));
}

QString projectTypeText(const TSA::Project::ProjectFileSummary& summary)
{
    if (!summary.valid) return ProjectCard::tr("Projet %1").arg(TSA::Product::name());
    const QString text = ProjectCard::tr("Projet %1 %2.%3").arg(TSA::Product::name()).arg(summary.formatMajor).arg(summary.formatMinor);
    return summary.passwordProtected ? ProjectCard::tr("%1 · protégé").arg(text) : text;
}

// -----------------------------------------------------------------------------
// ProjectCard
// -----------------------------------------------------------------------------

ProjectCard::ProjectCard(const QString& path, const QDateTime& lastOpened, bool removable, QWidget* parent)
    : QFrame(parent)
    , m_path(path)
    , m_lastOpened(lastOpened)
{
    const QFileInfo fi(path);
    m_name = fi.completeBaseName();
    m_lastModified = fi.lastModified();

    setObjectName("ProjectCard");
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::StrongFocus);
    setFixedWidth(kImageWidth + 4);
    setToolTip(tr("%1\nDouble-clic pour ouvrir").arg(QDir::toNativeSeparators(path)));
    setAccessibleName(m_name);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(2, 2, 2, 8);
    layout->setSpacing(3);

    m_image = new QLabel(this);
    m_image->setFixedSize(kImageWidth, kImageHeight);
    m_image->setStyleSheet("border-top-left-radius: 5px; border-top-right-radius: 5px;");
    m_image->setPixmap(placeholder(tr("Aucun aperçu")));
    layout->addWidget(m_image);
    layout->addSpacing(5);

    // Couleurs : feuille de style du Start Center (thème clair ou sombre)
    auto addLine = [&](const char* objectName) {
        auto* label = new QLabel(this);
        label->setObjectName(QString::fromLatin1(objectName));
        layout->addWidget(label);
        return label;
    };
    const char* detail = "ProjectCardDetail";
    m_title = addLine("ProjectCardTitle");
    setElided(m_title, m_name, Qt::ElideRight);
    setElided(addLine(detail), fi.fileName(), Qt::ElideMiddle);
    setElided(addLine(detail), QDir::toNativeSeparators(fi.absolutePath()), Qt::ElideMiddle);
    const QString opened = lastOpened.isValid()
        ? tr("Ouvert %1  ·  %2").arg(TSA::Project::RecentProjects::relativeTime(lastOpened), QLocale().toString(lastOpened, QLocale::ShortFormat))
        : tr("Modifié le %1").arg(QLocale().toString(m_lastModified, QLocale::ShortFormat));
    setElided(addLine(detail), opened, Qt::ElideRight);

    auto* actions = new QHBoxLayout();
    actions->setContentsMargins(0, 4, 8, 0);
    actions->setSpacing(4);
    m_type = addLine(detail);
    layout->removeWidget(m_type);
    setElided(m_type, projectTypeText({}), Qt::ElideRight);
    actions->addWidget(m_type, 1);
    auto* open = new QToolButton(this);
    open->setObjectName("ProjectCardOpen");
    open->setText(tr("Ouvrir"));
    open->setCursor(Qt::PointingHandCursor);
    open->setToolTip(tr("Ouvrir ce projet"));
    connect(open, &QToolButton::clicked, this, [this] { emit openRequested(m_path); });
    actions->addWidget(open);
    if (removable)
    {
        auto* remove = new QToolButton(this);
        remove->setObjectName("ProjectCardRemove");
        remove->setIcon(TSA::UI::themedIcon(QStringLiteral(":/icons/common/remove.svg")));
        remove->setIconSize(QSize(14, 14));
        remove->setAutoRaise(true);
        remove->setCursor(Qt::PointingHandCursor);
        remove->setToolTip(tr("Retirer de la liste (le fichier n'est pas supprimé)"));
        remove->setAccessibleName(tr("Retirer de la liste"));
        connect(remove, &QToolButton::clicked, this, [this] { emit removeRequested(m_path); });
        actions->addWidget(remove);
    }
    layout->addLayout(actions);

    m_shadow = new QGraphicsDropShadowEffect(this);
    m_shadow->setBlurRadius(0);
    m_shadow->setOffset(0, 0);
    m_shadow->setColor(QColor(0, 0, 0, 140));
    setGraphicsEffect(m_shadow);
}

void ProjectCard::setElided(QLabel* label, const QString& text, Qt::TextElideMode mode)
{
    // Largeur du texte : celle de la carte, moins les marges ; la ligne du type partage la sienne avec les boutons
    const int width = kImageWidth - (label == m_type ? 96 : 20);
    label->setText(label->fontMetrics().elidedText(text, mode, width));
}

QPixmap ProjectCard::placeholder(const QString& text)
{
    QPixmap pix(kImageWidth, kImageHeight);
    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);
    QLinearGradient g(0, 0, 0, kImageHeight);
    g.setColorAt(0, QColor(0x1C, 0x23, 0x2B));
    g.setColorAt(1, QColor(0x12, 0x17, 0x1D));
    p.fillRect(pix.rect(), g);
    QSvgRenderer logo(QString::fromLatin1(TSA::Product::kGlyphSvg));
    if (logo.isValid()) logo.render(&p, QRectF(kImageWidth / 2.0 - 26, kImageHeight / 2.0 - 40, 52, 52));
    p.setPen(QColor(0x8B, 0x94, 0x9E));
    p.drawText(QRect(0, kImageHeight / 2 + 18, kImageWidth, 24), Qt::AlignCenter, text);
    return pix;
}

void ProjectCard::setPreview(const QImage& image)
{
    if (image.isNull()) return;
    m_image->setPixmap(cropToFill(image, kImageWidth, kImageHeight));
    m_hasPreview = true;
}

void ProjectCard::setSummary(const TSA::Project::ProjectFileSummary& summary)
{
    if (!summary.projectName.trimmed().isEmpty() && summary.projectName != TSA::Product::kDefaultProjectName)
    {
        m_name = summary.projectName.trimmed();
        setElided(m_title, m_name, Qt::ElideRight);
        setAccessibleName(m_name);
    }
    setElided(m_type, projectTypeText(summary), Qt::ElideRight);
    if (summary.passwordProtected && !m_hasPreview) m_image->setPixmap(placeholder(tr("Projet protégé par mot de passe")));
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
// ProjectCardGrid
// -----------------------------------------------------------------------------

ProjectCardGrid::ProjectCardGrid(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    m_scroll = new QScrollArea(this);
    m_scroll->setWidgetResizable(true);
    m_scroll->setFrameShape(QFrame::NoFrame);
    m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_host = new QWidget(m_scroll);
    m_grid = new QGridLayout(m_host);
    m_grid->setContentsMargins(4, 4, 4, 4);
    m_grid->setHorizontalSpacing(20);
    m_grid->setVerticalSpacing(20);
    m_grid->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    m_scroll->setWidget(m_host);
    layout->addWidget(m_scroll);
}

void ProjectCardGrid::setProjects(const QVector<Entry>& entries, bool removable, int limit)
{
    for (auto& c : m_cards)
        if (c) c->deleteLater();
    m_cards.clear();
    m_visible.clear();
    m_selected = nullptr;
    m_limit = limit;
    for (const Entry& e : entries)
    {
        auto* card = new ProjectCard(e.path, e.lastOpened, removable, m_host);
        connect(card, &ProjectCard::clicked, this, &ProjectCardGrid::select);
        connect(card, &ProjectCard::openRequested, this, &ProjectCardGrid::openRequested);
        connect(card, &ProjectCard::removeRequested, this, &ProjectCardGrid::removeRequested);
        connect(card, &ProjectCard::contextMenuRequested, this, &ProjectCardGrid::contextMenuRequested);
        m_cards.push_back(card);
    }
    applyView();
}

void ProjectCardGrid::setFilter(const QString& filter)
{
    m_filter = filter;
    applyView();
}

void ProjectCardGrid::setSortKey(ProjectSortKey key)
{
    m_sort = key;
    applyView();
}

void ProjectCardGrid::applyView()
{
    QList<QPointer<ProjectCard>> shown;
    for (auto& c : m_cards)
        if (c && c->matches(m_filter)) shown.push_back(c);
    const ProjectSortKey key = m_sort;
    std::stable_sort(shown.begin(), shown.end(), [key](const QPointer<ProjectCard>& a, const QPointer<ProjectCard>& b) {
        if (key == ProjectSortKey::Name) return a->name().localeAwareCompare(b->name()) < 0;
        if (key == ProjectSortKey::Modified) return a->lastModified() > b->lastModified();
        return a->lastOpened() > b->lastOpened();
    });
    if (m_limit > 0 && shown.size() > m_limit) shown = shown.mid(0, m_limit);
    for (auto& c : m_cards)
        if (c) c->setVisible(shown.contains(c));
    m_visible = shown;
    if (m_selected && !shown.contains(m_selected))
    {
        m_selected->setSelected(false);
        m_selected = nullptr;
    }
    m_columns = 0;   // force la remise en page (ordre ou filtre changés)
    relayout();
}

void ProjectCardGrid::relayout()
{
    const int available = m_scroll->viewport()->width() - 8;
    const int spacing = m_grid->horizontalSpacing();
    const int columns = std::max(1, (available + spacing) / (ProjectCard::kImageWidth + 4 + spacing));
    if (columns == m_columns) return;
    m_columns = columns;
    while (m_grid->count() > 0) delete m_grid->takeAt(0);
    int i = 0;
    for (auto& c : m_visible)
    {
        if (!c) continue;
        m_grid->addWidget(c, i / columns, i % columns);
        ++i;
    }
}

void ProjectCardGrid::select(ProjectCard* card)
{
    if (m_selected == card) return;
    if (m_selected) m_selected->setSelected(false);
    m_selected = card;
    if (card) card->setSelected(true);
}

void ProjectCardGrid::updatePreview(const QString& path, const QImage& image)
{
    const QString key = TSA::Project::RecentProjects::normalize(path);
    for (auto& c : m_cards)
        if (c && TSA::Project::RecentProjects::normalize(c->path()).compare(key, Qt::CaseInsensitive) == 0) c->setPreview(image);
}

void ProjectCardGrid::updateSummary(const QString& path, const TSA::Project::ProjectFileSummary& summary)
{
    const QString key = TSA::Project::RecentProjects::normalize(path);
    for (auto& c : m_cards)
        if (c && TSA::Project::RecentProjects::normalize(c->path()).compare(key, Qt::CaseInsensitive) == 0) c->setSummary(summary);
}

void ProjectCardGrid::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    // La zone de défilement n'a sa taille définitive qu'après la mise en page des enfants.
    QTimer::singleShot(0, this, &ProjectCardGrid::relayout);
}

void ProjectCardGrid::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    QTimer::singleShot(0, this, &ProjectCardGrid::relayout);
}

} // namespace TSA::UI
