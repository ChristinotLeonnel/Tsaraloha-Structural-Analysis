#include "NodePropertiesView.h"
#include "../../Model/Model.h"
#include "../../Model/Node.h"

#include <QVBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLineEdit>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QPushButton>
#include <QColorDialog>

namespace TSA::UI
{

NodePropertiesView::NodePropertiesView(TSA::Model::Model* model, QWidget* parent)
    : IElementPropertyView(parent)
    , m_model(model)
{
    setupUi();
}

void NodePropertiesView::setModel(TSA::Model::Model* model)
{
    m_model = model;
    refreshView();
}

void NodePropertiesView::setElementId(int id)
{
    m_nodeId = id;
    refreshView();
}

void NodePropertiesView::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(8);

    auto* grp = new QGroupBox(tr("Propriétés du Nœud"), this);
    auto* form = new QFormLayout(grp);
    form->setContentsMargins(8, 8, 8, 8);
    form->setSpacing(6);

    m_editName = new QLineEdit(grp);
    connect(m_editName, &QLineEdit::editingFinished, this, &NodePropertiesView::onWidgetChanged);
    form->addRow(tr("Nom :"), m_editName);

    m_spinX = new QDoubleSpinBox(grp);
    m_spinX->setRange(-100000.0, 100000.0);
    m_spinX->setDecimals(3);
    m_spinX->setSuffix(" m");
    connect(m_spinX, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &NodePropertiesView::onWidgetChanged);
    form->addRow(tr("Coordonnée X :"), m_spinX);

    m_spinY = new QDoubleSpinBox(grp);
    m_spinY->setRange(-100000.0, 100000.0);
    m_spinY->setDecimals(3);
    m_spinY->setSuffix(" m");
    connect(m_spinY, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &NodePropertiesView::onWidgetChanged);
    form->addRow(tr("Coordonnée Y :"), m_spinY);

    m_spinZ = new QDoubleSpinBox(grp);
    m_spinZ->setRange(-100000.0, 100000.0);
    m_spinZ->setDecimals(3);
    m_spinZ->setSuffix(" m");
    connect(m_spinZ, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &NodePropertiesView::onWidgetChanged);
    form->addRow(tr("Coordonnée Z :"), m_spinZ);

    m_comboSupport = new QComboBox(grp);
    m_comboSupport->addItem(tr("Libre (Aucun appui)"), static_cast<int>(TSA::Model::SupportType::Free));
    m_comboSupport->addItem(tr("Encastrement total"), static_cast<int>(TSA::Model::SupportType::Fixed));
    m_comboSupport->addItem(tr("Articulation (Rotule)"), static_cast<int>(TSA::Model::SupportType::Pinned));
    m_comboSupport->addItem(tr("Appui simple (Rouleau)"), static_cast<int>(TSA::Model::SupportType::Roller));
    connect(m_comboSupport, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &NodePropertiesView::onWidgetChanged);
    form->addRow(tr("Condition d'Appui :"), m_comboSupport);

    m_btnColor = new QPushButton(grp);
    m_btnColor->setFixedHeight(24);
    connect(m_btnColor, &QPushButton::clicked, this, &NodePropertiesView::pickColor);
    form->addRow(tr("Couleur d'affichage :"), m_btnColor);

    mainLayout->addWidget(grp);
    mainLayout->addStretch();
}

void NodePropertiesView::refreshView()
{
    if (!m_model || m_nodeId <= 0)
    {
        setEnabled(false);
        return;
    }

    const auto* node = m_model->getNode(m_nodeId);
    if (!node)
    {
        setEnabled(false);
        return;
    }

    setEnabled(true);
    m_isLoading = true;

    m_editName->setText(QString::fromStdString(node->name()));
    m_spinX->setValue(node->x());
    m_spinY->setValue(node->y());
    m_spinZ->setValue(node->z());

    int idx = m_comboSupport->findData(static_cast<int>(node->supportType()));
    if (idx >= 0) m_comboSupport->setCurrentIndex(idx);

    m_colorHex = QString::fromStdString(node->color().empty() ? "#2563EB" : node->color());
    m_btnColor->setStyleSheet(QString("background-color: %1; border: 1px solid #555; border-radius: 3px;").arg(m_colorHex));

    m_isLoading = false;
}

void NodePropertiesView::pickColor()
{
    QColor c = QColorDialog::getColor(QColor(m_colorHex), this, tr("Couleur du Nœud"));
    if (c.isValid())
    {
        m_colorHex = c.name();
        m_btnColor->setStyleSheet(QString("background-color: %1; border: 1px solid #555; border-radius: 3px;").arg(m_colorHex));
        applyChanges();
    }
}

void NodePropertiesView::onWidgetChanged()
{
    if (m_isLoading) return;
    applyChanges();
}

void NodePropertiesView::applyChanges()
{
    if (!m_model || m_nodeId <= 0) return;
    auto* node = m_model->getNode(m_nodeId);
    if (!node) return;

    m_model->pushUndoState(tr("Modification Nœud %1").arg(m_nodeId).toStdString());

    node->setName(m_editName->text().toStdString());
    node->setCoordinates(m_spinX->value(), m_spinY->value(), m_spinZ->value());
    node->setSupportType(static_cast<TSA::Model::SupportType>(m_comboSupport->currentData().toInt()));
    node->setColor(m_colorHex.toStdString());

    m_model->notifyNodeModified(m_nodeId);
    emit elementModified();
}

} // namespace TSA::UI
