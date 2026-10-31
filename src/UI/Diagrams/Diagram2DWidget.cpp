#include "Diagram2DWidget.h"
#include "../../Model/Model.h"
#include "../../Model/Beam.h"
#include "../../Model/Column.h"
#include "../../Model/Node.h"

#include <QApplication>
#include <QClipboard>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLocale>
#include <QMenu>
#include <QMouseEvent>
#include <QPainterPath>
#include <QSettings>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>

namespace
{
using TSA::Geometry::DiagramType;

std::vector<TSA::Analysis::StationForces> getAllStations(const TSA::Analysis::ElementResults& el)
{
    std::vector<TSA::Analysis::StationForces> res;
    res.reserve(2 + el.intermediateStations.size());
    res.push_back(el.startForces);
    res.insert(res.end(), el.intermediateStations.begin(), el.intermediateStations.end());
    res.push_back(el.endForces);
    return res;
}

/// Clé enregistrée dans les réglages (indépendante de la valeur de l'énumération).
QString typeKey(DiagramType type)
{
    switch (type)
    {
    case DiagramType::AxialForceN: return QStringLiteral("N");
    case DiagramType::ShearForceVy: return QStringLiteral("Vy");
    case DiagramType::ShearForceVz: return QStringLiteral("Vz");
    case DiagramType::TorsionMx: return QStringLiteral("Mx");
    case DiagramType::BendingMy: return QStringLiteral("My");
    case DiagramType::BendingMz: return QStringLiteral("Mz");
    case DiagramType::DeflectionUx: return QStringLiteral("ux");
    case DiagramType::DeflectionUy: return QStringLiteral("uy");
    case DiagramType::DeflectionUz: return QStringLiteral("uz");
    case DiagramType::DeflectionUres: return QStringLiteral("u");
    case DiagramType::RotationRx: return QStringLiteral("rx");
    case DiagramType::RotationRy: return QStringLiteral("ry");
    case DiagramType::RotationRz: return QStringLiteral("rz");
    default: return QString();
    }
}

struct Preset
{
    const char* label;
    std::vector<DiagramType> types;
};

const std::vector<Preset>& presets()
{
    static const std::vector<Preset> list = {
        {QT_TRANSLATE_NOOP("TSA::UI::Diagram2DWidget", "Efforts N (Vx), Vy, Vz"),
         {DiagramType::AxialForceN, DiagramType::ShearForceVy, DiagramType::ShearForceVz}},
        {QT_TRANSLATE_NOOP("TSA::UI::Diagram2DWidget", "Efforts tranchants Vy, Vz"),
         {DiagramType::ShearForceVy, DiagramType::ShearForceVz}},
        {QT_TRANSLATE_NOOP("TSA::UI::Diagram2DWidget", "Moments Mx, My, Mz"),
         {DiagramType::TorsionMx, DiagramType::BendingMy, DiagramType::BendingMz}},
        {QT_TRANSLATE_NOOP("TSA::UI::Diagram2DWidget", "Flexion dans le plan local xy : Vy, Mz"),
         {DiagramType::ShearForceVy, DiagramType::BendingMz}},
        {QT_TRANSLATE_NOOP("TSA::UI::Diagram2DWidget", "Flexion dans le plan local xz : Vz, My"),
         {DiagramType::ShearForceVz, DiagramType::BendingMy}},
        {QT_TRANSLATE_NOOP("TSA::UI::Diagram2DWidget", "Tous les efforts"),
         {DiagramType::AxialForceN, DiagramType::ShearForceVy, DiagramType::ShearForceVz, DiagramType::TorsionMx,
          DiagramType::BendingMy, DiagramType::BendingMz}},
        {QT_TRANSLATE_NOOP("TSA::UI::Diagram2DWidget", "Déplacements ux, uy, uz"),
         {DiagramType::DeflectionUx, DiagramType::DeflectionUy, DiagramType::DeflectionUz}},
        {QT_TRANSLATE_NOOP("TSA::UI::Diagram2DWidget", "Rotations θx, θy, θz"),
         {DiagramType::RotationRx, DiagramType::RotationRy, DiagramType::RotationRz}},
    };
    return list;
}

const QColor kBackground(28, 28, 32);
const QColor kPlotBackground(22, 22, 26);
const QColor kFrame(60, 60, 70);
const QColor kGrid(48, 48, 58);
const QColor kText(180, 180, 190);
const QColor kMuted(160, 160, 170);

double valueToY(const QRect& r, double low, double high, double coord)
{
    const double span = (high - low) > 1e-12 ? (high - low) : 1.0;
    return r.bottom() - (coord - low) / span * r.height();
}

/// Place r en évitant les rectangles déjà posés (décalages verticaux alternés, d'abord du côté préféré) ;
/// faux si aucune place libre.
bool placeLabel(QRect& r, const QRect& bounds, const std::vector<QRect>& placed, bool preferUp)
{
    const int h = r.height() + 2;
    const int pref = preferUp ? -1 : 1;
    for (int k = 0; k < 9; ++k)
    {
        QRect c = r;
        if (k > 0) c.translate(0, ((k % 2 == 1) ? pref : -pref) * ((k + 1) / 2) * h);
        if (c.left() < bounds.left()) c.moveLeft(bounds.left() + 2);
        if (c.right() > bounds.right()) c.moveRight(bounds.right() - 2);
        if (c.top() < bounds.top() || c.bottom() > bounds.bottom()) continue;
        const bool free = std::none_of(placed.begin(), placed.end(), [&](const QRect& o) { return o.intersects(c); });
        if (free)
        {
            r = c;
            return true;
        }
    }
    return false;
}

/// Texte d'une graduation : décimales déduites du pas.
QString tickLabel(double v, double step)
{
    if (v == 0.0) return QStringLiteral("0");
    if (step < 1e-4 || std::abs(v) >= 1e6) return QString::number(v, 'g', 4);
    const int decimals = std::clamp(static_cast<int>(-std::floor(std::log10(step) + 1e-9)), 0, 6);
    return QString::number(v, 'f', decimals);
}

/// Graduations d'un axe vertical : scale convertit la coordonnée réduite en valeur affichée.
void drawYAxis(QPainter& p, const QRect& r, double low, double high, double scale, const QString& unit, bool right,
               bool grid, const QColor& textColor)
{
    const double vLow = low * scale, vHigh = high * scale;
    const double step = TSA::UI::Diagram2D::niceStep(vHigh - vLow, std::max(5, r.height() / 40));
    if (step <= 0.0) return;
    const QFontMetrics fm = p.fontMetrics();
    for (double t = std::ceil(vLow / step) * step; t <= vHigh + step * 1e-9; t += step)
    {
        const double v = std::abs(t) < step * 1e-6 ? 0.0 : t;
        const int y = static_cast<int>(std::lround(valueToY(r, low, high, v / scale)));
        if (grid && v != 0.0)
        {
            p.setPen(QPen(kGrid, 1));
            p.drawLine(r.left() + 1, y, r.right() - 1, y);
        }
        p.setPen(QPen(kFrame, 1));
        if (right) p.drawLine(r.right(), y, r.right() + 4, y);
        else p.drawLine(r.left() - 4, y, r.left(), y);
        p.setPen(textColor);
        const QString txt = tickLabel(v, step);
        const QRect box = right ? QRect(r.right() + 6, y - fm.height() / 2, 58, fm.height())
                               : QRect(r.left() - 64, y - fm.height() / 2, 58, fm.height());
        p.drawText(box, (right ? Qt::AlignLeft : Qt::AlignRight) | Qt::AlignVCenter, txt);
    }
    p.setPen(textColor);
    const QRect ur = right ? QRect(r.right() - 20, r.top() - fm.height() - 4, 80, fm.height())
                           : QRect(r.left() - 64, r.top() - fm.height() - 4, 80, fm.height());
    p.drawText(ur, (right ? Qt::AlignRight : Qt::AlignLeft) | Qt::AlignVCenter, unit);
}

} // namespace

namespace TSA::UI
{

// ---------------------------------------------------------------------------------------------------------------------
// Calculs du tracé
// ---------------------------------------------------------------------------------------------------------------------
namespace Diagram2D
{
std::vector<Sample> sampleCurve(const TSA::Analysis::ElementResults& el, DiagramType type, double length)
{
    std::vector<Sample> out;
    const double L = length > 1e-9 ? length : 1.0;
    for (const auto& st : getAllStations(el))
    {
        double v = TSA::Geometry::DiagramGeometry::getStationValue(st, type);
        if (!std::isfinite(v)) v = 0.0;
        out.push_back({std::clamp(st.position / L, 0.0, 1.0), v});
    }
    std::stable_sort(out.begin(), out.end(), [](const Sample& a, const Sample& b) { return a.s < b.s; });
    return out;
}

double interpolate(const std::vector<Sample>& samples, double s)
{
    if (samples.empty()) return 0.0;
    if (s <= samples.front().s) return samples.front().v;
    if (s >= samples.back().s) return samples.back().v;
    for (size_t i = 0; i + 1 < samples.size(); ++i)
    {
        const Sample& a = samples[i];
        const Sample& b = samples[i + 1];
        if (s >= a.s && s <= b.s)
        {
            if (b.s - a.s < 1e-12) return a.v;
            return a.v + (s - a.s) / (b.s - a.s) * (b.v - a.v);
        }
    }
    return samples.back().v;
}

Extremes extremes(const std::vector<Sample>& samples)
{
    Extremes e;
    if (samples.empty()) return e;
    e.maxV = e.minV = samples.front().v;
    e.maxS = e.minS = samples.front().s;
    for (const auto& sp : samples)
    {
        if (sp.v > e.maxV) { e.maxV = sp.v; e.maxS = sp.s; }
        if (sp.v < e.minV) { e.minV = sp.v; e.minS = sp.s; }
    }
    return e;
}

double niceStep(double range, int maxTicks)
{
    if (!(range > 0.0) || !std::isfinite(range) || maxTicks < 1) return 0.0;
    const double raw = range / maxTicks;
    const double mag = std::pow(10.0, std::floor(std::log10(raw)));
    for (double m : {1.0, 2.0, 5.0, 10.0})
        if (m * mag >= raw * (1.0 - 1e-9)) return m * mag;
    return 10.0 * mag;
}

QString formatValue(double v)
{
    const double a = std::abs(v);
    if (a == 0.0) return QStringLiteral("0");
    if (a >= 1000.0) return QString::number(v, 'f', 0);
    if (a >= 100.0) return QString::number(v, 'f', 1);
    if (a >= 1.0) return QString::number(v, 'f', 2);
    if (a >= 0.01) return QString::number(v, 'f', 3);
    return QString::number(v, 'e', 2);
}

const std::vector<DiagramType>& availableTypes()
{
    static const std::vector<DiagramType> types = {
        DiagramType::AxialForceN,  DiagramType::ShearForceVy, DiagramType::ShearForceVz,  DiagramType::TorsionMx,
        DiagramType::BendingMy,    DiagramType::BendingMz,    DiagramType::DeflectionUx,  DiagramType::DeflectionUy,
        DiagramType::DeflectionUz, DiagramType::DeflectionUres, DiagramType::RotationRx,  DiagramType::RotationRy,
        DiagramType::RotationRz};
    return types;
}

QString shortName(DiagramType type)
{
    switch (type)
    {
    case DiagramType::AxialForceN: return QStringLiteral("N (Vx)");
    case DiagramType::DeflectionUres: return QStringLiteral("|u|");
    case DiagramType::RotationRx: return QStringLiteral("θx");
    case DiagramType::RotationRy: return QStringLiteral("θy");
    case DiagramType::RotationRz: return QStringLiteral("θz");
    default: return typeKey(type);
    }
}

QColor curveColor(DiagramType type)
{
    switch (type)
    {
    case DiagramType::AxialForceN: return QColor(255, 140, 80);
    case DiagramType::ShearForceVy: return QColor(80, 180, 255);
    case DiagramType::ShearForceVz: return QColor(110, 210, 120);
    case DiagramType::TorsionMx: return QColor(200, 140, 230);
    case DiagramType::BendingMy: return QColor(255, 215, 80);
    case DiagramType::BendingMz: return QColor(240, 100, 110);
    case DiagramType::DeflectionUx: return QColor(255, 170, 70);
    case DiagramType::DeflectionUy: return QColor(70, 210, 225);
    case DiagramType::DeflectionUz: return QColor(175, 215, 90);
    case DiagramType::DeflectionUres: return QColor(235, 235, 240);
    case DiagramType::RotationRx: return QColor(185, 110, 205);
    case DiagramType::RotationRy: return QColor(255, 240, 120);
    case DiagramType::RotationRz: return QColor(240, 100, 160);
    default: return QColor(200, 200, 200);
    }
}
} // namespace Diagram2D

// ---------------------------------------------------------------------------------------------------------------------
// Widget
// ---------------------------------------------------------------------------------------------------------------------
Diagram2DWidget::Diagram2DWidget(QWidget* parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setupUi();
    restoreSettings();
}

void Diagram2DWidget::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(4);

    m_toolbar = new QWidget(this);
    auto* tbRows = new QVBoxLayout(m_toolbar);
    tbRows->setContentsMargins(4, 2, 4, 2);
    tbRows->setSpacing(3);
    auto* tbLayout = new QHBoxLayout();
    tbLayout->setSpacing(6);
    auto* chipLayout = new QHBoxLayout();
    chipLayout->setSpacing(3);
    tbRows->addLayout(tbLayout);
    tbRows->addLayout(chipLayout);

    m_elementCombo = new QComboBox(m_toolbar);
    m_elementCombo->setMinimumWidth(160);

    m_presetCombo = new QComboBox(m_toolbar);
    m_presetCombo->addItem(tr("Choix libre"), -1);
    for (int i = 0; i < static_cast<int>(presets().size()); ++i)
        m_presetCombo->addItem(tr(presets()[static_cast<size_t>(i)].label), i);
    m_presetCombo->setToolTip(tr("Groupes de courbes à superposer"));

    m_layoutCombo = new QComboBox(m_toolbar);
    m_layoutCombo->addItem(tr("Superposées"), static_cast<int>(Layout::Overlay));
    m_layoutCombo->addItem(tr("Empilées"), static_cast<int>(Layout::Stacked));
    m_layoutCombo->setToolTip(tr("Superposées : un seul graphe. Empilées : une bande par courbe, abscisses alignées."));

    m_scaleCombo = new QComboBox(m_toolbar);
    m_scaleCombo->addItem(tr("Valeurs réelles"), static_cast<int>(Scaling::PerUnit));
    m_scaleCombo->addItem(tr("Normalisée (%)"), static_cast<int>(Scaling::Normalized));
    m_scaleCombo->setToolTip(tr("Valeurs réelles : un axe par unité (gauche et droite).\n"
                                "Normalisée : chaque courbe rapportée à sa plus grande valeur absolue."));

    m_btnResetZoom = new QPushButton(tr("Recentrer"), m_toolbar);
    m_btnResetZoom->setToolTip(tr("Molette : zoom vertical. Ctrl + molette : zoom le long de la barre.\n"
                                  "Glisser : déplacer la vue zoomée. Double-clic : recentrer."));

    m_btnExport = new QToolButton(m_toolbar);
    m_btnExport->setText(tr("Exporter"));
    m_btnExport->setPopupMode(QToolButton::InstantPopup);
    auto* exportMenu = new QMenu(m_btnExport);
    connect(exportMenu->addAction(tr("Copier l'image")), &QAction::triggered, this, &Diagram2DWidget::copyImage);
    connect(exportMenu->addAction(tr("Enregistrer l'image (PNG)…")), &QAction::triggered, this, &Diagram2DWidget::saveImage);
    connect(exportMenu->addAction(tr("Copier les valeurs (tableur)")), &QAction::triggered, this, &Diagram2DWidget::copyValues);
    m_btnExport->setMenu(exportMenu);

    m_lblStatus = new QLabel(tr("Prêt"), m_toolbar);
    m_lblStatus->setStyleSheet("color: #a0a0b0; font-size: 11px;");

    tbLayout->addWidget(new QLabel(tr("Élément :"), m_toolbar));
    tbLayout->addWidget(m_elementCombo);
    tbLayout->addWidget(new QLabel(tr("Courbes :"), m_toolbar));
    tbLayout->addWidget(m_presetCombo);
    tbLayout->addWidget(m_layoutCombo);
    tbLayout->addWidget(m_scaleCombo);
    tbLayout->addWidget(m_btnResetZoom);
    tbLayout->addWidget(m_btnExport);
    tbLayout->addStretch();

    // Une puce par grandeur : cocher pour superposer la courbe, couleur = couleur de la courbe.
    for (DiagramType type : Diagram2D::availableTypes())
    {
        auto* chip = new QToolButton(m_toolbar);
        chip->setCheckable(true);
        chip->setText(Diagram2D::shortName(type));
        chip->setToolTip(tr("%1 (%2) — cliquer pour ajouter ou retirer la courbe")
                             .arg(TSA::Geometry::DiagramGeometry::diagramTypeName(type),
                                  TSA::Geometry::DiagramGeometry::diagramUnit(type)));
        const QColor c = Diagram2D::curveColor(type);
        chip->setStyleSheet(QStringLiteral("QToolButton { padding: 1px 6px; border: 1px solid #50505a; border-radius: 3px;"
                                           " color: #a0a0b0; }"
                                           "QToolButton:checked { border: 1px solid %1; color: %1;"
                                           " background: rgba(%2, %3, %4, 45); font-weight: bold; }")
                                .arg(c.name())
                                .arg(c.red())
                                .arg(c.green())
                                .arg(c.blue()));
        connect(chip, &QToolButton::clicked, this, [this, type](bool checked) {
            setDiagramTypeVisible(type, checked);
            saveSettings();
        });
        chipLayout->addWidget(chip);
        m_chips[type] = chip;
        if (type == DiagramType::BendingMz || type == DiagramType::DeflectionUres) chipLayout->addSpacing(8);
    }
    chipLayout->addStretch();
    chipLayout->addWidget(m_lblStatus);

    mainLayout->addWidget(m_toolbar);
    mainLayout->addStretch(1);

    connect(m_elementCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &Diagram2DWidget::onElementComboChanged);
    connect(m_presetCombo, QOverload<int>::of(&QComboBox::activated), this, &Diagram2DWidget::onPresetComboChanged);
    connect(m_layoutCombo, QOverload<int>::of(&QComboBox::activated), this, [this](int index) {
        setCurveLayout(static_cast<Layout>(m_layoutCombo->itemData(index).toInt()));
        saveSettings();
    });
    connect(m_scaleCombo, QOverload<int>::of(&QComboBox::activated), this, [this](int index) {
        setScaling(static_cast<Scaling>(m_scaleCombo->itemData(index).toInt()));
        saveSettings();
    });
    connect(m_btnResetZoom, &QPushButton::clicked, this, &Diagram2DWidget::onResetZoom);
    syncChips();
}

void Diagram2DWidget::setModel(TSA::Model::Model* model)
{
    m_model = model;
    updateElementList();
    update();
}

void Diagram2DWidget::setResultsModel(const std::shared_ptr<TSA::Analysis::ResultsModel>& results)
{
    m_results = results;
    updateElementList();
    update();
}

void Diagram2DWidget::updateElementList()
{
    using TSA::Analysis::StructuralElementKind;
    m_elementCombo->blockSignals(true);
    m_elementCombo->clear();

    auto lengthOf = [this](int startId, int endId) {
        const auto* n1 = m_model->getNode(startId);
        const auto* n2 = m_model->getNode(endId);
        return (n1 && n2) ? std::sqrt(std::pow(n2->x() - n1->x(), 2) + std::pow(n2->y() - n1->y(), 2) + std::pow(n2->z() - n1->z(), 2))
                          : 0.0;
    };
    auto add = [this](const QString& text, int id, StructuralElementKind kind) {
        m_elementCombo->addItem(text, id);
        m_elementCombo->setItemData(m_elementCombo->count() - 1, static_cast<int>(kind), Qt::UserRole + 1);
    };

    if (m_model)
    {
        for (const auto& [id, beam] : m_model->beams())
            add(tr("Poutre #%1 (L = %2 m)").arg(id).arg(lengthOf(beam.startNodeId(), beam.endNodeId()), 0, 'f', 2), id, StructuralElementKind::Beam);
        for (const auto& [id, col] : m_model->columns())
            add(tr("Poteau #%1 (H = %2 m)").arg(id).arg(lengthOf(col.startNodeId(), col.endNodeId()), 0, 'f', 2), id, StructuralElementKind::Column);
        for (const auto& [id, tm] : m_model->trussMembers())
            add(tr("Barre de treillis #%1 (L = %2 m)").arg(id).arg(lengthOf(tm.startNodeId(), tm.endNodeId()), 0, 'f', 2), id, StructuralElementKind::Truss);
        for (const auto& [id, cable] : m_model->cables())
            add(tr("Câble #%1 (L = %2 m)").arg(id).arg(lengthOf(cable.startNodeId(), cable.endNodeId()), 0, 'f', 2), id, StructuralElementKind::Cable);
    }

    if (m_elementCombo->count() > 0)
    {
        int idx = findElementItem(m_currentElementKind, m_currentElementId);
        if (idx >= 0)
        {
            m_elementCombo->setCurrentIndex(idx);
        }
        else
        {
            m_currentElementId = m_elementCombo->itemData(0).toInt();
            m_currentElementKind = static_cast<StructuralElementKind>(m_elementCombo->itemData(0, Qt::UserRole + 1).toInt());
            m_elementCombo->setCurrentIndex(0);
        }
    }
    m_elementCombo->blockSignals(false);
}

int Diagram2DWidget::findElementItem(TSA::Analysis::StructuralElementKind kind, int id) const
{
    for (int i = 0; i < m_elementCombo->count(); ++i)
    {
        if (m_elementCombo->itemData(i).toInt() == id
            && m_elementCombo->itemData(i, Qt::UserRole + 1).toInt() == static_cast<int>(kind))
            return i;
    }
    return -1;
}

void Diagram2DWidget::setSelectedElement(int elementId, TSA::Analysis::StructuralElementKind kind)
{
    if (m_currentElementId == elementId && m_currentElementKind == kind) return;
    m_currentElementId = elementId;
    m_currentElementKind = kind;
    int idx = findElementItem(kind, elementId);
    if (idx >= 0)
    {
        m_elementCombo->blockSignals(true);
        m_elementCombo->setCurrentIndex(idx);
        m_elementCombo->blockSignals(false);
    }
    m_hasHoverCursor = false;
    update();
}

void Diagram2DWidget::setDiagramType(DiagramType type)
{
    setDiagramTypes({type});
}

DiagramType Diagram2DWidget::diagramType() const
{
    return m_types.empty() ? DiagramType::None : m_types.front();
}

void Diagram2DWidget::setDiagramTypes(const std::vector<DiagramType>& types)
{
    std::vector<DiagramType> ordered;
    for (DiagramType t : Diagram2D::availableTypes())
        if (std::find(types.begin(), types.end(), t) != types.end()) ordered.push_back(t);
    if (ordered.empty()) return; // grandeur inconnue du widget (None, contrainte…) : affichage inchangé
    m_types = std::move(ordered);
    syncChips();
    update();
}

std::vector<DiagramType> Diagram2DWidget::diagramTypes() const
{
    return m_types;
}

void Diagram2DWidget::setDiagramTypeVisible(DiagramType type, bool visible)
{
    std::vector<DiagramType> next = m_types;
    const auto it = std::find(next.begin(), next.end(), type);
    if (visible && it == next.end()) next.push_back(type);
    else if (!visible && it != next.end() && next.size() > 1) next.erase(it);
    setDiagramTypes(next);
    syncChips(); // la dernière courbe reste cochée
}

void Diagram2DWidget::setCurveLayout(Layout layout)
{
    m_layout = layout;
    m_layoutCombo->setCurrentIndex(m_layoutCombo->findData(static_cast<int>(layout)));
    m_scaleCombo->setEnabled(layout == Layout::Overlay);
    update();
}

void Diagram2DWidget::setScaling(Scaling scaling)
{
    m_scaling = scaling;
    m_scaleCombo->setCurrentIndex(m_scaleCombo->findData(static_cast<int>(scaling)));
    update();
}

Diagram2DWidget::Scaling Diagram2DWidget::effectiveScaling() const
{
    if (m_scaling == Scaling::Normalized) return Scaling::Normalized;
    QStringList units;
    for (DiagramType t : m_types)
    {
        const QString u = TSA::Geometry::DiagramGeometry::diagramUnit(t);
        if (!units.contains(u)) units << u;
    }
    return (m_layout == Layout::Overlay && units.size() > 2) ? Scaling::Normalized : Scaling::PerUnit;
}

void Diagram2DWidget::syncChips()
{
    for (auto& [type, chip] : m_chips)
    {
        QSignalBlocker blocker(chip);
        chip->setChecked(std::find(m_types.begin(), m_types.end(), type) != m_types.end());
    }
    syncPresetCombo();
}

void Diagram2DWidget::syncPresetCombo()
{
    int index = 0;
    for (int i = 0; i < static_cast<int>(presets().size()); ++i)
        if (presets()[static_cast<size_t>(i)].types == m_types) index = m_presetCombo->findData(i);
    QSignalBlocker blocker(m_presetCombo);
    m_presetCombo->setCurrentIndex(index);
}

void Diagram2DWidget::onElementComboChanged(int index)
{
    m_currentElementId = m_elementCombo->itemData(index).toInt();
    m_currentElementKind = static_cast<TSA::Analysis::StructuralElementKind>(m_elementCombo->itemData(index, Qt::UserRole + 1).toInt());
    m_hasHoverCursor = false;
    update();
}

void Diagram2DWidget::onPresetComboChanged(int index)
{
    const int preset = m_presetCombo->itemData(index).toInt();
    if (preset < 0 || preset >= static_cast<int>(presets().size())) return; // « Choix libre » : rien à changer
    setDiagramTypes(presets()[static_cast<size_t>(preset)].types);
    saveSettings();
}

void Diagram2DWidget::onResetZoom()
{
    m_zoomFactor = 1.0;
    m_viewS0 = 0.0;
    m_viewS1 = 1.0;
    update();
}

void Diagram2DWidget::saveSettings() const
{
    QStringList keys;
    for (DiagramType t : m_types) keys << typeKey(t);
    QSettings settings;
    settings.beginGroup(QStringLiteral("Diagram2D"));
    settings.setValue(QStringLiteral("curves"), keys);
    settings.setValue(QStringLiteral("layout"), m_layout == Layout::Stacked ? QStringLiteral("stacked") : QStringLiteral("overlay"));
    settings.setValue(QStringLiteral("scaling"), m_scaling == Scaling::Normalized ? QStringLiteral("normalized") : QStringLiteral("unit"));
    settings.endGroup();
}

void Diagram2DWidget::restoreSettings()
{
    QSettings settings;
    settings.beginGroup(QStringLiteral("Diagram2D"));
    const QStringList keys = settings.value(QStringLiteral("curves")).toStringList();
    const QString layout = settings.value(QStringLiteral("layout")).toString();
    const QString scaling = settings.value(QStringLiteral("scaling")).toString();
    settings.endGroup();

    std::vector<DiagramType> types;
    for (DiagramType t : Diagram2D::availableTypes())
        if (keys.contains(typeKey(t))) types.push_back(t);
    if (!types.empty()) setDiagramTypes(types);
    setCurveLayout(layout == QStringLiteral("stacked") ? Layout::Stacked : Layout::Overlay);
    setScaling(scaling == QStringLiteral("normalized") ? Scaling::Normalized : Scaling::PerUnit);
}

// ---------------------------------------------------------------------------------------------------------------------
// Données du tracé
// ---------------------------------------------------------------------------------------------------------------------
const TSA::Analysis::ElementResults* Diagram2DWidget::currentResults() const
{
    if (!m_results || !m_results->isValid()) return nullptr;
    return m_results->getElementResults(m_currentElementKind, m_currentElementId);
}

double Diagram2DWidget::currentLength(const TSA::Analysis::ElementResults* el) const
{
    // Les positions des stations sont exprimées sur la longueur du solveur : elle sert de référence.
    if (el && el->length > 1e-6) return el->length;
    if (m_model)
    {
        using TSA::Analysis::StructuralElementKind;
        int sId = 0, eId = 0;
        switch (m_currentElementKind)
        {
        case StructuralElementKind::Beam:
            if (const auto* b = m_model->getBeam(m_currentElementId)) { sId = b->startNodeId(); eId = b->endNodeId(); }
            break;
        case StructuralElementKind::Column:
            if (const auto* c = m_model->getColumn(m_currentElementId)) { sId = c->startNodeId(); eId = c->endNodeId(); }
            break;
        case StructuralElementKind::Truss:
            if (const auto* t = m_model->getTrussMember(m_currentElementId)) { sId = t->startNodeId(); eId = t->endNodeId(); }
            break;
        case StructuralElementKind::Cable:
            if (const auto* c = m_model->getCable(m_currentElementId)) { sId = c->startNodeId(); eId = c->endNodeId(); }
            break;
        }
        const auto* n1 = m_model->getNode(sId);
        const auto* n2 = m_model->getNode(eId);
        if (n1 && n2)
        {
            const double L = std::sqrt(std::pow(n2->x() - n1->x(), 2) + std::pow(n2->y() - n1->y(), 2) + std::pow(n2->z() - n1->z(), 2));
            if (L > 1e-6) return L;
        }
    }
    return 1.0;
}

Diagram2DWidget::Frame Diagram2DWidget::buildFrame() const
{
    Frame f;
    if (!m_results || !m_results->isValid())
    {
        f.message = tr("Aucun résultat d'analyse disponible.\nLancez le calcul (F5) pour tracer les courbes.");
        return f;
    }
    const auto* el = currentResults();
    if (!el)
    {
        f.message = m_elementCombo->count() == 0 ? tr("Sélectionnez une barre pour afficher ses courbes de résultats.")
                                                 : tr("Aucun résultat pour cet élément.");
        return f;
    }
    f.length = currentLength(el);
    for (DiagramType t : m_types)
    {
        Curve c;
        c.type = t;
        c.name = Diagram2D::shortName(t);
        c.unit = TSA::Geometry::DiagramGeometry::diagramUnit(t);
        c.color = Diagram2D::curveColor(t);
        c.samples = Diagram2D::sampleCurve(*el, t, f.length);
        if (c.samples.empty()) continue;
        c.ext = Diagram2D::extremes(c.samples);
        f.curves.push_back(std::move(c));
    }
    if (f.curves.empty())
    {
        f.message = tr("Aucun résultat pour cet élément.");
        return f;
    }

    // Groupes d'échelle : une courbe par groupe (bandes empilées, échelle normalisée) ou un groupe par unité.
    f.normalized = m_layout == Layout::Overlay && effectiveScaling() == Scaling::Normalized;
    const bool perCurve = f.normalized || m_layout == Layout::Stacked;
    for (auto& c : f.curves)
    {
        int axis = -1;
        if (!perCurve)
            for (int a = 0; a < static_cast<int>(f.axes.size()); ++a)
                if (f.axes[static_cast<size_t>(a)].unit == c.unit) axis = a;
        if (axis < 0)
        {
            f.axes.push_back({f.normalized ? QStringLiteral("%") : c.unit, 0.0});
            axis = static_cast<int>(f.axes.size()) - 1;
        }
        c.axis = axis;
        auto& m = f.axes[static_cast<size_t>(axis)].magnitude;
        m = std::max({m, std::abs(c.ext.maxV), std::abs(c.ext.minV)});
    }
    for (auto& a : f.axes)
        if (a.magnitude < 1e-12) a.magnitude = 1.0; // grandeur nulle partout : échelle arbitraire, courbe sur le zéro

    std::vector<int> all(f.curves.size());
    for (size_t i = 0; i < all.size(); ++i) all[i] = static_cast<int>(i);
    computeRange(f, all, m_zoomFactor, f.low, f.high);
    return f;
}

void Diagram2DWidget::computeRange(const Frame& f, const std::vector<int>& curveIndices, double zoom, double& low, double& high)
{
    // Coordonnées réduites v / magnitude de l'axe : le zéro est au même endroit pour tous les axes.
    double up = 0.0, down = 0.0;
    for (int i : curveIndices)
    {
        const Curve& c = f.curves[static_cast<size_t>(i)];
        const double m = f.axes[static_cast<size_t>(c.axis)].magnitude;
        up = std::max(up, c.ext.maxV / m);
        down = std::max(down, -c.ext.minV / m);
    }
    if (up + down < 1e-12) up = down = 1.0;
    const double pad = 0.12 * (up + down);
    const double z = zoom > 1e-6 ? zoom : 1.0;
    low = (-down - pad) / z;
    high = (up + pad) / z;
}

// ---------------------------------------------------------------------------------------------------------------------
// Géométrie de l'écran
// ---------------------------------------------------------------------------------------------------------------------
QRect Diagram2DWidget::plotArea() const
{
    const int top = m_toolbar->geometry().bottom() + 2;
    return QRect(0, top, width(), std::max(0, height() - top));
}

QRect Diagram2DWidget::plotRectIn(const QRect& area, const Frame& f) const
{
    const bool rightAxis = m_layout == Layout::Overlay && !f.normalized && f.axes.size() > 1;
    return area.adjusted(70, 26, rightAxis ? -70 : -20, -30);
}

std::vector<QRect> Diagram2DWidget::stripRects(const QRect& plotRect, int count) const
{
    std::vector<QRect> strips;
    if (count <= 0) return strips;
    const int gap = 22;
    const int h = std::max(10, (plotRect.height() - gap * (count - 1)) / count);
    for (int i = 0; i < count; ++i)
        strips.emplace_back(plotRect.left(), plotRect.top() + i * (h + gap), plotRect.width(), h);
    return strips;
}

double Diagram2DWidget::sToX(const QRect& r, double s) const
{
    const double w = std::max(1e-9, m_viewS1 - m_viewS0);
    return r.left() + (s - m_viewS0) / w * r.width();
}

double Diagram2DWidget::xToS(const QRect& r, double x) const
{
    const double w = std::max(1e-9, m_viewS1 - m_viewS0);
    return m_viewS0 + (x - r.left()) / std::max(1, r.width()) * w;
}

// ---------------------------------------------------------------------------------------------------------------------
// Souris
// ---------------------------------------------------------------------------------------------------------------------
void Diagram2DWidget::wheelEvent(QWheelEvent* event)
{
    const double delta = event->angleDelta().y();
    if (delta == 0.0)
    {
        event->ignore();
        return;
    }
    if (event->modifiers() & Qt::ControlModifier)
    {
        // Zoom le long de la barre, centré sur le curseur.
        const Frame f = buildFrame();
        const QRect r = plotRectIn(plotArea(), f);
        const double sAt = std::clamp(xToS(r, event->position().x()), 0.0, 1.0);
        const double w = std::clamp((m_viewS1 - m_viewS0) * (delta > 0 ? 1.0 / 1.25 : 1.25), 0.02, 1.0);
        const double ratio = (sAt - m_viewS0) / std::max(1e-9, m_viewS1 - m_viewS0);
        m_viewS0 = std::clamp(sAt - ratio * w, 0.0, 1.0 - w);
        m_viewS1 = m_viewS0 + w;
    }
    else if (delta > 0)
    {
        m_zoomFactor = std::min(1000.0, m_zoomFactor * 1.15);
    }
    else
    {
        m_zoomFactor = std::max(0.2, m_zoomFactor / 1.15);
    }
    update();
    event->accept();
}

void Diagram2DWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && (m_viewS1 - m_viewS0) < 1.0 - 1e-9)
    {
        m_panning = true;
        m_panLastX = event->pos().x();
        setCursor(Qt::ClosedHandCursor);
    }
    QWidget::mousePressEvent(event);
}

void Diagram2DWidget::mouseReleaseEvent(QMouseEvent* event)
{
    if (m_panning && event->button() == Qt::LeftButton)
    {
        m_panning = false;
        unsetCursor();
    }
    QWidget::mouseReleaseEvent(event);
}

void Diagram2DWidget::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) onResetZoom();
    QWidget::mouseDoubleClickEvent(event);
}

void Diagram2DWidget::leaveEvent(QEvent* /*event*/)
{
    m_hasHoverCursor = false;
    update();
}

void Diagram2DWidget::mouseMoveEvent(QMouseEvent* event)
{
    const Frame f = buildFrame();
    const QRect plotRect = plotRectIn(plotArea(), f);

    if (m_panning)
    {
        const double w = m_viewS1 - m_viewS0;
        const double ds = -(event->pos().x() - m_panLastX) / std::max(1.0, static_cast<double>(plotRect.width())) * w;
        m_viewS0 = std::clamp(m_viewS0 + ds, 0.0, 1.0 - w);
        m_viewS1 = m_viewS0 + w;
        m_panLastX = event->pos().x();
    }

    if (!f.message.isEmpty() || !plotRect.contains(event->pos()))
    {
        m_hasHoverCursor = false;
        update();
        return;
    }

    m_hasHoverCursor = true;
    m_hoverPixel = event->pos();
    m_hoverS = std::clamp(xToS(plotRect, event->pos().x()), 0.0, 1.0);

    QStringList parts;
    parts << tr("x = %1 m (%2 %)").arg(m_hoverS * f.length, 0, 'f', 2).arg(m_hoverS * 100.0, 0, 'f', 0);
    for (const auto& c : f.curves)
        parts << QStringLiteral("%1 = %2 %3").arg(c.name, Diagram2D::formatValue(Diagram2D::interpolate(c.samples, m_hoverS)), c.unit);
    m_lblStatus->setText(parts.join(QStringLiteral(" | ")));

    emit elementInspected(m_currentElementId, m_hoverS, Diagram2D::interpolate(f.curves.front().samples, m_hoverS));
    update();
}

// ---------------------------------------------------------------------------------------------------------------------
// Dessin
// ---------------------------------------------------------------------------------------------------------------------
void Diagram2DWidget::paintEvent(QPaintEvent* /*event*/)
{
    QPainter p(this);
    p.fillRect(rect(), kBackground);
    paintPlot(p, plotArea(), true);
}

QImage Diagram2DWidget::renderImage(const QSize& size) const
{
    QImage image(size, QImage::Format_ARGB32_Premultiplied);
    image.fill(kBackground);
    QPainter p(&image);
    paintPlot(p, QRect(QPoint(0, 0), size), false);
    return image;
}

void Diagram2DWidget::paintPlot(QPainter& p, const QRect& area, bool withCursor) const
{
    p.setRenderHint(QPainter::Antialiasing);
    QFont font = this->font();
    font.setPointSizeF(9.0);
    p.setFont(font);

    const Frame f = buildFrame();
    const QRect plotRect = plotRectIn(area, f);
    if (plotRect.width() < 50 || plotRect.height() < 50) return;

    if (!f.message.isEmpty())
    {
        p.fillRect(plotRect, kPlotBackground);
        p.setPen(QPen(kFrame, 1));
        p.drawRect(plotRect);
        p.setPen(kMuted);
        p.drawText(plotRect, Qt::AlignCenter, f.message);
        return;
    }

    if (m_layout == Layout::Stacked && f.curves.size() > 1) drawStacked(p, plotRect, f, withCursor);
    else drawOverlay(p, plotRect, f, withCursor);
}

void Diagram2DWidget::drawXAxis(QPainter& p, const QRect& r, double length, bool labels) const
{
    const double x0 = m_viewS0 * length, x1 = m_viewS1 * length;
    const double step = Diagram2D::niceStep(x1 - x0, std::max(2, r.width() / 90));
    if (step <= 0.0) return;
    const QFontMetrics fm = p.fontMetrics();
    for (double t = std::ceil(x0 / step - 1e-9) * step; t <= x1 + step * 1e-9; t += step)
    {
        const int x = static_cast<int>(std::lround(sToX(r, t / length)));
        p.setPen(QPen(kGrid, 1));
        p.drawLine(x, r.top() + 1, x, r.bottom() - 1);
        if (!labels) continue;
        p.setPen(QPen(kFrame, 1));
        p.drawLine(x, r.bottom(), x, r.bottom() + 4);
        p.setPen(kText);
        p.drawText(QRect(x - 40, r.bottom() + 5, 80, fm.height()), Qt::AlignCenter,
                   QStringLiteral("%1 m").arg(std::abs(t) < step * 1e-6 ? 0.0 : t, 0, 'f', step < 0.1 ? 2 : (step < 1.0 ? 1 : 0)));
    }
}

void Diagram2DWidget::drawCurve(QPainter& p, const QRect& r, const Curve& c, double low, double high, double magnitude,
                                int fillAlpha) const
{
    p.save();
    p.setClipRect(r.adjusted(1, 1, -1, -1));
    const double yZero = valueToY(r, low, high, 0.0);

    QPainterPath curve;
    QPainterPath area;
    for (size_t i = 0; i < c.samples.size(); ++i)
    {
        const QPointF pt(sToX(r, c.samples[i].s), valueToY(r, low, high, c.samples[i].v / magnitude));
        if (i == 0)
        {
            curve.moveTo(pt);
            area.moveTo(pt.x(), yZero);
        }
        else
        {
            curve.lineTo(pt);
        }
        area.lineTo(pt);
    }
    area.lineTo(sToX(r, c.samples.back().s), yZero);
    area.closeSubpath();

    QColor fill = c.color;
    fill.setAlpha(fillAlpha);
    p.fillPath(area, fill);
    p.setPen(QPen(c.color, 2));
    p.setBrush(Qt::NoBrush);
    p.drawPath(curve);

    // Stations du solveur : marquées tant qu'elles restent lisibles.
    const double visible = c.samples.size() * (m_viewS1 - m_viewS0);
    if (visible <= r.width() / 12.0)
    {
        p.setBrush(QColor(255, 255, 255));
        p.setPen(QPen(c.color, 1));
        for (const auto& sp : c.samples)
            p.drawEllipse(QPointF(sToX(r, sp.s), valueToY(r, low, high, sp.v / magnitude)), 2.5, 2.5);
    }
    p.restore();
}

void Diagram2DWidget::drawExtremes(QPainter& p, const QRect& r, const Curve& c, double low, double high, double magnitude,
                                   std::vector<QRect>& placed) const
{
    struct Mark
    {
        double s, v;
        QString label;
    };
    std::vector<Mark> marks;
    const double tol = 1e-9 * std::max(1.0, std::max(std::abs(c.ext.maxV), std::abs(c.ext.minV)));
    if (std::abs(c.ext.maxV) > tol) marks.push_back({c.ext.maxS, c.ext.maxV, tr("max")});
    if (std::abs(c.ext.minV) > tol && std::abs(c.ext.minV - c.ext.maxV) > tol) marks.push_back({c.ext.minS, c.ext.minV, tr("min")});

    const QFontMetrics fm = p.fontMetrics();
    for (const auto& m : marks)
    {
        if (m.s < m_viewS0 - 1e-9 || m.s > m_viewS1 + 1e-9) continue;
        const QPointF pt(sToX(r, m.s), valueToY(r, low, high, m.v / magnitude));
        if (!QRectF(r).adjusted(-1.5, -1.5, 1.5, 1.5).contains(pt)) continue; // extrémités de la barre comprises

        p.setPen(QPen(QColor(20, 20, 24), 1));
        p.setBrush(c.color);
        p.drawEllipse(pt, 4.0, 4.0);

        const QString txt = QStringLiteral("%1 %2 = %3 %4").arg(c.name, m.label, Diagram2D::formatValue(m.v), c.unit);
        QRect box = fm.boundingRect(txt).adjusted(-5, -2, 5, 2);
        const bool up = m.v >= 0.0;
        box.moveCenter(QPoint(static_cast<int>(pt.x()), static_cast<int>(pt.y()) + (up ? -14 : 14)));
        if (!placeLabel(box, r, placed, up)) continue;
        placed.push_back(box);

        p.setPen(Qt::NoPen);
        p.setBrush(QColor(40, 40, 50, 225));
        p.drawRoundedRect(box, 3, 3);
        p.setPen(c.color);
        p.drawText(box, Qt::AlignCenter, txt);
    }
}

QRect Diagram2DWidget::drawLegend(QPainter& p, const QRect& plotRect, const Frame& f) const
{
    const QFontMetrics fm = p.fontMetrics();
    QStringList lines;
    int w = 0;
    for (const auto& c : f.curves)
    {
        QString line = QStringLiteral("%1   max %2 / min %3 %4")
                           .arg(c.name, Diagram2D::formatValue(c.ext.maxV), Diagram2D::formatValue(c.ext.minV), c.unit);
        if (!f.normalized && f.axes.size() > 1 && c.axis == 1) line += tr("  (axe de droite)");
        lines << line;
        w = std::max(w, fm.horizontalAdvance(line));
    }
    const int lineH = fm.height() + 2;
    QRect box(plotRect.left() + 8, plotRect.top() + 6, w + 34, lineH * static_cast<int>(lines.size()) + 8);
    p.setPen(QPen(kFrame, 1));
    p.setBrush(QColor(30, 30, 38, 215));
    p.drawRoundedRect(box, 4, 4);
    for (int i = 0; i < lines.size(); ++i)
    {
        const Curve& c = f.curves[static_cast<size_t>(i)];
        const int y = box.top() + 4 + i * lineH;
        p.setPen(QPen(c.color, 3));
        p.drawLine(box.left() + 8, y + lineH / 2, box.left() + 22, y + lineH / 2);
        p.setPen(c.color);
        p.drawText(QRect(box.left() + 28, y, w + 4, lineH), Qt::AlignLeft | Qt::AlignVCenter, lines[i]);
    }
    return box;
}

void Diagram2DWidget::drawOverlay(QPainter& p, const QRect& plotRect, const Frame& f, bool withCursor) const
{
    p.fillRect(plotRect, kPlotBackground);
    drawXAxis(p, plotRect, f.length, true);

    // Axe de gauche (premier groupe) avec grille ; axe de droite pour la seconde unité, teinté comme sa courbe.
    const bool dual = !f.normalized && f.axes.size() > 1;
    auto axisColor = [&](int axis) {
        if (!dual) return kText;
        for (const auto& c : f.curves)
            if (c.axis == axis) return c.color;
        return kText;
    };
    const double leftScale = f.normalized ? 100.0 : f.axes.front().magnitude;
    drawYAxis(p, plotRect, f.low, f.high, leftScale, f.normalized ? QStringLiteral("%") : f.axes.front().unit, false, true, axisColor(0));
    if (dual) drawYAxis(p, plotRect, f.low, f.high, f.axes[1].magnitude, f.axes[1].unit, true, false, axisColor(1));

    const double yZero = valueToY(plotRect, f.low, f.high, 0.0);
    p.setPen(QPen(QColor(120, 120, 140), 1, Qt::DashLine));
    p.drawLine(QPointF(plotRect.left(), yZero), QPointF(plotRect.right(), yZero));

    const int alpha = f.curves.size() == 1 ? 65 : 26;
    for (const auto& c : f.curves)
        drawCurve(p, plotRect, c, f.low, f.high, f.axes[static_cast<size_t>(c.axis)].magnitude, alpha);

    std::vector<QRect> placed{drawLegend(p, plotRect, f)};
    for (const auto& c : f.curves)
        drawExtremes(p, plotRect, c, f.low, f.high, f.axes[static_cast<size_t>(c.axis)].magnitude, placed);

    p.setPen(QPen(kFrame, 1));
    p.setBrush(Qt::NoBrush);
    p.drawRect(plotRect);

    if (withCursor && m_hasHoverCursor) drawCursor(p, plotRect, f, {});
}

void Diagram2DWidget::drawStacked(QPainter& p, const QRect& plotRect, const Frame& f, bool withCursor) const
{
    const std::vector<QRect> strips = stripRects(plotRect, static_cast<int>(f.curves.size()));
    const QFontMetrics fm = p.fontMetrics();
    for (size_t i = 0; i < f.curves.size(); ++i)
    {
        const Curve& c = f.curves[i];
        const QRect& r = strips[i];
        double low = -1.0, high = 1.0;
        computeRange(f, {static_cast<int>(i)}, m_zoomFactor, low, high);
        const double m = f.axes[static_cast<size_t>(c.axis)].magnitude;

        p.fillRect(r, kPlotBackground);
        drawXAxis(p, r, f.length, i + 1 == f.curves.size());
        drawYAxis(p, r, low, high, m, QString(), false, true, kText);

        const double yZero = valueToY(r, low, high, 0.0);
        p.setPen(QPen(QColor(120, 120, 140), 1, Qt::DashLine));
        p.drawLine(QPointF(r.left(), yZero), QPointF(r.right(), yZero));

        drawCurve(p, r, c, low, high, m, 55);

        // Titre de la bande : grandeur, unité, extrêmes.
        const QString title = QStringLiteral("%1 [%2]   max %3 / min %4")
                                  .arg(c.name, c.unit, Diagram2D::formatValue(c.ext.maxV), Diagram2D::formatValue(c.ext.minV));
        QRect titleBox = fm.boundingRect(title).adjusted(-6, -2, 6, 2);
        titleBox.moveTopLeft(QPoint(r.left() + 6, r.top() + 4));
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(30, 30, 38, 215));
        p.drawRoundedRect(titleBox, 3, 3);
        p.setPen(c.color);
        p.drawText(titleBox, Qt::AlignCenter, title);

        std::vector<QRect> placed{titleBox};
        drawExtremes(p, r, c, low, high, m, placed);

        p.setPen(QPen(kFrame, 1));
        p.setBrush(Qt::NoBrush);
        p.drawRect(r);
    }
    if (withCursor && m_hasHoverCursor) drawCursor(p, plotRect, f, strips);
}

void Diagram2DWidget::drawCursor(QPainter& p, const QRect& plotRect, const Frame& f, const std::vector<QRect>& strips) const
{
    const double x = sToX(plotRect, m_hoverS);
    p.setPen(QPen(QColor(255, 255, 255, 170), 1, Qt::DashLine));
    p.drawLine(QPointF(x, plotRect.top()), QPointF(x, plotRect.bottom()));

    QStringList lines;
    lines << tr("x = %1 m").arg(m_hoverS * f.length, 0, 'f', 3);
    for (size_t i = 0; i < f.curves.size(); ++i)
    {
        const Curve& c = f.curves[i];
        const double v = Diagram2D::interpolate(c.samples, m_hoverS);
        double low = f.low, high = f.high;
        QRect r = plotRect;
        if (!strips.empty())
        {
            r = strips[i];
            computeRange(f, {static_cast<int>(i)}, m_zoomFactor, low, high);
        }
        const double y = valueToY(r, low, high, v / f.axes[static_cast<size_t>(c.axis)].magnitude);
        if (y >= r.top() && y <= r.bottom())
        {
            p.setPen(QPen(QColor(0, 0, 0), 1));
            p.setBrush(c.color);
            p.drawEllipse(QPointF(x, y), 4.5, 4.5);
        }
        lines << QStringLiteral("%1 = %2 %3").arg(c.name, Diagram2D::formatValue(v), c.unit);
    }

    // Bulle des valeurs au curseur, retournée à gauche près du bord droit.
    const QFontMetrics fm = p.fontMetrics();
    int w = 0;
    for (const auto& l : lines) w = std::max(w, fm.horizontalAdvance(l));
    const int lineH = fm.height() + 1;
    QRect box(0, 0, w + 26, lineH * static_cast<int>(lines.size()) + 8);
    box.moveTopLeft(QPoint(static_cast<int>(x) + 12, std::max(plotRect.top() + 4, m_hoverPixel.y() - box.height() / 2)));
    if (box.right() > plotRect.right()) box.moveRight(static_cast<int>(x) - 12);
    if (box.bottom() > plotRect.bottom()) box.moveBottom(plotRect.bottom() - 4);
    p.setPen(QPen(QColor(90, 90, 105), 1));
    p.setBrush(QColor(36, 36, 44, 235));
    p.drawRoundedRect(box, 4, 4);
    for (int i = 0; i < lines.size(); ++i)
    {
        const int y = box.top() + 4 + i * lineH;
        if (i > 0)
        {
            const QColor color = f.curves[static_cast<size_t>(i - 1)].color;
            p.fillRect(QRect(box.left() + 7, y + lineH / 2 - 3, 7, 7), color);
            p.setPen(color);
        }
        else
        {
            p.setPen(QColor(230, 230, 235));
        }
        p.drawText(QRect(box.left() + (i > 0 ? 19 : 7), y, w + 6, lineH), Qt::AlignLeft | Qt::AlignVCenter, lines[i]);
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// Export
// ---------------------------------------------------------------------------------------------------------------------
QString Diagram2DWidget::valuesTable() const
{
    const Frame f = buildFrame();
    if (!f.message.isEmpty()) return QString();
    const QLocale locale;
    QStringList header{QStringLiteral("x (m)")};
    for (const auto& c : f.curves) header << QStringLiteral("%1 (%2)").arg(c.name, c.unit);
    QStringList rows{header.join(QLatin1Char('\t'))};
    // Toutes les courbes viennent des mêmes stations : une ligne par station.
    const auto& ref = f.curves.front().samples;
    for (size_t i = 0; i < ref.size(); ++i)
    {
        QStringList row{locale.toString(ref[i].s * f.length, 'g', 8)};
        for (const auto& c : f.curves) row << locale.toString(i < c.samples.size() ? c.samples[i].v : 0.0, 'g', 8);
        rows << row.join(QLatin1Char('\t'));
    }
    return rows.join(QLatin1Char('\n')) + QLatin1Char('\n');
}

void Diagram2DWidget::copyImage()
{
    const QRect area = plotArea();
    QApplication::clipboard()->setImage(renderImage(area.size().expandedTo(QSize(900, 450))));
    m_lblStatus->setText(tr("Image des courbes copiée dans le presse-papiers."));
}

void Diagram2DWidget::saveImage()
{
    const QString path = QFileDialog::getSaveFileName(this, tr("Enregistrer l'image des courbes"), QString(), tr("Image PNG (*.png)"));
    if (path.isEmpty()) return;
    const QRect area = plotArea();
    const bool ok = renderImage(area.size().expandedTo(QSize(1400, 700))).save(path, "PNG");
    m_lblStatus->setText(ok ? tr("Image enregistrée : %1").arg(path) : tr("Échec de l'enregistrement : %1").arg(path));
}

void Diagram2DWidget::copyValues()
{
    const QString table = valuesTable();
    if (table.isEmpty())
    {
        m_lblStatus->setText(tr("Aucune valeur à copier."));
        return;
    }
    QApplication::clipboard()->setText(table);
    m_lblStatus->setText(tr("Valeurs copiées (%1 lignes) : à coller dans un tableur.").arg(table.count(QLatin1Char('\n')) - 1));
}

} // namespace TSA::UI
