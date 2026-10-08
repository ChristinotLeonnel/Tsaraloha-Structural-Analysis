#include "ProjectStatusOverlay.h"
#include "App/ProductInfo.h"
#include "../../Model/Model.h"
#include "../../Grid/GridManager.h"
#include "../../Analysis/ResultsModel.h"
#include "../../Viewer/OccView.h"
#include "../Theme/ThemeManager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QPainter>
#include <QPainterPath>
#include <QSvgRenderer>
#include <QFileInfo>
#include <cmath>
#include <algorithm>

namespace TSA::UI {

// =============================================================================
// ModelMinimapWidget
// =============================================================================

ModelMinimapWidget::ModelMinimapWidget(OccView* occView, QWidget* parent)
    : QWidget(parent)
    , m_occView(occView)
{
    setFixedHeight(84);
    setMinimumWidth(180);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setAttribute(Qt::WA_TransparentForMouseEvents, true);

    if (m_occView)
    {
        connect(m_occView, &OccView::viewCameraChanged, this, &ModelMinimapWidget::updateView);
    }
}

void ModelMinimapWidget::setModel(const TSA::Model::Model* model)
{
    m_model = model;
    update();
}

void ModelMinimapWidget::updateView()
{
    update();
}

void ModelMinimapWidget::paintEvent(QPaintEvent* /*event*/)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const int w = width();
    const int h = height();

    // Fond subtil minimap
    bool dark = ThemeManager::instance().isDarkMode();
    QColor bgColor = dark ? QColor(16, 21, 28, 200) : QColor(240, 243, 246, 200);
    QColor borderColor = dark ? QColor(48, 54, 61, 220) : QColor(208, 215, 222, 220);

    painter.setBrush(bgColor);
    painter.setPen(QPen(borderColor, 1.0));
    painter.drawRoundedRect(QRectF(1, 1, w - 2, h - 2), 4, 4);

    if (!m_model || m_model->nodes().empty())
    {
        painter.setPen(dark ? QColor(100, 116, 139) : QColor(148, 163, 184));
        painter.setFont(QFont("Segoe UI", 8, QFont::Normal));
        painter.drawText(rect(), Qt::AlignCenter, tr("Aperçu du modèle"));
        return;
    }

    // Calcul de la Bounding Box du modèle réel
    const auto& nodes = m_model->nodes();
    double minX = 1e9, maxX = -1e9;
    double minY = 1e9, maxY = -1e9;
    double minZ = 1e9, maxZ = -1e9;

    for (const auto& [id, n] : nodes)
    {
        minX = std::min(minX, n.x()); maxX = std::max(maxX, n.x());
        minY = std::min(minY, n.y()); maxY = std::max(maxY, n.y());
        minZ = std::min(minZ, n.z()); maxZ = std::max(maxZ, n.z());
    }

    double cx = (minX + maxX) * 0.5;
    double cy = (minY + maxY) * 0.5;
    double cz = (minZ + maxZ) * 0.5;

    // Angle de vue déterminé par la caméra active ou isométrique par défaut
    double azim = 45.0 * 3.141592653589793 / 180.0;
    double elev = 30.0 * 3.141592653589793 / 180.0;

    if (m_occView && !m_occView->view().IsNull() && !m_occView->view()->Camera().IsNull())
    {
        const auto& cam = m_occView->view()->Camera();
        gp_Dir dir = cam->Direction();
        azim = std::atan2(-dir.Y(), -dir.X());
        double horizLen = std::sqrt(dir.X() * dir.X() + dir.Y() * dir.Y());
        elev = std::atan2(-dir.Z(), std::max(1e-4, horizLen));
    }

    double cosA = std::cos(azim);
    double sinA = std::sin(azim);
    double cosE = std::cos(elev);
    double sinE = std::sin(elev);

    auto project = [&](double X, double Y, double Z) -> QPointF {
        double dx = X - cx;
        double dy = Y - cy;
        double dz = Z - cz;

        double xr = -dx * sinA + dy * cosA;
        double yr =  dx * cosA + dy * sinA;
        double zr = dz;

        double u = xr;
        double v = -yr * sinE + zr * cosE;
        return QPointF(u, v);
    };

    // Calcul de l'échelle d'ajustement
    double maxExtent = 0.001;
    for (const auto& [id, n] : nodes)
    {
        QPointF p = project(n.x(), n.y(), n.z());
        maxExtent = std::max(maxExtent, std::max(std::abs(p.x()), std::abs(p.y())));
    }

    double scale = (std::min(w, h) * 0.38) / maxExtent;
    QPointF centerOffset(w * 0.5, h * 0.5);

    auto toScreen = [&](double X, double Y, double Z) -> QPointF {
        QPointF p = project(X, Y, Z);
        return QPointF(centerOffset.x() + p.x() * scale, centerOffset.y() - p.y() * scale);
    };

    // 1. Rendu des dalles (surfaces translucides)
    for (const auto& [id, slab] : m_model->slabs())
    {
        const auto& nids = slab.nodeIds();
        if (nids.size() < 3) continue;

        QPolygonF poly;
        for (int nid : nids)
        {
            const auto* n = m_model->getNode(nid);
            if (n) poly << toScreen(n->x(), n->y(), n->z());
        }
        if (poly.size() >= 3)
        {
            painter.setBrush(dark ? QColor(56, 189, 248, 40) : QColor(14, 165, 233, 40));
            painter.setPen(QPen(dark ? QColor(56, 189, 248, 120) : QColor(14, 165, 233, 120), 0.8));
            painter.drawPolygon(poly);
        }
    }

    // 2. Rendu des poteaux (colonnes verticales)
    painter.setPen(QPen(dark ? QColor(52, 211, 153) : QColor(16, 185, 129), 1.6));
    for (const auto& [id, col] : m_model->columns())
    {
        const auto* n1 = m_model->getNode(col.startNodeId());
        const auto* n2 = m_model->getNode(col.endNodeId());
        if (n1 && n2)
        {
            painter.drawLine(toScreen(n1->x(), n1->y(), n1->z()), toScreen(n2->x(), n2->y(), n2->z()));
        }
    }

    // 3. Rendu des poutres (barres horizontales)
    painter.setPen(QPen(dark ? QColor(96, 165, 250) : QColor(37, 99, 235), 1.4));
    for (const auto& [id, beam] : m_model->beams())
    {
        const auto* n1 = m_model->getNode(beam.startNodeId());
        const auto* n2 = m_model->getNode(beam.endNodeId());
        if (n1 && n2)
        {
            painter.drawLine(toScreen(n1->x(), n1->y(), n1->z()), toScreen(n2->x(), n2->y(), n2->z()));
        }
    }

    // 4. Rendu des nœuds (points subtils)
    painter.setPen(Qt::NoPen);
    painter.setBrush(dark ? QColor(248, 113, 113) : QColor(239, 68, 68));
    for (const auto& [id, n] : nodes)
    {
        QPointF pt = toScreen(n.x(), n.y(), n.z());
        painter.drawEllipse(pt, 1.5, 1.5);
    }
}

// =============================================================================
// ProjectStatusOverlay
// =============================================================================

ProjectStatusOverlay::ProjectStatusOverlay(OccView* occView, QWidget* parent)
    : QWidget(parent)
    , m_occView(occView)
{
    setupUi();
    updateTheme();
}

ProjectStatusOverlay::~ProjectStatusOverlay()
{
    if (m_model)
    {
        m_model->removeObserver(this);
    }
}

void ProjectStatusOverlay::setModel(TSA::Model::Model* model)
{
    if (m_model)
    {
        m_model->removeObserver(this);
    }
    m_model = model;
    if (m_model)
    {
        m_model->addObserver(this);
    }
    if (m_minimap) m_minimap->setModel(model);
    refreshStatus();
}

void ProjectStatusOverlay::setGridManager(const TSA::Grid::GridManager* gridManager)
{
    m_gridManager = gridManager;
    refreshStatus();
}

void ProjectStatusOverlay::setResultsModel(const std::shared_ptr<TSA::Analysis::ResultsModel>& results)
{
    m_resultsModel = results;
    refreshStatus();
}

void ProjectStatusOverlay::setProjectInfo(const QString& projectName, const QString& filePath)
{
    m_projectName = projectName;
    m_filePath = filePath;
    refreshStatus();
}

void ProjectStatusOverlay::setDarkMode(bool dark)
{
    m_isDarkMode = dark;
    updateTheme();
    if (m_minimap) m_minimap->update();
}

void ProjectStatusOverlay::setCollapsed(bool collapsed)
{
    m_isCollapsed = collapsed;
    m_statsContainer->setVisible(!m_isCollapsed);
    m_btnCollapse->setText(m_isCollapsed ? "▲" : "▼");
    m_btnCollapse->setToolTip(m_isCollapsed ? tr("Agrandir le panneau État du projet") : tr("Réduire le panneau"));
    adjustSize();
    emit overlayToggled(!m_isCollapsed);
}

void ProjectStatusOverlay::onToggleCollapse()
{
    setCollapsed(!m_isCollapsed);
}

void ProjectStatusOverlay::paintEvent(QPaintEvent* /*event*/)
{
    // Rendu géré par le stylesheet du cardWidget
}

void ProjectStatusOverlay::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    m_cardWidget = new QWidget(this);
    m_cardWidget->setObjectName("statusCard");
    auto* cardLayout = new QVBoxLayout(m_cardWidget);
    cardLayout->setContentsMargins(10, 8, 10, 10);
    cardLayout->setSpacing(6);

    // En-tête
    auto* headerLayout = new QHBoxLayout();
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(6);

    m_lblTitle = new QLabel(tr("ÉTAT DU PROJET"), m_cardWidget);
    m_lblTitle->setObjectName("statusTitle");

    m_btnCollapse = new QPushButton("▼", m_cardWidget);
    m_btnCollapse->setObjectName("btnCollapse");
    m_btnCollapse->setFixedSize(20, 20);
    m_btnCollapse->setToolTip(tr("Réduire le panneau"));
    connect(m_btnCollapse, &QPushButton::clicked, this, &ProjectStatusOverlay::onToggleCollapse);

    headerLayout->addWidget(m_lblTitle);
    headerLayout->addStretch();
    headerLayout->addWidget(m_btnCollapse);
    cardLayout->addLayout(headerLayout);

    // Conteneur rétractable
    m_statsContainer = new QWidget(m_cardWidget);
    auto* containerLayout = new QVBoxLayout(m_statsContainer);
    containerLayout->setContentsMargins(0, 0, 0, 0);
    containerLayout->setSpacing(6);

    // Nom du projet
    m_lblProjectName = new QLabel(tr("Projet : Sans titre"), m_statsContainer);
    m_lblProjectName->setObjectName("projectName");
    containerLayout->addWidget(m_lblProjectName);

    // Séparateur fin
    auto* sep1 = new QFrame(m_statsContainer);
    sep1->setFrameShape(QFrame::HLine);
    sep1->setObjectName("separator");
    containerLayout->addWidget(sep1);

    // Grille des statistiques
    auto* gridLayout = new QGridLayout();
    gridLayout->setContentsMargins(0, 2, 0, 2);
    gridLayout->setHorizontalSpacing(12);
    gridLayout->setVerticalSpacing(3);

    auto makeRow = [&](int row, const QString& labelText, QLabel*& valLabel) {
        auto* lbl = new QLabel(labelText, m_statsContainer);
        lbl->setObjectName("statLabel");
        valLabel = new QLabel("0", m_statsContainer);
        valLabel->setObjectName("statValue");
        valLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        gridLayout->addWidget(lbl, row, 0);
        gridLayout->addWidget(valLabel, row, 1);
    };

    makeRow(0, tr("Nœuds"), m_lblNodes);
    makeRow(1, tr("Poutres"), m_lblBeams);
    makeRow(2, tr("Poteaux"), m_lblColumns);
    makeRow(3, tr("Dalles"), m_lblSlabs);
    makeRow(4, tr("Voiles"), m_lblWalls);
    makeRow(5, tr("Fondations"), m_lblFoundations);

    // Ligne vide / séparation
    auto* sep2 = new QFrame(m_statsContainer);
    sep2->setFrameShape(QFrame::HLine);
    sep2->setObjectName("separator");
    gridLayout->addWidget(sep2, 6, 0, 1, 2);

    makeRow(7, tr("Niveaux"), m_lblLevels);
    makeRow(8, tr("Grilles"), m_lblGrids);
    makeRow(9, tr("Charges"), m_lblLoads);
    makeRow(10, tr("Combinaisons"), m_lblCombos);

    // Ligne état du calcul
    auto* sep3 = new QFrame(m_statsContainer);
    sep3->setFrameShape(QFrame::HLine);
    sep3->setObjectName("separator");
    gridLayout->addWidget(sep3, 11, 0, 1, 2);

    auto* lblCalcTitle = new QLabel(tr("Calcul"), m_statsContainer);
    lblCalcTitle->setObjectName("statLabel");
    m_lblCalculation = new QLabel(tr("Non calculé"), m_statsContainer);
    m_lblCalculation->setObjectName("calcStatus");
    m_lblCalculation->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    gridLayout->addWidget(lblCalcTitle, 12, 0);
    gridLayout->addWidget(m_lblCalculation, 12, 1);

    containerLayout->addLayout(gridLayout);

    // Minimap vectorielle temps réel
    m_minimap = new ModelMinimapWidget(m_occView, m_statsContainer);
    containerLayout->addWidget(m_minimap);

    cardLayout->addWidget(m_statsContainer);
    mainLayout->addWidget(m_cardWidget);
}

void ProjectStatusOverlay::refreshStatus()
{
    // 1. Nom du projet
    QString pName = m_projectName;
    if (pName.isEmpty() && !m_filePath.isEmpty())
    {
        pName = QFileInfo(m_filePath).fileName();
    }
    if (pName.isEmpty())
    {
        pName = tr("Sans titre");
    }
    m_lblProjectName->setText(QString("<b>%1 :</b> %2").arg(tr("Projet"), pName));

    if (!m_model)
    {
        m_lblNodes->setText("0");
        m_lblBeams->setText("0");
        m_lblColumns->setText("0");
        m_lblSlabs->setText("0");
        m_lblWalls->setText("0");
        m_lblFoundations->setText("0");
        m_lblLevels->setText("0");
        m_lblGrids->setText("0");
        m_lblLoads->setText("0");
        m_lblCombos->setText("0");
        m_lblCalculation->setText(tr("Non calculé"));
        return;
    }

    // 2. Statistiques réelles depuis le modèle unique source de vérité
    m_lblNodes->setText(QString::number(m_model->nodes().size()));
    m_lblBeams->setText(QString::number(m_model->beams().size()));
    m_lblColumns->setText(QString::number(m_model->columns().size()));
    m_lblSlabs->setText(QString::number(m_model->slabs().size()));
    m_lblWalls->setText(QString::number(m_model->walls().size()));
    m_lblFoundations->setText(QString::number(m_model->foundations().size()));

    int numLevels = m_model->levelManager() ? static_cast<int>(m_model->levelManager()->levels().size()) : 0;
    m_lblLevels->setText(QString::number(numLevels));

    int numGrids = m_gridManager ? static_cast<int>(m_gridManager->grids().size()) : 0;
    m_lblGrids->setText(QString::number(numGrids));

    const auto& lm = m_model->loadManager();
    int totalLoads = static_cast<int>(lm.nodalLoads().size() + lm.memberLoads().size());
    m_lblLoads->setText(QString::number(totalLoads));
    m_lblCombos->setText(QString::number(lm.combinations().size()));

    // 3. État du calcul
    if (m_resultsModel && m_resultsModel->hasResults())
    {
        if (m_model->isModified())
        {
            m_lblCalculation->setText(tr("Résultats obsolètes"));
            m_lblCalculation->setStyleSheet("color: #f59e0b; font-weight: bold;");
        }
        else
        {
            m_lblCalculation->setText(tr("Résultats disponibles"));
            m_lblCalculation->setStyleSheet("color: #10b981; font-weight: bold;");
        }
    }
    else
    {
        m_lblCalculation->setText(tr("Non calculé"));
        m_lblCalculation->setStyleSheet(m_isDarkMode ? "color: #94a3b8; font-weight: bold;" : "color: #64748b; font-weight: bold;");
    }

    if (m_minimap)
    {
        m_minimap->setModel(m_model);
        m_minimap->update();
    }
}

void ProjectStatusOverlay::updateTheme()
{
    if (m_isDarkMode)
    {
        m_cardWidget->setStyleSheet(
            "QWidget#statusCard { background: rgba(15, 23, 42, 0.88); border: 1px solid rgba(51, 65, 85, 0.90); border-radius: 6px; }"
            "QLabel#statusTitle { font-family: 'Segoe UI', sans-serif; font-size: 11px; font-weight: 800; color: #38bdf8; letter-spacing: 1px; }"
            "QLabel#projectName { font-family: 'Segoe UI', sans-serif; font-size: 11px; color: #f1f5f9; }"
            "QLabel#statLabel { font-family: 'Segoe UI', sans-serif; font-size: 10px; color: #94a3b8; font-weight: 500; }"
            "QLabel#statValue { font-family: 'Consolas', monospace; font-size: 11px; color: #f8fafc; font-weight: bold; }"
            "QFrame#separator { color: #334155; background: #334155; height: 1px; border: none; }"
            "QPushButton#btnCollapse { background: rgba(30, 41, 59, 0.8); border: 1px solid #475569; border-radius: 3px; color: #cbd5e1; font-size: 9px; }"
            "QPushButton#btnCollapse:hover { background: #334155; color: #ffffff; border-color: #38bdf8; }"
        );
    }
    else
    {
        m_cardWidget->setStyleSheet(
            "QWidget#statusCard { background: rgba(255, 255, 255, 0.92); border: 1px solid rgba(203, 213, 225, 0.95); border-radius: 6px; }"
            "QLabel#statusTitle { font-family: 'Segoe UI', sans-serif; font-size: 11px; font-weight: 800; color: #0284c7; letter-spacing: 1px; }"
            "QLabel#projectName { font-family: 'Segoe UI', sans-serif; font-size: 11px; color: #0f172a; }"
            "QLabel#statLabel { font-family: 'Segoe UI', sans-serif; font-size: 10px; color: #64748b; font-weight: 500; }"
            "QLabel#statValue { font-family: 'Consolas', monospace; font-size: 11px; color: #0f172a; font-weight: bold; }"
            "QFrame#separator { color: #e2e8f0; background: #e2e8f0; height: 1px; border: none; }"
            "QPushButton#btnCollapse { background: #f1f5f9; border: 1px solid #cbd5e1; border-radius: 3px; color: #475569; font-size: 9px; }"
            "QPushButton#btnCollapse:hover { background: #e2e8f0; color: #0f172a; border-color: #0284c7; }"
        );
    }
}

// =============================================================================
// TSALogoOverlay
// =============================================================================

TSALogoOverlay::TSALogoOverlay(QWidget* parent)
    : QWidget(parent)
{
    setFixedSize(125, 22);
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
}

void TSALogoOverlay::setDarkMode(bool dark)
{
    m_isDarkMode = dark;
    update();
}

void TSALogoOverlay::paintEvent(QPaintEvent* /*event*/)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    // Fond badge discret pour barre d'état
    QColor bg = m_isDarkMode ? QColor(30, 41, 59, 140) : QColor(241, 245, 249, 160);
    QColor border = m_isDarkMode ? QColor(51, 65, 85, 160) : QColor(203, 213, 225, 180);

    painter.setBrush(bg);
    painter.setPen(QPen(border, 1.0));
    painter.drawRoundedRect(QRectF(1, 1, width() - 2, height() - 2), 3, 3);

    // Rendu vectoriel du logo officiel du produit depuis resources.qrc
    QString logoPath = QString::fromLatin1(m_isDarkMode ? TSA::Product::kIconSvgOnDark : TSA::Product::kIconSvg);
    QSvgRenderer renderer(logoPath);
    if (renderer.isValid())
    {
        QRectF iconRect(4, 3, 16, 16);
        renderer.render(&painter, iconRect);
    }

    // Libellé officiel du produit
    painter.setPen(m_isDarkMode ? QColor(241, 245, 249, 220) : QColor(15, 23, 42, 220));
    QFont font("Segoe UI", 8, QFont::Bold);
    painter.setFont(font);
    const QString productName = TSA::Product::name();
    const qreal nameWidth = QFontMetricsF(font).horizontalAdvance(productName);
    painter.drawText(QRectF(24, 1, nameWidth + 2, 20), Qt::AlignLeft | Qt::AlignVCenter, productName);
    const qreal badgeX = std::max<qreal>(52, 24 + nameWidth + 6);

    painter.setPen(m_isDarkMode ? QColor(148, 163, 184, 200) : QColor(100, 116, 139, 200));
    QFont subFont("Segoe UI", 7, QFont::Normal);
    painter.setFont(subFont);
    painter.drawText(QRectF(badgeX, 1, 70, 20), Qt::AlignLeft | Qt::AlignVCenter, QString::fromLatin1(TSA::Product::kBadgeVersion));
}

} // namespace TSA::UI
