#include "ViewportContainer.h"
#include "ViewportRuler.h"
#include "../../Viewer/OccView.h"
#include "../Theme/ThemeManager.h"

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
    }

    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, &ViewportContainer::updateTheme);
}

void ViewportContainer::setupUi()
{
    auto* grid = new QGridLayout(this);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setSpacing(0);

    m_topBar = new QWidget(this);
    m_topBar->setFixedHeight(28);
    m_topBar->setStyleSheet(
        "QWidget { background: #161B22; border-bottom: 1px solid #30363D; font-family: Segoe UI, sans-serif; font-size: 11px; }"
        "QComboBox { background: #212830; border: 1px solid #444C56; border-radius: 2px; padding: 1px 6px; font-weight: 600; color: #E6EDF3; min-width: 180px; }"
        "QComboBox:hover { border-color: #58A6FF; }"
        "QPushButton { background: #212830; border: 1px solid #444C56; border-radius: 2px; padding: 2px 7px; color: #E6EDF3; font-weight: bold; max-width: 24px; }"
        "QPushButton:hover { background: #30363D; border-color: #58A6FF; }"
        "QPushButton:pressed { background: #1F3A5A; }"
        "QLabel { color: #8B949E; font-weight: 600; padding: 0 4px; }"
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
    lblHint->setStyleSheet("color: #8B949E; font-weight: normal; font-style: italic;");
    topLayout->addWidget(lblHint);

    topLayout->addStretch();

    connect(m_levelCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ViewportContainer::onLevelComboChanged);
    connect(m_btnLevelUp, &QPushButton::clicked, this, &ViewportContainer::onLevelUp);
    connect(m_btnLevelDown, &QPushButton::clicked, this, &ViewportContainer::onLevelDown);

    m_corner = new CornerWidget(this);
    m_topRuler = new HorizontalRulerWidget(m_occView, this);
    m_leftRuler = new VerticalRulerWidget(m_occView, VerticalRulerWidget::Position::Left, this);
    m_rightRuler = new VerticalRulerWidget(m_occView, VerticalRulerWidget::Position::Right, this);

    m_topCornerRight = new QWidget(this);
    m_topCornerRight->setFixedSize(34, 22);
    m_topCornerRight->setStyleSheet(ThemeManager::instance().isDarkMode()
        ? "background: #161B22; border-left: 1px solid #30363D; border-bottom: 1px solid #30363D;"
        : "background: #F6F8FA; border-left: 1px solid #D0D7DE; border-bottom: 1px solid #D0D7DE;");

    // Row 0: Barre supérieure de sélection d'étage
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

void ViewportContainer::updateTheme(bool isDark)
{
    if (m_topBar)
    {
        if (isDark)
        {
            m_topBar->setStyleSheet(
                "QWidget { background: #161B22; border-bottom: 1px solid #30363D; font-family: Segoe UI, sans-serif; font-size: 11px; }"
                "QComboBox { background: #212830; border: 1px solid #444C56; border-radius: 2px; padding: 1px 6px; font-weight: 600; color: #E6EDF3; min-width: 180px; }"
                "QComboBox:hover { border-color: #58A6FF; }"
                "QPushButton { background: #212830; border: 1px solid #444C56; border-radius: 2px; padding: 2px 7px; color: #E6EDF3; font-weight: bold; max-width: 24px; }"
                "QPushButton:hover { background: #30363D; border-color: #58A6FF; }"
                "QPushButton:pressed { background: #1F3A5A; }"
                "QLabel { color: #8B949E; font-weight: 600; padding: 0 4px; }"
            );
        }
        else
        {
            m_topBar->setStyleSheet(
                "QWidget { background: #F6F8FA; border-bottom: 1px solid #D0D7DE; font-family: Segoe UI, sans-serif; font-size: 11px; }"
                "QComboBox { background: #FFFFFF; border: 1px solid #D0D7DE; border-radius: 2px; padding: 1px 6px; font-weight: 600; color: #24292F; min-width: 180px; }"
                "QComboBox:hover { border-color: #0969DA; }"
                "QPushButton { background: #FFFFFF; border: 1px solid #D0D7DE; border-radius: 2px; padding: 2px 7px; color: #24292F; font-weight: bold; max-width: 24px; }"
                "QPushButton:hover { background: #EAEEF2; border-color: #0969DA; }"
                "QPushButton:pressed { background: #DDF4FF; }"
                "QLabel { color: #57606A; font-weight: 600; padding: 0 4px; }"
            );
        }
    }

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
    m_isDarkMode = dark;
    updateTheme(dark);
}

} // namespace TSA::UI
