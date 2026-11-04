#include "StartCenter.h"
#include "NewProjectDialog.h"
#include "../Common/RecentFilesView.h"
#include "UI/Theme/ThemedIcon.h"
#include "App/ProductInfo.h"

#include "../HelpLauncher.h"
#include "../Theme/ThemeManager.h"
#include "../../Core/I18n/LanguageManager.h"
#include "../../Help/HelpTopics.h"
#include "../../Project/ModelPreviewCache.h"
#include "../../Project/ProjectFileSummary.h"
#include "../../Project/RecentProjects.h"

#include <QApplication>
#include <QButtonGroup>
#include <QComboBox>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMenu>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QStackedWidget>
#include <QThread>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>

namespace TSA::UI
{

namespace
{
constexpr int kSideBarWidth = 248;
constexpr int kCompactHeight = 620;    ///< fenêtre basse : le logo cède sa place
constexpr int kHomeCards = 8;          ///< projets récents de la page Accueil ; les autres : Fichiers récents
constexpr int kFolderProjects = 200;   ///< projets affichés pour un dossier (recherche pour les autres)
constexpr auto kProjectsFolderKey = "Home/projectsFolder";

QLabel* pageTitle(const QString& text, QWidget* parent)
{
    auto* label = new QLabel(text, parent);
    label->setObjectName("HomePageTitle");
    return label;
}

QLabel* emptyLabel(QWidget* parent)
{
    auto* label = new QLabel(parent);
    label->setObjectName("HomeEmpty");
    label->setAlignment(Qt::AlignCenter);
    label->setWordWrap(true);
    label->hide();
    return label;
}

QVBoxLayout* pageLayout(QWidget* page)
{
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(32, 26, 28, 18);
    layout->setSpacing(10);
    return layout;
}
} // namespace

StartCenter::StartCenter(QWidget* parent)
    : QWidget(parent)
{
    setObjectName("StartCenter");
    setAutoFillBackground(true);

    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    m_navigation = new QButtonGroup(this);
    m_navigation->setExclusive(true);
    root->addWidget(createSideBar());

    m_pages = new QStackedWidget(this);
    m_pages->addWidget(createHomePage());
    m_pages->addWidget(createProjectsPage());
    m_pages->addWidget(createRecentPage());
    m_pages->addWidget(createResourcesPage());
    root->addWidget(m_pages, 1);

    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, &StartCenter::applyTheme);
    applyTheme(ThemeManager::instance().isDarkMode());
    showPage(Page::Home);
}

QToolButton* StartCenter::addNavigation(QVBoxLayout* layout, const QString& icon, const QString& text, Page page)
{
    auto* button = new QToolButton(m_sideBar);
    button->setObjectName("HomeNavigation");
    button->setIcon(TSA::UI::themedIcon(icon));
    button->setIconSize(QSize(18, 18));
    button->setText(text);
    button->setToolTip(text);
    button->setAccessibleName(text);
    button->setCheckable(true);
    button->setCursor(Qt::PointingHandCursor);
    button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    button->setFixedHeight(36);
    m_navigation->addButton(button, static_cast<int>(page));
    connect(button, &QToolButton::clicked, this, [this, page] { showPage(page); });
    layout->addWidget(button);
    return button;
}

QWidget* StartCenter::createSideBar()
{
    m_sideBar = new QWidget(this);
    m_sideBar->setObjectName("HomeSideBar");
    m_sideBar->setAttribute(Qt::WA_StyledBackground);
    m_sideBar->setFixedWidth(kSideBarWidth);
    auto* layout = new QVBoxLayout(m_sideBar);
    layout->setContentsMargins(14, 20, 14, 14);
    layout->setSpacing(4);

    // Identité
    m_identity = new QWidget(m_sideBar);
    auto* identity = new QHBoxLayout(m_identity);
    identity->setContentsMargins(4, 0, 0, 0);
    identity->setSpacing(10);
    m_logo = new QLabel(m_identity);
    m_logo->setPixmap(productLogoIcon(ThemeManager::instance().isDarkMode()).pixmap(40, 40));
    identity->addWidget(m_logo);
    auto* names = new QVBoxLayout();
    names->setSpacing(0);
    auto* title = new QLabel(TSA::Product::name(), m_identity);
    title->setObjectName("StartCenterTitle");
    auto* subtitle = new QLabel(QString::fromUtf8(TSA::Product::kStartCenterSubtitle), m_identity);
    subtitle->setObjectName("StartCenterSubtitle");
    names->addWidget(title);
    names->addWidget(subtitle);
    identity->addLayout(names, 1);
    layout->addWidget(m_identity);
    layout->addSpacing(18);

    // Actions principales
    m_btnNew = new QPushButton(TSA::UI::themedIcon(QStringLiteral(":/icons/file_new.svg")), tr("Nouveau projet"), m_sideBar);
    m_btnNew->setObjectName("StartCenterPrimary");
    m_btnOpen = new QPushButton(TSA::UI::themedIcon(QStringLiteral(":/icons/file_open.svg")), tr("Ouvrir un projet"), m_sideBar);
    m_btnOpen->setObjectName("StartCenterSecondary");
    m_btnNew->setToolTip(tr("Créer un projet %1 (Ctrl+N)").arg(TSA::Product::name()));
    m_btnOpen->setToolTip(tr("Ouvrir un fichier %1 (Ctrl+O)").arg(TSA::Product::projectExtension()));
    for (auto* b : { m_btnNew, m_btnOpen })
    {
        b->setFixedHeight(38);
        b->setIconSize(QSize(18, 18));
        b->setCursor(Qt::PointingHandCursor);
        layout->addWidget(b);
    }
    connect(m_btnNew, &QPushButton::clicked, this, &StartCenter::newProjectRequested);
    connect(m_btnOpen, &QPushButton::clicked, this, &StartCenter::openDialogRequested);

    auto addAction = [&](const QString& icon, const QString& text, const QString& tip) {
        auto* button = new QToolButton(m_sideBar);
        button->setObjectName("HomeNavigation");
        button->setIcon(TSA::UI::themedIcon(icon));
        button->setIconSize(QSize(18, 18));
        button->setText(text);
        button->setToolTip(tip);
        button->setAccessibleName(text);
        button->setCursor(Qt::PointingHandCursor);
        button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        button->setFixedHeight(36);
        layout->addWidget(button);
        return button;
    };
    m_btnOpenRecent = addAction(QStringLiteral(":/icons/common/recent.svg"), tr("Ouvrir un projet récent"), tr("Choisir dans la liste des projets récents"));
    m_recentMenu = new QMenu(m_btnOpenRecent);
    m_btnOpenRecent->setMenu(m_recentMenu);
    m_btnOpenRecent->setPopupMode(QToolButton::InstantPopup);
    m_btnBrowse = addAction(QStringLiteral(":/icons/common/projects.svg"), tr("Parcourir les projets"), tr("Voir les projets d'un dossier"));
    connect(m_btnBrowse, &QToolButton::clicked, this, [this] { showPage(Page::Projects); });

    // Navigation
    layout->addSpacing(18);
    addNavigation(layout, QStringLiteral(":/icons/common/home.svg"), tr("Accueil"), Page::Home);
    addNavigation(layout, QStringLiteral(":/icons/common/projects.svg"), tr("Projets"), Page::Projects);
    addNavigation(layout, QStringLiteral(":/icons/common/recent.svg"), tr("Fichiers récents"), Page::Recent);
    addNavigation(layout, QStringLiteral(":/icons/common/resources.svg"), tr("Ressources"), Page::Resources);
    layout->addStretch(1);

    // Langue de l'interface : nom de la langue actuelle, dans cette langue (docs/I18N.md)
    m_btnLanguage = new QPushButton(TSA::UI::themedIcon(QStringLiteral(":/icons/common/language.svg")),
                                    TSA::Core::I18n::LanguageManager::nativeName(TSA::Core::I18n::LanguageManager::currentLanguage()),
                                    m_sideBar);
    m_btnLanguage->setObjectName("StartCenterLanguage");
    m_btnLanguage->setFlat(true);
    m_btnLanguage->setCursor(Qt::PointingHandCursor);
    m_btnLanguage->setIconSize(QSize(16, 16));
    m_btnLanguage->setToolTip(tr("Langue de l'interface : English, Français, Malagasy"));
    m_btnLanguage->setAccessibleName(tr("Langue de l'interface"));
    connect(m_btnLanguage, &QPushButton::clicked, this, &StartCenter::languageRequested);
    layout->addWidget(m_btnLanguage);
    auto* version = new QLabel(tr("Version %1").arg(QString::fromLatin1(TSA::Product::kVersion)), m_sideBar);
    version->setObjectName("StartCenterSubtitle");
    version->setAlignment(Qt::AlignCenter);
    layout->addWidget(version);
    return m_sideBar;
}

QWidget* StartCenter::createHomePage()
{
    auto* page = new QWidget(this);
    auto* layout = pageLayout(page);
    layout->addWidget(pageTitle(tr("Accueil"), page));

    // Projets ouverts dans les onglets : l'accueil ne ferme rien
    m_openSection = new QWidget(page);
    auto* open = new QVBoxLayout(m_openSection);
    open->setContentsMargins(0, 0, 0, 8);
    open->setSpacing(6);
    auto* openTitle = new QLabel(tr("Projets ouverts"), m_openSection);
    openTitle->setObjectName("StartCenterSection");
    open->addWidget(openTitle);
    m_openList = new QHBoxLayout();
    m_openList->setSpacing(8);
    m_openList->addStretch();
    open->addLayout(m_openList);
    layout->addWidget(m_openSection);
    m_openSection->hide();

    auto* header = new QHBoxLayout();
    auto* recentTitle = new QLabel(tr("Projets récents"), page);
    recentTitle->setObjectName("StartCenterSection");
    header->addWidget(recentTitle);
    m_homeCount = new QLabel(page);
    m_homeCount->setObjectName("StartCenterSubtitle");
    header->addWidget(m_homeCount);
    header->addStretch();
    m_homeShowAll = new QPushButton(tr("Tout afficher"), page);
    m_homeShowAll->setObjectName("HomeLink");
    m_homeShowAll->setFlat(true);
    m_homeShowAll->setCursor(Qt::PointingHandCursor);
    connect(m_homeShowAll, &QPushButton::clicked, this, [this] { showPage(Page::Recent); });
    header->addWidget(m_homeShowAll);
    layout->addLayout(header);

    m_homeGrid = new ProjectCardGrid(page);
    connect(m_homeGrid, &ProjectCardGrid::openRequested, this, &StartCenter::openRequested);
    connect(m_homeGrid, &ProjectCardGrid::removeRequested, this, &StartCenter::forget);
    connect(m_homeGrid, &ProjectCardGrid::contextMenuRequested, this, &StartCenter::showCardMenu);
    layout->addWidget(m_homeGrid, 1);
    m_homeEmpty = emptyLabel(page);
    m_homeEmpty->setText(tr("Aucun projet récent.\nCréez un nouveau projet ou ouvrez un fichier %1 : son aperçu apparaîtra ici.")
                             .arg(TSA::Product::projectExtension()));
    layout->addWidget(m_homeEmpty, 1);
    return page;
}

QWidget* StartCenter::createProjectsPage()
{
    auto* page = new QWidget(this);
    auto* layout = pageLayout(page);
    layout->addWidget(pageTitle(tr("Projets"), page));

    auto* header = new QHBoxLayout();
    header->setSpacing(10);
    m_folderLabel = new QLabel(page);
    m_folderLabel->setObjectName("StartCenterSubtitle");
    m_folderLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    header->addWidget(m_folderLabel, 1);
    auto* change = new QPushButton(tr("Changer de dossier…"), page);
    change->setCursor(Qt::PointingHandCursor);
    connect(change, &QPushButton::clicked, this, &StartCenter::chooseProjectsFolder);
    header->addWidget(change);
    m_projectsSearch = new QLineEdit(page);
    m_projectsSearch->setPlaceholderText(tr("Rechercher dans ce dossier…"));
    m_projectsSearch->setClearButtonEnabled(true);
    m_projectsSearch->setFixedWidth(260);
    header->addWidget(m_projectsSearch);
    layout->addLayout(header);

    m_projectsGrid = new ProjectCardGrid(page);
    connect(m_projectsGrid, &ProjectCardGrid::openRequested, this, &StartCenter::openRequested);
    connect(m_projectsGrid, &ProjectCardGrid::contextMenuRequested, this, &StartCenter::showCardMenu);
    layout->addWidget(m_projectsGrid, 1);
    m_projectsEmpty = emptyLabel(page);
    layout->addWidget(m_projectsEmpty, 1);
    connect(m_projectsSearch, &QLineEdit::textChanged, this, [this](const QString& text) {
        m_projectsGrid->setFilter(text.trimmed());
        updateProjectsState();
    });

    m_projectsFolder = QSettings().value(QString::fromLatin1(kProjectsFolderKey)).toString();
    if (m_projectsFolder.isEmpty() || !QFileInfo(m_projectsFolder).isDir()) m_projectsFolder = NewProjectDialog::defaultDirectory();
    return page;
}

QWidget* StartCenter::createRecentPage()
{
    auto* page = new QWidget(this);
    auto* layout = pageLayout(page);
    auto* header = new QHBoxLayout();
    header->setSpacing(10);
    header->addWidget(pageTitle(tr("Fichiers récents"), page));
    m_recentCount = new QLabel(page);
    m_recentCount->setObjectName("StartCenterSubtitle");
    header->addWidget(m_recentCount);
    header->addStretch();
    m_recentSearch = new QLineEdit(page);
    m_recentSearch->setPlaceholderText(tr("Rechercher un projet (nom ou dossier)…"));
    m_recentSearch->setClearButtonEnabled(true);
    m_recentSearch->setFixedWidth(280);
    header->addWidget(m_recentSearch);
    m_recentSort = new QComboBox(page);
    m_recentSort->addItem(tr("Dernière ouverture"), static_cast<int>(ProjectSortKey::LastOpened));
    m_recentSort->addItem(tr("Nom"), static_cast<int>(ProjectSortKey::Name));
    m_recentSort->addItem(tr("Date de modification"), static_cast<int>(ProjectSortKey::Modified));
    m_recentSort->setToolTip(tr("Trier les projets récents"));
    header->addWidget(m_recentSort);
    layout->addLayout(header);

    m_recentView = new RecentFilesView(page);
    connect(m_recentView, &RecentFilesView::openRequested, this, &StartCenter::openRequested);
    connect(m_recentView, &RecentFilesView::removeRequested, this, &StartCenter::forget);
    connect(m_recentView, &RecentFilesView::contextMenuRequested, this, &StartCenter::showCardMenu);
    layout->addWidget(m_recentView, 1);
    m_recentEmpty = emptyLabel(page);
    layout->addWidget(m_recentEmpty, 1);

    connect(m_recentSearch, &QLineEdit::textChanged, this, [this](const QString& text) {
        m_recentView->setFilter(text.trimmed());
        updateRecentState();
    });
    connect(m_recentSort, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this] {
        m_recentView->setSortKey(static_cast<ProjectSortKey>(m_recentSort->currentData().toInt()));
    });
    return page;
}

QWidget* StartCenter::createResourcesPage()
{
    auto* page = new QWidget(this);
    auto* layout = pageLayout(page);
    layout->addWidget(pageTitle(tr("Ressources"), page));

    struct Texts { QString title, description, icon; };
    const auto textsOf = [this](const QString& id) -> Texts {
        if (id == QLatin1String("documentation")) return { tr("Documentation %1").arg(TSA::Product::name()), tr("Modélisation, charges, calcul et résultats, page par page"), QStringLiteral("resources") };
        if (id == QLatin1String("tutorials")) return { tr("Tutoriels"), tr("Premiers pas : créer un projet, étape par étape"), QStringLiteral("recent") };
        if (id == QLatin1String("guide")) return { tr("Guide utilisateur"), tr("L'interface et les commandes de %1").arg(TSA::Product::name()), QStringLiteral("help") };
        if (id == QLatin1String("website")) return { tr("Site officiel %1").arg(TSA::Product::name()), tr("Présentation, téléchargements et nouveautés"), QStringLiteral("home") };
        if (id == QLatin1String("github")) return { tr("GitHub %1").arg(TSA::Product::name()), tr("Code source et suivi du développement"), QStringLiteral("projects") };
        return { tr("Documentation technique"), tr("Architecture, formats de fichier et calculs, pour les développeurs"), QStringLiteral("resources") };
    };

    auto* grid = new QGridLayout();
    grid->setHorizontalSpacing(14);
    grid->setVerticalSpacing(14);
    grid->setColumnStretch(0, 1);
    grid->setColumnStretch(1, 1);
    int index = 0;
    for (const auto& resource : TSA::Help::homeResources(QString::fromUtf8(TSA::Product::kDocsBaseUrl), QString::fromUtf8(TSA::Product::kSourceRepositoryUrl)))
    {
        const Texts texts = textsOf(resource.id);
        auto* tile = new QPushButton(page);
        tile->setObjectName("HomeResource");
        tile->setCursor(Qt::PointingHandCursor);
        tile->setText(texts.title + QLatin1Char('\n') + texts.description);
        tile->setIcon(TSA::UI::themedIcon(QStringLiteral(":/icons/common/%1.svg").arg(texts.icon)));
        tile->setIconSize(QSize(22, 22));
        tile->setMinimumHeight(72);
        tile->setAccessibleName(texts.title);
        const QString id = resource.id;
        connect(tile, &QPushButton::clicked, this, [this, id] { openResource(id); });
        grid->addWidget(tile, index / 2, index % 2);
        ++index;
    }
    layout->addLayout(grid);
    layout->addStretch(1);
    return page;
}

void StartCenter::openResource(const QString& id)
{
    const auto resources = TSA::Help::homeResources(QString::fromUtf8(TSA::Product::kDocsBaseUrl), QString::fromUtf8(TSA::Product::kSourceRepositoryUrl));
    for (const auto& resource : resources)
    {
        if (resource.id != id) continue;
        if (!resource.topic.isEmpty()) TSA::UI::openHelpTopic(this, resource.topic);
        else QDesktopServices::openUrl(resource.url);
        return;
    }
}

void StartCenter::applyTheme(bool dark)
{
    if (m_logo) m_logo->setPixmap(productLogoIcon(dark).pixmap(40, 40));
    // Contrastes vérifiés sur les captures : texte secondaire lisible sur les deux thèmes (palette(mid) et le texte indicatif
    // de la palette sont trop proches du fond sombre)
    const QString accent = ThemeManager::instance().primaryColor().name();
    const QString strong = dark ? QStringLiteral("#58A6FF") : QStringLiteral("#0550AE");
    const QString side = dark ? QStringLiteral("#161B22") : QStringLiteral("#EEF1F4");
    const QString line = dark ? QStringLiteral("#3A424C") : QStringLiteral("#D0D7DE");
    const QString soft = dark ? QStringLiteral("#2A313A") : QStringLiteral("#E1E6EB");
    const QString muted = dark ? QStringLiteral("#9DA7B3") : QStringLiteral("#57606A");
    setStyleSheet(QStringLiteral(
        "#HomeSideBar { background: %1; border-right: 1px solid %2; }"
        "#StartCenterTitle { font-size: 22px; font-weight: 700; letter-spacing: 1px; }"
        "#StartCenterSubtitle { font-size: 12px; color: %3; }"
        "#HomePageTitle { font-size: 22px; font-weight: 600; }"
        "#StartCenterSection { font-size: 15px; font-weight: 600; }"
        "#HomeEmpty { font-size: 13px; color: %3; }"
        "QPushButton#StartCenterPrimary { background: %4; color: white; border: none; border-radius: 4px; font-size: 13px; font-weight: 600; }"
        "QPushButton#StartCenterPrimary:hover { background: %5; }"
        "QPushButton#StartCenterSecondary { background: transparent; border: 1px solid %2; border-radius: 4px; font-size: 13px; font-weight: 600; }"
        "QPushButton#StartCenterSecondary:hover { border-color: %5; }"
        "QPushButton#StartCenterLanguage { color: %3; }"
        "QToolButton#HomeNavigation { background: transparent; border: none; border-radius: 4px; padding: 0 10px; font-size: 13px; }"
        "QToolButton#HomeNavigation::menu-indicator { image: none; }"
        "QToolButton#HomeNavigation:hover { background: %6; }"
        "QToolButton#HomeNavigation:disabled { color: %3; }"
        "QToolButton#HomeNavigation:checked { background: %6; font-weight: 600; border-left: 3px solid %4; }"
        "TSA--UI--ProjectCard { background: palette(base); border: 1px solid %2; border-radius: 6px; }"
        "TSA--UI--ProjectCard[hovered=\"true\"] { border: 1px solid %5; }"
        "TSA--UI--ProjectCard[selected=\"true\"] { border: 2px solid %5; }"
        "#ProjectCardTitle { font-weight: 600; font-size: 13px; padding: 0 10px; border: none; }"
        "#ProjectCardDetail { color: %3; font-size: 11px; padding: 0 10px; border: none; }"
        "QToolButton#ProjectCardOpen { border: 1px solid %2; border-radius: 3px; padding: 2px 10px; font-size: 11px; }"
        "QToolButton#ProjectCardOpen:hover { border-color: %5; }"
        "QPushButton#HomeLink { border: none; color: %5; font-size: 12px; }"
        "QPushButton#HomeOpenDocument { border: 1px solid %2; border-radius: 4px; padding: 6px 14px; font-size: 12px; }"
        "QPushButton#HomeOpenDocument:hover { border-color: %5; }"
        "QPushButton#HomeResource { text-align: left; padding: 12px 14px; border: 1px solid %2; border-radius: 6px; font-size: 13px; background: palette(base); }"
        "QPushButton#HomeResource:hover { border-color: %5; }"
        "#RecentFilesTable { background: transparent; }"
        "#RecentFilesTable::item { padding: 2px 6px; }")
        .arg(side, line, muted, accent, strong, soft));
}

void StartCenter::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    m_logo->setVisible(height() >= kCompactHeight);   // fenêtre basse : le logo cède sa place aux commandes
}

void StartCenter::showPage(Page page)
{
    m_pages->setCurrentIndex(static_cast<int>(page));
    if (auto* button = m_navigation->button(static_cast<int>(page))) button->setChecked(true);
    if (page == Page::Projects) refreshProjectsPage();
}

StartCenter::Page StartCenter::currentPage() const
{
    return static_cast<Page>(m_pages->currentIndex());
}

void StartCenter::refresh()
{
    ++m_generation;
    m_recent.clear();
    for (const auto& r : TSA::Project::RecentProjects().list(false)) m_recent.push_back({ r.path, r.lastOpened });

    m_homeGrid->setProjects(m_recent, true, kHomeCards);
    m_recentView->setProjects(m_recent);
    rebuildOpenRecentMenu();
    updateHomeState();
    updateRecentState();
    if (currentPage() == Page::Projects) refreshProjectsPage();

    QStringList paths;
    for (const auto& entry : m_recent) paths << entry.path;
    loadDetailsAsync(paths);
}

void StartCenter::updateHomeState()
{
    const int count = m_recent.size();
    m_homeCount->setText(count ? tr("(%1)").arg(count) : QString());
    m_homeGrid->setVisible(count > 0);
    m_homeEmpty->setVisible(count == 0);
    m_homeShowAll->setVisible(count > 0);
}

void StartCenter::updateRecentState()
{
    const QString filter = m_recentSearch->text().trimmed();
    const int total = m_recentView->count(), shown = m_recentView->visibleCount();
    m_recentCount->setText(!total ? QString() : filter.isEmpty() ? tr("(%1)").arg(total) : tr("(%1 sur %2)").arg(shown).arg(total));
    m_recentEmpty->setText(!total ? tr("Aucun projet récent.\nCréez un nouveau projet ou ouvrez un fichier %1 : son aperçu apparaîtra ici.")
                                        .arg(TSA::Product::projectExtension())
                                  : tr("Aucun projet ne correspond à « %1 ».").arg(filter));
    m_recentView->setVisible(shown > 0);
    m_recentEmpty->setVisible(shown == 0);
    m_recentSearch->setEnabled(total > 0);
    m_recentSort->setEnabled(total > 0);
}

void StartCenter::rebuildOpenRecentMenu()
{
    m_recentMenu->clear();
    for (const auto& entry : m_recent)
    {
        const QString path = entry.path;
        QAction* action = m_recentMenu->addAction(QFileInfo(path).fileName());
        action->setToolTip(QDir::toNativeSeparators(path));
        connect(action, &QAction::triggered, this, [this, path] { emit openRequested(path); });
    }
    m_btnOpenRecent->setEnabled(!m_recent.isEmpty());
}

void StartCenter::refreshProjectsPage()
{
    m_folderLabel->setText(QDir::toNativeSeparators(m_projectsFolder));
    const QString pattern = QStringLiteral("*") + TSA::Product::projectExtension();
    const QFileInfoList files = QDir(m_projectsFolder).entryInfoList({ pattern }, QDir::Files, QDir::Time);
    QVector<ProjectCardGrid::Entry> entries;
    QStringList paths;
    for (const QFileInfo& fi : files)
    {
        if (entries.size() == kFolderProjects) break;
        entries.push_back({ TSA::Project::RecentProjects::normalize(fi.absoluteFilePath()), QDateTime() });
        paths << entries.back().path;
    }
    m_projectsGrid->setSortKey(ProjectSortKey::Modified);
    m_projectsGrid->setProjects(entries, false);
    updateProjectsState();
    loadDetailsAsync(paths);
}

void StartCenter::updateProjectsState()
{
    const QString filter = m_projectsSearch->text().trimmed();
    const bool any = m_projectsGrid->visibleCount() > 0;
    m_projectsGrid->setVisible(any);
    m_projectsEmpty->setVisible(!any);
    m_projectsEmpty->setText(m_projectsGrid->count() == 0
                                 ? tr("Aucun projet %1 dans ce dossier.").arg(TSA::Product::projectExtension())
                                 : tr("Aucun projet ne correspond à « %1 ».").arg(filter));
}

void StartCenter::chooseProjectsFolder()
{
    const QString folder = QFileDialog::getExistingDirectory(this, tr("Dossier de projets"), m_projectsFolder);
    if (folder.isEmpty()) return;
    m_projectsFolder = folder;
    QSettings().setValue(QString::fromLatin1(kProjectsFolderKey), folder);
    refreshProjectsPage();
}

void StartCenter::setOpenDocuments(const QList<OpenDocumentEntry>& documents)
{
    while (m_openList->count() > 1)
    {
        QLayoutItem* item = m_openList->takeAt(0);
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }
    for (int i = 0; i < documents.size(); ++i)
    {
        auto* button = new QPushButton(TSA::UI::themedIcon(QStringLiteral(":/icons/file_open.svg")), documents[i].label, m_openSection);
        button->setObjectName("HomeOpenDocument");
        button->setCursor(Qt::PointingHandCursor);
        button->setToolTip(documents[i].path.isEmpty() ? tr("Revenir à ce projet") : QDir::toNativeSeparators(documents[i].path));
        connect(button, &QPushButton::clicked, this, [this, i] { emit openDocumentRequested(i); });
        m_openList->insertWidget(i, button);
    }
    m_openSection->setVisible(!documents.isEmpty());
}

void StartCenter::updatePreview(const QString& path, const QImage& image)
{
    m_homeGrid->updatePreview(path, image);
    m_projectsGrid->updatePreview(path, image);
    m_recentView->updatePreview(path, image);
}

void StartCenter::loadDetailsAsync(const QStringList& paths)
{
    if (paths.isEmpty()) return;

    // Lecture disque et décodage hors du thread UI ; chaque résultat est publié dès qu'il est prêt.
    const quint64 generation = m_generation;
    QPointer<StartCenter> self(this);
    QThread* worker = QThread::create([self, paths, generation] {
        TSA::Project::ModelPreviewCache cache;
        for (const QString& path : paths)
        {
            TSA::Project::ProjectFileSummary summary;
            QImage img;
            try
            {
                summary = TSA::Project::readProjectFileSummary(path);
                img = cache.preview(path);
            }
            catch (...)   // fichier ou aperçu illisible : ignoré (frontière du thread, BUG-155)
            {
                continue;
            }
            QMetaObject::invokeMethod(qApp, [self, path, summary, img, generation] {
                if (!self || self->m_generation != generation) return;
                self->m_homeGrid->updateSummary(path, summary);
                self->m_projectsGrid->updateSummary(path, summary);
                self->m_recentView->updateSummary(path, summary);
                if (!img.isNull()) self->updatePreview(path, img);
            }, Qt::QueuedConnection);
        }
    });
    connect(worker, &QThread::finished, worker, &QObject::deleteLater);
    worker->start(QThread::LowPriority);
}

void StartCenter::forget(const QString& path)
{
    // Retire uniquement l'entrée de la liste (et son aperçu en cache) : le fichier reste sur disque.
    TSA::Project::RecentProjects().remove(path);
    TSA::Project::ModelPreviewCache().remove(path);
    refresh();
}

void StartCenter::showCardMenu(const QString& path, const QPoint& globalPos)
{
    const QFileInfo fi(path);
    TSA::Project::RecentProjects recent;
    TSA::Project::ModelPreviewCache cache;

    QMenu menu(this);
    QAction* actOpen = menu.addAction(TSA::UI::themedIcon(QStringLiteral(":/icons/file_open.svg")), tr("Ouvrir"));
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
        forget(path);
        return;
    }
    else if (chosen == actRefresh)
    {
        cache.remove(path); // repli sur la miniature embarquée dans le fichier
    }
    else if (chosen == actRename)
    {
        bool ok = false;
        QString name = QInputDialog::getText(this, tr("Renommer le projet"), tr("Nouveau nom :"), QLineEdit::Normal,
                                             fi.completeBaseName(), &ok).trimmed();
        if (!ok || name.isEmpty()) return;
        // Renommer conserve l'extension du fichier (format natif ou format importé).
        const QString suffix = QStringLiteral(".") + fi.suffix();
        if (!name.endsWith(suffix, Qt::CaseInsensitive)) name += suffix;
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
        const QString suffix = QStringLiteral(".") + fi.suffix();
        QString target = fi.absoluteDir().filePath(fi.completeBaseName() + tr(" - copie") + suffix);
        for (int i = 2; QFileInfo::exists(target); ++i)
            target = fi.absoluteDir().filePath(fi.completeBaseName() + tr(" - copie (%1)").arg(i) + suffix);
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
                    .arg(source == TSA::Project::PreviewSource::Cache ? tr("dernier état capturé dans %1").arg(TSA::Product::name())
                         : source == TSA::Project::PreviewSource::Embedded ? tr("miniature enregistrée dans le fichier")
                                                                           : tr("aucune"));
        QMessageBox::information(this, tr("Propriétés du projet"), text);
        return;
    }
    refresh();
}

} // namespace TSA::UI
