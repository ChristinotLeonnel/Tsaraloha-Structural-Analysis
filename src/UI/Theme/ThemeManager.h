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
    void setDarkMode(bool dark, bool force = false);
    void toggleTheme();

    // Méthodes statiques pratiques
    static bool isDark() { return instance().isDarkMode(); }
    static void setDark(bool dark, bool force = false) { instance().setDarkMode(dark, force); }

    // Couleurs des règles graduées
    QColor rulerBackground() const;
    QColor rulerBorder() const;
    QColor rulerMajorTick() const;
    QColor rulerMinorTick() const;
    QColor rulerText() const;

    // Couleurs sémantiques et de thème
    QColor primaryColor() const;
    QColor windowColor() const;
    QColor surfaceColor() const;
    QColor borderColor() const;
    QColor textColor() const;
    QColor textMutedColor() const;
    QColor successColor() const;
    QColor warningColor() const;
    QColor destructiveColor() const;
    QColor infoColor() const;

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
    QString primaryButtonStyleSheet() const;
    QString destructiveButtonStyleSheet() const;
    QString successButtonStyleSheet() const;

signals:
    void themeChanged(bool isDark);

private:
    ThemeManager();
    void applyApplicationTheme();

    bool m_isDark = true;
};

} // namespace TSA::UI
