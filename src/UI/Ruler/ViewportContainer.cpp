#include "ViewportContainer.h"
#include "ViewportRuler.h"
#include "../../Viewer/OccView.h"
#include "../../Model/Model.h"
#include "../Theme/ThemeManager.h"

#include <QScopedValueRollback>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QCheckBox>
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
        if (m_occView->model())
        {
            m_model = m_occView->model();
        }
    }

    refreshAvailablePlanes();

    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, &ViewportContainer::updateTheme);
}

void ViewportContainer::setModel(TSA::Model::Model* model)
{
    m_model = model;
    refreshAvailablePlanes();
}

void ViewportContainer::setupUi()
{
    auto* grid = new QGridLayout(this);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setSpacing(0);

    m_topBar = new QWidget(this);
    m_topBar->setFixedHeight(30);

    auto* topLayout = new QHBoxLayout(m_topBar);
    topLayout->setContentsMargins(6, 2, 8, 2);
    topLayout->setSpacing(5);

    // 1. Direction / Axe
    m_lblAxis = new QLabel(tr("Axe :"), m_topBar);
    topLayout->addWidget(m_lblAxis);

    m_axisCombo = new QComboBox(m_topBar);
    m_axisCombo->setToolTip(tr("Choisir l'axe du plan de travail : Z (Horizontal), X (Coupe YZ), Y (Coupe XZ)"));
    m_axisCombo->addItem("Z (Plancher)", static_cast<int>(TSA::Coordinate::WorkPlaneAxis::Z));
    m_axisCombo->addItem("X (Coupe YZ)", static_cast<int>(TSA::Coordinate::WorkPlaneAxis::X));
    m_axisCombo->addItem("Y (Coupe XZ)", static_cast<int>(TSA::Coordinate::WorkPlaneAxis::Y));
    m_axisCombo->setFixedWidth(115);
    topLayout->addWidget(m_axisCombo);

    // 2. Boutons rapides de navigation [ Z ] [ X ] [ Y ]
    m_btnQuickZ = new QPushButton("Z", m_topBar);
    m_btnQuickZ->setToolTip(tr("Activer plan horizontal Z (Alt+Z)"));
    m_btnQuickZ->setFixedSize(26, 22);
    m_btnQuickZ->setShortcut(QKeySequence("Alt+Z"));
    topLayout->addWidget(m_btnQuickZ);

    m_btnQuickX = new QPushButton("X", m_topBar);
    m_btnQuickX->setToolTip(tr("Activer coupe verticale X (Alt+X)"));
    m_btnQuickX->setFixedSize(26, 22);
    m_btnQuickX->setShortcut(QKeySequence("Alt+X"));
    topLayout->addWidget(m_btnQuickX);

    m_btnQuickY = new QPushButton("Y", m_topBar);
    m_btnQuickY->setToolTip(tr("Activer coupe verticale Y (Alt+Y)"));
    m_btnQuickY->setFixedSize(26, 22);
    m_btnQuickY->setShortcut(QKeySequence("Alt+Y"));
    topLayout->addWidget(m_btnQuickY);

    topLayout->addSpacing(4);

    // 3. Choix du niveau ou du plan détecté
    m_lblPlan = new QLabel(tr("Niveau :"), m_topBar);
    topLayout->addWidget(m_lblPlan);

    m_planCombo = new QComboBox(m_topBar);
    m_planCombo->setToolTip(tr("Sélectionner le niveau ou la coupe active"));
    m_planCombo->setMinimumWidth(180);
    topLayout->addWidget(m_planCombo);

    m_btnPlanUp = new QPushButton(tr("▲"), m_topBar);
    m_btnPlanUp->setToolTip(tr("Plan suivant"));
    m_btnPlanUp->setFixedSize(24, 22);
    topLayout->addWidget(m_btnPlanUp);

    m_btnPlanDown = new QPushButton(tr("▼"), m_topBar);
    m_btnPlanDown->setToolTip(tr("Plan précédent"));
    m_btnPlanDown->setFixedSize(24, 22);
    topLayout->addWidget(m_btnPlanDown);

    topLayout->addSpacing(6);

    // 4. Synchronisation WorkPlane
    m_chkSyncWorkPlane = new QCheckBox(tr("Sync WorkPlane"), m_topBar);
    m_chkSyncWorkPlane->setToolTip(tr("Le plan sélectionné aligne automatiquement le WorkPlane 3D et le repère local."));
    m_chkSyncWorkPlane->setChecked(m_occView ? m_occView->syncWorkPlaneWithLevel() : true);
    topLayout->addWidget(m_chkSyncWorkPlane);

    connect(m_chkSyncWorkPlane, &QCheckBox::toggled, this, [this](bool on) {
        if (!m_occView)
            return;
        m_occView->setSyncWorkPlaneWithLevel(on);
        if (on && m_planCombo && m_planCombo->currentIndex() >= 0)
        {
            double off = m_planCombo->currentData().toDouble();
            m_occView->setWorkPlaneAxisAndOffset(m_currentAxis, off, m_planCombo->currentText().toStdString());
        }
    });

    topLayout->addSpacing(6);

    // 5. Mode 2D
    m_chkMode2D = new QCheckBox(tr("2D"), m_topBar);
    m_chkMode2D->setToolTip(tr("Mode 2D CAO : Isoler automatiquement le plan actif et orienter la caméra perpendiculaire"));
    m_chkMode2D->setChecked(m_occView ? m_occView->isMode2D() : false);
    topLayout->addWidget(m_chkMode2D);

    // 6. Badge Mode 2D
    m_lblMode2DBadge = new QLabel(m_topBar);
    m_lblMode2DBadge->setStyleSheet(
        "background: #1F3A5A; color: #79C0FF; border: 1px solid #388BFD; border-radius: 3px; padding: 1px 8px; font-size: 11px; font-weight: 600;"
    );
    m_lblMode2DBadge->setVisible(false);
    topLayout->addWidget(m_lblMode2DBadge);

    connect(m_chkMode2D, &QCheckBox::toggled, this, [this](bool on) {
        if (m_occView)
        {
            m_occView->setMode2D(on);
        }
        updateMode2DBadge(on);
        emit mode2DChanged(on);
    });

    topLayout->addSpacing(8);

    m_lblHint = new QLabel(tr("Plan ou coupe active"), m_topBar);
    m_lblHint->setStyleSheet("color: #8B949E; font-weight: normal; font-style: italic;");
    topLayout->addWidget(m_lblHint);

    topLayout->addStretch();

    // Connexions
    connect(m_axisCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ViewportContainer::onAxisComboChanged);
    connect(m_btnQuickZ, &QPushButton::clicked, this, [this]() { setWorkPlaneAxis(TSA::Coordinate::WorkPlaneAxis::Z); });
    connect(m_btnQuickX, &QPushButton::clicked, this, [this]() { setWorkPlaneAxis(TSA::Coordinate::WorkPlaneAxis::X); });
    connect(m_btnQuickY, &QPushButton::clicked, this, [this]() { setWorkPlaneAxis(TSA::Coordinate::WorkPlaneAxis::Y); });

    connect(m_planCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ViewportContainer::onPlanComboChanged);
    connect(m_btnPlanUp, &QPushButton::clicked, this, &ViewportContainer::onPlanUp);
    connect(m_btnPlanDown, &QPushButton::clicked, this, &ViewportContainer::onPlanDown);

    m_corner = new CornerWidget(this);
    m_topRuler = new HorizontalRulerWidget(m_occView, this);
    m_leftRuler = new VerticalRulerWidget(m_occView, VerticalRulerWidget::Position::Left, this);
    m_rightRuler = new VerticalRulerWidget(m_occView, VerticalRulerWidget::Position::Right, this);

    m_topCornerRight = new QWidget(this);
    m_topCornerRight->setFixedSize(34, 22);

    // Row 0: Barre supérieure de sélection d'étage / plan
    grid->addWidget(m_topBar, 0, 0, 1, 3);

    // Row 1: Règles supérieures
    grid->addWidget(m_corner, 1, 0);
    grid->addWidget(m_topRuler, 1, 1);
    grid->addWidget(m_topCornerRight, 1, 2);

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

    if (m_occView)
    {
        connect(m_occView, &OccView::viewPlaneModeChanged, this, [this](OccView::ViewPlaneMode) {
            updateRulers();
        });
        connect(m_occView, &OccView::mode2DChanged, this, [this](bool active) {
            if (m_chkMode2D && m_chkMode2D->isChecked() != active)
            {
                QSignalBlocker blocker(m_chkMode2D);
                m_chkMode2D->setChecked(active);
            }
            updateMode2DBadge(active);
        });
        connect(m_occView, &OccView::workPlaneChanged, this, [this](const TSA::Coordinate::WorkPlane& wp) {
            // Mettre à jour l'axe courant si le WorkPlane a changé ailleurs
            TSA::Coordinate::WorkPlaneAxis detectedAxis = TSA::Coordinate::WorkPlaneAxis::Z;
            if (wp.type() == TSA::Coordinate::WorkPlaneType::GlobalYZ)
            {
                detectedAxis = TSA::Coordinate::WorkPlaneAxis::X;
            }
            else if (wp.type() == TSA::Coordinate::WorkPlaneType::GlobalXZ)
            {
                detectedAxis = TSA::Coordinate::WorkPlaneAxis::Y;
            }

            if (detectedAxis != m_currentAxis)
            {
                m_currentAxis = detectedAxis;
                m_axisCombo->blockSignals(true);
                m_axisCombo->setCurrentIndex(static_cast<int>(m_currentAxis));
                m_axisCombo->blockSignals(false);
                populatePlansForCurrentAxis();
            }

            if (m_occView && m_occView->isMode2D())
            {
                updateMode2DBadge(true);
            }
        });
    }

    updateTheme(ThemeManager::instance().isDarkMode());
}

void ViewportContainer::updateQuickButtonsState()
{
    auto applyBtnStyle = [this](QPushButton* btn, bool active) {
        if (!btn) return;
        if (m_isDarkMode)
        {
            if (active)
            {
                btn->setStyleSheet("QPushButton { background: #1F3A5A; border: 1px solid #58A6FF; border-radius: 2px; color: #79C0FF; font-weight: bold; }");
            }
            else
            {
                btn->setStyleSheet("QPushButton { background: #212830; border: 1px solid #444C56; border-radius: 2px; color: #8B949E; font-weight: bold; }"
                                   "QPushButton:hover { background: #30363D; border-color: #58A6FF; color: #E6EDF3; }");
            }
        }
        else
        {
            if (active)
            {
                btn->setStyleSheet("QPushButton { background: #DDF4FF; border: 1px solid #0969DA; border-radius: 2px; color: #0969DA; font-weight: bold; }");
            }
            else
            {
                btn->setStyleSheet("QPushButton { background: #FFFFFF; border: 1px solid #D0D7DE; border-radius: 2px; color: #57606A; font-weight: bold; }"
                                   "QPushButton:hover { background: #EAEEF2; border-color: #0969DA; color: #24292F; }");
            }
        }
    };

    applyBtnStyle(m_btnQuickZ, m_currentAxis == TSA::Coordinate::WorkPlaneAxis::Z);
    applyBtnStyle(m_btnQuickX, m_currentAxis == TSA::Coordinate::WorkPlaneAxis::X);
    applyBtnStyle(m_btnQuickY, m_currentAxis == TSA::Coordinate::WorkPlaneAxis::Y);
}

void ViewportContainer::refreshAvailablePlanes()
{
    populatePlansForCurrentAxis();
}

void ViewportContainer::populatePlansForCurrentAxis()
{
    if (!m_planCombo)
        return;

    m_planCombo->blockSignals(true);
    m_planCombo->clear();

    // 1. Récupérer les plans détectés selon l'axe actif
    if (m_model)
    {
        m_currentPlanes = m_model->detectStructuralPlanes(m_currentAxis);
    }
    else if (m_occView && m_occView->model())
    {
        m_currentPlanes = m_occView->model()->detectStructuralPlanes(m_currentAxis);
    }
    else if (m_currentAxis == TSA::Coordinate::WorkPlaneAxis::Z && !m_legacyElevations.empty())
    {
        m_currentPlanes.clear();
        for (size_t i = 0; i < m_legacyElevations.size(); ++i)
        {
            TSA::Coordinate::DetectedPlaneInfo info;
            info.axis = TSA::Coordinate::WorkPlaneAxis::Z;
            info.offset = m_legacyElevations[i];
            info.name = (i < m_legacyNames.size()) ? m_legacyNames[i] : ("Z = " + std::to_string(m_legacyElevations[i]) + " m");
            info.id = "z_" + std::to_string(i);
            m_currentPlanes.push_back(info);
        }
    }
    else
    {
        TSA::Coordinate::CoordinateSystem cs;
        m_currentPlanes = cs.detectStructuralPlanes(m_currentAxis, nullptr);
    }

    if (m_currentPlanes.empty())
    {
        TSA::Coordinate::DetectedPlaneInfo defInfo;
        defInfo.axis = m_currentAxis;
        defInfo.offset = 0.0;
        defInfo.name = (m_currentAxis == TSA::Coordinate::WorkPlaneAxis::Z) ? "Z = 0.00 m (Base)"
                     : ((m_currentAxis == TSA::Coordinate::WorkPlaneAxis::X) ? "Axe X = 0.00 m" : "Axe Y = 0.00 m");
        m_currentPlanes.push_back(defInfo);
    }

    for (const auto& plane : m_currentPlanes)
    {
        m_planCombo->addItem(QString::fromStdString(plane.name), QVariant(plane.offset));
    }

    // Libellé adapté à l'axe
    if (m_lblPlan)
    {
        switch (m_currentAxis)
        {
        case TSA::Coordinate::WorkPlaneAxis::Z: m_lblPlan->setText(tr("Niveau :")); break;
        case TSA::Coordinate::WorkPlaneAxis::X: m_lblPlan->setText(tr("Coupe X :")); break;
        case TSA::Coordinate::WorkPlaneAxis::Y: m_lblPlan->setText(tr("Coupe Y :")); break;
        }
    }

    updateQuickButtonsState();

    m_planCombo->blockSignals(false);

    if (m_planCombo->count() > 0)
    {
        onPlanComboChanged(m_planCombo->currentIndex());
    }
}

void ViewportContainer::setWorkPlaneAxis(TSA::Coordinate::WorkPlaneAxis axis)
{
    if (m_currentAxis == axis && m_planCombo && m_planCombo->count() > 0)
        return;

    m_currentAxis = axis;
    if (m_axisCombo)
    {
        m_axisCombo->blockSignals(true);
        m_axisCombo->setCurrentIndex(static_cast<int>(axis));
        m_axisCombo->blockSignals(false);
    }

    populatePlansForCurrentAxis();
}

void ViewportContainer::setActivePlane(TSA::Coordinate::WorkPlaneAxis axis, double offset)
{
    if (m_currentAxis != axis)
    {
        m_currentAxis = axis;
        if (m_axisCombo)
        {
            m_axisCombo->blockSignals(true);
            m_axisCombo->setCurrentIndex(static_cast<int>(axis));
            m_axisCombo->blockSignals(false);
        }
        populatePlansForCurrentAxis();
    }

    if (!m_planCombo)
        return;

    // Trouver le plan le plus proche
    int bestIdx = -1;
    double minDiff = 1e9;
    for (int i = 0; i < m_planCombo->count(); ++i)
    {
        double off = m_planCombo->itemData(i).toDouble();
        double diff = std::abs(off - offset);
        if (diff < minDiff)
        {
            minDiff = diff;
            bestIdx = i;
        }
    }

    if (bestIdx >= 0 && minDiff < 0.06) // 6 cm de tolérance
    {
        if (m_planCombo->currentIndex() != bestIdx)
        {
            m_planCombo->setCurrentIndex(bestIdx);
        }
        else
        {
            onPlanComboChanged(bestIdx);
        }
    }
    else
    {
        // Plan personnalisé à l'offset demandé
        QString customName = QString("%1 = %2 m")
            .arg(axis == TSA::Coordinate::WorkPlaneAxis::Z ? "Z" : (axis == TSA::Coordinate::WorkPlaneAxis::X ? "X" : "Y"))
            .arg(offset, 0, 'f', 2);
        m_planCombo->blockSignals(true);
        m_planCombo->addItem(customName, QVariant(offset));
        m_planCombo->setCurrentIndex(m_planCombo->count() - 1);
        m_planCombo->blockSignals(false);
        onPlanComboChanged(m_planCombo->currentIndex());
    }
}

void ViewportContainer::onAxisComboChanged(int index)
{
    if (index < 0 || !m_axisCombo)
        return;

    auto axis = static_cast<TSA::Coordinate::WorkPlaneAxis>(m_axisCombo->itemData(index).toInt());
    setWorkPlaneAxis(axis);
}

void ViewportContainer::onPlanComboChanged(int index)
{
    if (!m_planCombo || index < 0 || index >= m_planCombo->count())
        return;

    // Écho synchrone de notre propre application du plan (OccView::workPlaneChanged -> MainWindow
    // -> setActiveLevelElevation) : les combos sont déjà à jour, ne pas réappliquer le plan.
    if (m_isApplyingPlane)
        return;

    double offset = m_planCombo->itemData(index).toDouble();
    QString text = m_planCombo->itemText(index);

    if (m_occView)
    {
        const QScopedValueRollback<bool> applying(m_isApplyingPlane, true);
        m_occView->setWorkPlaneAxisAndOffset(m_currentAxis, offset, text.toStdString());
        if (m_currentAxis == TSA::Coordinate::WorkPlaneAxis::Z)
        {
            m_occView->setActiveLevelElevation(offset);
        }
    }

    updateRulers();
    if (m_occView && m_occView->isMode2D())
    {
        updateMode2DBadge(true);
    }

    if (m_currentAxis == TSA::Coordinate::WorkPlaneAxis::Z)
    {
        emit activeLevelChanged(offset, text);
    }
    emit activeWorkPlaneChanged(m_currentAxis, offset, text);
}

void ViewportContainer::onPlanUp()
{
    if (!m_planCombo)
        return;
    int cur = m_planCombo->currentIndex();
    if (cur < m_planCombo->count() - 1)
    {
        m_planCombo->setCurrentIndex(cur + 1);
    }
}

void ViewportContainer::onPlanDown()
{
    if (!m_planCombo)
        return;
    int cur = m_planCombo->currentIndex();
    if (cur > 0)
    {
        m_planCombo->setCurrentIndex(cur - 1);
    }
}

void ViewportContainer::updateLevelsList(const std::vector<double>& elevations, const std::vector<std::string>& names)
{
    m_legacyElevations = elevations;
    m_legacyNames = names;

    if (m_currentAxis == TSA::Coordinate::WorkPlaneAxis::Z)
    {
        populatePlansForCurrentAxis();
    }
}

double ViewportContainer::activeLevelElevation() const
{
    if (!m_planCombo || m_planCombo->currentIndex() < 0)
        return 0.0;
    return m_planCombo->currentData().toDouble();
}

void ViewportContainer::setActiveLevelIndex(int index)
{
    if (m_currentAxis != TSA::Coordinate::WorkPlaneAxis::Z)
    {
        setWorkPlaneAxis(TSA::Coordinate::WorkPlaneAxis::Z);
    }
    if (m_planCombo && index >= 0 && index < m_planCombo->count())
    {
        m_planCombo->setCurrentIndex(index);
    }
}

void ViewportContainer::setActiveLevelElevation(double elevation)
{
    setActivePlane(TSA::Coordinate::WorkPlaneAxis::Z, elevation);
}

void ViewportContainer::setMode2D(bool enabled)
{
    if (m_chkMode2D)
    {
        m_chkMode2D->setChecked(enabled);
    }
    else if (m_occView)
    {
        m_occView->setMode2D(enabled);
    }
}

bool ViewportContainer::isMode2D() const
{
    if (m_chkMode2D)
        return m_chkMode2D->isChecked();
    return m_occView ? m_occView->isMode2D() : false;
}

void ViewportContainer::updateMode2DBadge(bool active)
{
    if (!m_lblMode2DBadge)
        return;

    if (!active)
    {
        m_lblMode2DBadge->setVisible(false);
        return;
    }

    QString axisStr;
    switch (m_currentAxis)
    {
    case TSA::Coordinate::WorkPlaneAxis::Z: axisStr = tr("Plan Z"); break;
    case TSA::Coordinate::WorkPlaneAxis::X: axisStr = tr("Coupe X"); break;
    case TSA::Coordinate::WorkPlaneAxis::Y: axisStr = tr("Coupe Y"); break;
    }

    QString planName;
    if (m_planCombo && m_planCombo->currentIndex() >= 0)
    {
        planName = m_planCombo->currentText();
    }
    if (planName.isEmpty() && m_occView)
    {
        planName = QString::fromStdString(m_occView->activeWorkPlane().name());
    }
    if (planName.isEmpty())
    {
        planName = tr("Actif");
    }

    m_lblMode2DBadge->setText(QString("☑ 2D | %1 : %2 | Ortho").arg(axisStr).arg(planName));
    m_lblMode2DBadge->setVisible(true);
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
    if (m_corner) m_corner->update();
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

void ViewportContainer::updateTheme(bool isDark)
{
    m_isDarkMode = isDark;

    if (m_topBar)
    {
        if (isDark)
        {
            m_topBar->setStyleSheet(
                "QWidget { background: #161B22; border-bottom: 1px solid #30363D; font-family: Segoe UI, sans-serif; font-size: 11px; }"
                "QComboBox { background: #212830; border: 1px solid #444C56; border-radius: 2px; padding: 1px 6px; font-weight: 600; color: #E6EDF3; }"
                "QComboBox:hover { border-color: #58A6FF; }"
                "QPushButton { background: #212830; border: 1px solid #444C56; border-radius: 2px; padding: 2px 7px; color: #E6EDF3; font-weight: bold; }"
                "QPushButton:hover { background: #30363D; border-color: #58A6FF; }"
                "QPushButton:pressed { background: #1F3A5A; }"
                "QLabel { color: #8B949E; font-weight: 600; padding: 0 2px; }"
                "QCheckBox { color: #C9D1D9; font-weight: 600; spacing: 4px; }"
            );
        }
        else
        {
            m_topBar->setStyleSheet(
                "QWidget { background: #F6F8FA; border-bottom: 1px solid #D0D7DE; font-family: Segoe UI, sans-serif; font-size: 11px; }"
                "QComboBox { background: #FFFFFF; border: 1px solid #D0D7DE; border-radius: 2px; padding: 1px 6px; font-weight: 600; color: #24292F; }"
                "QComboBox:hover { border-color: #0969DA; }"
                "QPushButton { background: #FFFFFF; border: 1px solid #D0D7DE; border-radius: 2px; padding: 2px 7px; color: #24292F; font-weight: bold; }"
                "QPushButton:hover { background: #EAEEF2; border-color: #0969DA; }"
                "QPushButton:pressed { background: #DDF4FF; }"
                "QLabel { color: #57606A; font-weight: 600; padding: 0 2px; }"
                "QCheckBox { color: #24292F; font-weight: 600; spacing: 4px; }"
            );
        }
    }

    if (m_chkMode2D)
    {
        m_chkMode2D->setStyleSheet(
            "QCheckBox { font-weight: 700; color: #58A6FF; spacing: 4px; padding: 1px 6px; border: 1px solid #30363D; border-radius: 3px; background: #212830; }"
            "QCheckBox:hover { border-color: #58A6FF; background: #262C36; }"
            "QCheckBox:checked { background: #1F3A5A; border-color: #58A6FF; color: #79C0FF; }"
        );
    }

    updateQuickButtonsState();

    if (m_topCornerRight)
    {
        m_topCornerRight->setStyleSheet(isDark
            ? "background: #161B22; border-left: 1px solid #30363D; border-bottom: 1px solid #30363D;"
            : "background: #F6F8FA; border-left: 1px solid #D0D7DE; border-bottom: 1px solid #D0D7DE;");
    }

    updateRulers();
}

void ViewportContainer::setDarkMode(bool dark)
{
    updateTheme(dark);
}

} // namespace TSA::UI
