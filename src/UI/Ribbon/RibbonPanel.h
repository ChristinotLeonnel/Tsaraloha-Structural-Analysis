#pragma once

#include <QWidget>
#include <vector>
#include "RibbonTypes.h"

class QAction;
class QMenu;
class QHBoxLayout;
class QVBoxLayout;
class QLabel;

namespace TSA::UI
{

class RibbonButton;

class RibbonPanel : public QWidget
{
    Q_OBJECT

public:
    explicit RibbonPanel(const QString& title, QWidget* parent = nullptr);

    const QString& title() const { return m_title; }

    // Ajoute un grand bouton principal (32x32 avec texte en-dessous)
    RibbonButton* addLargeAction(QAction* action, QMenu* menu = nullptr);

    // Ajoute une colonne verticale contenant 2 ou 3 petits boutons compacts (façon AutoCAD)
    std::vector<RibbonButton*> addSmallColumn(const std::vector<QAction*>& actions);

    // Ajoute un bouton de menu déroulant (dropdown / split button)
    RibbonButton* addMenuButton(const QString& text, const QIcon& icon, QMenu* menu, RibbonButtonSize size = RibbonButtonSize::Large);

    // Ajoute un séparateur interne discret
    void addInternalSeparator();

    // Ajoute un widget personnalisé dans le panneau
    void addCustomWidget(QWidget* widget);

    void updateTheme(bool isDark);

private:
    void setupUi();

private:
    QString m_title;
    QHBoxLayout* m_contentLayout = nullptr;
    QLabel* m_lblTitle = nullptr;
    std::vector<QWidget*> m_separators;
};

} // namespace TSA::UI
