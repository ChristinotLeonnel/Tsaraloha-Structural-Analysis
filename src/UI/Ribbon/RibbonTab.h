#pragma once

#include <QWidget>
#include <vector>

class QHBoxLayout;
class QMenu;

namespace TSA::UI
{

class RibbonPanel;
class RibbonButton;

// Un onglet du ruban : une rangée de panneaux qui s'adapte à la largeur disponible
// (jamais de défilement horizontal) :
//   1. panneaux complets ;
//   2. petits boutons réduits à leur icône ;
//   3. panneaux repliés en bouton déroulant, en partant de la droite ;
//   4. à défaut, les derniers panneaux passent dans le menu « Plus ».
class RibbonTab : public QWidget
{
    Q_OBJECT

public:
    explicit RibbonTab(QWidget* parent = nullptr);

    void addPanel(RibbonPanel* panel);
    const std::vector<RibbonPanel*>& panels() const { return m_panels; }

    void updateTheme(bool isDark);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;

private:
    void setupUi();
    /// Disposition pour la largeur donnée (fonction de la seule largeur, largeurs en cache).
    void relayout(int width);
    int availableWidth() const;
    /// Contrôle unique après le premier affichage : si une largeur mesurée a changé (polissage
    /// différé par Qt), la disposition est recalculée tout de suite plutôt qu'au passage suivant.
    void verifyMeasurements();

private:
    QWidget* m_container = nullptr;
    QHBoxLayout* m_panelLayout = nullptr;
    RibbonButton* m_moreButton = nullptr;
    std::vector<RibbonPanel*> m_panels;
    bool m_inRelayout = false;
    bool m_verified = false;
    int m_lastWidth = -1;
};

} // namespace TSA::UI
