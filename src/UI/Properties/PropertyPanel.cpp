#include "PropertyPanel.h"

#include <QVBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QDoubleSpinBox>
#include <QSpinBox>

namespace TSA::UI
{

PropertyPanel::PropertyPanel(TSA::Model::Model* model, QWidget* parent)
    : QWidget(parent)
    , m_model(model)
{
    setupUi();
    clearProperties();
}

void PropertyPanel::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(12);

    m_titleLabel = new QLabel(tr("PROPERTIES"), this);
    QFont titleFont = m_titleLabel->font();
    titleFont.setBold(true);
    titleFont.setPointSize(titleFont.pointSize() + 1);
    m_titleLabel->setFont(titleFont);
    mainLayout->addWidget(m_titleLabel);

    m_emptyLabel = new QLabel(tr("No element selected"), this);
    m_emptyLabel->setAlignment(Qt::AlignCenter);
    m_emptyLabel->setStyleSheet("color: #888888; font-style: italic; padding: 20px;");
    mainLayout->addWidget(m_emptyLabel);

    // -------------------------------------------------------------
    // Groupe Nœud
    // -------------------------------------------------------------
    m_nodeGroup = new QGroupBox(tr("Node Information"), this);
    auto* nodeForm = new QFormLayout(m_nodeGroup);
    nodeForm->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    m_nodeIdLabel = new QLabel(m_nodeGroup);
    m_nodeXSpin = new QDoubleSpinBox(m_nodeGroup);
    m_nodeYSpin = new QDoubleSpinBox(m_nodeGroup);
    m_nodeZSpin = new QDoubleSpinBox(m_nodeGroup);

    for (auto* spin : { m_nodeXSpin, m_nodeYSpin, m_nodeZSpin })
    {
        spin->setRange(-10000.0, 10000.0);
        spin->setDecimals(3);
        spin->setSingleStep(0.5);
        spin->setSuffix(" m");
        spin->setKeyboardTracking(true);
    }

    nodeForm->addRow(tr("ID:"), m_nodeIdLabel);
    nodeForm->addRow(tr("X:"), m_nodeXSpin);
    nodeForm->addRow(tr("Y:"), m_nodeYSpin);
    nodeForm->addRow(tr("Z:"), m_nodeZSpin);

    mainLayout->addWidget(m_nodeGroup);

    connect(m_nodeXSpin, &QDoubleSpinBox::valueChanged, this, &PropertyPanel::onNodeCoordinatesChanged);
    connect(m_nodeYSpin, &QDoubleSpinBox::valueChanged, this, &PropertyPanel::onNodeCoordinatesChanged);
    connect(m_nodeZSpin, &QDoubleSpinBox::valueChanged, this, &PropertyPanel::onNodeCoordinatesChanged);

    // -------------------------------------------------------------
    // Groupe Poutre
    // -------------------------------------------------------------
    m_beamGroup = new QGroupBox(tr("Beam Information"), this);
    auto* beamForm = new QFormLayout(m_beamGroup);
    beamForm->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    m_beamIdLabel = new QLabel(m_beamGroup);
    m_beamStartNodeLabel = new QLabel(m_beamGroup);
    m_beamEndNodeLabel = new QLabel(m_beamGroup);
    m_beamLengthLabel = new QLabel(m_beamGroup);

    m_beamWidthSpin = new QDoubleSpinBox(m_beamGroup);
    m_beamWidthSpin->setRange(0.01, 10.0);
    m_beamWidthSpin->setDecimals(2);
    m_beamWidthSpin->setSingleStep(0.05);
    m_beamWidthSpin->setSuffix(" m");

    m_beamHeightSpin = new QDoubleSpinBox(m_beamGroup);
    m_beamHeightSpin->setRange(0.01, 10.0);
    m_beamHeightSpin->setDecimals(2);
    m_beamHeightSpin->setSingleStep(0.05);
    m_beamHeightSpin->setSuffix(" m");

    beamForm->addRow(tr("ID:"), m_beamIdLabel);
    beamForm->addRow(tr("Start Node:"), m_beamStartNodeLabel);
    beamForm->addRow(tr("End Node:"), m_beamEndNodeLabel);
    beamForm->addRow(tr("Length:"), m_beamLengthLabel);
    beamForm->addRow(tr("Width (b):"), m_beamWidthSpin);
    beamForm->addRow(tr("Height (h):"), m_beamHeightSpin);

    mainLayout->addWidget(m_beamGroup);

    connect(m_beamWidthSpin, &QDoubleSpinBox::valueChanged, this, &PropertyPanel::onBeamDimensionsChanged);
    connect(m_beamHeightSpin, &QDoubleSpinBox::valueChanged, this, &PropertyPanel::onBeamDimensionsChanged);

    // -------------------------------------------------------------
    // Groupe Poteau
    // -------------------------------------------------------------
    m_columnGroup = new QGroupBox(tr("Column Information"), this);
    auto* columnForm = new QFormLayout(m_columnGroup);
    columnForm->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    m_columnIdLabel = new QLabel(m_columnGroup);
    m_columnStartNodeLabel = new QLabel(m_columnGroup);
    m_columnEndNodeLabel = new QLabel(m_columnGroup);
    m_columnHeightLabel = new QLabel(m_columnGroup);

    m_columnWidthSpin = new QDoubleSpinBox(m_columnGroup);
    m_columnWidthSpin->setRange(0.01, 10.0);
    m_columnWidthSpin->setDecimals(2);
    m_columnWidthSpin->setSingleStep(0.05);
    m_columnWidthSpin->setSuffix(" m");

    m_columnDepthSpin = new QDoubleSpinBox(m_columnGroup);
    m_columnDepthSpin->setRange(0.01, 10.0);
    m_columnDepthSpin->setDecimals(2);
    m_columnDepthSpin->setSingleStep(0.05);
    m_columnDepthSpin->setSuffix(" m");

    columnForm->addRow(tr("ID:"), m_columnIdLabel);
    columnForm->addRow(tr("Bottom Node:"), m_columnStartNodeLabel);
    columnForm->addRow(tr("Top Node:"), m_columnEndNodeLabel);
    columnForm->addRow(tr("Height (H):"), m_columnHeightLabel);
    columnForm->addRow(tr("Width (b):"), m_columnWidthSpin);
    columnForm->addRow(tr("Depth (h):"), m_columnDepthSpin);

    mainLayout->addWidget(m_columnGroup);

    connect(m_columnWidthSpin, &QDoubleSpinBox::valueChanged, this, &PropertyPanel::onColumnDimensionsChanged);
    connect(m_columnDepthSpin, &QDoubleSpinBox::valueChanged, this, &PropertyPanel::onColumnDimensionsChanged);

    // -------------------------------------------------------------
    // Groupe Dalle
    // -------------------------------------------------------------
    m_slabGroup = new QGroupBox(tr("Slab Information"), this);
    auto* slabForm = new QFormLayout(m_slabGroup);
    slabForm->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    m_slabIdLabel = new QLabel(m_slabGroup);
    m_slabNodesLabel = new QLabel(m_slabGroup);
    m_slabAreaLabel = new QLabel(m_slabGroup);

    m_slabThicknessSpin = new QDoubleSpinBox(m_slabGroup);
    m_slabThicknessSpin->setRange(0.01, 5.0);
    m_slabThicknessSpin->setDecimals(2);
    m_slabThicknessSpin->setSingleStep(0.02);
    m_slabThicknessSpin->setSuffix(" m");

    slabForm->addRow(tr("ID:"), m_slabIdLabel);
    slabForm->addRow(tr("Nodes:"), m_slabNodesLabel);
    slabForm->addRow(tr("Surface Area:"), m_slabAreaLabel);
    slabForm->addRow(tr("Thickness (e):"), m_slabThicknessSpin);

    mainLayout->addWidget(m_slabGroup);

    connect(m_slabThicknessSpin, &QDoubleSpinBox::valueChanged, this, &PropertyPanel::onSlabPropertiesChanged);

    mainLayout->addStretch();
}

void PropertyPanel::clearProperties()
{
    m_currentNodeId = -1;
    m_currentBeamId = -1;
    m_currentColumnId = -1;
    m_currentSlabId = -1;

    m_emptyLabel->show();
    m_nodeGroup->hide();
    m_beamGroup->hide();
    m_columnGroup->hide();
    m_slabGroup->hide();
    m_titleLabel->setText(tr("PROPERTIES"));
}

void PropertyPanel::showNodeProperties(int nodeId)
{
    if (!m_model)
        return;

    const auto* node = m_model->getNode(nodeId);
    if (!node)
    {
        clearProperties();
        return;
    }

    m_isUpdating = true;
    m_currentNodeId = nodeId;
    m_currentBeamId = -1;
    m_currentColumnId = -1;
    m_currentSlabId = -1;

    m_titleLabel->setText(QString("NODE %1").arg(nodeId));
    m_nodeIdLabel->setText(QString::number(nodeId));
    m_nodeXSpin->setValue(node->x());
    m_nodeYSpin->setValue(node->y());
    m_nodeZSpin->setValue(node->z());

    m_emptyLabel->hide();
    m_beamGroup->hide();
    m_columnGroup->hide();
    m_slabGroup->hide();
    m_nodeGroup->show();

    m_isUpdating = false;
}

void PropertyPanel::showBeamProperties(int beamId)
{
    if (!m_model)
        return;

    const auto* beam = m_model->getBeam(beamId);
    if (!beam)
    {
        clearProperties();
        return;
    }

    m_isUpdating = true;
    m_currentBeamId = beamId;
    m_currentNodeId = -1;
    m_currentColumnId = -1;
    m_currentSlabId = -1;

    m_titleLabel->setText(QString("BEAM %1").arg(beamId));
    m_beamIdLabel->setText(QString::number(beamId));
    m_beamStartNodeLabel->setText(QString("Node %1").arg(beam->startNodeId()));
    m_beamEndNodeLabel->setText(QString("Node %1").arg(beam->endNodeId()));
    m_beamLengthLabel->setText(QString("%1 m").arg(beam->length(*m_model), 0, 'f', 3));
    m_beamWidthSpin->setValue(beam->width());
    m_beamHeightSpin->setValue(beam->height());

    m_emptyLabel->hide();
    m_nodeGroup->hide();
    m_columnGroup->hide();
    m_slabGroup->hide();
    m_beamGroup->show();

    m_isUpdating = false;
}

void PropertyPanel::showColumnProperties(int columnId)
{
    if (!m_model)
        return;

    const auto* col = m_model->getColumn(columnId);
    if (!col)
    {
        clearProperties();
        return;
    }

    m_isUpdating = true;
    m_currentColumnId = columnId;
    m_currentNodeId = -1;
    m_currentBeamId = -1;
    m_currentSlabId = -1;

    m_titleLabel->setText(QString("COLUMN %1").arg(columnId));
    m_columnIdLabel->setText(QString::number(columnId));
    m_columnStartNodeLabel->setText(QString("Node %1").arg(col->startNodeId()));
    m_columnEndNodeLabel->setText(QString("Node %1").arg(col->endNodeId()));
    m_columnHeightLabel->setText(QString("%1 m").arg(col->length(*m_model), 0, 'f', 3));
    m_columnWidthSpin->setValue(col->width());
    m_columnDepthSpin->setValue(col->height());

    m_emptyLabel->hide();
    m_nodeGroup->hide();
    m_beamGroup->hide();
    m_slabGroup->hide();
    m_columnGroup->show();

    m_isUpdating = false;
}

void PropertyPanel::showSlabProperties(int slabId)
{
    if (!m_model)
        return;

    const auto* slab = m_model->getSlab(slabId);
    if (!slab)
    {
        clearProperties();
        return;
    }

    m_isUpdating = true;
    m_currentSlabId = slabId;
    m_currentNodeId = -1;
    m_currentBeamId = -1;
    m_currentColumnId = -1;

    QString nodesStr;
    for (size_t i = 0; i < slab->nodeIds().size(); ++i)
    {
        if (i > 0) nodesStr += ", ";
        nodesStr += QString::number(slab->nodeIds()[i]);
    }

    m_titleLabel->setText(QString("SLAB %1").arg(slabId));
    m_slabIdLabel->setText(QString::number(slabId));
    m_slabNodesLabel->setText(nodesStr);
    m_slabAreaLabel->setText(QString("%1 m²").arg(slab->area(*m_model), 0, 'f', 3));
    m_slabThicknessSpin->setValue(slab->thickness());

    m_emptyLabel->hide();
    m_nodeGroup->hide();
    m_beamGroup->hide();
    m_columnGroup->hide();
    m_slabGroup->show();

    m_isUpdating = false;
}

void PropertyPanel::onNodeCoordinatesChanged()
{
    if (m_isUpdating || !m_model || m_currentNodeId <= 0)
        return;

    auto* node = m_model->getNode(m_currentNodeId);
    if (node)
    {
        node->setCoordinates(m_nodeXSpin->value(), m_nodeYSpin->value(), m_nodeZSpin->value());
        m_model->notifyNodeModified(m_currentNodeId);
    }
}

void PropertyPanel::onBeamDimensionsChanged()
{
    if (m_isUpdating || !m_model || m_currentBeamId <= 0)
        return;

    auto* beam = m_model->getBeam(m_currentBeamId);
    if (beam)
    {
        beam->setDimensions(m_beamWidthSpin->value(), m_beamHeightSpin->value());
        m_model->notifyBeamModified(m_currentBeamId);
    }
}

void PropertyPanel::onColumnDimensionsChanged()
{
    if (m_isUpdating || !m_model || m_currentColumnId <= 0)
        return;

    auto* col = m_model->getColumn(m_currentColumnId);
    if (col)
    {
        col->setDimensions(m_columnWidthSpin->value(), m_columnDepthSpin->value());
        m_model->notifyColumnModified(m_currentColumnId);
    }
}

void PropertyPanel::onSlabPropertiesChanged()
{
    if (m_isUpdating || !m_model || m_currentSlabId <= 0)
        return;

    auto* slab = m_model->getSlab(m_currentSlabId);
    if (slab)
    {
        slab->setThickness(m_slabThicknessSpin->value());
        m_model->notifySlabModified(m_currentSlabId);
    }
}

} // namespace TSA::UI
