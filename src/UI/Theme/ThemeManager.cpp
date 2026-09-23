#include "ThemeManager.h"

#include <QSettings>

namespace TSA::UI
{

bool ThemeManager::s_darkMode = false;

bool ThemeManager::isDarkMode()
{
    return s_darkMode;
}

void ThemeManager::setDarkMode(bool dark)
{
    s_darkMode = dark;
    QSettings settings("TSAEngineering", "TSA");
    settings.setValue("Theme/DarkMode", dark);
}

QString ThemeManager::topBarLightStyle()
{
    return "QWidget#topBar { background: #EEF2F6; border-bottom: 1px solid #CCD3DC; font-family: Segoe UI, sans-serif; font-size: 11px; }"
           "QComboBox { background: #FFFFFF; border: 1px solid #B0BDCC; border-radius: 2px; padding: 1px 6px; font-weight: 600; color: #1E293B; min-width: 180px; }"
           "QComboBox:hover { border-color: #3884D8; }"
           "QPushButton { background: #FFFFFF; border: 1px solid #B0BDCC; border-radius: 2px; padding: 2px 7px; color: #2C3E50; font-weight: bold; max-width: 24px; }"
           "QPushButton:hover { background: #E0EBF8; border-color: #3884D8; }"
           "QPushButton:pressed { background: #C8DCF2; }"
           "QLabel { color: #334455; font-weight: 600; padding: 0 4px; }";
}

QString ThemeManager::topBarDarkStyle()
{
    return "QWidget#topBar { background: #21252B; border-bottom: 1px solid #3B4048; font-family: Segoe UI, sans-serif; font-size: 11px; }"
           "QComboBox { background: #1B1D23; border: 1px solid #3B4048; border-radius: 2px; padding: 1px 6px; font-weight: 600; color: #E5E7EB; min-width: 180px; }"
           "QComboBox:hover { border-color: #4C8DFF; }"
           "QPushButton { background: #2B303C; border: 1px solid #3B4048; border-radius: 2px; padding: 2px 7px; color: #E5E7EB; font-weight: bold; max-width: 24px; }"
           "QPushButton:hover { background: #353B48; border-color: #4C8DFF; }"
           "QPushButton:pressed { background: #1E222A; }"
           "QLabel { color: #D1D5DB; font-weight: 600; padding: 0 4px; }";
}

QString ThemeManager::bottomBarLightStyle()
{
    return "QWidget#bottomBar { background: #E6EBF2; border-top: 1px solid #CCD3DC; font-family: Segoe UI, sans-serif; font-size: 11px; }"
           "QPushButton { background: #FFFFFF; border: 1px solid #B0BDCC; border-radius: 2px; padding: 2px 7px; color: #2C3E50; font-weight: 600; }"
           "QPushButton:hover { background: #E0EBF8; border-color: #3884D8; }"
           "QPushButton:pressed { background: #C8DCF2; }"
           "QLabel { color: #334455; padding: 0 4px; }";
}

QString ThemeManager::bottomBarDarkStyle()
{
    return "QWidget#bottomBar { background: #21252B; border-top: 1px solid #3B4048; font-family: Segoe UI, sans-serif; font-size: 11px; }"
           "QPushButton { background: #2B303C; border: 1px solid #3B4048; border-radius: 2px; padding: 2px 7px; color: #E5E7EB; font-weight: 600; }"
           "QPushButton:hover { background: #353B48; border-color: #4C8DFF; }"
           "QPushButton:pressed { background: #1E222A; }"
           "QLabel { color: #D1D5DB; padding: 0 4px; }";
}

QString ThemeManager::lightStyleSheet()
{
    // Réinitialise vers le thème standard Fusion clair
    return QString();
}

QString ThemeManager::darkStyleSheet()
{
    return R"(
/* ========================================================================= */
/* TSA - Thème Sombre Professionnel (Style Robot Structural Analysis / CAD)  */
/* ========================================================================= */

QMainWindow, QWidget#centralWidget {
    background-color: #1E2227;
    color: #ABB2BF;
}

QWidget {
    color: #ABB2BF;
    font-family: "Segoe UI", sans-serif;
    font-size: 12px;
}

/* --- Barre de Menus --- */
QMenuBar {
    background-color: #21252B;
    border-bottom: 1px solid #181A1F;
    color: #ABB2BF;
    padding: 2px;
}

QMenuBar::item {
    background-color: transparent;
    padding: 4px 8px;
    border-radius: 3px;
}

QMenuBar::item:selected {
    background-color: #2C313A;
    color: #FFFFFF;
}

QMenuBar::item:pressed {
    background-color: #1B4D8E;
    color: #FFFFFF;
}

/* --- Menus Déroulants --- */
QMenu {
    background-color: #21252B;
    border: 1px solid #3B4048;
    color: #ABB2BF;
    padding: 4px;
    border-radius: 4px;
}

QMenu::item {
    padding: 5px 24px 5px 20px;
    border-radius: 3px;
}

QMenu::item:selected {
    background-color: #2D72D9;
    color: #FFFFFF;
}

QMenu::separator {
    height: 1px;
    background-color: #3B4048;
    margin: 4px 6px;
}

/* --- Barres d'Outils --- */
QToolBar {
    background-color: #21252B;
    border: none;
    border-bottom: 1px solid #181A1F;
    spacing: 3px;
    padding: 3px 6px;
}

QToolButton {
    background-color: transparent;
    border: 1px solid transparent;
    border-radius: 4px;
    padding: 3px 5px;
    color: #ABB2BF;
}

QToolButton:hover {
    background-color: #2C313A;
    border: 1px solid #3B4048;
    color: #FFFFFF;
}

QToolButton:checked {
    background-color: #1B4D8E;
    border: 1px solid #2D72D9;
    color: #FFFFFF;
}

QToolButton:pressed {
    background-color: #163660;
}

/* --- Dock Widgets (Panneaux Latéraux) --- */
QDockWidget {
    color: #D7DAE0;
    font-weight: bold;
    titlebar-close-icon: url();
}

QDockWidget::title {
    background-color: #21252B;
    border-bottom: 1px solid #181A1F;
    padding: 6px 10px;
    text-align: left;
    color: #D7DAE0;
}

/* --- Arborescence & Tables --- */
QTreeWidget, QTreeView, QTableWidget, QTableView, QListView {
    background-color: #1B1D23;
    alternate-background-color: #21252B;
    border: 1px solid #282C34;
    border-radius: 3px;
    color: #D7DAE0;
    selection-background-color: #2D72D9;
    selection-color: #FFFFFF;
    outline: none;
}

QTreeWidget::item:hover, QTableWidget::item:hover {
    background-color: #282C34;
}

QTreeWidget::item:selected, QTableWidget::item:selected {
    background-color: #2D72D9;
    color: #FFFFFF;
}

QHeaderView::section {
    background-color: #21252B;
    color: #D7DAE0;
    padding: 4px 6px;
    border: none;
    border-right: 1px solid #282C34;
    border-bottom: 1px solid #282C34;
    font-weight: bold;
}

/* --- Champs de Saisie --- */
QLineEdit, QSpinBox, QDoubleSpinBox, QComboBox {
    background-color: #1B1D23;
    border: 1px solid #3B4048;
    border-radius: 3px;
    padding: 3px 6px;
    color: #E5E7EB;
    selection-background-color: #2D72D9;
    selection-color: #FFFFFF;
}

QLineEdit:focus, QSpinBox:focus, QDoubleSpinBox:focus, QComboBox:focus {
    border-color: #4C8DFF;
}

QComboBox::drop-down {
    border: none;
    width: 20px;
}

QComboBox QAbstractItemView {
    background-color: #21252B;
    border: 1px solid #3B4048;
    selection-background-color: #2D72D9;
    selection-color: #FFFFFF;
    color: #E5E7EB;
}

/* --- Boutons Standards --- */
QPushButton {
    background-color: #2B303C;
    border: 1px solid #3B4048;
    border-radius: 4px;
    padding: 5px 12px;
    color: #E5E7EB;
    font-weight: 500;
}

QPushButton:hover {
    background-color: #353B48;
    border-color: #4C8DFF;
    color: #FFFFFF;
}

QPushButton:pressed {
    background-color: #1E222A;
}

QPushButton:disabled {
    background-color: #1E2227;
    border-color: #2C313A;
    color: #5C6370;
}

/* --- Barre d'État --- */
QStatusBar {
    background-color: #181A1F;
    border-top: 1px solid #282C34;
    color: #828997;
}

QStatusBar::item {
    border: none;
}

/* --- Barres de Défilement (Scrollbars fines modernes) --- */
QScrollBar:vertical {
    background-color: #1E2227;
    width: 10px;
    margin: 0;
    border: none;
}

QScrollBar::handle:vertical {
    background-color: #3B4048;
    min-height: 24px;
    border-radius: 5px;
    margin: 1px;
}

QScrollBar::handle:vertical:hover {
    background-color: #4C8DFF;
}

QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
    height: 0px;
}

QScrollBar:horizontal {
    background-color: #1E2227;
    height: 10px;
    margin: 0;
    border: none;
}

QScrollBar::handle:horizontal {
    background-color: #3B4048;
    min-width: 24px;
    border-radius: 5px;
    margin: 1px;
}

QScrollBar::handle:horizontal:hover {
    background-color: #4C8DFF;
}

QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {
    width: 0px;
}

/* --- Onglets (Tabs) --- */
QTabWidget::pane {
    border: 1px solid #282C34;
    background-color: #1E2227;
}

QTabBar::tab {
    background-color: #21252B;
    color: #ABB2BF;
    border: 1px solid #181A1F;
    border-bottom: none;
    padding: 6px 14px;
    border-top-left-radius: 4px;
    border-top-right-radius: 4px;
    margin-right: 2px;
}

QTabBar::tab:selected {
    background-color: #1E2227;
    color: #FFFFFF;
    border-top: 2px solid #2D72D9;
    font-weight: bold;
}

QTabBar::tab:hover:!selected {
    background-color: #282C34;
    color: #FFFFFF;
}

/* --- Group Boxes --- */
QGroupBox {
    border: 1px solid #3B4048;
    border-radius: 5px;
    margin-top: 14px;
    padding-top: 10px;
    font-weight: bold;
    color: #D7DAE0;
}

QGroupBox::title {
    subcontrol-origin: margin;
    subcontrol-position: top left;
    left: 12px;
    padding: 0 4px;
    background-color: #1E2227;
}

/* --- Cases à cocher & Boutons radio --- */
QCheckBox, QRadioButton {
    color: #D7DAE0;
    spacing: 7px;
}

QCheckBox::indicator, QRadioButton::indicator {
    width: 15px;
    height: 15px;
    border: 1px solid #3B4048;
    border-radius: 3px;
    background-color: #1B1D23;
}

QCheckBox::indicator:hover, QRadioButton::indicator:hover {
    border-color: #4C8DFF;
}

QCheckBox::indicator:checked, QRadioButton::indicator:checked {
    background-color: #2D72D9;
    border-color: #4C8DFF;
}

/* --- Infobulles Tooltips --- */
QToolTip {
    background-color: #21252B;
    color: #F8FAFC;
    border: 1px solid #4C8DFF;
    border-radius: 4px;
    padding: 5px 8px;
    font-size: 11px;
}
)";
}

} // namespace TSA::UI
