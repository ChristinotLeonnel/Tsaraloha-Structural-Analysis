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
#include <QMenu>
#include <QPushButton>
#include <QLabel>
#include <QCheckBox>
#include <QCursor>
#include <QAction>

namespace TSA::UI
{

PortAreaWidget::PortAreaWidget(OccView* primaryOccView, ViewportContainer* primaryContainer, QWidget* parent)
    : QWidget(parent)
    , m_primaryOccView(primaryOccView)
    , m_primaryContainer(primaryContainer)
{
    m_diagramWidget = new Diagram2DWidget(this);
    m_diagramWidget2 = new Diagram2DWidget(this);
    m_ndcWidget = new TSA::NDC::NDCViewerWidget(this);

    setupUi();
    setLayoutMode(PortLayout::Single);
}

void PortAreaWidget::setupUi()
{
    m_rootLayout = new QVBoxLayout(this);
    m_rootLayout->setContentsMargins(0, 0, 0, 0);
    m_rootLayout->setSpacing(0);

    // 1. Barre supérieure CAD de sélection des ports et ajout (+)
    m_portBar = new QWidget(this);
    m_portBar->setFixedHeight(28);
    m_portBar->setStyleSheet("background-color: #1e2028; border-bottom: 1px solid #2e323f;");

    auto* barLayout = new QHBoxLayout(m_portBar);
    barLayout->setContentsMargins(8, 2, 8, 2);
    barLayout->setSpacing(6);

    auto* lblPorts = new QLabel(tr("PORTS :"), m_portBar);
    lblPorts->setStyleSheet("font-family: 'Segoe UI', sans-serif; font-size: 10px; font-weight: 800; color: #64748b; letter-spacing: 0.5px;");
    barLayout->addWidget(lblPorts);

    auto makeLayoutBtn = [&](const QString& label, const QString& tooltip, PortLayout layout) -> QPushButton* {
        auto* btn = new QPushButton(label, m_portBar);
        btn->setToolTip(tooltip);
        btn->setCheckable(true);
        btn->setFixedHeight(22);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet(
            "QPushButton { background: transparent; color: #94a3b8; border: 1px solid transparent; border-radius: 3px; font-size: 11px; padding: 1px 8px; }"
            "QPushButton:hover { background: #2d313f; color: #f1f5f9; border-color: #475569; }"
            "QPushButton:checked { background: #2563eb; color: #ffffff; border-color: #3b82f6; font-weight: bold; }"
        );
        connect(btn, &QPushButton::clicked, this, [this, layout]() {
            setLayoutMode(layout);
        });
        return btn;
    };

    m_btnSingle = makeLayoutBtn("🗖 1 Vue", tr("Vue unique plein écran"), PortLayout::Single);
    m_btnSplitH = makeLayoutBtn("◫ 2 H", tr("Double vue horizontale côte à côte"), PortLayout::SplitHorizontal);
    m_btnSplitV = makeLayoutBtn("⬒ 2 V", tr("Double vue verticale superposée"), PortLayout::SplitVertical);
    m_btnGrid2x2 = makeLayoutBtn("⊞ 4 Vues", tr("Grille 4 vues (AutoCAD 2x2)"), PortLayout::Grid2x2);
    m_btnTabbed = makeLayoutBtn("📑 Onglets", tr("Ports sous forme d'onglets"), PortLayout::Tabbed);

    barLayout->addWidget(m_btnSingle);
    barLayout->addWidget(m_btnSplitH);
    barLayout->addWidget(m_btnSplitV);
    barLayout->addWidget(m_btnGrid2x2);
    barLayout->addWidget(m_btnTabbed);

    // Bouton '+' explicite et attractif
    m_btnBarAdd = new QPushButton(tr("➕ Ajouter Port"), m_portBar);
    m_btnBarAdd->setFixedHeight(22);
    m_btnBarAdd->setToolTip(tr("Ajouter un autre port / nouvelle vue dans l'espace de travail (+)"));
    m_btnBarAdd->setCursor(Qt::PointingHandCursor);
    m_btnBarAdd->setStyleSheet(
        "QPushButton { background-color: #2563eb; color: #ffffff; border: none; border-radius: 3px; font-size: 11px; font-weight: bold; padding: 1px 10px; }"
        "QPushButton:hover { background-color: #3b82f6; }"
        "QPushButton:pressed { background-color: #1d4ed8; }"
    );
    connect(m_btnBarAdd, &QPushButton::clicked, this, [this]() {
        onAddPortRequested(m_activePortIndex);
    });
    barLayout->addWidget(m_btnBarAdd);

    barLayout->addStretch();

    m_chkSyncCameras = new QCheckBox(tr("Lier Caméras"), m_portBar);
    m_chkSyncCameras->setStyleSheet("QCheckBox { color: #94a3b8; font-size: 10px; } QCheckBox::indicator { width: 12px; height: 12px; }");
    connect(m_chkSyncCameras, &QCheckBox::toggled, this, &PortAreaWidget::setSyncCameras);
    barLayout->addWidget(m_chkSyncCameras);

    m_rootLayout->addWidget(m_portBar);

    // 2. Initialisation des 4 ports
    m_ports[0] = new PortWidget(0, PortType::Model3D, this);
    m_ports[1] = new PortWidget(1, PortType::Diagram2D, this);
    m_ports[2] = new PortWidget(2, PortType::Diagram2D, this);
    m_ports[3] = new PortWidget(3, PortType::CalculationNote, this);

    // Attribution des contenus par défaut
    m_ports[0]->setContentWidget(m_primaryContainer ? static_cast<QWidget*>(m_primaryContainer) : static_cast<QWidget*>(m_primaryOccView));
    m_ports[1]->setContentWidget(m_diagramWidget);
    m_ports[2]->setContentWidget(m_diagramWidget2);
    m_ports[3]->setContentWidget(m_ndcWidget);

    for (int i = 0; i < 4; ++i)
    {
        connect(m_ports[i], &PortWidget::portTypeChanged, this, &PortAreaWidget::onPortTypeChanged);
        connect(m_ports[i], &PortWidget::maximizeRequested, this, &PortAreaWidget::onPortMaximizeRequested);
        connect(m_ports[i], &PortWidget::portActivated, this, &PortAreaWidget::onPortActivated);
        connect(m_ports[i], &PortWidget::addPortRequested, this, &PortAreaWidget::onAddPortRequested);
        connect(m_ports[i], &PortWidget::closePortRequested, this, &PortAreaWidget::onClosePortRequested);
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
    if (m_diagramWidget2) m_diagramWidget2->setModel(model);
    if (m_ndcWidget) m_ndcWidget->setModel(model);
}

void PortAreaWidget::setResultsModel(const std::shared_ptr<TSA::Analysis::ResultsModel>& results)
{
    m_results = results;
    if (m_primaryOccView) m_primaryOccView->setResultsModel(results);
    if (m_diagramWidget) m_diagramWidget->setResultsModel(results);
    if (m_diagramWidget2) m_diagramWidget2->setResultsModel(results);
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
                m_ports[i]->setCanClose(false);
                m_ports[i]->show();
            }
            else
            {
                m_ports[i]->hide();
            }
        }
        m_rootLayout->addWidget(m_containerWidget);
        updateLayoutButtons();
        return;
    }

    switch (m_layoutMode)
    {
    case PortLayout::Single:
    {
        m_ports[0]->setCanClose(false);
        cLayout->addWidget(m_ports[0]);
        m_ports[0]->show();
        for (int i = 1; i < 4; ++i) m_ports[i]->hide();
        break;
    }
    case PortLayout::SplitHorizontal:
    {
        auto* split = new QSplitter(Qt::Horizontal, m_containerWidget);
        m_ports[0]->setCanClose(true);
        m_ports[1]->setCanClose(true);
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
        m_ports[0]->setCanClose(true);
        m_ports[1]->setCanClose(true);
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

        for (int i = 0; i < 4; ++i) m_ports[i]->setCanClose(true);

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
        tab->setTabsClosable(true);
        tab->setStyleSheet(
            "QTabWidget::pane { border: 1px solid #334155; background: #0f172a; }"
            "QTabBar::tab { background: #1e293b; color: #94a3b8; padding: 5px 12px; border: 1px solid #334155; border-bottom: none; border-top-left-radius: 4px; border-top-right-radius: 4px; font-size: 11px; margin-right: 2px; }"
            "QTabBar::tab:selected { background: #2563eb; color: #ffffff; font-weight: bold; border-color: #3b82f6; }"
            "QTabBar::tab:hover:!selected { background: #334155; color: #f1f5f9; }"
        );

        tab->addTab(m_ports[0], portTypeName(m_ports[0]->portType()));
        tab->addTab(m_ports[1], portTypeName(m_ports[1]->portType()));
        tab->addTab(m_ports[2], portTypeName(m_ports[2]->portType()));
        tab->addTab(m_ports[3], portTypeName(m_ports[3]->portType()));

        auto* btnTabAdd = new QPushButton("+", tab);
        btnTabAdd->setFixedSize(24, 22);
        btnTabAdd->setToolTip(tr("Ajouter un nouvel onglet"));
        btnTabAdd->setCursor(Qt::PointingHandCursor);
        btnTabAdd->setStyleSheet(
            "QPushButton { background-color: #2563eb; color: #ffffff; border: none; border-radius: 3px; font-weight: bold; font-size: 14px; }"
            "QPushButton:hover { background-color: #3b82f6; }"
        );
        connect(btnTabAdd, &QPushButton::clicked, this, [this]() {
            onAddPortRequested(m_activePortIndex);
        });
        tab->setCornerWidget(btnTabAdd, Qt::TopRightCorner);

        connect(tab, &QTabWidget::tabCloseRequested, this, &PortAreaWidget::onClosePortRequested);

        for (int i = 0; i < 4; ++i)
        {
            m_ports[i]->setCanClose(true);
            m_ports[i]->show();
        }
        cLayout->addWidget(tab);
        break;
    }
    }

    m_rootLayout->addWidget(m_containerWidget);
    updateLayoutButtons();
}

void PortAreaWidget::updateLayoutButtons()
{
    if (!m_btnSingle) return;
    m_btnSingle->setChecked(m_layoutMode == PortLayout::Single && m_maximizedPortIndex < 0);
    m_btnSplitH->setChecked(m_layoutMode == PortLayout::SplitHorizontal && m_maximizedPortIndex < 0);
    m_btnSplitV->setChecked(m_layoutMode == PortLayout::SplitVertical && m_maximizedPortIndex < 0);
    m_btnGrid2x2->setChecked(m_layoutMode == PortLayout::Grid2x2 && m_maximizedPortIndex < 0);
    m_btnTabbed->setChecked(m_layoutMode == PortLayout::Tabbed && m_maximizedPortIndex < 0);
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
        content = (targetPort->portId() == 2 && m_diagramWidget2) ? m_diagramWidget2 : m_diagramWidget;
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

void PortAreaWidget::onAddPortRequested(int portId)
{
    QWidget* senderBtn = nullptr;
    if (portId >= 0 && portId < 4 && m_ports[portId])
    {
        senderBtn = m_ports[portId]->addPortButton();
    }
    if (!senderBtn)
    {
        senderBtn = m_btnBarAdd;
    }

    // Le menu doit avoir la fenêtre principale comme parent : PortAreaWidget possède un handle
    // natif (imposé par l'OccView natif) et Qt exige un parent transitoire de niveau supérieur.
    QMenu menu(window());
    menu.setStyleSheet(
        "QMenu { background-color: #1e293b; color: #f8fafc; border: 1px solid #475569; padding: 4px; border-radius: 5px; font-size: 11px; }"
        "QMenu::item { padding: 6px 20px 6px 12px; border-radius: 3px; }"
        "QMenu::item:selected { background-color: #2563eb; color: #ffffff; }"
        "QMenu::separator { height: 1px; background-color: #334155; margin: 4px 6px; }"
    );

    auto* actSplitH = menu.addAction(tr("◫ Ajouter port à droite (Double Horizontale)"));
    auto* actSplitV = menu.addAction(tr("⬒ Ajouter port en bas (Double Verticale)"));
    auto* actGrid4  = menu.addAction(tr("⊞ Grille 4 ports (AutoCAD 2x2)"));
    auto* actTabbed = menu.addAction(tr("📑 Mode Onglets (Multi-Vues)"));
    menu.addSeparator();

    auto* actDiag = menu.addAction(tr("📈 Ouvrir Diagrammes 2D (M, V, N)"));
    auto* actNDC  = menu.addAction(tr("📄 Ouvrir Note de Calcul (NDC)"));
    auto* act3D   = menu.addAction(tr("🏢 Vue 3D Modèle"));

    QPoint pos = senderBtn ? senderBtn->mapToGlobal(QPoint(0, senderBtn->height())) : QCursor::pos();
    QAction* selected = menu.exec(pos);
    if (!selected) return;

    if (selected == actSplitH)
    {
        setLayoutMode(PortLayout::SplitHorizontal);
    }
    else if (selected == actSplitV)
    {
        setLayoutMode(PortLayout::SplitVertical);
    }
    else if (selected == actGrid4)
    {
        setLayoutMode(PortLayout::Grid2x2);
    }
    else if (selected == actTabbed)
    {
        setLayoutMode(PortLayout::Tabbed);
    }
    else if (selected == actDiag)
    {
        if (m_layoutMode == PortLayout::Single)
        {
            setLayoutMode(PortLayout::SplitHorizontal);
        }
        m_ports[1]->setPortType(PortType::Diagram2D);
        setActivePort(1);
    }
    else if (selected == actNDC)
    {
        if (m_layoutMode == PortLayout::Single)
        {
            setLayoutMode(PortLayout::SplitHorizontal);
        }
        m_ports[1]->setPortType(PortType::CalculationNote);
        setActivePort(1);
    }
    else if (selected == act3D)
    {
        if (m_layoutMode == PortLayout::Single)
        {
            setLayoutMode(PortLayout::SplitHorizontal);
        }
        m_ports[0]->setPortType(PortType::Model3D);
        setActivePort(0);
    }
}

void PortAreaWidget::onClosePortRequested(int /*portId*/)
{
    if (m_layoutMode == PortLayout::SplitHorizontal || m_layoutMode == PortLayout::SplitVertical)
    {
        setLayoutMode(PortLayout::Single);
    }
    else if (m_layoutMode == PortLayout::Grid2x2)
    {
        setLayoutMode(PortLayout::SplitHorizontal);
    }
    else if (m_layoutMode == PortLayout::Tabbed)
    {
        setLayoutMode(PortLayout::Single);
    }
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
