// API Win32 incluse en premier : des en-têtes tiers (OCCT via MainWindow.h) incluent <windows.h>
// avec un sous-ensemble réduit (sans winuser.h) qui le rendrait inutilisable ensuite.
#ifdef _WIN32
#include <windows.h>
#include <windowsx.h>
#include <dwmapi.h>
#endif

#include "AppShell.h"

#include "../../App/ProductInfo.h"

#include "TitleBar.h"
#include "../MainWindow.h"
#include "../Home/NewProjectDialog.h"
#include "../Home/StartCenter.h"
#include "../Theme/ThemeManager.h"
#include "../../Project/ProjectManager.h"
#include "../../Project/RecentProjects.h"

#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QMenu>
#include <QMimeData>
#include <QAbstractNativeEventFilter>
#include <QPointer>
#include <QScreen>
#include <QSettings>
#include <QShortcut>
#include <QStackedWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace TSA::UI
{

namespace
{
const char* kGeometryKey = "Shell/geometry";
constexpr QSize kWorkspaceMinimumSize(960, 560);

QString firstTsaUrl(const QMimeData* mime)
{
    if (!mime || !mime->hasUrls()) return QString();
    for (const QUrl& url : mime->urls())
        if (TSA::Product::isOpenableProjectFile(url.toLocalFile())) return url.toLocalFile();
    return QString();
}

QIcon themeIcon(bool dark)
{
    return QIcon(dark ? ":/icons/common/theme_dark.svg" : ":/icons/common/theme_light.svg");
}
} // namespace

AppShell::AppShell(QWidget* parent)
    : QWidget(parent)
{
    setObjectName("TSAAppShell");
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint | Qt::WindowSystemMenuHint | Qt::WindowMinMaxButtonsHint);
    setWindowIcon(QIcon(QString::fromLatin1(TSA::Product::kIconIco)));
    setAutoFillBackground(true);
    setAcceptDrops(true);

    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->setSpacing(0);

    m_titleBar = new TitleBar(this);
    m_layout->addWidget(m_titleBar);

    m_stack = new QStackedWidget(this);
    m_layout->addWidget(m_stack, 1);

    // Seul le Start Center est construit au lancement : le workspace l'est à l'ouverture d'un projet.
    m_startCenter = new StartCenter(m_stack);
    m_stack->addWidget(m_startCenter);
    connect(m_startCenter, &StartCenter::newProjectRequested, this, &AppShell::createNewProject);
    connect(m_startCenter, &StartCenter::openDialogRequested, this, &AppShell::openProject);
    connect(m_startCenter, &StartCenter::openRequested, this, &AppShell::openProjectFile);

    createActions();
    installResizeBorderFilter();
    restoreShellGeometry();
    showStartCenter();
}

AppShell::~AppShell()
{
    if (m_resizeFilter) qApp->removeNativeEventFilter(m_resizeFilter.get());
}

void AppShell::createActions()
{
    m_actNew = new QAction(QIcon(":/icons/file_new.svg"), tr("Nouveau projet…"), this);
    m_actOpen = new QAction(QIcon(":/icons/file_open.svg"), tr("Ouvrir…"), this);
    m_actSave = new QAction(QIcon(":/icons/file_save.svg"), tr("Enregistrer"), this);
    m_actSaveAs = new QAction(QIcon(":/icons/file/file_save_as.svg"), tr("Enregistrer sous…"), this);
    m_actUndo = new QAction(QIcon(":/icons/undo.svg"), tr("Annuler"), this);
    m_actRedo = new QAction(QIcon(":/icons/redo.svg"), tr("Rétablir"), this);
    m_actCloseProject = new QAction(QIcon(":/icons/file/file_close.svg"), tr("Fermer le projet"), this);
    m_actTheme = new QAction(themeIcon(ThemeManager::instance().isDarkMode()), tr("Thème clair / sombre"), this);
    m_actExit = new QAction(QIcon(":/icons/file_exit.svg"), tr("Quitter %1").arg(TSA::Product::name()), this);

    m_actNew->setToolTip(tr("Nouveau projet (Ctrl+N)"));
    m_actOpen->setToolTip(tr("Ouvrir un projet (Ctrl+O)"));
    m_actCloseProject->setToolTip(tr("Fermer le projet et revenir au Start Center"));
    m_actTheme->setToolTip(tr("Basculer entre le thème clair et le thème sombre"));

    connect(m_actNew, &QAction::triggered, this, &AppShell::createNewProject);
    connect(m_actOpen, &QAction::triggered, this, &AppShell::openProject);
    connect(m_actCloseProject, &QAction::triggered, this, &AppShell::closeProject);
    connect(m_actTheme, &QAction::triggered, this, [] { ThemeManager::instance().toggleTheme(); });
    connect(m_actExit, &QAction::triggered, this, &QWidget::close);
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, [this](bool dark) { m_actTheme->setIcon(themeIcon(dark)); });

    // Raccourcis du Start Center (actifs dans ce mode seulement : dans le workspace, ce sont ceux
    // des actions du workspace, sinon Qt les jugerait ambigus).
    for (auto [key, action] : { std::pair{ QKeySequence(QKeySequence::New), m_actNew }, std::pair{ QKeySequence(QKeySequence::Open), m_actOpen } })
    {
        auto* sc = new QShortcut(key, this);
        connect(sc, &QShortcut::activated, action, &QAction::trigger);
        m_startCenterShortcuts << sc;
    }

    m_titleBar->addQuickAccess(m_actNew);
    m_titleBar->addQuickAccess(m_actOpen);
    m_titleBar->addQuickAccess(m_actSave);
    m_titleBar->addQuickAccessSeparator();
    m_titleBar->addQuickAccess(m_actUndo);
    m_titleBar->addQuickAccess(m_actRedo);
    m_titleBar->addTrailing(m_actCloseProject);
    m_titleBar->addTrailing(m_actTheme);

    m_recentMenu = new QMenu(tr("Projets récents"), this);
    m_recentMenu->setIcon(QIcon(":/icons/file_tsa.svg"));
    connect(m_titleBar->applicationMenu(), &QMenu::aboutToShow, this, &AppShell::rebuildApplicationMenu);

    // Commandes qui n'ont de sens qu'avec un projet ouvert : actives seulement en mode Workspace.
    for (QAction* proxy : { m_actSave, m_actSaveAs, m_actUndo, m_actRedo })
        m_proxyTargets.insert(proxy, nullptr);
    syncProxyActions();
}

void AppShell::bindToWorkspace(QAction* proxy, QAction* target)
{
    if (!target) return;
    m_proxyTargets[proxy] = target;
    connect(proxy, &QAction::triggered, target, &QAction::trigger);
    connect(target, &QAction::changed, this, &AppShell::syncProxyActions);
}

void AppShell::syncProxyActions()
{
    for (auto it = m_proxyTargets.cbegin(); it != m_proxyTargets.cend(); ++it)
    {
        QAction* proxy = it.key();
        QAction* target = it.value();
        proxy->setEnabled(isProjectOpen() && target && target->isEnabled());
        QString tip = proxy->text().remove('&').remove(QStringLiteral("…"));
        if (target && !target->shortcut().isEmpty())
            tip += QStringLiteral(" (%1)").arg(target->shortcut().toString(QKeySequence::NativeText));
        proxy->setToolTip(tip);
    }
    m_actCloseProject->setEnabled(isProjectOpen());
    m_actCloseProject->setVisible(isProjectOpen());
    for (QShortcut* sc : m_startCenterShortcuts) sc->setEnabled(!isProjectOpen());
}

MainWindow* AppShell::ensureWorkspace()
{
    if (m_workspace) return m_workspace;

    QApplication::setOverrideCursor(Qt::WaitCursor);
    m_workspace = new MainWindow(m_stack);
    // La somme des minima des panneaux (barre d'état ≈ 1 680 px, docks + barre du viewport ≈ 1 520 px)
    // pousserait la fenêtre hors de l'écran : le workspace se contente d'un minimum raisonnable.
    m_workspace->setMinimumSize(kWorkspaceMinimumSize);
    m_stack->addWidget(m_workspace);

    connect(m_workspace, &MainWindow::newProjectRequested, this, &AppShell::createNewProject);
    connect(m_workspace, &MainWindow::openProjectRequested, this, &AppShell::openProject);
    connect(m_workspace, &MainWindow::closeProjectRequested, this, &AppShell::closeProject);
    connect(m_workspace, &MainWindow::exitRequested, this, &QWidget::close);
    connect(m_workspace, &MainWindow::fullScreenRequested, this, [this](bool on) {
        if (on && !isFullScreen())
        {
            m_wasMaximizedBeforeFullScreen = isMaximized();
            showFullScreen();
        }
        else if (!on && isFullScreen())
        {
            m_wasMaximizedBeforeFullScreen ? showMaximized() : showNormal();
        }
    });
    connect(m_workspace, &MainWindow::previewCaptured, m_startCenter, &StartCenter::updatePreview);
    connect(m_workspace, &QWidget::windowTitleChanged, this, &AppShell::updateTitle);

    bindToWorkspace(m_actSave, m_workspace->actionSave());
    bindToWorkspace(m_actSaveAs, m_workspace->actionSaveAs());
    bindToWorkspace(m_actUndo, m_workspace->actionUndo());
    bindToWorkspace(m_actRedo, m_workspace->actionRedo());
    QApplication::restoreOverrideCursor();
    return m_workspace;
}

void AppShell::setMode(ApplicationMode mode)
{
    m_mode = mode;
    if (mode == ApplicationMode::ProjectWorkspace)
    {
        // Le viewport doit être visible (OCCT initialisé) avant tout chargement ou capture.
        m_stack->setCurrentWidget(ensureWorkspace());
    }
    else
    {
        m_startCenter->refresh();
        m_stack->setCurrentWidget(m_startCenter);
    }
    // La page cachée n'impose pas sa taille minimale (QStackedLayout prend le maximum des pages).
    if (m_workspace)
        m_workspace->setMinimumSize(mode == ApplicationMode::ProjectWorkspace ? kWorkspaceMinimumSize : QSize(1, 1));
    syncProxyActions();
    updateTitle();
}

void AppShell::showStartCenter()
{
    setMode(ApplicationMode::StartCenter);
}

void AppShell::showProjectWorkspace()
{
    setMode(ApplicationMode::ProjectWorkspace);
}

void AppShell::updateTitle()
{
    const QString title = isProjectOpen() && m_workspace && !m_workspace->windowTitle().isEmpty()
        ? QString(m_workspace->windowTitle()).replace(TSA::Product::name() + QStringLiteral(" - "), TSA::Product::name() + QStringLiteral(" — "))
        : tr("%1 — Start Center").arg(TSA::Product::name());
    setWindowTitle(title);
    m_titleBar->setTitle(title);
}

void AppShell::createNewProject()
{
    if (isProjectOpen() && (!m_workspace->confirmNoRunningAnalysis(tr("créer un projet")) || !m_workspace->maybeSave())) return;

    NewProjectDialog dialog(this);
    if (dialog.exec() != QDialog::Accepted) return;

    MainWindow* workspace = ensureWorkspace();
    showProjectWorkspace();
    if (!workspace->createProject(dialog.settings()))
    {
        workspace->closeProject();
        showStartCenter();
    }
}

void AppShell::openProject()
{
    if (isProjectOpen() && (!m_workspace->confirmNoRunningAnalysis(tr("ouvrir un autre projet")) || !m_workspace->maybeSave())) return;

    QString initialDir;
    const auto recent = TSA::Project::RecentProjects().list(false);
    if (!recent.isEmpty()) initialDir = QFileInfo(recent.first().path).absolutePath();
    const QString path = QFileDialog::getOpenFileName(this, tr("Ouvrir un projet %1").arg(TSA::Product::name()), initialDir,
                                                      TSA::Product::openFileFilter());
    if (path.isEmpty()) return;
    loadIntoWorkspace(path);
}

bool AppShell::openProjectFile(const QString& path)
{
    if (path.isEmpty()) return false;
    if (isProjectOpen())
    {
        const auto* pm = m_workspace->projectManager();
        if (pm && pm->hasFilePath()
            && TSA::Project::RecentProjects::normalize(pm->currentFilePath()).compare(TSA::Project::RecentProjects::normalize(path), Qt::CaseInsensitive) == 0)
            return true; // déjà ouvert
        if (!m_workspace->confirmNoRunningAnalysis(tr("ouvrir un autre projet")) || !m_workspace->maybeSave()) return false;
    }
    return loadIntoWorkspace(path);
}

bool AppShell::loadIntoWorkspace(const QString& path)
{
    const bool wasOpen = isProjectOpen();
    MainWindow* workspace = ensureWorkspace();
    showProjectWorkspace();
    if (workspace->loadFile(path)) return true;
    if (!wasOpen)
    {
        workspace->closeProject();
        showStartCenter();
    }
    return false;
}

bool AppShell::closeProject()
{
    if (!isProjectOpen()) return true;
    if (!m_workspace->closeProject()) return false;
    showStartCenter();
    return true;
}

void AppShell::rebuildApplicationMenu()
{
    QMenu* menu = m_titleBar->applicationMenu();
    menu->clear();
    menu->addAction(m_actNew);
    menu->addAction(m_actOpen);

    m_recentMenu->clear();
    const auto recent = TSA::Project::RecentProjects().list(false);
    for (int i = 0; i < std::min<int>(recent.size(), 10); ++i)
    {
        const QString path = recent[i].path;
        QAction* a = m_recentMenu->addAction(QStringLiteral("%1  %2").arg(i + 1).arg(QFileInfo(path).fileName()));
        a->setToolTip(QDir::toNativeSeparators(path));
        connect(a, &QAction::triggered, this, [this, path] { openProjectFile(path); });
    }
    if (recent.isEmpty()) m_recentMenu->addAction(tr("Aucun projet récent"))->setEnabled(false);
    menu->addMenu(m_recentMenu);

    menu->addSeparator();
    menu->addAction(m_actSave);
    menu->addAction(m_actSaveAs);
    menu->addAction(m_actCloseProject);

    if (isProjectOpen() && m_workspace)
    {
        menu->addSeparator();
        for (QMenu* sub : m_workspace->applicationMenus()) menu->addMenu(sub);
    }
    menu->addSeparator();
    menu->addAction(m_actExit);
}

void AppShell::closeEvent(QCloseEvent* event)
{
    if (m_workspace && !m_workspace->prepareToClose())
    {
        event->ignore();
        return;
    }
    saveShellGeometry();
    event->accept();
}

void AppShell::restoreShellGeometry()
{
    const QByteArray geometry = QSettings().value(kGeometryKey).toByteArray();
    if (!geometry.isEmpty() && restoreGeometry(geometry)) return;

    const QRect avail = QGuiApplication::primaryScreen() ? QGuiApplication::primaryScreen()->availableGeometry() : QRect(0, 0, 1600, 1000);
    resize(std::min(1440, avail.width() - 40), std::min(880, avail.height() - 40));
    move(avail.center() - rect().center());
}

void AppShell::saveShellGeometry() const
{
    QSettings().setValue(kGeometryKey, saveGeometry());
}

void AppShell::dragEnterEvent(QDragEnterEvent* event)
{
    if (!firstTsaUrl(event->mimeData()).isEmpty())
    {
        event->acceptProposedAction();
        return;
    }
    QWidget::dragEnterEvent(event);
}

void AppShell::dropEvent(QDropEvent* event)
{
    const QString path = firstTsaUrl(event->mimeData());
    if (path.isEmpty())
    {
        QWidget::dropEvent(event);
        return;
    }
    event->acceptProposedAction();
    openProjectFile(path);
}

void AppShell::changeEvent(QEvent* event)
{
    QWidget::changeEvent(event);
    if (event->type() == QEvent::WindowStateChange)
    {
        m_titleBar->updateWindowState(windowState());
        m_titleBar->setVisible(!isFullScreen());
        if (m_workspace) m_workspace->setFullScreenState(isFullScreen());
        if (!isFullScreen()) applyNativeFrame(); // Qt rétablit son propre style en sortie de plein écran
        updateMaximizedMargins();
    }
}

void AppShell::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    if (!m_nativeFrameApplied)
    {
        applyNativeFrame();
        m_nativeFrameApplied = true;
    }
    m_titleBar->updateWindowState(windowState());
    updateMaximizedMargins();
}

void AppShell::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    updateMaximizedMargins();
}

#ifdef _WIN32

namespace
{
int frameThickness(HWND hwnd)
{
    const UINT dpi = GetDpiForWindow(hwnd);
    return GetSystemMetricsForDpi(SM_CXSIZEFRAME, dpi) + GetSystemMetricsForDpi(SM_CXPADDEDBORDER, dpi);
}

/// Code de redimensionnement (HTLEFT, HTTOPRIGHT…) si le point est sur la bordure de la fenêtre, sinon 0.
LRESULT resizeHitTest(HWND root, POINT pt)
{
    if (IsZoomed(root)) return 0;
    RECT wr;
    GetWindowRect(root, &wr);
    const int b = frameThickness(root);
    const bool left = pt.x < wr.left + b, right = pt.x >= wr.right - b;
    const bool top = pt.y < wr.top + b, bottom = pt.y >= wr.bottom - b;
    if (top && left) return HTTOPLEFT;
    if (top && right) return HTTOPRIGHT;
    if (bottom && left) return HTBOTTOMLEFT;
    if (bottom && right) return HTBOTTOMRIGHT;
    if (left) return HTLEFT;
    if (right) return HTRIGHT;
    if (bottom) return HTBOTTOM;
    if (top) return HTTOP;
    return 0;
}

// Le viewport OCCT est une fenêtre native : Qt rend aussi natifs ses ancêtres (workspace, pile de
// pages). Ces fenêtres enfants reçoivent WM_NCHITTEST sur les bords d'AppShell et répondraient
// HTCLIENT : sur la bordure, elles deviennent transparentes et Windows interroge AppShell.
class ResizeBorderFilter : public QAbstractNativeEventFilter
{
public:
    explicit ResizeBorderFilter(QWidget* shell) : m_shell(shell) {}

    bool nativeEventFilter(const QByteArray& /*eventType*/, void* message, qintptr* result) override
    {
        const MSG* msg = static_cast<const MSG*>(message);
        if (msg->message != WM_NCHITTEST || !m_shell || m_shell->isFullScreen()) return false;
        const HWND root = reinterpret_cast<HWND>(m_shell->winId());
        if (msg->hwnd == root || GetAncestor(msg->hwnd, GA_ROOT) != root) return false;
        const POINT pt{ GET_X_LPARAM(msg->lParam), GET_Y_LPARAM(msg->lParam) };
        if (resizeHitTest(root, pt) == 0) return false;
        *result = HTTRANSPARENT;
        return true;
    }

private:
    QPointer<QWidget> m_shell;
};
} // namespace

void AppShell::installResizeBorderFilter()
{
    m_resizeFilter = std::make_unique<ResizeBorderFilter>(this);
    qApp->installNativeEventFilter(m_resizeFilter.get());
}

void AppShell::applyNativeFrame()
{
    // Style de fenêtre standard (bordure redimensionnable, légende, boutons système) : Windows garde
    // l'ancrage, les animations et le menu système ; WM_NCCALCSIZE retire ensuite la barre native.
    HWND hwnd = reinterpret_cast<HWND>(winId());
    const LONG_PTR style = GetWindowLongPtrW(hwnd, GWL_STYLE);
    const LONG_PTR wanted = (style & ~WS_POPUP) | WS_OVERLAPPEDWINDOW;
    if (style != wanted) SetWindowLongPtrW(hwnd, GWL_STYLE, wanted);
    const MARGINS shadow = { 1, 1, 1, 1 }; // ombre DWM conservée autour de la fenêtre sans cadre
    DwmExtendFrameIntoClientArea(hwnd, &shadow);
    SetWindowPos(hwnd, nullptr, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
}

void AppShell::updateMaximizedMargins()
{
    // Agrandie, une fenêtre à bordure redimensionnable déborde de l'écran de l'épaisseur du cadre :
    // le contenu est rentré d'autant pour que la barre de titre et les bords restent visibles.
    // Réduite, la fenêtre est placée par Windows en (-32000, -32000) tout en gardant l'état « agrandie » :
    // les marges calculées vaudraient ~32000 px et imposeraient une taille minimale géante (crash
    // CreateDIBSection). On conserve alors les marges courantes, réappliquées à la restauration.
    HWND hwnd = reinterpret_cast<HWND>(winId());
    if (isMinimized() || IsIconic(hwnd)) return;
    QMargins margins;
    if (isMaximized() && !isFullScreen())
    {
        RECT wr;
        MONITORINFO mi{};
        mi.cbSize = sizeof(mi);
        if (GetWindowRect(hwnd, &wr) && GetMonitorInfoW(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &mi))
        {
            const qreal dpr = devicePixelRatioF();
            // Débordement attendu = épaisseur du cadre (quelques pixels) ; toute valeur aberrante est ignorée.
            constexpr long kMaxFrameOverflowPx = 64;
            auto toLogical = [dpr](long px)
            { return px > 0 && px <= kMaxFrameOverflowPx ? static_cast<int>(std::ceil(px / dpr)) : 0; };
            margins = QMargins(toLogical(mi.rcWork.left - wr.left), toLogical(mi.rcWork.top - wr.top),
                               toLogical(wr.right - mi.rcWork.right), toLogical(wr.bottom - mi.rcWork.bottom));
        }
    }
    if (m_layout->contentsMargins() != margins) m_layout->setContentsMargins(margins);
}

bool AppShell::nativeEvent(const QByteArray& eventType, void* message, qintptr* result)
{
    MSG* msg = static_cast<MSG*>(message);
    switch (msg->message)
    {
    case WM_NCCALCSIZE:
        if (msg->wParam == TRUE)
        {
            *result = 0; // toute la fenêtre est zone cliente : pas de barre de titre ni de bordure système
            return true;
        }
        break;

    case WM_NCHITTEST:
    {
        if (isFullScreen()) break;
        const POINT pt{ GET_X_LPARAM(msg->lParam), GET_Y_LPARAM(msg->lParam) };
        RECT wr;
        GetWindowRect(msg->hwnd, &wr);

        if (const LRESULT edge = resizeHitTest(msg->hwnd, pt))
        {
            *result = edge;
            return true;
        }

        // Pixels écran → coordonnées logiques de la fenêtre (la fenêtre entière est zone cliente).
        const qreal dpr = devicePixelRatioF();
        const QPoint local(static_cast<int>((pt.x - wr.left) / dpr), static_cast<int>((pt.y - wr.top) / dpr));
        const QPoint inTitle = m_titleBar->mapFrom(this, local);
        if (m_titleBar->isVisible() && m_titleBar->isCaptionArea(inTitle))
        {
            *result = HTCAPTION; // déplacement, double-clic, ancrage et menu système natifs
            return true;
        }
        *result = HTCLIENT;
        return true;
    }
    default:
        break;
    }
    return QWidget::nativeEvent(eventType, message, result);
}

#else

void AppShell::installResizeBorderFilter() {}
void AppShell::applyNativeFrame() {}
void AppShell::updateMaximizedMargins() {}

#endif

} // namespace TSA::UI
