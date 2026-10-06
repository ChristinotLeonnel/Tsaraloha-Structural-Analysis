#include "ModelCleanupDialog.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace TSA::UI
{

ModelCleanupDialog::ModelCleanupDialog(const TSA::Model::Model& model, QWidget* parent)
    : QDialog(parent)
    , m_model(model)
{
    setWindowTitle(tr("Nettoyer le modèle"));
    setMinimumSize(620, 520);
    auto* root = new QVBoxLayout(this);

    auto* intro = new QLabel(tr("Corrige les défauts de topologie qui faussent le calcul ou la validation. "
                                "Le bilan ci-dessous est calculé sans modifier le modèle ; « Nettoyer » applique "
                                "les opérations en une seule action annulable (Ctrl+Z)."), this);
    intro->setWordWrap(true);
    root->addWidget(intro);

    auto* group = new QGroupBox(tr("Opérations"), this);
    auto* form = new QFormLayout(group);
    m_merge = new QCheckBox(tr("Fusionner les nœuds confondus"), group);
    m_duplicates = new QCheckBox(tr("Supprimer les barres en double (charges reportées)"), group);
    m_connect = new QCheckBox(tr("Raccorder les nœuds posés sur une barre (division de la barre)"), group);
    m_crossings = new QCheckBox(tr("Créer un nœud aux croisements de barres"), group);
    m_crossings->setToolTip(tr("Désactivé par défaut : deux diagonales d'un contreventement en X, ou deux barres "
                               "à des niveaux différents vues en plan, ne sont pas forcément assemblées."));
    m_orphans = new QCheckBox(tr("Supprimer les nœuds parasites (sans élément, appui ni charge)"), group);
    for (auto* c : { m_merge, m_duplicates, m_connect, m_crossings, m_orphans }) form->addRow(c);
    m_tolerance = new QDoubleSpinBox(group);
    m_tolerance->setRange(0.01, 100.0);
    m_tolerance->setDecimals(2);
    m_tolerance->setSuffix(tr(" mm"));
    m_tolerance->setToolTip(tr("Distance en dessous de laquelle deux nœuds sont confondus, ou un nœud est sur une barre."));
    form->addRow(tr("Tolérance :"), m_tolerance);
    root->addWidget(group);

    m_summary = new QLabel(this);
    m_summary->setWordWrap(true);
    root->addWidget(m_summary);
    m_details = new QPlainTextEdit(this);
    m_details->setReadOnly(true);
    root->addWidget(m_details, 1);

    auto* buttons = new QDialogButtonBox(this);
    auto* preview = buttons->addButton(tr("Actualiser le bilan"), QDialogButtonBox::ActionRole);
    m_apply = buttons->addButton(tr("Nettoyer"), QDialogButtonBox::AcceptRole);
    buttons->addButton(QDialogButtonBox::Cancel);
    root->addWidget(buttons);
    connect(preview, &QPushButton::clicked, this, [this] { refreshPreview(); });
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    for (auto* c : { m_merge, m_duplicates, m_connect, m_crossings, m_orphans })
        connect(c, &QCheckBox::toggled, this, [this] { refreshPreview(); });

    setOptions({});
}

TSA::Model::CleanupOptions ModelCleanupDialog::options() const
{
    TSA::Model::CleanupOptions o;
    o.tolerance = m_tolerance->value() / 1000.0;
    o.mergeCoincidentNodes = m_merge->isChecked();
    o.removeDuplicateBars = m_duplicates->isChecked();
    o.connectNodesOnBars = m_connect->isChecked();
    o.splitCrossingBars = m_crossings->isChecked();
    o.removeOrphanNodes = m_orphans->isChecked();
    return o;
}

void ModelCleanupDialog::setOptions(const TSA::Model::CleanupOptions& o)
{
    const QSignalBlocker b1(m_merge), b2(m_duplicates), b3(m_connect), b4(m_crossings), b5(m_orphans);
    m_merge->setChecked(o.mergeCoincidentNodes);
    m_duplicates->setChecked(o.removeDuplicateBars);
    m_connect->setChecked(o.connectNodesOnBars);
    m_crossings->setChecked(o.splitCrossingBars);
    m_orphans->setChecked(o.removeOrphanNodes);
    m_tolerance->setValue(o.tolerance * 1000.0);
    refreshPreview();
}

TSA::Model::CleanupReport ModelCleanupDialog::refreshPreview()
{
    const TSA::Model::CleanupReport r = TSA::Model::ModelCleanup::analyze(m_model, options());
    m_summary->setText(r.changed() ? tr("<b>Bilan :</b> %1").arg(QString::fromStdString(r.summary()))
                                   : tr("<b>Aucune correction nécessaire.</b> %1")
                                         .arg(r.warnings.empty() ? QString() : tr("%1 point(s) à vérifier ci-dessous.").arg(r.warnings.size())));
    QStringList lines;
    for (const auto& d : r.details) lines << QStringLiteral("• ") + QString::fromStdString(d);
    if (!r.refused.empty())
    {
        lines << QString() << tr("Non corrigé :");
        for (const auto& d : r.refused) lines << QStringLiteral("✗ ") + QString::fromStdString(d);
    }
    if (!r.warnings.empty())
    {
        lines << QString() << tr("À vérifier :");
        for (const auto& d : r.warnings) lines << QStringLiteral("⚠ ") + QString::fromStdString(d);
    }
    m_details->setPlainText(lines.join('\n'));
    m_apply->setEnabled(r.changed());
    return r;
}

} // namespace TSA::UI
