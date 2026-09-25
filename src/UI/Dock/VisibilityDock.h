#pragma once

#include <QDockWidget>

class QCheckBox;
class QAction;

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

private:
    void setupUi();

private:
    QCheckBox* m_chkGrid = nullptr;
    QCheckBox* m_chkLevels = nullptr;
    QCheckBox* m_chkLabels = nullptr;
    QCheckBox* m_chkRulers = nullptr;
    QCheckBox* m_chkCoords = nullptr;

    QCheckBox* m_chkNodes = nullptr;
    QCheckBox* m_chkBeams = nullptr;
    QCheckBox* m_chkColumns = nullptr;
    QCheckBox* m_chkSlabs = nullptr;
};

} // namespace TSA::UI
