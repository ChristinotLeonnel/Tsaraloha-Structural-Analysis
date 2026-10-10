#include "DimensionDialogs.h"

#include "../../Annotation/DimensionGeometry.h"
#include "../../Annotation/DimensionService.h"
#include "../../Model/Model.h"

#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QSpinBox>
#include <QVBoxLayout>

namespace TSA::UI
{

using namespace TSA::Annotation;

namespace
{
QDoubleSpinBox* dspin(QWidget* p, double min, double max, int dec, const QString& suffix)
{
    auto* s = new QDoubleSpinBox(p);
    s->setRange(min, max);
    s->setDecimals(dec);
    s->setSuffix(suffix);
    return s;
}
} // namespace

// ─── Modification d'une cotation ──────────────────────────────────────────────────────────────

DimensionEditDialog::DimensionEditDialog(TSA::Model::Model* model, int dimensionId, QWidget* parent)
    : QDialog(parent)
    , m_model(model)
    , m_id(dimensionId)
{
    setWindowTitle(tr("Modifier la cotation"));
    setObjectName(QStringLiteral("dimensionEditDialog"));
    auto* root = new QVBoxLayout(this);
    const auto& d = model->dimensions().items.at(dimensionId);
    const DimensionLayout layout = layoutFor(*model, d, model->dimensions().style);

    auto* form = new QFormLayout();
    form->addRow(tr("Type :"), new QLabel(QString::fromStdString(kindDisplayName(d.kind, d.axis)) + QStringLiteral(" #%1").arg(d.id), this));
    QStringList values;
    for (const auto& t : layout.texts) values << QString::fromStdString(t.text);
    m_lblValue = new QLabel(layout.valid ? values.join(QStringLiteral(" ; ")) : tr("géométrie invalide : %1").arg(QString::fromStdString(layout.error)), this);
    form->addRow(tr("Valeur mesurée :"), m_lblValue);
    root->addLayout(form);

    auto* grpAnchors = new QGroupBox(tr("Points d'ancrage"), this);
    auto* fa = new QFormLayout(grpAnchors);
    for (size_t i = 0; i < d.anchors.size(); ++i)
    {
        const auto& a = d.anchors[i];
        auto* spin = new QSpinBox(grpAnchors);
        spin->setRange(0, 99999999);
        spin->setSpecialValueText(tr("point fixe"));
        spin->setValue(a.nodeId > 0 ? a.nodeId : 0);
        spin->setToolTip(tr("Identifiant interne du nœud associé (0 : point fixe). Un nouveau nœud réassocie l'ancrage."));
        QString state;
        if (a.orphaned) state = tr(" — référence invalide (nœud supprimé) : saisissez un nœud pour réassocier");
        else if (a.nodeId > 0) state = tr(" — suit le nœud");
        auto* row = new QWidget(grpAnchors);
        auto* rl = new QHBoxLayout(row);
        rl->setContentsMargins(0, 0, 0, 0);
        rl->addWidget(spin);
        auto* lbl = new QLabel(QStringLiteral("(%1 ; %2 ; %3) m").arg(a.point[0], 0, 'f', 3).arg(a.point[1], 0, 'f', 3).arg(a.point[2], 0, 'f', 3) + state, row);
        lbl->setWordWrap(true);
        if (a.orphaned) lbl->setStyleSheet(QStringLiteral("color:#FF5252;"));
        rl->addWidget(lbl, 1);
        fa->addRow(tr("Point %1 :").arg(i + 1), row);
        m_anchorNodes.push_back(spin);
    }
    root->addWidget(grpAnchors);

    auto* grpPos = new QGroupBox(tr("Position de la ligne de cote"), this);
    auto* fp = new QFormLayout(grpPos);
    const char* axes[] = { "X :", "Y :", "Z :" };
    for (int k = 0; k < 3; ++k)
    {
        m_pos[k] = dspin(grpPos, -1e7, 1e7, 3, QStringLiteral(" m"));
        m_pos[k]->setValue(d.position[k]);
        fp->addRow(tr(axes[k]), m_pos[k]);
    }
    root->addWidget(grpPos);

    auto* fo = new QFormLayout();
    m_editText = new QLineEdit(QString::fromStdString(d.textOverride), this);
    m_editText->setObjectName(QStringLiteral("dimensionText"));
    m_editText->setPlaceholderText(tr("vide : valeur mesurée ; « <> » insère la valeur (ex. « L = <> »)"));
    fo->addRow(tr("Texte :"), m_editText);
    m_editColor = new QLineEdit(QString::fromStdString(d.color), this);
    m_editColor->setObjectName(QStringLiteral("dimensionColor"));
    m_editColor->setPlaceholderText(tr("vide : couleur du style (ex. #00BCD4)"));
    fo->addRow(tr("Couleur :"), m_editColor);
    root->addLayout(fo);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, [this] {
        QString err;
        if (applyChanges(&err)) accept();
        else QMessageBox::warning(this, windowTitle(), err);
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(buttons);
}

bool DimensionEditDialog::applyChanges(QString* error)
{
    auto it = m_model->dimensions().items.find(m_id);
    if (it == m_model->dimensions().items.end())
    {
        if (error) *error = tr("La cotation n'existe plus.");
        return false;
    }
    Dimension d = it->second;
    for (size_t i = 0; i < d.anchors.size() && i < m_anchorNodes.size(); ++i)
    {
        auto& a = d.anchors[i];
        const int node = m_anchorNodes[i]->value();
        if (node <= 0)
        {
            a.nodeId = -1; // point fixe (dernière position connue)
            a.orphaned = false;
        }
        else if (node != a.nodeId || a.orphaned)
        {
            const auto* n = m_model->getNode(node);
            if (!n)
            {
                if (error) *error = tr("Le nœud %1 n'existe pas.").arg(node);
                return false;
            }
            a.nodeId = node;
            a.orphaned = false;
            a.point = { n->x(), n->y(), n->z() };
        }
    }
    for (int k = 0; k < 3; ++k) d.position[k] = m_pos[k]->value();
    d.textOverride = m_editText->text().toStdString();
    const QString c = m_editColor->text().trimmed();
    if (!c.isEmpty() && !QColor(c).isValid())
    {
        if (error) *error = tr("Couleur invalide : %1").arg(c);
        return false;
    }
    d.color = c.toStdString();
    if (d == it->second) return true;
    std::string err;
    if (!updateDimension(*m_model, d, &err))
    {
        if (error) *error = QString::fromStdString(err);
        return false;
    }
    return true;
}

// ─── Style des cotations ──────────────────────────────────────────────────────────────────────

DimensionStyleDialog::DimensionStyleDialog(const DimensionStyle& style, QWidget* parent)
    : QDialog(parent)
    , m_base(style)
{
    setWindowTitle(tr("Style des cotations"));
    setObjectName(QStringLiteral("dimensionStyleDialog"));
    auto* root = new QVBoxLayout(this);
    auto* form = new QFormLayout();
    m_unit = new QComboBox(this);
    m_unit->addItem(tr("mètres (m)"), static_cast<int>(LengthUnit::Meter));
    m_unit->addItem(tr("centimètres (cm)"), static_cast<int>(LengthUnit::Centimeter));
    m_unit->addItem(tr("millimètres (mm)"), static_cast<int>(LengthUnit::Millimeter));
    m_unit->setCurrentIndex(m_unit->findData(static_cast<int>(style.unit)));
    form->addRow(tr("Unité des longueurs :"), m_unit);
    m_decimals = new QSpinBox(this);
    m_decimals->setRange(0, 6);
    m_decimals->setValue(style.decimals);
    form->addRow(tr("Décimales :"), m_decimals);
    m_rounding = dspin(this, 0.0, 1000.0, 3, QString());
    m_rounding->setValue(style.rounding);
    m_rounding->setToolTip(tr("Pas d'arrondi dans l'unité affichée (0 : aucun ; ex. 5 en mm)."));
    form->addRow(tr("Arrondi :"), m_rounding);
    m_showUnit = new QCheckBox(tr("Afficher l'unité"), this);
    m_showUnit->setChecked(style.showUnit);
    form->addRow(QString(), m_showUnit);
    m_angleDecimals = new QSpinBox(this);
    m_angleDecimals->setRange(0, 4);
    m_angleDecimals->setValue(style.angleDecimals);
    form->addRow(tr("Décimales des angles :"), m_angleDecimals);
    m_textHeight = dspin(this, 6, 72, 0, QStringLiteral(" px"));
    m_textHeight->setValue(style.textHeightPx);
    form->addRow(tr("Hauteur du texte :"), m_textHeight);
    m_arrowSize = dspin(this, 2, 60, 0, QStringLiteral(" px"));
    m_arrowSize->setValue(style.arrowSizePx);
    form->addRow(tr("Taille des flèches :"), m_arrowSize);
    m_gap = dspin(this, 0, 10, 3, QStringLiteral(" m"));
    m_gap->setValue(style.extensionGap);
    form->addRow(tr("Écart des lignes d'attache :"), m_gap);
    m_overshoot = dspin(this, 0, 10, 3, QStringLiteral(" m"));
    m_overshoot->setValue(style.extensionOvershoot);
    form->addRow(tr("Dépassement des lignes d'attache :"), m_overshoot);
    m_levelRef = dspin(this, -1e5, 1e5, 3, QStringLiteral(" m"));
    m_levelRef->setValue(style.levelReference);
    form->addRow(tr("Référence des niveaux :"), m_levelRef);
    m_color = new QLineEdit(QString::fromStdString(style.color), this);
    form->addRow(tr("Couleur :"), m_color);
    m_textInPlane = new QCheckBox(tr("Texte dans le plan de la cotation (sinon face à la caméra)"), this);
    m_textInPlane->setChecked(style.textInPlane);
    form->addRow(QString(), m_textInPlane);
    m_visible = new QCheckBox(tr("Afficher les cotations"), this);
    m_visible->setChecked(style.visible);
    form->addRow(QString(), m_visible);
    root->addLayout(form);
    auto* note = new QLabel(tr("Le texte et les flèches gardent une taille constante à l'écran ; les valeurs sont "
                               "calculées à partir des coordonnées du modèle."),
                            this);
    note->setWordWrap(true);
    root->addWidget(note);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, [this] {
        if (!QColor(m_color->text().trimmed()).isValid())
        {
            QMessageBox::warning(this, windowTitle(), tr("Couleur invalide."));
            return;
        }
        accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(buttons);
}

DimensionStyle DimensionStyleDialog::style() const
{
    DimensionStyle s = m_base;
    s.unit = static_cast<LengthUnit>(m_unit->currentData().toInt());
    s.decimals = m_decimals->value();
    s.rounding = m_rounding->value();
    s.showUnit = m_showUnit->isChecked();
    s.angleDecimals = m_angleDecimals->value();
    s.textHeightPx = m_textHeight->value();
    s.arrowSizePx = m_arrowSize->value();
    s.extensionGap = m_gap->value();
    s.extensionOvershoot = m_overshoot->value();
    s.levelReference = m_levelRef->value();
    s.color = m_color->text().trimmed().toStdString();
    s.textInPlane = m_textInPlane->isChecked();
    s.visible = m_visible->isChecked();
    return s;
}

} // namespace TSA::UI
