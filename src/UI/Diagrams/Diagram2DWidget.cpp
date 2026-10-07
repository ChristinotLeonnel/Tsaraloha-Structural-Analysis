#include "Diagram2DWidget.h"
#include "../../Model/Model.h"
#include "../../Model/Beam.h"
#include "../../Model/Column.h"
#include "../../Model/Node.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPainterPath>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QFontMetrics>
#include <cmath>
#include <algorithm>

namespace
{
static std::vector<TSA::Analysis::StationForces> getAllStations(const TSA::Analysis::ElementResults& el)
{
    std::vector<TSA::Analysis::StationForces> res;
    res.reserve(2 + el.intermediateStations.size());
    res.push_back(el.startForces);
    res.insert(res.end(), el.intermediateStations.begin(), el.intermediateStations.end());
    res.push_back(el.endForces);
    return res;
}
} // namespace

namespace TSA::UI
{

Diagram2DWidget::Diagram2DWidget(QWidget* parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setupUi();
}

void Diagram2DWidget::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(4);

    m_toolbar = new QWidget(this);
    auto* tbLayout = new QHBoxLayout(m_toolbar);
    tbLayout->setContentsMargins(4, 2, 4, 2);
    tbLayout->setSpacing(6);

    m_elementCombo = new QComboBox(m_toolbar);
    m_elementCombo->setMinimumWidth(160);

    m_typeCombo = new QComboBox(m_toolbar);
    m_typeCombo->addItem(tr("Moment Fléchissant Mz (kNm)"), static_cast<int>(TSA::Geometry::DiagramType::BendingMz));
    m_typeCombo->addItem(tr("Moment Fléchissant My (kNm)"), static_cast<int>(TSA::Geometry::DiagramType::BendingMy));
    m_typeCombo->addItem(tr("Effort Tranchant Vz (kN)"), static_cast<int>(TSA::Geometry::DiagramType::ShearForceVz));
    m_typeCombo->addItem(tr("Effort Tranchant Vy (kN)"), static_cast<int>(TSA::Geometry::DiagramType::ShearForceVy));
    m_typeCombo->addItem(tr("Effort Normal N (kN)"), static_cast<int>(TSA::Geometry::DiagramType::AxialForceN));
    m_typeCombo->addItem(tr("Moment de Torsion Mx (kNm)"), static_cast<int>(TSA::Geometry::DiagramType::TorsionMx));

    m_btnResetZoom = new QPushButton(tr("Recentrer"), m_toolbar);
    m_lblStatus = new QLabel(tr("Prêt"), m_toolbar);
    m_lblStatus->setStyleSheet("color: #a0a0b0; font-size: 11px;");

    tbLayout->addWidget(new QLabel(tr("Élément :"), m_toolbar));
    tbLayout->addWidget(m_elementCombo);
    tbLayout->addWidget(new QLabel(tr("Grandeur :"), m_toolbar));
    tbLayout->addWidget(m_typeCombo);
    tbLayout->addWidget(m_btnResetZoom);
    tbLayout->addStretch();
    tbLayout->addWidget(m_lblStatus);

    mainLayout->addWidget(m_toolbar);
    mainLayout->addStretch(1);

    connect(m_elementCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &Diagram2DWidget::onElementComboChanged);
    connect(m_typeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &Diagram2DWidget::onTypeComboChanged);
    connect(m_btnResetZoom, &QPushButton::clicked, this, &Diagram2DWidget::onResetZoom);
}

void Diagram2DWidget::setModel(TSA::Model::Model* model)
{
    m_model = model;
    updateElementList();
    update();
}

void Diagram2DWidget::setResultsModel(const std::shared_ptr<TSA::Analysis::ResultsModel>& results)
{
    m_results = results;
    updateElementList();
    update();
}

void Diagram2DWidget::updateElementList()
{
    m_elementCombo->blockSignals(true);
    m_elementCombo->clear();

    if (m_model)
    {
        for (const auto& [id, beam] : m_model->beams())
        {
            const auto* n1 = m_model->getNode(beam.startNodeId());
            const auto* n2 = m_model->getNode(beam.endNodeId());
            double len = (n1 && n2) ? std::sqrt(std::pow(n2->x()-n1->x(),2) + std::pow(n2->y()-n1->y(),2) + std::pow(n2->z()-n1->z(),2)) : 0.0;
            m_elementCombo->addItem(tr("Poutre #%1 (L = %2 m)").arg(id).arg(len, 0, 'f', 2), id);
            m_elementCombo->setItemData(m_elementCombo->count() - 1, static_cast<int>(TSA::Analysis::StructuralElementKind::Beam), Qt::UserRole + 1);
        }
        for (const auto& [id, col] : m_model->columns())
        {
            const auto* n1 = m_model->getNode(col.startNodeId());
            const auto* n2 = m_model->getNode(col.endNodeId());
            double len = (n1 && n2) ? std::sqrt(std::pow(n2->x()-n1->x(),2) + std::pow(n2->y()-n1->y(),2) + std::pow(n2->z()-n1->z(),2)) : 0.0;
            m_elementCombo->addItem(tr("Poteau #%1 (H = %2 m)").arg(id).arg(len, 0, 'f', 2), id);
            m_elementCombo->setItemData(m_elementCombo->count() - 1, static_cast<int>(TSA::Analysis::StructuralElementKind::Column), Qt::UserRole + 1);
        }
    }

    if (m_elementCombo->count() > 0)
    {
        int idx = findElementItem(m_currentElementKind, m_currentElementId);
        if (idx >= 0)
        {
            m_elementCombo->setCurrentIndex(idx);
        }
        else
        {
            m_currentElementId = m_elementCombo->itemData(0).toInt();
            m_currentElementKind = static_cast<TSA::Analysis::StructuralElementKind>(m_elementCombo->itemData(0, Qt::UserRole + 1).toInt());
            m_elementCombo->setCurrentIndex(0);
        }
    }
    m_elementCombo->blockSignals(false);
}

int Diagram2DWidget::findElementItem(TSA::Analysis::StructuralElementKind kind, int id) const
{
    for (int i = 0; i < m_elementCombo->count(); ++i)
    {
        if (m_elementCombo->itemData(i).toInt() == id
            && m_elementCombo->itemData(i, Qt::UserRole + 1).toInt() == static_cast<int>(kind))
            return i;
    }
    return -1;
}

void Diagram2DWidget::setSelectedElement(int elementId, TSA::Analysis::StructuralElementKind kind)
{
    if (m_currentElementId == elementId && m_currentElementKind == kind) return;
    m_currentElementId = elementId;
    m_currentElementKind = kind;
    int idx = findElementItem(kind, elementId);
    if (idx >= 0)
    {
        m_elementCombo->blockSignals(true);
        m_elementCombo->setCurrentIndex(idx);
        m_elementCombo->blockSignals(false);
    }
    update();
}

void Diagram2DWidget::setDiagramType(TSA::Geometry::DiagramType type)
{
    m_currentType = type;
    int idx = m_typeCombo->findData(static_cast<int>(type));
    if (idx >= 0)
    {
        m_typeCombo->blockSignals(true);
        m_typeCombo->setCurrentIndex(idx);
        m_typeCombo->blockSignals(false);
    }
    update();
}

void Diagram2DWidget::onElementComboChanged(int index)
{
    m_currentElementId = m_elementCombo->itemData(index).toInt();
    m_currentElementKind = static_cast<TSA::Analysis::StructuralElementKind>(m_elementCombo->itemData(index, Qt::UserRole + 1).toInt());
    update();
}

void Diagram2DWidget::onTypeComboChanged(int index)
{
    m_currentType = static_cast<TSA::Geometry::DiagramType>(m_typeCombo->itemData(index).toInt());
    update();
}

void Diagram2DWidget::onResetZoom()
{
    m_zoomFactor = 1.0;
    m_panOffset = 0.0;
    update();
}

void Diagram2DWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
}

void Diagram2DWidget::wheelEvent(QWheelEvent* event)
{
    double delta = event->angleDelta().y();
    if (delta > 0)
    {
        m_zoomFactor *= 1.15;
    }
    else if (delta < 0)
    {
        m_zoomFactor = std::max(0.2, m_zoomFactor / 1.15);
    }
    update();
    event->accept();
}

void Diagram2DWidget::leaveEvent(QEvent* /*event*/)
{
    m_hasHoverCursor = false;
    update();
}

void Diagram2DWidget::mouseMoveEvent(QMouseEvent* event)
{
    QRect plotRect = rect().adjusted(65, 45, -25, -35);
    if (plotRect.contains(event->pos()))
    {
        m_hasHoverCursor = true;
        m_hoverPixel = event->pos();

        double normX = static_cast<double>(event->pos().x() - plotRect.left()) / plotRect.width();
        normX = std::clamp(normX, 0.0, 1.0);

        // Récupérer la valeur interpolée
        if (m_results && m_results->isValid())
        {
            const auto* eb = m_results->getElementResults(m_currentElementKind, m_currentElementId);
            if (eb)
            {
                auto stations = getAllStations(*eb);
                double elLen = (eb->length > 1e-6) ? eb->length : 1.0;
                if (stations.size() >= 2)
                {
                    // Interpolation linéaire entre stations
                    for (size_t i = 0; i < stations.size() - 1; ++i)
                    {
                        double s1 = stations[i].position / elLen;
                        double s2 = stations[i + 1].position / elLen;
                        if (normX >= s1 && normX <= s2)
                        {
                            double v1 = TSA::Geometry::DiagramGeometry::getStationValue(stations[i], m_currentType);
                            double v2 = TSA::Geometry::DiagramGeometry::getStationValue(stations[i + 1], m_currentType);
                            double factor = (s2 > s1) ? (normX - s1) / (s2 - s1) : 0.0;
                            m_hoverVal = v1 + factor * (v2 - v1);
                            break;
                        }
                    }
                }
            }
        }

        // Longueur de la barre
        double L = 1.0;
        if (m_model)
        {
            int sId = 0, eId = 0;
            const auto* b = m_currentElementKind == TSA::Analysis::StructuralElementKind::Beam ? m_model->getBeam(m_currentElementId) : nullptr;
            if (b) { sId = b->startNodeId(); eId = b->endNodeId(); }
            else {
                const auto* c = m_currentElementKind == TSA::Analysis::StructuralElementKind::Column ? m_model->getColumn(m_currentElementId) : nullptr;
                if (c) { sId = c->startNodeId(); eId = c->endNodeId(); }
            }
            const auto* n1 = m_model->getNode(sId);
            const auto* n2 = m_model->getNode(eId);
            if (n1 && n2)
            {
                L = std::sqrt(std::pow(n2->x()-n1->x(),2) + std::pow(n2->y()-n1->y(),2) + std::pow(n2->z()-n1->z(),2));
            }
        }
        m_hoverX = normX * L;

        QString unit = TSA::Geometry::DiagramGeometry::diagramUnit(m_currentType);
        m_lblStatus->setText(tr("x = %1 m (%2%) | %3 = %4 %5")
                             .arg(m_hoverX, 0, 'f', 2)
                             .arg(normX * 100.0, 0, 'f', 0)
                             .arg(TSA::Geometry::DiagramGeometry::diagramTypeName(m_currentType))
                             .arg(m_hoverVal, 0, 'f', 2)
                             .arg(unit));

        emit elementInspected(m_currentElementId, normX, m_hoverVal);
        update();
    }
    else
    {
        m_hasHoverCursor = false;
        update();
    }
}

void Diagram2DWidget::paintEvent(QPaintEvent* /*event*/)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // Fond foncé pro de CAO
    p.fillRect(rect(), QColor(28, 28, 32));

    QRect plotRect = rect().adjusted(65, 45, -25, -35);
    if (plotRect.width() < 50 || plotRect.height() < 50) return;

    p.fillRect(plotRect, QColor(22, 22, 26));
    p.setPen(QPen(QColor(60, 60, 70), 1));
    p.drawRect(plotRect);

    drawMemberForces(p, plotRect);
}

void Diagram2DWidget::drawMemberForces(QPainter& p, const QRect& plotRect)
{
    if (!m_results || !m_results->isValid())
    {
        p.setPen(QColor(160, 160, 170));
        p.drawText(plotRect, Qt::AlignCenter, tr("Aucun résultat OpenSees disponible.\nLancez le calcul EF (F5) pour générer les diagrammes."));
        return;
    }

    const auto* eb = m_results->getElementResults(m_currentElementKind, m_currentElementId);
    if (!eb)
    {
        p.setPen(QColor(160, 160, 170));
        p.drawText(plotRect, Qt::AlignCenter, tr("Sélectionnez une barre pour afficher son diagramme d'efforts."));
        return;
    }

    auto stations = getAllStations(*eb);
    if (stations.empty())
    {
        p.setPen(QColor(160, 160, 170));
        p.drawText(plotRect, Qt::AlignCenter, tr("Sélectionnez une barre pour afficher son diagramme d'efforts."));
        return;
    }

    // Longueur de la barre
    double L = (eb->length > 1e-6) ? eb->length : 1.0;
    if (m_model)
    {
        int sId = 0, eId = 0;
        const auto* b = m_currentElementKind == TSA::Analysis::StructuralElementKind::Beam ? m_model->getBeam(m_currentElementId) : nullptr;
        if (b) { sId = b->startNodeId(); eId = b->endNodeId(); }
        else {
            const auto* c = m_currentElementKind == TSA::Analysis::StructuralElementKind::Column ? m_model->getColumn(m_currentElementId) : nullptr;
            if (c) { sId = c->startNodeId(); eId = c->endNodeId(); }
        }
        const auto* n1 = m_model->getNode(sId);
        const auto* n2 = m_model->getNode(eId);
        if (n1 && n2)
        {
            L = std::sqrt(std::pow(n2->x()-n1->x(),2) + std::pow(n2->y()-n1->y(),2) + std::pow(n2->z()-n1->z(),2));
        }
    }

    double maxVal = -1e9, minVal = 1e9;
    for (const auto& st : stations)
    {
        double v = TSA::Geometry::DiagramGeometry::getStationValue(st, m_currentType);
        if (v > maxVal) maxVal = v;
        if (v < minVal) minVal = v;
    }

    double bound = std::max(std::abs(maxVal), std::abs(minVal));
    if (bound < 1e-4) bound = 1.0;
    bound *= 1.25 / m_zoomFactor;

    auto valToY = [&](double v) -> int {
        return plotRect.center().y() - static_cast<int>((v / bound) * (plotRect.height() / 2.0));
    };

    auto posToX = [&](double s) -> int {
        return plotRect.left() + static_cast<int>(s * plotRect.width());
    };

    int yZero = valToY(0.0);

    // Ligne zéro (fibre neutre de référence)
    p.setPen(QPen(QColor(120, 120, 140), 1, Qt::DashLine));
    p.drawLine(plotRect.left(), yZero, plotRect.right(), yZero);

    // Graduations axe Y
    p.setFont(QFont("Arial", 9));
    p.setPen(QColor(180, 180, 190));
    QString unit = TSA::Geometry::DiagramGeometry::diagramUnit(m_currentType);
    p.drawText(QRect(5, yZero - 10, 55, 20), Qt::AlignRight | Qt::AlignVCenter, "0.0");
    p.drawText(QRect(5, plotRect.top(), 55, 20), Qt::AlignRight | Qt::AlignVCenter, QString("+%1").arg(bound, 0, 'f', 1));
    p.drawText(QRect(5, plotRect.bottom() - 20, 55, 20), Qt::AlignRight | Qt::AlignVCenter, QString("-%1").arg(bound, 0, 'f', 1));
    p.drawText(QRect(5, 10, 80, 20), Qt::AlignLeft | Qt::AlignVCenter, unit);

    // Graduations axe X
    p.drawText(QRect(plotRect.left() - 20, plotRect.bottom() + 4, 40, 20), Qt::AlignCenter, "0.0 m");
    p.drawText(QRect(plotRect.right() - 30, plotRect.bottom() + 4, 60, 20), Qt::AlignCenter, QString("%1 m").arg(L, 0, 'f', 2));
    p.drawText(QRect(plotRect.center().x() - 30, plotRect.bottom() + 4, 60, 20), Qt::AlignCenter, QString("%1 m").arg(L / 2.0, 0, 'f', 2));

    // Construction du polygone de remplissage et de la courbe
    QPainterPath pathCurve;
    QPainterPath pathArea;

    pathArea.moveTo(posToX(0.0), yZero);

    for (size_t i = 0; i < stations.size(); ++i)
    {
        double s = (L > 1e-6) ? (stations[i].position / L) : 0.0;
        double v = TSA::Geometry::DiagramGeometry::getStationValue(stations[i], m_currentType);
        int px = posToX(s);
        int py = valToY(v);

        if (i == 0) pathCurve.moveTo(px, py);
        else pathCurve.lineTo(px, py);

        pathArea.lineTo(px, py);
    }

    pathArea.lineTo(posToX(1.0), yZero);
    pathArea.closeSubpath();

    // Remplissage avec dégradé subtil
    QColor fillColor = (maxVal >= -minVal) ? QColor(255, 120, 60, 70) : QColor(60, 140, 255, 70);
    p.fillPath(pathArea, fillColor);

    // Dessin de la courbe principale
    QColor curveColor = (maxVal >= -minVal) ? QColor(255, 140, 80) : QColor(80, 160, 255);
    p.setPen(QPen(curveColor, 2));
    p.drawPath(pathCurve);

    // Marqueurs de points de station
    for (const auto& st : stations)
    {
        double s = (L > 1e-6) ? (st.position / L) : 0.0;
        int px = posToX(s);
        int py = valToY(TSA::Geometry::DiagramGeometry::getStationValue(st, m_currentType));
        p.setBrush(QColor(255, 255, 255));
        p.setPen(QPen(curveColor, 1));
        p.drawEllipse(QPoint(px, py), 3, 3);
    }

    // Affichage des extrêmes Max et Min
    auto drawBadge = [&](double pos, double val, const QString& label) {
        int bx = posToX(pos);
        int by = valToY(val);
        QString txt = QString("%1: %2 %3").arg(label).arg(val, 0, 'f', 1).arg(unit);
        QRect textRect = p.fontMetrics().boundingRect(txt).adjusted(-6, -3, 6, 3);
        textRect.moveCenter(QPoint(bx, by - 16));
        if (textRect.top() < plotRect.top()) textRect.moveTop(by + 10);

        p.setPen(Qt::NoPen);
        p.setBrush(QColor(40, 40, 50, 220));
        p.drawRoundedRect(textRect, 4, 4);

        p.setPen(QColor(255, 220, 100));
        p.drawText(textRect, Qt::AlignCenter, txt);
    };

    drawBadge(0.5, maxVal, "Max");
    if (std::abs(minVal - maxVal) > 0.5)
    {
        drawBadge(0.0, minVal, "Min");
    }

    // Curseur interactif sous la souris
    if (m_hasHoverCursor)
    {
        p.setPen(QPen(QColor(255, 255, 255, 180), 1, Qt::DashLine));
        p.drawLine(m_hoverPixel.x(), plotRect.top(), m_hoverPixel.x(), plotRect.bottom());

        int curY = valToY(m_hoverVal);
        p.setBrush(QColor(255, 230, 80));
        p.setPen(QPen(QColor(0, 0, 0), 1));
        p.drawEllipse(QPoint(m_hoverPixel.x(), curY), 5, 5);
    }
}

} // namespace TSA::UI
