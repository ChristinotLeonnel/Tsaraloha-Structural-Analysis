#include "IfcDialogs.h"

#include <QCheckBox>
#include <QCloseEvent>
#include <QDialogButtonBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHeaderView>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QTabWidget>
#include <QTableWidget>
#include <QTextStream>
#include <QVBoxLayout>

#include <algorithm>

namespace TSA::UI
{

using TSA::BIM::Ifc::IfcImporter;
using TSA::BIM::Ifc::IfcImportItem;
using TSA::BIM::Ifc::IfcImportOptions;
using TSA::BIM::Ifc::IfcImportReport;

namespace
{
QString outcomeText(IfcImportItem::Outcome o)
{
    switch (o)
    {
    case IfcImportItem::Outcome::Analytical: return QObject::tr("Relié au calcul");
    case IfcImportItem::Outcome::ToPrepare: return QObject::tr("À préparer");
    case IfcImportItem::Outcome::NotStructural: return QObject::tr("Non porteur");
    case IfcImportItem::Outcome::Partial: return QObject::tr("Partiel");
    case IfcImportItem::Outcome::Ignored: return QObject::tr("Ignoré");
    }
    return {};
}

QString csvField(QString s)
{
    s.replace('"', "\"\"");
    return '"' + s + '"';
}
} // namespace

IfcProgressDialog::IfcProgressDialog(const QString& title, const QString& label, QWidget* parent)
    : QProgressDialog(label, tr("Annuler"), 0, 100, parent)
{
    setWindowTitle(title);
    setWindowModality(Qt::WindowModal);
    setMinimumDuration(0);
    setAutoClose(false);
    setAutoReset(false);
    // Liaison par défaut canceled() → cancel() : elle masque la fenêtre, remplacée par une simple demande d'arrêt.
    // (nullptr : retire la liaison quelle que soit sa forme, chaîne SIGNAL/SLOT ou pointeur de fonction.)
    disconnect(this, &QProgressDialog::canceled, this, nullptr);
    connect(this, &QProgressDialog::canceled, this, &IfcProgressDialog::requestCancel);
    setValue(0);
}

void IfcProgressDialog::setStage(int percent, const QString& stage)
{
    setValue(std::clamp(percent, 0, 99));   // 100 seulement à la fin effective
    if (!m_cancelRequested && !stage.isEmpty()) setLabelText(stage);
}

void IfcProgressDialog::reject()
{
    emit canceled();   // Échap (QDialog::reject masquerait la fenêtre sans émettre canceled)
}

void IfcProgressDialog::closeEvent(QCloseEvent* event)
{
    event->ignore();
    emit canceled();
}

void IfcProgressDialog::requestCancel()
{
    auto* button = findChild<QPushButton*>();
    if (m_cancelRequested || !button) return;   // sans bouton (setCancelButton(nullptr)) : travail non interruptible
    m_cancelRequested = true;
    setLabelText(tr("Annulation en cours… (fin de l'étape en cours)"));
    button->setEnabled(false);
}

IfcImportOptionsDialog::IfcImportOptionsDialog(const QString& path, QWidget* parent) : QDialog(parent)
{
    setWindowTitle(tr("Importer un fichier IFC"));
    auto* layout = new QVBoxLayout(this);
    auto* file = new QLabel(tr("Fichier : %1").arg(QFileInfo(path).fileName()), this);
    file->setWordWrap(true);
    layout->addWidget(file);
    auto* engine = new QLabel(tr("Lecture : %1").arg(QString::fromStdString(IfcImporter::engineName())), this);
    engine->setWordWrap(true);
    layout->addWidget(engine);
    if (!IfcImporter::fullEngineAvailable())
    {
        auto* warn = new QLabel(tr("Noyau IFC complet absent de cette version : seules les extrusions paramétriques et le modèle "
                                   "analytique du fichier sont lus. Les objets décrits autrement (maillages, B-rep, booléens) "
                                   "seront importés sans géométrie."),
                                this);
        warn->setWordWrap(true);
        layout->addWidget(warn);
    }

    m_geometry = new QCheckBox(tr("Importer la géométrie de tous les objets (affichage, réexportation)"), this);
    m_geometry->setChecked(true);
    m_geometry->setEnabled(IfcImporter::fullEngineAvailable());
    m_analytical = new QCheckBox(tr("Reprendre le modèle de calcul du fichier (IfcStructuralAnalysisModel), s'il existe"), this);
    m_analytical->setChecked(true);
    m_derive = new QCheckBox(tr("Déduire des éléments de calcul de la géométrie des poutres, poteaux, dalles et voiles qui n'en ont pas"), this);
    m_derive->setChecked(false);
    m_derive->setToolTip(tr("Axes d'extrusion convertis en barres et surfaces de calcul. Les connexions, appuis, excentrements, "
                            "sections et matériaux sont à vérifier : chaque élément déduit est signalé."));
    layout->addWidget(m_geometry);
    layout->addWidget(m_analytical);
    layout->addWidget(m_derive);
    auto* note = new QLabel(tr("Les objets sans élément de calcul sont conservés « à préparer » (porteurs) ou « non porteurs » : "
                               "aucune connexion, aucun appui ni aucune charge n'est inventé. Le projet ouvert n'est remplacé "
                               "qu'après un import réussi."),
                            this);
    note->setWordWrap(true);
    layout->addWidget(note);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("Importer"));
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}

IfcImportOptions IfcImportOptionsDialog::options() const
{
    IfcImportOptions o;
    o.importGeometry = m_geometry->isChecked();
    o.useAnalyticalModel = m_analytical->isChecked();
    o.deriveAnalyticalFromGeometry = m_derive->isChecked();
    return o;
}

IfcImportReportDialog::IfcImportReportDialog(const QString& path, const IfcImportReport& report, QWidget* parent)
    : QDialog(parent), m_path(path), m_report(report)
{
    setWindowTitle(tr("Rapport d'import IFC"));
    resize(900, 560);
    auto* layout = new QVBoxLayout(this);
    auto* summary = new QLabel(QFileInfo(path).fileName() + "\n\n" + QString::fromStdString(report.summary()), this);
    summary->setWordWrap(true);
    summary->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(summary);

    auto* tabs = new QTabWidget(this);
    auto* table = new QTableWidget(static_cast<int>(report.items.size()), 5, tabs);
    table->setHorizontalHeaderLabels({ tr("Classe IFC"), tr("Nom"), tr("Résultat"), tr("Remarque"), tr("GlobalId") });
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSortingEnabled(false);
    for (int i = 0; i < static_cast<int>(report.items.size()); ++i)
    {
        const auto& it = report.items[static_cast<std::size_t>(i)];
        table->setItem(i, 0, new QTableWidgetItem(QString::fromStdString(it.ifcClass)));
        table->setItem(i, 1, new QTableWidgetItem(QString::fromStdString(it.name)));
        table->setItem(i, 2, new QTableWidgetItem(outcomeText(it.outcome)));
        table->setItem(i, 3, new QTableWidgetItem(QString::fromStdString(it.note)));
        table->setItem(i, 4, new QTableWidgetItem(QString::fromStdString(it.globalId)));
    }
    table->setSortingEnabled(true);
    table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    tabs->addTab(table, tr("Objets (%1)").arg(report.items.size()));

    auto* classes = new QTableWidget(static_cast<int>(report.byClass.size()), 2, tabs);
    classes->setHorizontalHeaderLabels({ tr("Classe IFC"), tr("Produits importés") });
    classes->setEditTriggers(QAbstractItemView::NoEditTriggers);
    int row = 0;
    for (const auto& [cls, n] : report.byClass)
    {
        classes->setItem(row, 0, new QTableWidgetItem(QString::fromStdString(cls)));
        auto* count = new QTableWidgetItem;
        count->setData(Qt::DisplayRole, n);
        classes->setItem(row++, 1, count);
    }
    classes->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    tabs->addTab(classes, tr("Par classe"));

    auto* warnings = new QListWidget(tabs);
    for (const auto& w : report.warnings) warnings->addItem(QString::fromStdString(w));
    tabs->addTab(warnings, tr("Avertissements (%1)").arg(report.warnings.size()));
    layout->addWidget(tabs, 1);

    auto* advice = new QLabel(tr("Objets « à préparer » : définir les éléments de calcul (barres, surfaces, appuis) avant tout calcul. "
                                 "Vérifier le modèle de calcul (appuis, relâchements, charges) dans tous les cas."),
                              this);
    advice->setWordWrap(true);
    layout->addWidget(advice);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    auto* save = buttons->addButton(tr("Enregistrer le rapport…"), QDialogButtonBox::ActionRole);
    connect(save, &QPushButton::clicked, this, &IfcImportReportDialog::saveCsv);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    layout->addWidget(buttons);
}

void IfcImportReportDialog::saveCsv()
{
    const QString target = QFileDialog::getSaveFileName(this, tr("Enregistrer le rapport d'import"),
                                                        QFileInfo(m_path).completeBaseName() + "-rapport-import.csv", tr("CSV (*.csv)"));
    if (target.isEmpty()) return;
    QFile f(target);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
    {
        QMessageBox::warning(this, windowTitle(), tr("Impossible d'écrire « %1 ».").arg(target));
        return;
    }
    QTextStream out(&f);
    out.setEncoding(QStringConverter::Utf8);
    out.setGenerateByteOrderMark(true);   // accents lus correctement par les tableurs
    out << "fichier;" << csvField(m_path) << "\n" << "résumé;" << csvField(QString::fromStdString(m_report.summary())) << "\n\n";
    out << "classe IFC;nom;résultat;remarque;GlobalId\n";
    for (const auto& it : m_report.items)
        out << csvField(QString::fromStdString(it.ifcClass)) << ';' << csvField(QString::fromStdString(it.name)) << ';'
            << csvField(outcomeText(it.outcome)) << ';' << csvField(QString::fromStdString(it.note)) << ';'
            << csvField(QString::fromStdString(it.globalId)) << '\n';
    out << "\navertissements\n";
    for (const auto& w : m_report.warnings) out << csvField(QString::fromStdString(w)) << '\n';
}

} // namespace TSA::UI
