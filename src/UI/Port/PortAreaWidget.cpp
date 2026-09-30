#include "PortAreaWidget.h"
#include "../../Viewer/OccView.h"
#include "../../Viewer/ResultsVisualManager.h"
#include "../Ruler/ViewportContainer.h"
#include "../Diagrams/Diagram2DWidget.h"
#include "../../NDC/NDCViewerWidget.h"
#include "../../Model/Model.h"
#include "../../Analysis/ResultsModel.h"

#include <QVBoxLayout>
#include <QHBoxLayout>

namespace TSA::UI
{

PortAreaWidget::PortAreaWidget(OccView* primaryOccView, ViewportContainer* primaryContainer, QWidget* parent)
    : QWidget(parent)
    , m_primaryOccView(primaryOccView)
    , m_primaryContainer(primaryContainer)
{
    m_diagramWidget = new Diagram2DWidget(this);
    m_ndcWidget = new TSA::NDC::NDCViewerWidget(this);

    setupUi();
    setLayoutMode(PortLayout::Single);
}

void PortAreaWidget::setupUi()
{
    m_rootLayout = new QVBoxLayout(this);
    m_rootLayout->setContentsMargins(0, 0, 0, 0);
    m_rootLayout->setSpacing(0);

    // Initialisation des 4 ports
    m_ports[0] = new PortWidget(0, PortType::Model3D, this);
    m_ports[1] = new PortWidget(1, PortType::Diagram2D, this);
    m_ports[2] = new PortWidget(2, PortType::Results3D, this);
    m_ports[3] = new PortWidget(3, PortType::CalculationNote, this);

    // Attribution des contenus par défaut
    m_ports[0]->setContentWidget(m_primaryContainer ? static_cast<QWidget*>(m_primaryContainer) : static_cast<QWidget*>(m_primaryOccView));
    m_ports[1]->setContentWidget(m_diagramWidget);
    m_ports[3]->setContentWidget(m_ndcWidget);

    for (int i = 0; i < 4; ++i)
    {
        connect(m_ports[i], &PortWidget::portTypeChanged, this, &PortAreaWidget::onPortTypeChanged);
        connect(m_ports[i], &PortWidget::maximizeRequested, this, &PortAreaWidget::onPortMaximizeRequested);
        connect(m_ports[i], &PortWidget::portActivated, this, &PortAreaWidget::onPortActivated);
    }

    m_ports[0]->setActive(true);

    if (m_primaryOccView)
    {
        connect(m_primaryOccView, &OccView::viewCameraChanged, this, &PortAreaWidget::onPrimaryCameraChanged);
    }
}

void PortAreaWidget::setModel(TSA::Model::Model* model)
{
    m_model = model;
    if (m_primaryOccView) m_primaryOccView->setModel(model);
    if (m_diagramWidget) m_diagramWidget->setModel(model);
    if (m_ndcWidget) m_ndcWidget->setModel(model);
}

void PortAreaWidget::setResultsModel(const std::shared_ptr<TSA::Analysis::ResultsModel>& results)
{
    m_results = results;
    if (m_primaryOccView) m_primaryOccView->setResultsModel(results);
    if (m_diagramWidget) m_diagramWidget->setResultsModel(results);
    if (m_ndcWidget) m_ndcWidget->setResultsModel(results);
}

PortWidget* PortAreaWidget::port(int index) const
{
    if (index >= 0 && index < 4) return m_ports[index];
    return nullptr;
}

PortWidget* PortAreaWidget::activePort() const
{
    return port(m_activePortIndex);
}

void PortAreaWidget::setActivePort(int index)
{
    if (index < 0 || index >= 4 || m_activePortIndex == index) return;

    for (int i = 0; i < 4; ++i)
    {
        m_ports[i]->setActive(i == index);
    }
    m_activePortIndex = index;
    emit activePortChanged(index, m_ports[index]->portType());
}

void PortAreaWidget::onPortActivated(int portId)
{
    setActivePort(portId);
}

void PortAreaWidget::setLayoutMode(PortLayout mode)
{
    m_layoutMode = mode;
    m_maximizedPortIndex = -1;
    applyLayout();
    emit layoutModeChanged(mode);
}

void PortAreaWidget::toggleMaximizePort(int index)
{
    if (m_maximizedPortIndex == index)
    {
        // Restaurer la disposition multi-ports
        m_maximizedPortIndex = -1;
        for (int i = 0; i < 4; ++i) m_ports[i]->setPortMaximized(false);
    }
    else
    {
        // Agrandir ce port
        m_maximizedPortIndex = index;
        for (int i = 0; i < 4; ++i) m_ports[i]->setPortMaximized(i == index);
    }
    applyLayout();
}

void PortAreaWidget::onPortMaximizeRequested(int portId)
{
    toggleMaximizePort(portId);
}

void PortAreaWidget::applyLayout()
{
    // Détacher d'abord tous les ports de l'ancien conteneur/splitter et les ré-assigner à this
    for (int i = 0; i < 4; ++i)
    {
        if (m_ports[i])
        {
            m_ports[i]->setParent(this);
            m_ports[i]->hide();
        }
    }

    // Nettoyer les conteneurs précédents en toute sécurité
    if (m_containerWidget)
    {
        m_rootLayout->removeWidget(m_containerWidget);
        delete m_containerWidget;
        m_containerWidget = nullptr;
    }

    m_containerWidget = new QWidget(this);
    auto* cLayout = new QVBoxLayout(m_containerWidget);
    cLayout->setContentsMargins(0, 0, 0, 0);
    cLayout->setSpacing(0);

    // Cas où un port est agrandi au maximum
    if (m_maximizedPortIndex >= 0 && m_maximizedPortIndex < 4)
    {
        for (int i = 0; i < 4; ++i)
        {
            if (i == m_maximizedPortIndex)
            {
                cLayout->addWidget(m_ports[i]);
                m_ports[i]->show();
            }
            else
            {
                m_ports[i]->hide();
            }
        }
        m_rootLayout->addWidget(m_containerWidget);
        return;
    }

    switch (m_layoutMode)
    {
    case PortLayout::Single:
    {
        cLayout->addWidget(m_ports[0]);
        m_ports[0]->show();
        for (int i = 1; i < 4; ++i) m_ports[i]->hide();
        break;
    }
    case PortLayout::SplitHorizontal:
    {
        auto* split = new QSplitter(Qt::Horizontal, m_containerWidget);
        split->addWidget(m_ports[0]);
        split->addWidget(m_ports[1]);
        split->setStretchFactor(0, 1);
        split->setStretchFactor(1, 1);
        m_ports[0]->show();
        m_ports[1]->show();
        m_ports[2]->hide();
        m_ports[3]->hide();
        cLayout->addWidget(split);
        break;
    }
    case PortLayout::SplitVertical:
    {
        auto* split = new QSplitter(Qt::Vertical, m_containerWidget);
        split->addWidget(m_ports[0]);
        split->addWidget(m_ports[1]);
        split->setStretchFactor(0, 1);
        split->setStretchFactor(1, 1);
        m_ports[0]->show();
        m_ports[1]->show();
        m_ports[2]->hide();
        m_ports[3]->hide();
        cLayout->addWidget(split);
        break;
    }
    case PortLayout::Grid2x2:
    {
        auto* hSplit = new QSplitter(Qt::Horizontal, m_containerWidget);
        auto* vLeft = new QSplitter(Qt::Vertical, hSplit);
        auto* vRight = new QSplitter(Qt::Vertical, hSplit);

        vLeft->addWidget(m_ports[0]);
        vLeft->addWidget(m_ports[2]);
        vRight->addWidget(m_ports[1]);
        vRight->addWidget(m_ports[3]);

        vLeft->setStretchFactor(0, 1); vLeft->setStretchFactor(1, 1);
        vRight->setStretchFactor(0, 1); vRight->setStretchFactor(1, 1);
        hSplit->addWidget(vLeft);
        hSplit->addWidget(vRight);
        hSplit->setStretchFactor(0, 1); hSplit->setStretchFactor(1, 1);

        for (int i = 0; i < 4; ++i) m_ports[i]->show();
        cLayout->addWidget(hSplit);
        break;
    }
    case PortLayout::Tabbed:
    {
        auto* tab = new QTabWidget(m_containerWidget);
        tab->addTab(m_ports[0], portTypeName(m_ports[0]->portType()));
        tab->addTab(m_ports[1], portTypeName(m_ports[1]->portType()));
        tab->addTab(m_ports[2], portTypeName(m_ports[2]->portType()));
        tab->addTab(m_ports[3], portTypeName(m_ports[3]->portType()));
        for (int i = 0; i < 4; ++i) m_ports[i]->show();
        cLayout->addWidget(tab);
        break;
    }
    }

    m_rootLayout->addWidget(m_containerWidget);
}

void PortAreaWidget::attachContentToPort(PortWidget* targetPort, PortType type)
{
    if (!targetPort) return;

    QWidget* content = nullptr;
    switch (type)
    {
    case PortType::Model3D:
        content = m_primaryContainer ? static_cast<QWidget*>(m_primaryContainer) : static_cast<QWidget*>(m_primaryOccView);
        break;
    case PortType::Results3D:
        if (m_primaryOccView && m_primaryOccView->resultsVisual())
        {
            m_primaryOccView->resultsVisual()->setDeformedVisible(true);
        }
        content = m_primaryContainer ? static_cast<QWidget*>(m_primaryContainer) : static_cast<QWidget*>(m_primaryOccView);
        break;
    case PortType::Diagram2D:
        content = m_diagramWidget;
        break;
    case PortType::CalculationNote:
        if (m_ndcWidget) m_ndcWidget->refreshDocument();
        content = m_ndcWidget;
        break;
    }

    // Si ce contenu est déjà assigné à un autre port, le détacher d'abord
    if (content)
    {
        for (int i = 0; i < 4; ++i)
        {
            if (m_ports[i] && m_ports[i] != targetPort && m_ports[i]->contentWidget() == content)
            {
                m_ports[i]->setContentWidget(nullptr);
            }
        }
    }

    targetPort->setContentWidget(content);
}

void PortAreaWidget::onPortTypeChanged(int portId, PortType newType)
{
    attachContentToPort(m_ports[portId], newType);
}

void PortAreaWidget::setSyncCameras(bool sync)
{
    m_syncCameras = sync;
    emit cameraSyncChanged(sync);
}

void PortAreaWidget::onPrimaryCameraChanged()
{
    if (!m_syncCameras) return;
    // Si d'autres ports 3D sont actifs, répercuter l'orientation
}

} // namespace TSA::UI
