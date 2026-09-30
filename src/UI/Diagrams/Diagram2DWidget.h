#pragma once

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
 * @brief Widget interactif 2D de visualisation des diagrammes d'efforts (M, V, N) et courbes OpenSees.
 * Permet d'inspecter les valeurs à n'importe quel point le long de l'élément au survol de la souris,
 * d'afficher les extrêmes, et de visualiser les courbes de capacité Pushover et spectres modaux.
 */
class Diagram2DWidget : public QWidget
{
    Q_OBJECT

public:
    enum class ViewMode
    {
        MemberForces,       ///< Diagrammes le long d'une barre (M, V, N, T)
        PushoverCapacity,   ///< Courbe de capacité Pushover Vb vs Delta
        ModalSpectrum       ///< Spectre des fréquences et périodes modales
    };

    explicit Diagram2DWidget(QWidget* parent = nullptr);
    ~Diagram2DWidget() override = default;

    void setModel(TSA::Model::Model* model);
    void setResultsModel(const std::shared_ptr<TSA::Analysis::ResultsModel>& results);

    void setSelectedElement(int elementId);
    int selectedElement() const { return m_currentElementId; }

    void setDiagramType(TSA::Geometry::DiagramType type);
    TSA::Geometry::DiagramType diagramType() const { return m_currentType; }

    void setViewMode(ViewMode mode);
    ViewMode viewMode() const { return m_viewMode; }

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
    void onModeComboChanged(int index);
    void onResetZoom();

private:
    void setupUi();
    void updateElementList();
    void drawMemberForces(QPainter& p, const QRect& plotRect);
    void drawPushoverCurve(QPainter& p, const QRect& plotRect);
    void drawModalSpectrum(QPainter& p, const QRect& plotRect);

private:
    TSA::Model::Model* m_model = nullptr;
    std::shared_ptr<TSA::Analysis::ResultsModel> m_results;

    ViewMode m_viewMode = ViewMode::MemberForces;
    TSA::Geometry::DiagramType m_currentType = TSA::Geometry::DiagramType::BendingMz;
    int m_currentElementId = 0;

    // Commandes UI
    QWidget* m_toolbar = nullptr;
    QComboBox* m_modeCombo = nullptr;
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
