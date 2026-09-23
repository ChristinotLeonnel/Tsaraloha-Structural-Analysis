#pragma once

#include <QObject>
#include <QString>
#include <QColor>

namespace TSA::UI
{

class ThemeManager : public QObject
{
    Q_OBJECT

public:
    static ThemeManager& instance();

    bool isDarkMode() const { return m_isDark; }
    void setDarkMode(bool dark);
    void toggleTheme();

    // Méthodes statiques pratiques
    static bool isDark() { return instance().isDarkMode(); }
    static void setDark(bool dark) { instance().setDarkMode(dark); }

    // Couleurs des règles graduées
    QColor rulerBackground() const;
    QColor rulerBorder() const;
    QColor rulerMajorTick() const;
    QColor rulerMinorTick() const;
    QColor rulerText() const;

    // Stylesheets spécifiques
    QString ribbonTabWidgetStyleSheet() const;
    QString ribbonScrollStyleSheet() const;
    QString ribbonContainerStyleSheet() const;
    QString ribbonPanelStyleSheet() const;
    QString ribbonPanelTitleStyleSheet() const;
    QString ribbonSeparatorStyleSheet() const;
    QString ribbonButtonLargeStyleSheet() const;
    QString ribbonButtonSmallStyleSheet() const;
    QString topBarStyleSheet() const;
    QString topBarButtonStyleSheet() const;
    QString topBarComboStyleSheet() const;

signals:
    void themeChanged(bool isDark);

private:
    ThemeManager();
    void applyApplicationTheme();

    bool m_isDark = true;
};

} // namespace TSA::UI
