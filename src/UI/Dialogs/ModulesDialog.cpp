#include "ModulesDialog.h"

#include "../../Core/AppPaths.h"

#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QUrl>
#include <QVBoxLayout>

namespace TSA::UI
{

using TSA::Modules::ModuleState;

ModulesDialog::ModulesDialog(TSA::Modules::ModuleRegistry& registry, TSA::Modules::ModuleHostServices services, QWidget* parent)
    : QDialog(parent)
    , m_registry(registry)
    , m_services(std::move(services))
{
    setWindowTitle(tr("Modules"));
    resize(820, 520);
    auto* layout = new QVBoxLayout(this);
    auto* intro = new QLabel(tr("Modules livrés avec l'application (approuvés) et modules installés dans le dossier utilisateur. "
                                "Un module utilisateur n'est activé — ni plugin chargé, ni convertisseur exécuté — qu'après votre "
                                "approbation ; toute modification de son manifeste exige une nouvelle approbation."),
                             this);
    intro->setWordWrap(true);
    layout->addWidget(intro);

    m_table = new QTableWidget(0, 5, this);
    m_table->setHorizontalHeaderLabels({ tr("Identifiant"), tr("Nom"), tr("Version"), tr("Origine"), tr("État") });
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->verticalHeader()->hide();
    layout->addWidget(m_table, 3);

    m_details = new QPlainTextEdit(this);
    m_details->setReadOnly(true);
    layout->addWidget(m_details, 2);

    auto* buttons = new QHBoxLayout;
    m_approve = new QPushButton(tr("Approuver"), this);
    m_approve->setToolTip(tr("Autoriser ce module (empreinte du manifeste mémorisée) puis l'activer"));
    m_toggle = new QPushButton(tr("Désactiver"), this);
    m_toggle->setToolTip(tr("Désactiver ou réactiver le module pour la session en cours"));
    auto* reloadBtn = new QPushButton(tr("Recharger"), this);
    reloadBtn->setToolTip(tr("Arrêter les modules, relire les dossiers, réactiver"));
    auto* folder = new QPushButton(tr("Dossier des modules utilisateur"), this);
    buttons->addWidget(m_approve);
    buttons->addWidget(m_toggle);
    buttons->addWidget(reloadBtn);
    buttons->addStretch();
    buttons->addWidget(folder);
    layout->addLayout(buttons);
    auto* box = new QDialogButtonBox(QDialogButtonBox::Close, this);
    layout->addWidget(box);

    connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_table, &QTableWidget::itemSelectionChanged, this, &ModulesDialog::updateDetails);
    connect(reloadBtn, &QPushButton::clicked, this, &ModulesDialog::reload);
    connect(folder, &QPushButton::clicked, this,
            [] { QDesktopServices::openUrl(QUrl::fromLocalFile(TSA::Core::AppPaths::userModulesDir())); });
    connect(m_approve, &QPushButton::clicked, this, [this] {
        const QString id = selectedId();
        const auto* m = m_registry.module(id);
        if (!m) return;
        const auto answer = QMessageBox::warning(
            this, tr("Approuver un module"),
            tr("Le module « %1 » (%2, éditeur : %3) pourra exécuter du code sur ce poste (plugin ou convertisseurs).\n\n"
               "Dossier : %4\n\nN'approuvez que des modules de source sûre. Continuer ?")
                .arg(m->manifest.name, id, m->manifest.publisher.isEmpty() ? tr("non indiqué") : m->manifest.publisher, m->dir),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer != QMessageBox::Yes) return;
        QString error;
        if (!m_registry.approve(id, &error))
        {
            QMessageBox::critical(this, tr("Approuver un module"), error);
            return;
        }
        m_registry.activate(m_services);
        populate();
        emit modulesChanged();
    });
    connect(m_toggle, &QPushButton::clicked, this, [this] {
        const QString id = selectedId();
        const auto* m = m_registry.module(id);
        if (!m) return;
        m_registry.setEnabled(id, m->state == ModuleState::Disabled);
        reload();
    });
    populate();
}

void ModulesDialog::populate()
{
    const QString keep = selectedId();
    m_table->setRowCount(0);
    for (const auto& m : m_registry.modules())
    {
        const int row = m_table->rowCount();
        m_table->insertRow(row);
        const QStringList cells = { m.manifest.id, m.manifest.name, m.manifest.version.toString(),
                                    m.shipped ? tr("livré") : tr("utilisateur"), TSA::Modules::moduleStateName(m.state) };
        for (int c = 0; c < cells.size(); ++c) m_table->setItem(row, c, new QTableWidgetItem(cells[c]));
        if (m.manifest.id == keep) m_table->selectRow(row);
    }
    m_table->resizeColumnsToContents();
    if (m_table->rowCount() && !m_table->selectionModel()->hasSelection()) m_table->selectRow(0);
    updateDetails();
}

QString ModulesDialog::selectedId() const
{
    const auto rows = m_table ? m_table->selectionModel()->selectedRows() : QModelIndexList {};
    return rows.isEmpty() ? QString() : m_table->item(rows.first().row(), 0)->text();
}

void ModulesDialog::updateDetails()
{
    const auto* m = m_registry.module(selectedId());
    m_approve->setEnabled(m && m->state == ModuleState::Untrusted);
    m_toggle->setEnabled(m && m->state != ModuleState::Invalid);
    m_toggle->setText(m && m->state == ModuleState::Disabled ? tr("Réactiver") : tr("Désactiver"));
    if (!m)
    {
        m_details->setPlainText(m_registry.modules().empty() ? tr("Aucun module trouvé.\nDossier utilisateur : %1").arg(TSA::Core::AppPaths::userModulesDir())
                                                             : QString());
        return;
    }
    QStringList t;
    t << tr("%1 — %2 %3").arg(m->manifest.id, m->manifest.name, m->manifest.version.toString());
    if (!m->manifest.description.isEmpty()) t << m->manifest.description;
    t << tr("Éditeur : %1    Licence : %2").arg(m->manifest.publisher, m->manifest.license);
    t << tr("Dossier : %1").arg(m->dir);
    t << tr("Capacités : %1").arg(m->manifest.capabilities.join(", "));
    for (const auto& c : m->manifest.importers) t << tr("Import : %1 (*.%2)").arg(c.title, c.extensions.join(" *."));
    for (const auto& c : m->manifest.exporters) t << tr("Export : %1 (*.%2)").arg(c.title, c.extensions.join(" *."));
    for (const auto& tpl : m->manifest.templates) t << tr("Template : %1").arg(tpl);
    t << tr("Empreinte du manifeste (SHA-256) : %1").arg(m->manifestSha256);
    if (!m->messages.isEmpty()) t << QString() << tr("Messages :") << m->messages;
    m_details->setPlainText(t.join('\n'));
}

void ModulesDialog::reload()
{
    m_registry.shutdown(m_services);
    m_registry.discover({ TSA::Core::AppPaths::shippedModulesDir() }, { TSA::Core::AppPaths::userModulesDir() });
    m_registry.activate(m_services);
    populate();
    emit modulesChanged();
}

} // namespace TSA::UI
