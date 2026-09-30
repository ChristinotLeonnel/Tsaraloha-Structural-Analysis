#include "PortWidget.h"
#include <QHBoxLayout>
#include <QMouseEvent>

namespace TSA::UI
{

PortWidget::PortWidget(int portId, PortType initialType, QWidget* parent)
    : QWidget(parent)
    , m_portId(portId)
    , m_portType(initialType)
{
    setupUi();
    updateStyle();
}

void PortWidget::setupUi()
{
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(1, 1, 1, 1);
    m_mainLayout->setSpacing(0);

    // En-tête du Port
    m_header = new QWidget(this);
    m_header->setFixedHeight(28);
    m_header->setStyleSheet("background-color: #23232a; border-bottom: 1px solid #363644;");

    auto* hLayout = new QHBoxLayout(m_header);
    hLayout->setContentsMargins(8, 2, 6, 2);
    hLayout->setSpacing(6);

    m_lblTitle = new QLabel(QString("Port %1").arg(m_portId + 1), m_header);
    m_lblTitle->setStyleSheet("font-weight: 600; color: #cbd5e1; font-size: 11px;");

    m_typeCombo = new QComboBox(m_header);
    m_typeCombo->addItem(portTypeName(PortType::Model3D), static_cast<int>(PortType::Model3D));
    m_typeCombo->addItem(portTypeName(PortType::Results3D), static_cast<int>(PortType::Results3D));
    m_typeCombo->addItem(portTypeName(PortType::Diagram2D), static_cast<int>(PortType::Diagram2D));
    m_typeCombo->addItem(portTypeName(PortType::CalculationNote), static_cast<int>(PortType::CalculationNote));
    m_typeCombo->setStyleSheet("QComboBox { background-color: #2d2d38; color: #f1f5f9; border: 1px solid #47475a; border-radius: 3px; font-size: 11px; padding: 1px 6px; }");

    int idx = m_typeCombo->findData(static_cast<int>(m_portType));
    if (idx >= 0) m_typeCombo->setCurrentIndex(idx);

    m_btnMaximize = new QPushButton("🗖", m_header);
    m_btnMaximize->setFixedSize(22, 20);
    m_btnMaximize->setToolTip(tr("Plein écran / Restaurer le port"));
    m_btnMaximize->setStyleSheet("QPushButton { background-color: transparent; color: #94a3b8; border: none; font-size: 12px; } QPushButton:hover { color: #f8fafc; background-color: #3b3b4a; }");

    hLayout->addWidget(m_lblTitle);
    hLayout->addWidget(m_typeCombo);
    hLayout->addStretch();
    hLayout->addWidget(m_btnMaximize);

    m_mainLayout->addWidget(m_header);

    connect(m_typeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &PortWidget::onTypeComboChanged);
    connect(m_btnMaximize, &QPushButton::clicked, this, &PortWidget::onMaximizeClicked);
}

void PortWidget::setContentWidget(QWidget* widget)
{
    if (m_contentWidget)
    {
        m_mainLayout->removeWidget(m_contentWidget);
        m_contentWidget->setParent(nullptr);
    }

    m_contentWidget = widget;
    if (m_contentWidget)
    {
        m_mainLayout->addWidget(m_contentWidget, 1);
        m_contentWidget->show();
    }
}

void PortWidget::setPortType(PortType type)
{
    if (m_portType == type) return;
    m_portType = type;
    int idx = m_typeCombo->findData(static_cast<int>(type));
    if (idx >= 0)
    {
        m_typeCombo->blockSignals(true);
        m_typeCombo->setCurrentIndex(idx);
        m_typeCombo->blockSignals(false);
    }
    emit portTypeChanged(m_portId, type);
}

void PortWidget::onTypeComboChanged(int index)
{
    auto type = static_cast<PortType>(m_typeCombo->itemData(index).toInt());
    m_portType = type;
    emit portTypeChanged(m_portId, type);
}

void PortWidget::onMaximizeClicked()
{
    emit maximizeRequested(m_portId);
}

void PortWidget::setPortMaximized(bool max)
{
    m_isMaximized = max;
    m_btnMaximize->setText(max ? "🗗" : "🗖");
    m_btnMaximize->setToolTip(max ? tr("Restaurer la disposition multi-ports") : tr("Agrandir ce port en plein écran"));
}

void PortWidget::setActive(bool active)
{
    if (m_isActive == active) return;
    m_isActive = active;
    updateStyle();
}

void PortWidget::updateStyle()
{
    if (m_isActive)
    {
        setStyleSheet("PortWidget { border: 1.5px solid #2563eb; background-color: #1a1a20; }");
        m_lblTitle->setStyleSheet("font-weight: 700; color: #60a5fa; font-size: 11px;");
    }
    else
    {
        setStyleSheet("PortWidget { border: 1px solid #32323f; background-color: #1a1a20; }");
        m_lblTitle->setStyleSheet("font-weight: 500; color: #94a3b8; font-size: 11px;");
    }
}

void PortWidget::mousePressEvent(QMouseEvent* event)
{
    emit portActivated(m_portId);
    QWidget::mousePressEvent(event);
}

} // namespace TSA::UI
