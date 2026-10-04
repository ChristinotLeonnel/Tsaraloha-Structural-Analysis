#pragma once

#include <QDockWidget>
#include <vector>

class QCheckBox;
class QAction;
class QGroupBox;
class QLineEdit;

namespace TSA::UI
{

class VisibilityDock : public QDockWidget
{
    Q_OBJECT

public:
    explicit VisibilityDock(QWidget* parent = nullptr);

    // Lie les cases à cocher aux QActions correspondantes
    void bindGridVisibleAction(QAction* act);
    void bindLevelsVisibleAction(QAction* act);
    void bindGridLabelsAction(QAction* act);
    void bindRulersVisibleAction(QAction* act);
    void bindCoordSystemAction(QAction* act);
    void bindWorkPlaneVisibleAction(QAction* act);
    void bindNodesVisibleAction(QAction* act);
    void bindNodeLabelsAction(QAction* act);
    void bindLoadsVisibleAction(QAction* act);
    void bindLoadValuesVisibleAction(QAction* act);

signals:
    // Famille d'éléments (valeur de OccView::ElementCategory) affichée / masquée.
    void elementCategoryToggled(int category, bool visible);

private:
    void setupUi();
    void applyFilter(const QString& text);
    void setAllStructureVisible(bool visible);

private:
    QCheckBox* m_chkGrid = nullptr;
    QCheckBox* m_chkLevels = nullptr;
    QCheckBox* m_chkLabels = nullptr;
    QCheckBox* m_chkRulers = nullptr;
    QCheckBox* m_chkCoords = nullptr;
    QCheckBox* m_chkWorkPlane = nullptr;

    QCheckBox* m_chkNodes = nullptr;
    QCheckBox* m_chkNodeLabels = nullptr;
    QCheckBox* m_chkLoads = nullptr;
    QCheckBox* m_chkLoadValues = nullptr;
    QCheckBox* m_chkBeams = nullptr;
    QCheckBox* m_chkColumns = nullptr;
    QCheckBox* m_chkSlabs = nullptr;

    QLineEdit* m_filter = nullptr;
    QGroupBox* m_guidesGroup = nullptr;
    QGroupBox* m_modelGroup = nullptr;
    std::vector<QCheckBox*> m_allChecks;        // toutes les cases (filtre de recherche)
    std::vector<QCheckBox*> m_structureChecks;  // cases du groupe « Composants » (Tout afficher / masquer)
};

} // namespace TSA::UI
