#include "ViewportContainer.h"
#include "ViewportRuler.h"
#include "../../Viewer/OccView.h"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QFrame>

namespace TSA::UI
{

ViewportContainer::ViewportContainer(OccView* occView, QWidget* parent)
    : QWidget(parent)
    , m_occView(occView)
{
    setupUi();

    if (m_occView)
    {
        connect(m_occView, &OccView::mousePixelPositionChanged, this, &ViewportContainer::onMouseMovedInViewport);
        connect(m_occView, &OccView::viewCameraChanged, this, &ViewportContainer::onCameraChanged);
        connect(m_occView, &OccView::mouseCoordinatesChanged, this, &ViewportContainer::onMouseCoordsChanged);
    }
}

void ViewportContainer::setupUi()
{
    auto* grid = new QGridLayout(this);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setSpacing(0);

    m_topBar = new QWidget(this);
    m_topBar->setFixedHeight(28);
    m_topBar->setStyleSheet(
        "QWidget { background: #EEF2F6; border-bottom: 1px solid #CCD3DC; font-family: Segoe UI, sans-serif; font-size: 11px; }"
        "QComboBox { background: #FFFFFF; border: 1px solid #B0BDCC; border-radius: 2px; padding: 1px 6px; font-weight: 600; color: #1E293B; min-width: 180px; }"
        "QComboBox:hover { border-color: #3884D8; }"
        "QPushButton { background: #FFFFFF; border: 1px solid #B0BDCC; border-radius: 2px; padding: 2px 7px; color: #2C3E50; font-weight: bold; max-width: 24px; }"
        "QPushButton:hover { background: #E0EBF8; border-color: #3884D8; }"
        "QPushButton:pressed { background: #C8DCF2; }"
        "QLabel { color: #334455; font-weight: 600; padding: 0 4px; }"
    );

    auto* topLayout = new QHBoxLayout(m_topBar);
    topLayout->setContentsMargins(6, 2, 8, 2);
    topLayout->setSpacing(4);

    m_levelCombo = new QComboBox(m_topBar);
    m_levelCombo->setToolTip(tr("Sélectionner l'étage / niveau de travail actif pour le dessin en hauteur"));
    topLayout->addWidget(m_levelCombo);

    m_btnLevelUp = new QPushButton(tr("▲"), m_topBar);
    m_btnLevelUp->setToolTip(tr("Monter à l'étage supérieur"));
    topLayout->addWidget(m_btnLevelUp);

    m_btnLevelDown = new QPushButton(tr("▼"), m_topBar);
    m_btnLevelDown->setToolTip(tr("Descendre à l'étage inférieur"));
    topLayout->addWidget(m_btnLevelDown);

    topLayout->addSpacing(8);

    auto* lblHint = new QLabel(tr("Plan de dessin actif en hauteur"), m_topBar);
    lblHint->setStyleSheet("color: #64748B; font-weight: normal; font-style: italic;");
    topLayout->addWidget(lblHint);

    topLayout->addStretch();

    connect(m_levelCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ViewportContainer::onLevelComboChanged);
    connect(m_btnLevelUp, &QPushButton::clicked, this, &ViewportContainer::onLevelUp);
    connect(m_btnLevelDown, &QPushButton::clicked, this, &ViewportContainer::onLevelDown);

    m_corner = new CornerWidget(this);
    m_topRuler = new HorizontalRulerWidget(m_occView, this);
    m_leftRuler = new VerticalRulerWidget(m_occView, VerticalRulerWidget::Position::Left, this);
    m_rightRuler = new VerticalRulerWidget(m_occView, VerticalRulerWidget::Position::Right, this);

    auto* topCornerRight = new QWidget(this);
    topCornerRight->setFixedSize(34, 22);
    topCornerRight->setStyleSheet("background: #EEF2F6; border-left: 1px solid #CCD3DC; border-bottom: 1px solid #CCD3DC;");

    // Row 0: Barre supérieure de sélection d'étage
    grid->addWidget(m_topBar, 0, 0, 1, 3);

    // Row 1: Règles supérieures
    grid->addWidget(m_corner, 1, 0);
    grid->addWidget(m_topRuler, 1, 1);
    grid->addWidget(topCornerRight, 1, 2);

    // Row 2: Règles latérales & Viewport 3D
    grid->addWidget(m_leftRuler, 2, 0);
    if (m_occView)
    {
        grid->addWidget(m_occView, 2, 1);
        m_occView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    }
    grid->addWidget(m_rightRuler, 2, 2);

    grid->setRowStretch(2, 1);
    grid->setColumnStretch(1, 1);

    // Row 3: Barre inférieure de statut et plan de travail (Style Robot Structural Analysis)
    m_bottomBar = new QWidget(this);
    m_bottomBar->setFixedHeight(26);
    m_bottomBar->setStyleSheet(
        "QWidget { background: #E6EBF2; border-top: 1px solid #CCD3DC; font-family: Segoe UI, sans-serif; font-size: 11px; }"
        "QPushButton { background: #FFFFFF; border: 1px solid #B0BDCC; border-radius: 2px; padding: 2px 7px; color: #2C3E50; font-weight: 600; }"
        "QPushButton:hover { background: #E0EBF8; border-color: #3884D8; }"
        "QPushButton:pressed { background: #C8DCF2; }"
        "QLabel { color: #334455; padding: 0 4px; }"
    );

    auto* barLayout = new QHBoxLayout(m_bottomBar);
    barLayout->setContentsMargins(6, 2, 8, 2);
    barLayout->setSpacing(8);

    m_btnPlane = new QPushButton(tr("XZ"), m_bottomBar);
    m_btnPlane->setToolTip(tr("Changer le plan de projection actif (XZ, XY, YZ, 3D)"));
    barLayout->addWidget(m_btnPlane);

    m_lblElevation = new QLabel(tr("Y = 0,00 m"), m_bottomBar);
    barLayout->addWidget(m_lblElevation);

    auto* sep1 = new QFrame(m_bottomBar);
    sep1->setFrameShape(QFrame::VLine);
    sep1->setFrameShadow(QFrame::Sunken);
    barLayout->addWidget(sep1);

    m_btnViewFront = new QPushButton(tr("Face (AVANT)"), m_bottomBar);
    m_btnViewFront->setToolTip(tr("Vue de face (Plan XZ)"));
    barLayout->addWidget(m_btnViewFront);

    m_btnViewTop = new QPushButton(tr("Dessus"), m_bottomBar);
    m_btnViewTop->setToolTip(tr("Vue de dessus (Plan XY)"));
    barLayout->addWidget(m_btnViewTop);

    m_btnViewIso = new QPushButton(tr("3D Iso"), m_bottomBar);
    m_btnViewIso->setToolTip(tr("Vue axonométrique 3D"));
    barLayout->addWidget(m_btnViewIso);

    auto* sep2 = new QFrame(m_bottomBar);
    sep2->setFrameShape(QFrame::VLine);
    sep2->setFrameShadow(QFrame::Sunken);
    barLayout->addWidget(sep2);

    barLayout->addStretch();

    m_lblCoords = new QLabel(tr("X: 0,00 m   Y: 0,00 m   Z: 0,00 m"), m_bottomBar);
    m_lblCoords->setStyleSheet("font-family: Consolas, monospace; font-size: 11px; color: #1E2D3D; font-weight: bold;");
    barLayout->addWidget(m_lblCoords);

    grid->addWidget(m_bottomBar, 3, 0, 1, 3);
    grid->setRowStretch(3, 0);

    connect(m_btnViewFront, &QPushButton::clicked, this, &ViewportContainer::onViewFront);
    connect(m_btnViewTop, &QPushButton::clicked, this, &ViewportContainer::onViewTop);
    connect(m_btnViewIso, &QPushButton::clicked, this, &ViewportContainer::onViewIso);
    connect(m_btnPlane, &QPushButton::clicked, this, &ViewportContainer::onWorkplaneClicked);

    if (m_occView)
    {
        connect(m_occView, &OccView::viewPlaneModeChanged, this, [this](OccView::ViewPlaneMode mode) {
            if (mode == OccView::ViewPlaneMode::PlanXY)
            {
                m_btnPlane->setText("XY");
                m_lblElevation->setText(QString("Z = %1 m").arg(m_occView->activeLevelElevation(), 0, 'f', 2));
            }
            else if (mode == OccView::ViewPlaneMode::PlanXZ)
            {
                m_btnPlane->setText("XZ");
                m_lblElevation->setText("Y = 0,00 m");
            }
            else if (mode == OccView::ViewPlaneMode::PlanYZ)
            {
                m_btnPlane->setText("YZ");
                m_lblElevation->setText("X = 0,00 m");
            }
            else
            {
                m_btnPlane->setText("3D");
                m_lblElevation->setText("Vue Globale");
            }
            updateRulers();
        });
    }
}

void ViewportContainer::setRulersVisible(bool visible)
{
    m_rulersVisible = visible;
    m_corner->setVisible(visible);
    m_topRuler->setVisible(visible);
    m_leftRuler->setVisible(visible);
    m_rightRuler->setVisible(visible);
}

void ViewportContainer::updateRulers()
{
    if (m_topRuler) m_topRuler->updateRuler();
    if (m_leftRuler) m_leftRuler->updateRuler();
    if (m_rightRuler) m_rightRuler->updateRuler();
}

void ViewportContainer::onMouseMovedInViewport(int px, int py)
{
    if (m_topRuler)
    {
        m_topRuler->setCursorPos(px);
    }
    if (m_leftRuler)
    {
        m_leftRuler->setCursorPos(py);
    }
    if (m_rightRuler)
    {
        m_rightRuler->setCursorPos(py);
    }
}

void ViewportContainer::onCameraChanged()
{
    updateRulers();
}

void ViewportContainer::onMouseCoordsChanged(double x, double y, double z)
{
    if (m_lblCoords)
    {
        m_lblCoords->setText(QString("X: %1 m   Y: %2 m   Z: %3 m")
            .arg(x, 7, 'f', 2)
            .arg(y, 7, 'f', 2)
            .arg(z, 7, 'f', 2));
    }
}

void ViewportContainer::onViewFront()
{
    if (m_occView)
    {
        m_occView->setViewPlaneMode(OccView::ViewPlaneMode::PlanXZ);
    }
}

void ViewportContainer::onViewTop()
{
    if (m_occView)
    {
        m_occView->setViewPlaneMode(OccView::ViewPlaneMode::PlanXY);
    }
}

void ViewportContainer::onViewIso()
{
    if (m_occView)
    {
        m_occView->setViewPlaneMode(OccView::ViewPlaneMode::Perspective3D);
    }
}

void ViewportContainer::onWorkplaneClicked()
{
    QString current = m_btnPlane->text();
    if (current == "XZ")
    {
        onViewTop();
    }
    else if (current == "XY")
    {
        if (m_occView) m_occView->setViewPlaneMode(OccView::ViewPlaneMode::PlanYZ);
    }
    else if (current == "YZ")
    {
        onViewIso();
    }
    else
    {
        onViewFront();
    }
}

void ViewportContainer::updateLevelsList(const std::vector<double>& elevations, const std::vector<std::string>& names)
{
    if (!m_levelCombo)
        return;

    m_levelCombo->blockSignals(true);
    m_levelCombo->clear();

    for (size_t i = 0; i < elevations.size(); ++i)
    {
        double elev = elevations[i];
        QString name = (i < names.size()) ? QString::fromStdString(names[i]) : tr("Niveau %1").arg(i);
        QString itemText = QString("Z = %1 m = %2")
            .arg(elev, 0, 'f', 2)
            .arg(name);
        m_levelCombo->addItem(itemText, QVariant(elev));
    }

    if (m_levelCombo->count() == 0)
    {
        m_levelCombo->addItem("Z = 0,00 m = Base", QVariant(0.0));
    }

    m_levelCombo->blockSignals(false);
    onLevelComboChanged(m_levelCombo->currentIndex());
}

double ViewportContainer::activeLevelElevation() const
{
    if (!m_levelCombo || m_levelCombo->currentIndex() < 0)
        return 0.0;
    return m_levelCombo->currentData().toDouble();
}

void ViewportContainer::setActiveLevelIndex(int index)
{
    if (m_levelCombo && index >= 0 && index < m_levelCombo->count())
    {
        m_levelCombo->setCurrentIndex(index);
    }
}

void ViewportContainer::onLevelComboChanged(int index)
{
    if (!m_levelCombo || index < 0 || index >= m_levelCombo->count())
        return;

    double elev = m_levelCombo->itemData(index).toDouble();
    QString text = m_levelCombo->itemText(index);

    if (m_lblElevation)
    {
        m_lblElevation->setText(QString("Z = %1 m").arg(elev, 0, 'f', 2));
    }

    if (m_occView)
    {
        m_occView->setActiveLevelElevation(elev);
    }

    updateRulers();
    emit activeLevelChanged(elev, text);
}

void ViewportContainer::onLevelUp()
{
    if (!m_levelCombo)
        return;
    int cur = m_levelCombo->currentIndex();
    if (cur < m_levelCombo->count() - 1)
    {
        m_levelCombo->setCurrentIndex(cur + 1);
    }
}

void ViewportContainer::onLevelDown()
{
    if (!m_levelCombo)
        return;
    int cur = m_levelCombo->currentIndex();
    if (cur > 0)
    {
        m_levelCombo->setCurrentIndex(cur - 1);
    }
}

} // namespace TSA::UI
