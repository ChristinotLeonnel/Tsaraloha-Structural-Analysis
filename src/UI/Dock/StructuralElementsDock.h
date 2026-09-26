#pragma once

#include <QDockWidget>
#include <QTreeWidget>
#include <QPushButton>
#include <QLineEdit>

namespace TSA::UI
{

/**
 * @brief Volet de navigation et sélection des éléments structuraux à modéliser.
 * Présente la hiérarchie conceptuelle :
 * Éléments structuraux
 * ├── Éléments filaires (Poutre, Poteau, Barre, Cable, Treillis)
 * ├── Éléments surfaciques (Dalle, Voile, Panneau)
 * └── Fondations & Appuis
 */
class StructuralElementsDock : public QDockWidget
{
    Q_OBJECT

public:
    explicit StructuralElementsDock(QWidget* parent = nullptr);
    ~StructuralElementsDock() override = default;

signals:
    void drawBeamTriggered();
    void drawColumnTriggered();
    void drawBarTriggered();
    void drawCableTriggered();
    void drawTrussTriggered();
    void drawSlabTriggered();
    void drawWallTriggered();
    void drawPanelTriggered();
    void drawFootingTriggered();

private slots:
    void onItemClicked(QTreeWidgetItem* item, int column);
    void onItemDoubleClicked(QTreeWidgetItem* item, int column);
    void onFilterChanged(const QString& filter);

private:
    void setupUi();
    void buildElementTree();
    void triggerActionForRole(const QString& role);

private:
    QLineEdit* m_searchEdit = nullptr;
    QPushButton* m_btnQuickCable = nullptr;
    QPushButton* m_btnQuickBeam = nullptr;
    QPushButton* m_btnQuickColumn = nullptr;
    QPushButton* m_btnQuickSlab = nullptr;
    QPushButton* m_btnQuickWall = nullptr;

    QTreeWidget* m_tree = nullptr;
    QTreeWidgetItem* m_rootItem = nullptr;
    QTreeWidgetItem* m_linearCategory = nullptr;
    QTreeWidgetItem* m_surfaceCategory = nullptr;
    QTreeWidgetItem* m_foundationCategory = nullptr;
};

} // namespace TSA::UI
