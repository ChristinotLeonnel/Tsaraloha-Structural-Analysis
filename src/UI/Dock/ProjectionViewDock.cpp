#include "ProjectionViewDock.h"
#include "../../Model/Model.h"
#include "../../Coordinate/WorkPlaneManager.h"
#include "../../Coordinate/AxisColorConfig.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QRadioButton>
#include <QComboBox>
#include <QCheckBox>
#include <QSlider>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QLabel>
#include <QButtonGroup>
#include <QScrollArea>

namespace TSA::UI
{

ProjectionViewDock::ProjectionViewDock(QWidget* parent)
    : QDockWidget(tr("PROJECTION & VUE"), parent)
{
    setObjectName("ProjectionViewDock");
    setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    setupUi();
}

void ProjectionViewDock::setupUi()
{
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    auto* container = new QWidget(scroll);
    auto* mainLayout = new QVBoxLayout(container);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(10);

    // =========================================================================
    // 1. SECTION VUE (VIEW)
    // =========================================================================
    auto* grpView = new QGroupBox(tr("VUE DE CAMÉRA (VIEW)"), container);
    auto* viewLayout = new QVBoxLayout(grpView);
    viewLayout->setSpacing(6);

    m_viewGroup = new QButtonGroup(this);

    auto* viewGrid = new QGridLayout();
    viewGrid->setSpacing(4);

    m_rbView3D = new QRadioButton(tr("3D (Axo)"), grpView);
    m_rbViewFront = new QRadioButton(tr("Face (Front)"), grpView);
    m_rbViewBack = new QRadioButton(tr("Arrière (Back)"), grpView);
    m_rbViewLeft = new QRadioButton(tr("Gauche (Left)"), grpView);
    m_rbViewRight = new QRadioButton(tr("Droite (Right)"), grpView);
    m_rbViewTop = new QRadioButton(tr("Dessus (Top)"), grpView);
    m_rbViewBottom = new QRadioButton(tr("Dessous (Bottom)"), grpView);
    m_rbViewNormal = new QRadioButton(tr("Face au Plan (Normale)"), grpView);
    m_rbViewOpposite = new QRadioButton(tr("Dos au Plan (-Normale)"), grpView);

    m_rbView3D->setChecked(true);

    m_viewGroup->addButton(m_rbView3D, static_cast<int>(TSA::Viewer::StandardCameraView::ThreeD_Axo));
    m_viewGroup->addButton(m_rbViewFront, static_cast<int>(TSA::Viewer::StandardCameraView::Front));
    m_viewGroup->addButton(m_rbViewBack, static_cast<int>(TSA::Viewer::StandardCameraView::Back));
    m_viewGroup->addButton(m_rbViewLeft, static_cast<int>(TSA::Viewer::StandardCameraView::Left));
    m_viewGroup->addButton(m_rbViewRight, static_cast<int>(TSA::Viewer::StandardCameraView::Right));
    m_viewGroup->addButton(m_rbViewTop, static_cast<int>(TSA::Viewer::StandardCameraView::Top));
    m_viewGroup->addButton(m_rbViewBottom, static_cast<int>(TSA::Viewer::StandardCameraView::Bottom));
    m_viewGroup->addButton(m_rbViewNormal, static_cast<int>(TSA::Viewer::StandardCameraView::NormalToPlane));
    m_viewGroup->addButton(m_rbViewOpposite, static_cast<int>(TSA::Viewer::StandardCameraView::OppositePlane));

    viewGrid->addWidget(m_rbView3D, 0, 0, 1, 2);
    viewGrid->addWidget(m_rbViewFront, 1, 0);
    viewGrid->addWidget(m_rbViewBack, 1, 1);
    viewGrid->addWidget(m_rbViewLeft, 2, 0);
    viewGrid->addWidget(m_rbViewRight, 2, 1);
    viewGrid->addWidget(m_rbViewTop, 3, 0);
    viewGrid->addWidget(m_rbViewBottom, 3, 1);
    viewGrid->addWidget(m_rbViewNormal, 4, 0, 1, 2);
    viewGrid->addWidget(m_rbViewOpposite, 5, 0, 1, 2);

    viewLayout->addLayout(viewGrid);

    m_chkOrtho = new QCheckBox(tr("Projection Caméra Orthographique"), grpView);
    viewLayout->addWidget(m_chkOrtho);

    mainLayout->addWidget(grpView);

    // =========================================================================
    // 2. SECTION PROJECTION (PROJECTION)
    // =========================================================================
    auto* grpProj = new QGroupBox(tr("PROJECTION DE TRAVAIL"), container);
    auto* projLayout = new QVBoxLayout(grpProj);
    projLayout->setSpacing(6);

    m_projGroup = new QButtonGroup(this);
    m_rbProj3D = new QRadioButton(tr("Projection 3D (Espace 3D libre)"), grpProj);
    m_rbProj2D = new QRadioButton(tr("Projection 2D (Verrouillée sur Xwp / Ywp)"), grpProj);
    m_rbProj3D->setChecked(true);

    m_projGroup->addButton(m_rbProj3D, static_cast<int>(TSA::Viewer::ProjectionMode::ThreeD));
    m_projGroup->addButton(m_rbProj2D, static_cast<int>(TSA::Viewer::ProjectionMode::TwoD));

    projLayout->addWidget(m_rbProj3D);
    projLayout->addWidget(m_rbProj2D);

    auto* dirRow = new QHBoxLayout();
    dirRow->addWidget(new QLabel(tr("Direction :"), grpProj));
    m_comboDirection = new QComboBox(grpProj);
    m_comboDirection->addItem(tr("Normale au plan (+Zwp)"), static_cast<int>(TSA::Viewer::ProjectionDirection::Normal));
    m_comboDirection->addItem(tr("Opposée (-Zwp)"), static_cast<int>(TSA::Viewer::ProjectionDirection::OppositeNormal));
    m_comboDirection->addItem(tr("Rayon caméra"), static_cast<int>(TSA::Viewer::ProjectionDirection::CameraRay));
    m_comboDirection->addItem(tr("Personnalisée"), static_cast<int>(TSA::Viewer::ProjectionDirection::Custom));
    dirRow->addWidget(m_comboDirection, 1);
    projLayout->addLayout(dirRow);

    mainLayout->addWidget(grpProj);

    // =========================================================================
    // 3. SECTION WORKPLANE (PLAN DE TRAVAIL)
    // =========================================================================
    auto* grpWp = new QGroupBox(tr("PLAN DE TRAVAIL ACTIF (WORKPLANE)"), container);
    auto* wpLayout = new QVBoxLayout(grpWp);
    wpLayout->setSpacing(6);

    auto* wpRow = new QHBoxLayout();
    wpRow->addWidget(new QLabel(tr("Plan :"), grpWp));
    m_comboWorkPlanes = new QComboBox(grpWp);
    wpRow->addWidget(m_comboWorkPlanes, 1);

    m_btnNewPlane = new QPushButton(tr("+ Nouveau"), grpWp);
    m_btnNewPlane->setMaximumWidth(80);
    wpRow->addWidget(m_btnNewPlane);
    wpLayout->addLayout(wpRow);

    m_lblPlaneStatus = new QLabel(tr("Plan de projection : <b>WorkPlane Local XY (Xwp, Ywp)</b>"), grpWp);
    m_lblPlaneStatus->setWordWrap(true);
    wpLayout->addWidget(m_lblPlaneStatus);

    m_lblOriginInfo = new QLabel(tr("Origine : (0.00, 0.00, 0.00) m"), grpWp);
    m_lblOriginInfo->setStyleSheet("font-family: monospace; font-size: 11px;");
    wpLayout->addWidget(m_lblOriginInfo);

    m_lblAxesInfo = new QLabel(tr("Axes : Xwp (1,0,0)  Ywp (0,1,0)  Zwp (0,0,1)"), grpWp);
    m_lblAxesInfo->setStyleSheet("font-family: monospace; font-size: 11px;");
    wpLayout->addWidget(m_lblAxesInfo);

    m_chkPlaneVisible = new QCheckBox(tr("Afficher la surface du plan"), grpWp);
    m_chkPlaneVisible->setChecked(true);
    wpLayout->addWidget(m_chkPlaneVisible);

    m_chkAxesVisible = new QCheckBox(tr("Afficher le repère local (Xwp, Ywp, Zwp)"), grpWp);
    m_chkAxesVisible->setChecked(true);
    wpLayout->addWidget(m_chkAxesVisible);

    m_chkGridVisible = new QCheckBox(tr("Afficher la grille locale"), grpWp);
    m_chkGridVisible->setChecked(true);
    wpLayout->addWidget(m_chkGridVisible);

    m_btnAlignView = new QPushButton(tr("👁 Aligner la caméra face au plan"), grpWp);
    wpLayout->addWidget(m_btnAlignView);

    mainLayout->addWidget(grpWp);

    // =========================================================================
    // 4. SECTION GIZMO 3D (GIZMO SIZE)
    // =========================================================================
    auto* grpGizmo = new QGroupBox(tr("GIZMO & MANIPULATION 3D"), container);
    auto* gizmoLayout = new QVBoxLayout(grpGizmo);
    gizmoLayout->setSpacing(6);

    m_chkGizmoActive = new QCheckBox(tr("Activer le Gizmo 3D interactif"), grpGizmo);
    gizmoLayout->addWidget(m_chkGizmoActive);

    auto* sizeRow = new QHBoxLayout();
    sizeRow->addWidget(new QLabel(tr("Taille Gizmo :"), grpGizmo));

    m_sliderGizmoSize = new QSlider(Qt::Horizontal, grpGizmo);
    m_sliderGizmoSize->setRange(40, 300);
    m_sliderGizmoSize->setValue(100);
    sizeRow->addWidget(m_sliderGizmoSize, 1);

    m_spinGizmoSize = new QDoubleSpinBox(grpGizmo);
    m_spinGizmoSize->setRange(40.0, 300.0);
    m_spinGizmoSize->setValue(100.0);
    m_spinGizmoSize->setSuffix(" px");
    sizeRow->addWidget(m_spinGizmoSize);

    gizmoLayout->addLayout(sizeRow);
    mainLayout->addWidget(grpGizmo);

    mainLayout->addStretch(1);

    scroll->setWidget(container);
    setWidget(scroll);

    // --- Connecteurs signaux / slots ---
    connect(m_viewGroup, &QButtonGroup::idClicked, this, [this](int id) {
        emit standardViewRequested(static_cast<TSA::Viewer::StandardCameraView>(id));
    });

    connect(m_chkOrtho, &QCheckBox::toggled, this, &ProjectionViewDock::cameraOrthographicChanged);

    connect(m_projGroup, &QButtonGroup::idClicked, this, [this](int id) {
        emit projectionModeRequested(static_cast<TSA::Viewer::ProjectionMode>(id));
    });

    connect(m_comboDirection, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ProjectionViewDock::onDirectionComboChanged);

    connect(m_comboWorkPlanes, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ProjectionViewDock::onWorkPlaneComboChanged);

    connect(m_chkPlaneVisible, &QCheckBox::toggled, this, &ProjectionViewDock::workPlaneVisibleToggled);
    connect(m_chkAxesVisible, &QCheckBox::toggled, this, &ProjectionViewDock::workPlaneAxesVisibleToggled);
    connect(m_chkGizmoActive, &QCheckBox::toggled, this, &ProjectionViewDock::workPlaneManipulatorToggled);

    connect(m_sliderGizmoSize, &QSlider::valueChanged, this, &ProjectionViewDock::onGizmoSliderChanged);
    connect(m_spinGizmoSize, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &ProjectionViewDock::onGizmoSpinChanged);

    connect(m_btnAlignView, &QPushButton::clicked, this, &ProjectionViewDock::alignViewToWorkPlaneRequested);
    connect(m_btnNewPlane, &QPushButton::clicked, this, &ProjectionViewDock::createNewWorkPlaneRequested);
}

void ProjectionViewDock::setModel(TSA::Model::Model* model)
{
    m_model = model;
    if (m_model && m_model->workPlaneManager())
    {
        updateWorkPlaneList(m_model->workPlaneManager());
    }
}

void ProjectionViewDock::updateWorkPlaneList(const TSA::Coordinate::WorkPlaneManager* wpm)
{
    if (!wpm) return;

    bool oldBlock = m_comboWorkPlanes->blockSignals(true);
    m_comboWorkPlanes->clear();

    for (const auto& [id, wp] : wpm->workPlanes())
    {
        QString label = QString("WP%1 : %2").arg(id).arg(QString::fromStdString(wp.name()));
        m_comboWorkPlanes->addItem(label, id);
    }

    int activeId = wpm->activeWorkPlaneId();
    int idx = m_comboWorkPlanes->findData(activeId);
    if (idx >= 0)
    {
        m_comboWorkPlanes->setCurrentIndex(idx);
    }

    m_comboWorkPlanes->blockSignals(oldBlock);

    const auto* actWp = wpm->activeWorkPlane();
    if (actWp)
    {
        syncFromWorkPlane(*actWp);
    }
}

void ProjectionViewDock::syncFromWorkPlane(const TSA::Coordinate::WorkPlane& wp)
{
    m_isUpdating = true;

    // Mise à jour de l'affichage textuel
    gp_Pnt o = wp.origin();
    m_lblOriginInfo->setText(QString("Origine : (%1, %2, %3) m")
                             .arg(o.X(), 0, 'f', 2)
                             .arg(o.Y(), 0, 'f', 2)
                             .arg(o.Z(), 0, 'f', 2));

    gp_Dir xd = wp.xDirection();
    gp_Dir yd = wp.yDirection();
    gp_Dir zd = wp.normal();

    m_lblAxesInfo->setText(QString("<span style='color:#e63946;'><b>Xwp</b>(%1,%2,%3)</span><br>"
                                   "<span style='color:#2a9d8f;'><b>Ywp</b>(%4,%5,%6)</span><br>"
                                   "<span style='color:#457b9d;'><b>Zwp</b>(%7,%8,%9)</span>")
                           .arg(xd.X(), 0, 'f', 2).arg(xd.Y(), 0, 'f', 2).arg(xd.Z(), 0, 'f', 2)
                           .arg(yd.X(), 0, 'f', 2).arg(yd.Y(), 0, 'f', 2).arg(yd.Z(), 0, 'f', 2)
                           .arg(zd.X(), 0, 'f', 2).arg(zd.Y(), 0, 'f', 2).arg(zd.Z(), 0, 'f', 2));

    m_chkPlaneVisible->setChecked(wp.isVisible());
    m_chkGridVisible->setChecked(wp.isGridVisible());

    m_isUpdating = false;
}

void ProjectionViewDock::syncProjectionMode(TSA::Viewer::ProjectionMode mode)
{
    m_isUpdating = true;
    if (mode == TSA::Viewer::ProjectionMode::TwoD)
    {
        m_rbProj2D->setChecked(true);
        m_lblPlaneStatus->setText(tr("Plan de projection : <b>[MODE 2D ACTIF] WorkPlane Local (Xwp, Ywp)</b>"));
    }
    else
    {
        m_rbProj3D->setChecked(true);
        m_lblPlaneStatus->setText(tr("Plan de projection : <b>[MODE 3D] Espace 3D</b>"));
    }
    m_isUpdating = false;
}

void ProjectionViewDock::syncCameraView(TSA::Viewer::StandardCameraView view, bool isOrtho)
{
    m_isUpdating = true;
    m_chkOrtho->setChecked(isOrtho);

    switch (view)
    {
    case TSA::Viewer::StandardCameraView::ThreeD_Axo: m_rbView3D->setChecked(true); break;
    case TSA::Viewer::StandardCameraView::Front: m_rbViewFront->setChecked(true); break;
    case TSA::Viewer::StandardCameraView::Back: m_rbViewBack->setChecked(true); break;
    case TSA::Viewer::StandardCameraView::Left: m_rbViewLeft->setChecked(true); break;
    case TSA::Viewer::StandardCameraView::Right: m_rbViewRight->setChecked(true); break;
    case TSA::Viewer::StandardCameraView::Top: m_rbViewTop->setChecked(true); break;
    case TSA::Viewer::StandardCameraView::Bottom: m_rbViewBottom->setChecked(true); break;
    case TSA::Viewer::StandardCameraView::NormalToPlane: m_rbViewNormal->setChecked(true); break;
    case TSA::Viewer::StandardCameraView::OppositePlane: m_rbViewOpposite->setChecked(true); break;
    default: break;
    }

    m_isUpdating = false;
}

void ProjectionViewDock::setGizmoSize(double size)
{
    m_isUpdating = true;
    m_sliderGizmoSize->setValue(static_cast<int>(size));
    m_spinGizmoSize->setValue(size);
    m_isUpdating = false;
}

void ProjectionViewDock::onViewRadioToggled()
{
    if (m_isUpdating) return;
    int id = m_viewGroup->checkedId();
    if (id >= 0)
    {
        emit standardViewRequested(static_cast<TSA::Viewer::StandardCameraView>(id));
    }
}

void ProjectionViewDock::onProjectionModeRadioToggled()
{
    if (m_isUpdating) return;
    int id = m_projGroup->checkedId();
    if (id >= 0)
    {
        emit projectionModeRequested(static_cast<TSA::Viewer::ProjectionMode>(id));
    }
}

void ProjectionViewDock::onDirectionComboChanged(int index)
{
    if (m_isUpdating || index < 0) return;
    int dirVal = m_comboDirection->itemData(index).toInt();
    emit projectionDirectionRequested(static_cast<TSA::Viewer::ProjectionDirection>(dirVal));
}

void ProjectionViewDock::onWorkPlaneComboChanged(int index)
{
    if (m_isUpdating || index < 0) return;
    int wpId = m_comboWorkPlanes->itemData(index).toInt();
    emit activeWorkPlaneSelected(wpId);
}

void ProjectionViewDock::onGizmoSliderChanged(int value)
{
    if (m_isUpdating) return;
    m_isUpdating = true;
    m_spinGizmoSize->setValue(static_cast<double>(value));
    m_isUpdating = false;
    emit gizmoSizeChanged(static_cast<double>(value));
}

void ProjectionViewDock::onGizmoSpinChanged(double value)
{
    if (m_isUpdating) return;
    m_isUpdating = true;
    m_sliderGizmoSize->setValue(static_cast<int>(value));
    m_isUpdating = false;
    emit gizmoSizeChanged(value);
}

} // namespace TSA::UI
