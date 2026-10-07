#pragma once

#include "../../Analysis/AnalysisTypes.h"
#include <QWidget>
#include <QPainter>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <memory>
#include <vector>

#include "../../Analysis/ResultsModel.h"
#include "../../Geometry/DiagramGeometry.h"

namespace TSA::Model
{
class Model;
}

namespace TSA::UI
{

/**
 * @brief Widget interactif 2D de visualisation des diagrammes d'efforts (M, V, N, T) le long d'une barre.
 * Permet d'inspecter les valeurs à n'importe quel point le long de l'élément au survol de la souris
 * et d'afficher les extrêmes.
 */
class Diagram2DWidget : public QWidget
{
    Q_OBJECT

public:
    explicit Diagram2DWidget(QWidget* parent = nullptr);
    ~Diagram2DWidget() override = default;

    void setModel(TSA::Model::Model* model);
    void setResultsModel(const std::shared_ptr<TSA::Analysis::ResultsModel>& results);

    void setSelectedElement(int elementId, TSA::Analysis::StructuralElementKind kind = TSA::Analysis::StructuralElementKind::Beam);
    int selectedElement() const { return m_currentElementId; }

    void setDiagramType(TSA::Geometry::DiagramType type);
    TSA::Geometry::DiagramType diagramType() const { return m_currentType; }


signals:
    void elementInspected(int elementId, double posNorm, double forceValue);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private slots:
    void onElementComboChanged(int index);
    void onTypeComboChanged(int index);
    void onResetZoom();

private:
    void setupUi();
    void updateElementList();
    void drawMemberForces(QPainter& p, const QRect& plotRect);

private:
    TSA::Model::Model* m_model = nullptr;
    std::shared_ptr<TSA::Analysis::ResultsModel> m_results;

    TSA::Geometry::DiagramType m_currentType = TSA::Geometry::DiagramType::BendingMz;
    int m_currentElementId = 0;
    TSA::Analysis::StructuralElementKind m_currentElementKind = TSA::Analysis::StructuralElementKind::Beam; ///< ids poutre/poteau non uniques entre familles
    int findElementItem(TSA::Analysis::StructuralElementKind kind, int id) const;

    // Commandes UI
    QWidget* m_toolbar = nullptr;
    QComboBox* m_elementCombo = nullptr;
    QComboBox* m_typeCombo = nullptr;
    QPushButton* m_btnResetZoom = nullptr;
    QLabel* m_lblStatus = nullptr;

    // Curseur interactif
    bool m_hasHoverCursor = false;
    QPoint m_hoverPixel;
    double m_hoverX = 0.0;
    double m_hoverVal = 0.0;

    // Facteurs de zoom/pan 2D
    double m_zoomFactor = 1.0;
    double m_panOffset = 0.0;
};

} // namespace TSA::UI
