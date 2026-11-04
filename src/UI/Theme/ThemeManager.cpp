#include "ThemeManager.h"

#include <QApplication>
#include <QPalette>
#include <QStyle>
#include <QWidget>

#include <algorithm>
#include <utility>

namespace TSA::UI
{

namespace
{

// Table unique nom → jeton : resolve(), tokenNames() et tokenColor() en dérivent (aucune liste parallèle).
using TokenMember = QColor ThemeTokens::*;
const std::vector<std::pair<QString, TokenMember>>& tokenTable()
{
    static const std::vector<std::pair<QString, TokenMember>> table {
        { "canvas", &ThemeTokens::canvas },
        { "panel", &ThemeTokens::panel },
        { "surface", &ThemeTokens::surface },
        { "surfaceAlt", &ThemeTokens::surfaceAlt },
        { "raised", &ThemeTokens::raised },
        { "chrome", &ThemeTokens::chrome },
        { "hover", &ThemeTokens::hover },
        { "pressed", &ThemeTokens::pressed },
        { "selection", &ThemeTokens::selection },
        { "selectionText", &ThemeTokens::selectionText },
        { "border", &ThemeTokens::border },
        { "borderSubtle", &ThemeTokens::borderSubtle },
        { "borderStrong", &ThemeTokens::borderStrong },
        { "focus", &ThemeTokens::focus },
        { "text", &ThemeTokens::text },
        { "textSecondary", &ThemeTokens::textSecondary },
        { "textMuted", &ThemeTokens::textMuted },
        { "textDisabled", &ThemeTokens::textDisabled },
        { "textOnAccent", &ThemeTokens::textOnAccent },
        { "hoverText", &ThemeTokens::hoverText },
        { "accent", &ThemeTokens::accent },
        { "accentHover", &ThemeTokens::accentHover },
        { "accentPressed", &ThemeTokens::accentPressed },
        { "accentBorder", &ThemeTokens::accentBorder },
        { "accentText", &ThemeTokens::accentText },
        { "accentSubtle", &ThemeTokens::accentSubtle },
        { "success", &ThemeTokens::success },
        { "successHover", &ThemeTokens::successHover },
        { "successPressed", &ThemeTokens::successPressed },
        { "successText", &ThemeTokens::successText },
        { "successSubtle", &ThemeTokens::successSubtle },
        { "warning", &ThemeTokens::warning },
        { "warningText", &ThemeTokens::warningText },
        { "warningSubtle", &ThemeTokens::warningSubtle },
        { "danger", &ThemeTokens::danger },
        { "dangerHover", &ThemeTokens::dangerHover },
        { "dangerPressed", &ThemeTokens::dangerPressed },
        { "dangerText", &ThemeTokens::dangerText },
        { "dangerSubtle", &ThemeTokens::dangerSubtle },
        { "console", &ThemeTokens::console },
        { "consoleText", &ThemeTokens::consoleText },
        { "logTime", &ThemeTokens::logTime },
        { "logInfo", &ThemeTokens::logInfo },
        { "logCommand", &ThemeTokens::logCommand },
        { "logSystem", &ThemeTokens::logSystem },
        { "logWarning", &ThemeTokens::logWarning },
        { "logError", &ThemeTokens::logError },
        { "logCritical", &ThemeTokens::logCritical },
        { "logDebug", &ThemeTokens::logDebug },
        { "rulerMajorTick", &ThemeTokens::rulerMajorTick },
        { "rulerMinorTick", &ThemeTokens::rulerMinorTick },
    };
    return table;
}

// Ordre de substitution : noms les plus longs d'abord (« @textMuted » avant « @text »).
const std::vector<std::pair<QString, TokenMember>>& substitutionOrder()
{
    static const std::vector<std::pair<QString, TokenMember>> ordered = [] {
        auto v = tokenTable();
        std::stable_sort(v.begin(), v.end(), [](const auto& a, const auto& b) { return a.first.size() > b.first.size(); });
        return v;
    }();
    return ordered;
}

QColor c(const char* hex) { return QColor(QString::fromLatin1(hex)); }

// Feuille de style de l'application, commune aux deux thèmes. Polices : 'Segoe UI' (Windows), repli système.
// Les tailles sont en pixels logiques : Qt les met à l'échelle du facteur d'affichage (100 % à 200 %).
const char* const kApplicationStyleSheet = R"(
QMainWindow { background-color: @canvas; }
QMainWindow::separator { background-color: @border; width: 4px; height: 4px; border: none; }
QMainWindow::separator:hover { background-color: @focus; }

QMenuBar { background-color: @chrome; color: @textSecondary; font-family: 'Segoe UI', -apple-system, sans-serif; font-size: 11px; border-bottom: 1px solid @border; }
QMenuBar::item { background: transparent; padding: 4px 10px; }
QMenuBar::item:selected { background-color: @hover; color: @hoverText; border-radius: 2px; }
QMenu { background-color: @panel; color: @text; border: 1px solid @border; padding: 3px; font-family: 'Segoe UI', sans-serif; font-size: 11px; }
QMenu::item { padding: 4px 20px 4px 24px; border-radius: 2px; }
QMenu::item:selected { background-color: @selection; color: @selectionText; }
QMenu::item:disabled { color: @textDisabled; background: transparent; }
QMenu::separator { height: 1px; background-color: @border; margin: 4px 6px; }

QToolBar { background-color: @chrome; border: none; margin: 0px; padding: 0px; }
QStatusBar { background-color: @chrome; color: @textMuted; border-top: 1px solid @border; font-family: 'Segoe UI', sans-serif; font-size: 11px; }
QDockWidget { background-color: @panel; color: @text; font-family: 'Segoe UI', sans-serif; font-size: 10px; font-weight: bold; border: 1px solid @border; }
QDockWidget::title { background-color: @chrome; padding: 4px 8px; border-bottom: 1px solid @border; border-top: 1px solid @border; color: @textSecondary; }
QToolTip { background-color: @surface; color: @text; border: 1px solid @borderStrong; font-family: 'Segoe UI', sans-serif; font-size: 11px; padding: 3px 6px; border-radius: 3px; }

QSplitter::handle { background-color: @border; }
QSplitter::handle:horizontal { width: 4px; }
QSplitter::handle:vertical { height: 4px; }
QSplitter::handle:hover { background-color: @focus; }

QTabWidget::pane { border: 1px solid @border; background-color: @panel; }
QTabBar { background-color: @chrome; qproperty-drawBase: 0; }
QTabBar::tab { background-color: @chrome; color: @textMuted; border: 1px solid @border; padding: 4px 10px; font-family: 'Segoe UI', sans-serif; font-size: 11px; font-weight: 600; }
QTabBar::tab:hover { background-color: @hover; color: @text; }
QTabBar::tab:selected { background-color: @raised; color: @accentText; border-bottom: 2px solid @accentText; }
QTabBar::tab:focus { border-color: @focus; }

QFrame[frameShape="4"], QFrame[frameShape="5"] { background-color: @border; border: none; }

QTreeView, QTreeWidget { background-color: @panel; alternate-background-color: @surfaceAlt; color: @text; border: 1px solid @border; font-family: 'Segoe UI', sans-serif; font-size: 11px; }
QListView, QListWidget, QTableView, QTableWidget { background-color: @surface; alternate-background-color: @surfaceAlt; color: @text; border: 1px solid @border; gridline-color: @borderSubtle; font-family: 'Segoe UI', sans-serif; font-size: 11px; }
QTreeView::item:hover, QListView::item:hover, QTableView::item:hover { background-color: @hover; }
QTreeView::item:selected, QListView::item:selected, QTableView::item:selected { background-color: @selection; color: @selectionText; }
QTreeView::item:disabled, QListView::item:disabled, QTableView::item:disabled { color: @textDisabled; }
QTableView QTableCornerButton::section { background-color: @chrome; border: 1px solid @border; }
QHeaderView::section { background-color: @chrome; color: @textMuted; border: 1px solid @border; padding: 3px 6px; font-family: 'Segoe UI', sans-serif; font-size: 10px; font-weight: bold; }

QScrollBar:vertical { background: @chrome; width: 8px; margin: 0px; }
QScrollBar::handle:vertical { background: @border; min-height: 20px; border-radius: 3px; }
QScrollBar::handle:vertical:hover { background: @borderStrong; }
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }
QScrollBar:horizontal { background: @chrome; height: 8px; margin: 0px; }
QScrollBar::handle:horizontal { background: @border; min-width: 20px; border-radius: 3px; }
QScrollBar::handle:horizontal:hover { background: @borderStrong; }
QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0px; }

QGroupBox { color: @text; font-weight: bold; border: 1px solid @border; border-radius: 4px; margin-top: 8px; padding-top: 10px; }
QGroupBox::title { subcontrol-origin: margin; subcontrol-position: top left; left: 8px; padding: 0 4px; color: @accentText; }

QLineEdit, QSpinBox, QDoubleSpinBox { background-color: @surface; color: @text; border: 1px solid @border; border-radius: 3px; padding: 3px 6px; font-family: 'Segoe UI', sans-serif; font-size: 11px; selection-background-color: @accent; selection-color: @textOnAccent; }
QLineEdit:hover, QSpinBox:hover, QDoubleSpinBox:hover { border-color: @borderStrong; }
QLineEdit:focus, QSpinBox:focus, QDoubleSpinBox:focus { border-color: @focus; }
QLineEdit:read-only, QAbstractSpinBox[readOnly="true"] { background-color: @surfaceAlt; color: @textSecondary; }
QLineEdit:disabled, QSpinBox:disabled, QDoubleSpinBox:disabled { background-color: @surfaceAlt; color: @textDisabled; border-color: @borderSubtle; }
QLineEdit[invalid="true"], QAbstractSpinBox[invalid="true"], QComboBox[invalid="true"] { border: 1px solid @danger; background-color: @dangerSubtle; }
QLineEdit[emphasis="true"], QAbstractSpinBox[emphasis="true"] { border: 1px solid @accentBorder; background-color: @accentSubtle; font-weight: 600; }

QComboBox { background-color: @surface; color: @text; border: 1px solid @border; border-radius: 3px; padding: 3px 6px; font-family: 'Segoe UI', sans-serif; font-size: 11px; }
QComboBox:hover { border-color: @focus; }
QComboBox:focus { border-color: @focus; }
QComboBox:disabled { background-color: @surfaceAlt; color: @textDisabled; border-color: @borderSubtle; }
QComboBox::drop-down { border: none; width: 16px; }
QComboBox QAbstractItemView { background-color: @surface; color: @text; border: 1px solid @border; selection-background-color: @selection; selection-color: @selectionText; }

QCheckBox, QRadioButton { color: @text; spacing: 6px; }
QCheckBox:disabled, QRadioButton:disabled { color: @textDisabled; }
QCheckBox[emphasis="true"] { color: @accentText; font-weight: 600; }
QCheckBox[chip="true"] { color: @accentText; font-weight: 700; spacing: 4px; padding: 1px 6px; border: 1px solid @border; border-radius: 3px; background: @raised; }
QCheckBox[chip="true"]:hover { border-color: @focus; background: @hover; }
QCheckBox[chip="true"]:checked { background: @accentSubtle; border-color: @accentBorder; }

QPushButton { background-color: @raised; color: @text; border: 1px solid @border; border-radius: 3px; padding: 4px 10px; font-family: 'Segoe UI', sans-serif; font-size: 11px; }
QPushButton:hover { background-color: @hover; border-color: @focus; }
QPushButton:focus { border-color: @focus; }
QPushButton:pressed { background-color: @pressed; }
QPushButton:checked, QPushButton[active="true"], QPushButton[accentOutline="true"] { background-color: @accentSubtle; border: 1px solid @accentBorder; color: @accentText; font-weight: 600; }
QPushButton[active="false"] { color: @textMuted; }
QPushButton[compact="true"] { font-size: 10px; padding: 2px 8px; }
QPushButton:disabled { background-color: @surfaceAlt; color: @textDisabled; border-color: @borderSubtle; }
QDialog { background-color: @panel; color: @text; }

QPushButton[primary="true"] { background-color: @accent; color: @textOnAccent; border: 1px solid @accentBorder; border-radius: 3px; padding: 5px 14px; font-family: 'Segoe UI', sans-serif; font-size: 11px; font-weight: 600; }
QPushButton[primary="true"]:hover { background-color: @accentHover; border-color: @accentHover; }
QPushButton[primary="true"]:focus { border: 1px solid @focus; }
QPushButton[primary="true"]:pressed { background-color: @accentPressed; }
QPushButton[primary="true"]:disabled { background-color: @surfaceAlt; color: @textDisabled; border-color: @borderSubtle; }
QPushButton[destructive="true"] { background-color: @danger; color: @textOnAccent; border: 1px solid @danger; border-radius: 3px; padding: 5px 14px; font-family: 'Segoe UI', sans-serif; font-size: 11px; font-weight: 600; }
QPushButton[destructive="true"]:hover { background-color: @dangerHover; }
QPushButton[destructive="true"]:pressed { background-color: @dangerPressed; }
QPushButton[destructive="true"]:disabled { background-color: @surfaceAlt; color: @textDisabled; border-color: @borderSubtle; }
QPushButton[success="true"] { background-color: @success; color: @textOnAccent; border: 1px solid @success; border-radius: 3px; padding: 5px 14px; font-family: 'Segoe UI', sans-serif; font-size: 11px; font-weight: 600; }
QPushButton[success="true"]:hover { background-color: @successHover; }
QPushButton[success="true"]:pressed { background-color: @successPressed; }

QLabel[dialogTitle="true"] { font-family: 'Segoe UI', sans-serif; font-size: 13px; font-weight: bold; color: @text; }
QLabel[dialogSubtitle="true"] { font-family: 'Segoe UI', sans-serif; font-size: 10px; color: @textMuted; }
QFrame[dialogHeaderDivider="true"] { background-color: @border; height: 1px; border: none; margin: 4px 0px 8px 0px; }
QLabel[status="info"] { color: @accentText; font-weight: 600; }
QLabel[status="warning"] { color: @warningText; font-weight: 600; }
QLabel[status="error"] { color: @dangerText; font-weight: 600; }
QLabel[status="success"] { color: @successText; font-weight: 600; }
QLabel[hint="true"] { color: @textMuted; font-style: italic; font-weight: normal; }
QLabel[caption="true"] { color: @textMuted; font-size: 10px; font-weight: bold; }
QLabel[badge="accent"] { background-color: @accentSubtle; color: @accentText; border: 1px solid @accentBorder; border-radius: 3px; padding: 1px 8px; font-size: 11px; font-weight: 600; }
QLabel[panelHeader="true"] { background-color: @chrome; color: @text; font-family: 'Segoe UI', sans-serif; font-size: 11px; font-weight: bold; padding: 7px 10px; border-bottom: 2px solid @accentText; }
QLabel[panelEmptyState="true"] { color: @textMuted; font-style: italic; font-size: 12px; }

QWidget[toolStrip="true"] { background-color: @chrome; border-bottom: 1px solid @border; font-family: 'Segoe UI', sans-serif; font-size: 11px; }
QWidget[toolStrip="true"] QLabel, QWidget[toolStrip="true"] QPushButton, QWidget[toolStrip="true"] QComboBox, QWidget[toolStrip="true"] QCheckBox { font-size: 11px; }
QWidget[toolStrip="true"] QLabel { color: @textMuted; font-weight: 600; padding: 0 2px; }
QWidget[toolStrip="true"] QLabel[hint="true"] { font-weight: normal; font-style: italic; }
QWidget[toolStrip="true"] QPushButton { padding: 2px 7px; font-weight: bold; }
QWidget[toolStrip="true"] QComboBox { padding: 1px 6px; font-weight: 600; }
QWidget[cornerPatch="true"] { background-color: @chrome; border-left: 1px solid @border; border-bottom: 1px solid @border; }

QTextEdit[console="true"], QPlainTextEdit[console="true"] { background-color: @console; color: @consoleText; font-family: 'Cascadia Code', 'Consolas', 'Courier New', monospace; font-size: 11px; border: 1px solid @border; border-radius: 3px; }
QLineEdit[console="true"] { background-color: @console; color: @logCommand; font-family: 'Cascadia Code', 'Consolas', monospace; font-size: 11px; padding: 2px 6px; }
QLabel[consolePrompt="true"] { font-family: 'Cascadia Code', 'Consolas', monospace; font-weight: bold; color: @logCommand; }
QComboBox[compact="true"] { font-size: 10px; padding: 2px 4px; }
)";

} // namespace

ThemeManager& ThemeManager::instance()
{
    static ThemeManager mgr;
    return mgr;
}

ThemeManager::ThemeManager()
    : m_isDark(true)
{
}

const ThemeTokens& ThemeManager::darkTokens()
{
    // Thème sombre de référence (valeurs historiques de TSA, contrastes vérifiés : texte ≥ 13:1, atténué ≥ 5:1).
    static const ThemeTokens t = [] {
        ThemeTokens k;
        k.canvas = c("#1E2328");
        k.panel = c("#1E2328");
        k.surface = c("#161B22");
        k.surfaceAlt = c("#212830");
        k.raised = c("#212830");
        k.chrome = c("#161B22");
        k.hover = c("#2D333B");
        k.pressed = c("#373E47");
        k.selection = c("#1F3A5A");
        k.selectionText = c("#79C0FF");
        k.border = c("#30363D");
        k.borderSubtle = c("#282E37");
        k.borderStrong = c("#484F58");
        k.focus = c("#58A6FF");
        k.text = c("#E6EDF3");
        k.textSecondary = c("#C9D1D9");
        k.textMuted = c("#8B949E");
        k.textDisabled = c("#6E7681");
        k.textOnAccent = c("#FFFFFF");
        k.hoverText = c("#FFFFFF");
        k.accent = c("#1F6FEB");
        k.accentHover = c("#388BFD");
        k.accentPressed = c("#1158C7");
        k.accentBorder = c("#388BFD");
        k.accentText = c("#58A6FF");
        k.accentSubtle = c("#1F3A5A");
        k.success = c("#238636");
        k.successHover = c("#2EA043");
        k.successPressed = c("#1A6328");
        k.successText = c("#3FB950");
        k.successSubtle = c("#12261A");
        k.warning = c("#D29922");
        k.warningText = c("#D29922");
        k.warningSubtle = c("#2E2410");
        k.danger = c("#DA3633");
        k.dangerHover = c("#F85149");
        k.dangerPressed = c("#B62324");
        k.dangerText = c("#F85149");
        k.dangerSubtle = c("#3A1D1F");
        k.console = c("#0D1117");
        k.consoleText = c("#C9D1D9");
        k.logTime = c("#768390");
        k.logInfo = c("#ADBAC7");
        k.logCommand = c("#58A6FF");
        k.logSystem = c("#3FB950");
        k.logWarning = c("#D29922");
        k.logError = c("#F85149");
        k.logCritical = c("#FF7B72");
        k.logDebug = c("#BC8CFF");
        k.rulerMajorTick = c("#6E7681");
        k.rulerMinorTick = c("#38414D");
        return k;
    }();
    return t;
}

const ThemeTokens& ThemeManager::lightTokens()
{
    // Thème clair professionnel (pas une inversion du sombre) : gris froids neutres d'une seule teinte, fond de fenêtre
    // plus soutenu que les panneaux pour distinguer les docks, champs blancs, un seul accent bleu. Contrastes WCAG
    // vérifiés sur panneau, champ et barres : texte ≥ 13:1, atténué ≥ 5,3:1, textes d'état ≥ 4,5:1.
    static const ThemeTokens t = [] {
        ThemeTokens k;
        k.canvas = c("#E7EAEE");
        k.panel = c("#F7F8FA");
        k.surface = c("#FFFFFF");
        k.surfaceAlt = c("#F1F3F6");
        k.raised = c("#FFFFFF");
        k.chrome = c("#EEF1F4");
        k.hover = c("#E8EEF6");
        k.pressed = c("#D5E3F5");
        k.selection = c("#D7E6FA");
        k.selectionText = c("#0A3D7C");
        k.border = c("#CDD4DC");
        k.borderSubtle = c("#E1E5EA");
        k.borderStrong = c("#A9B3BE");
        k.focus = c("#0B63CE");
        k.text = c("#1F2328");
        k.textSecondary = c("#3B434C");
        k.textMuted = c("#59636E");
        k.textDisabled = c("#8C959F");
        k.textOnAccent = c("#FFFFFF");
        k.hoverText = c("#0B5CC0");
        k.accent = c("#0B63CE");
        k.accentHover = c("#0A55B0");
        k.accentPressed = c("#08458F");
        k.accentBorder = c("#0B63CE");
        k.accentText = c("#0B5CC0");
        k.accentSubtle = c("#E3EEFC");
        k.success = c("#1A7F37");
        k.successHover = c("#157030");
        k.successPressed = c("#105A26");
        k.successText = c("#18732F");
        k.successSubtle = c("#E3F5E8");
        k.warning = c("#9A6700");
        k.warningText = c("#8A5A00");
        k.warningSubtle = c("#FFF4D6");
        k.danger = c("#C21F2C");
        k.dangerHover = c("#A51A25");
        k.dangerPressed = c("#86141E");
        k.dangerText = c("#C21F2C");
        k.dangerSubtle = c("#FDECEC");
        k.console = c("#FFFFFF");
        k.consoleText = c("#1F2328");
        k.logTime = c("#6E7781");
        k.logInfo = c("#3B434C");
        k.logCommand = c("#0B5CC0");
        k.logSystem = c("#18732F");
        k.logWarning = c("#8A5A00");
        k.logError = c("#C21F2C");
        k.logCritical = c("#A0111F");
        k.logDebug = c("#7A3EC8");
        k.rulerMajorTick = c("#6E7781");
        k.rulerMinorTick = c("#AFB8C1");
        return k;
    }();
    return t;
}

const ThemeTokens& ThemeManager::tokens() const
{
    return m_isDark ? darkTokens() : lightTokens();
}

std::vector<QString> ThemeManager::tokenNames()
{
    std::vector<QString> names;
    for (const auto& [name, member] : tokenTable()) names.push_back(name);
    return names;
}

QColor ThemeManager::tokenColor(const ThemeTokens& tokens, const QString& name)
{
    for (const auto& [n, member] : tokenTable())
        if (n == name) return tokens.*member;
    return {};
}

QString ThemeManager::resolve(const QString& styleSheetTemplate, const ThemeTokens& tokens)
{
    QString out = styleSheetTemplate;
    for (const auto& [name, member] : substitutionOrder())
        out.replace(QLatin1Char('@') + name, (tokens.*member).name(QColor::HexRgb));
    return out;
}

QString ThemeManager::resolve(const QString& styleSheetTemplate) const
{
    return resolve(styleSheetTemplate, tokens());
}

QString ThemeManager::applicationStyleSheet() const
{
    return resolve(QString::fromUtf8(kApplicationStyleSheet));
}

QColor ThemeManager::logLevelColor(const QString& level) const
{
    const ThemeTokens& t = tokens();
    if (level == QLatin1String("CMD")) return t.logCommand;
    if (level == QLatin1String("ERR") || level == QLatin1String("ERROR")) return t.logError;
    if (level == QLatin1String("WARN")) return t.logWarning;
    if (level == QLatin1String("SYS")) return t.logSystem;
    if (level == QLatin1String("CRIT")) return t.logCritical;
    if (level == QLatin1String("DEBUG")) return t.logDebug;
    return t.logInfo;
}

void ThemeManager::setDarkMode(bool dark, bool force)
{
    if (m_isDark != dark || force)
    {
        m_isDark = dark;
        applyApplicationTheme();
        emit themeChanged(m_isDark);
    }
}

void ThemeManager::toggleTheme()
{
    setDarkMode(!m_isDark);
}

void ThemeManager::applyApplicationTheme()
{
    if (!qApp) return;

    // La palette sert aux widgets non couverts par la feuille de style (dessins personnalisés, styles natifs).
    const ThemeTokens& t = tokens();
    QPalette palette;
    palette.setColor(QPalette::Window, t.panel);
    palette.setColor(QPalette::WindowText, t.text);
    palette.setColor(QPalette::Base, t.surface);
    palette.setColor(QPalette::AlternateBase, t.surfaceAlt);
    palette.setColor(QPalette::ToolTipBase, t.surface);
    palette.setColor(QPalette::ToolTipText, t.text);
    palette.setColor(QPalette::PlaceholderText, t.textMuted);
    palette.setColor(QPalette::Text, t.text);
    palette.setColor(QPalette::Button, t.raised);
    palette.setColor(QPalette::ButtonText, t.text);
    palette.setColor(QPalette::BrightText, t.dangerText);
    palette.setColor(QPalette::Link, t.accentText);
    palette.setColor(QPalette::Highlight, t.accent);
    palette.setColor(QPalette::HighlightedText, t.textOnAccent);
    palette.setColor(QPalette::Mid, t.border);
    palette.setColor(QPalette::Dark, t.borderStrong);
    palette.setColor(QPalette::Disabled, QPalette::Text, t.textDisabled);
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, t.textDisabled);
    palette.setColor(QPalette::Disabled, QPalette::WindowText, t.textDisabled);
    qApp->setPalette(palette);
    qApp->setStyleSheet(applicationStyleSheet());
}

QColor ThemeManager::primaryColor() const { return tokens().accent; }
QColor ThemeManager::windowColor() const { return tokens().panel; }
QColor ThemeManager::surfaceColor() const { return tokens().surface; }
QColor ThemeManager::borderColor() const { return tokens().border; }
QColor ThemeManager::textColor() const { return tokens().text; }
QColor ThemeManager::textMutedColor() const { return tokens().textMuted; }
QColor ThemeManager::successColor() const { return tokens().success; }
QColor ThemeManager::warningColor() const { return tokens().warning; }
QColor ThemeManager::destructiveColor() const { return tokens().danger; }
QColor ThemeManager::infoColor() const { return tokens().accentText; }

QColor ThemeManager::rulerBackground() const { return tokens().chrome; }
QColor ThemeManager::rulerBorder() const { return tokens().border; }
QColor ThemeManager::rulerMajorTick() const { return tokens().rulerMajorTick; }
QColor ThemeManager::rulerMinorTick() const { return tokens().rulerMinorTick; }
QColor ThemeManager::rulerText() const { return tokens().textMuted; }

QString ThemeManager::primaryButtonStyleSheet() const
{
    return resolve(QStringLiteral(
        "QPushButton { background-color: @accent; color: @textOnAccent; border: 1px solid @accentBorder; border-radius: 3px; padding: 5px 14px; font-weight: 600; }"
        "QPushButton:hover { background-color: @accentHover; border-color: @accentHover; }"
        "QPushButton:pressed { background-color: @accentPressed; }"
        "QPushButton:disabled { background-color: @surfaceAlt; color: @textDisabled; border-color: @borderSubtle; }"));
}

QString ThemeManager::destructiveButtonStyleSheet() const
{
    return resolve(QStringLiteral(
        "QPushButton { background-color: @danger; color: @textOnAccent; border: 1px solid @danger; border-radius: 3px; padding: 5px 14px; font-weight: 600; }"
        "QPushButton:hover { background-color: @dangerHover; }"
        "QPushButton:pressed { background-color: @dangerPressed; }"
        "QPushButton:disabled { background-color: @surfaceAlt; color: @textDisabled; border-color: @borderSubtle; }"));
}

QString ThemeManager::successButtonStyleSheet() const
{
    return resolve(QStringLiteral(
        "QPushButton { background-color: @success; color: @textOnAccent; border: 1px solid @success; border-radius: 3px; padding: 5px 14px; font-weight: 600; }"
        "QPushButton:hover { background-color: @successHover; }"
        "QPushButton:pressed { background-color: @successPressed; }"));
}

QString ThemeManager::ribbonTabWidgetStyleSheet() const
{
    return resolve(QStringLiteral(
        "QTabWidget::pane { border-top: 1px solid @border; border-bottom: 1px solid @border; background-color: @raised; }"
        "QTabBar { background-color: @chrome; }"
        "QTabBar::tab { font-family: 'Segoe UI', -apple-system, sans-serif; font-size: 11px; font-weight: 600; color: @textMuted;"
        "   background-color: @chrome; border: 1px solid @borderSubtle; border-bottom: none; padding: 4px 12px; margin-right: 2px;"
        "   border-top-left-radius: 4px; border-top-right-radius: 4px; }"
        "QTabBar::tab:hover { background-color: @raised; color: @text; }"
        "QTabBar::tab:selected { background-color: @raised; color: @accentText; border-color: @border; border-top: 2px solid @accent; }"));
}

QString ThemeManager::ribbonScrollStyleSheet() const
{
    return resolve(QStringLiteral(
        "QScrollArea { background-color: @raised; border: none; }"
        "QScrollBar:horizontal { height: 4px; background: @chrome; margin: 0px; }"
        "QScrollBar::handle:horizontal { background: @border; min-width: 20px; border-radius: 2px; }"
        "QScrollBar::handle:horizontal:hover { background: @borderStrong; }"
        "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0px; }"));
}

QString ThemeManager::ribbonContainerStyleSheet() const
{
    return resolve(QStringLiteral("background-color: @raised;"));
}

QString ThemeManager::ribbonPanelStyleSheet() const
{
    return resolve(QStringLiteral(
        "TSA--UI--RibbonPanel { background-color: transparent; border-right: 1px solid @border; margin-right: 2px; padding-right: 4px; }"));
}

QString ThemeManager::ribbonPanelTitleStyleSheet() const
{
    return resolve(QStringLiteral(
        "QLabel { font-family: 'Segoe UI', -apple-system, sans-serif; font-size: 9px; font-weight: 600; color: @textMuted; background: transparent; padding: 1px 2px; }"
        "QLabel:hover { color: @text; }"));
}

QString ThemeManager::ribbonSeparatorStyleSheet() const
{
    return resolve(QStringLiteral("QFrame { background-color: @border; border: none; margin: 4px 3px; }"));
}

QString ThemeManager::ribbonButtonLargeStyleSheet() const
{
    return resolve(QStringLiteral(
        "QToolButton { background-color: transparent; border: 1px solid transparent; border-radius: 3px; padding: 1px 2px;"
        "   font-family: 'Segoe UI', -apple-system, sans-serif; font-size: 9.5px; font-weight: 500; color: @text; }"
        "QToolButton:hover { background-color: @hover; border: 1px solid @borderStrong; color: @hoverText; }"
        "QToolButton:focus { border: 1px solid @focus; }"
        "QToolButton:pressed { background-color: @pressed; border: 1px solid @accentBorder; }"
        "QToolButton:checked { background-color: @accentSubtle; border: 1px solid @accentBorder; border-bottom: 3px solid @accentBorder; color: @accentText; font-weight: 600; }"
        "QToolButton:disabled { color: @textDisabled; }"
        "QToolButton::menu-indicator { subcontrol-origin: padding; subcontrol-position: bottom right; bottom: 1px; right: 1px; }"));
}

QString ThemeManager::ribbonButtonSmallStyleSheet() const
{
    return resolve(QStringLiteral(
        "QToolButton { background-color: transparent; border: 1px solid transparent; border-radius: 2px; padding: 1px 5px;"
        "   font-family: 'Segoe UI', -apple-system, sans-serif; font-size: 10px; color: @textSecondary; text-align: left; }"
        "QToolButton:hover { background-color: @hover; border: 1px solid @borderStrong; color: @hoverText; }"
        "QToolButton:focus { border: 1px solid @focus; }"
        "QToolButton:pressed { background-color: @pressed; border: 1px solid @accentBorder; }"
        "QToolButton:checked { background-color: @accentSubtle; border: 1px solid @accentBorder; border-bottom: 3px solid @accentBorder; color: @accentText; font-weight: 600; }"
        "QToolButton:disabled { color: @textDisabled; }"));
}

QString ThemeManager::topBarStyleSheet() const
{
    return resolve(QStringLiteral("background-color: @chrome; border-bottom: 1px solid @border;"));
}

QString ThemeManager::topBarButtonStyleSheet() const
{
    return resolve(QStringLiteral(
        "QToolButton { background: @raised; border: 1px solid @border; border-radius: 2px; color: @text; font-weight: bold; }"
        "QToolButton:hover { background: @hover; border-color: @focus; color: @accentText; }"
        "QToolButton:focus { border-color: @focus; }"
        "QToolButton:disabled { background: @chrome; border-color: @borderSubtle; color: @textDisabled; }"));
}

QString ThemeManager::topBarComboStyleSheet() const
{
    return resolve(QStringLiteral(
        "QComboBox { background: @surface; color: @text; border: 1px solid @border; border-radius: 2px; padding: 1px 6px; font-size: 11px; font-weight: 500; }"
        "QComboBox:hover { border-color: @focus; }"
        "QComboBox::drop-down { border: none; width: 14px; }"
        "QComboBox QAbstractItemView { background: @surface; color: @text; border: 1px solid @border; selection-background-color: @selection; selection-color: @selectionText; }"));
}

void setStyleProperty(QWidget* widget, const char* name, const QVariant& value)
{
    if (!widget) return;
    if (widget->property(name) == value) return;
    widget->setProperty(name, value);
    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
    widget->update();
}

} // namespace TSA::UI
