// Fichiers liés, bibliothèques requises et historique des enregistrements du projet (bloc PMAN du .tsa,
// docs/PROJECT_STATE_PLAN.md §3.5) : enregistrement des liens, contrôle à l'ouverture, boîte « Fichiers liés ».
#include "MainWindow.h"

#include "Dock/LogConsoleDock.h"
#include "../ExtensionSystem/LibraryManager.h"
#include "../Model/Model.h"
#include "../Project/ProjectManager.h"
#include "../Project/ProjectManifest.h"

#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QSettings>
#include <QTableWidget>
#include <QVBoxLayout>

namespace
{
const QString kStoreAbsoluteLinkPathsKey = QStringLiteral("Project/storeAbsoluteLinkPaths");

std::vector<std::pair<std::string, std::string>> installedLibraries()
{
    std::vector<std::pair<std::string, std::string>> out;
    for (const auto& m : TSA::ExtensionSystem::LibraryManager::instance().installedExtensions())
        out.emplace_back(m.id, m.version.toString());
    return out;
}

QString statusText(TSA::Project::LinkStatus s)
{
    switch (s)
    {
    case TSA::Project::LinkStatus::Present: return QObject::tr("présent");
    case TSA::Project::LinkStatus::Modified: return QObject::tr("modifié depuis");
    case TSA::Project::LinkStatus::Missing: return QObject::tr("introuvable");
    }
    return {};
}
} // namespace

bool MainWindow::storesAbsoluteLinkPaths()
{
    return QSettings().value(kStoreAbsoluteLinkPathsKey, true).toBool();
}

void MainWindow::registerLinkedFile(const QString& role, const QString& path)
{
    if (!m_manifest || !m_model || path.isEmpty())
        return;
    const std::string project = m_projectManager && m_projectManager->hasFilePath() ? m_projectManager->currentFilePath().toStdString() : std::string();
    if (!m_manifest->registerLink(role.toStdString(), path.toStdString(), project))
        return;
    // Le lien fait partie du projet : à enregistrer (révision inchangée, aucun résultat invalidé).
    m_model->setUnsavedFlag(true);
    updateWindowTitle();
}

void MainWindow::reportManifestOnOpen(const QString& projectFile)
{
    if (!m_manifest || !m_consoleDock)
        return;
    int missing = 0, changed = 0;
    for (const auto& c : m_manifest->checkLinks(projectFile.toStdString()))
    {
        const QString name = QString::fromStdString(c.link.relativePath.empty() ? c.link.absolutePath : c.link.relativePath);
        const QString role = QString::fromStdString(TSA::Project::linkRoleLabel(c.link.role));
        if (c.status == TSA::Project::LinkStatus::Missing)
        {
            ++missing;
            m_consoleDock->appendLog(tr("Fichier lié introuvable (%1) : %2").arg(role, name), "WARN");
        }
        else if (c.status == TSA::Project::LinkStatus::Modified)
        {
            ++changed;
            m_consoleDock->appendLog(tr("Fichier lié modifié depuis son enregistrement (%1) : %2").arg(role, name), "WARN");
        }
    }
    for (const auto& c : m_manifest->checkLibraries(installedLibraries()))
    {
        if (!c.installed)
            m_consoleDock->appendLog(tr("Bibliothèque requise absente de ce poste : %1 %2 — calcul reproductible (propriétés figées dans le "
                                        "projet), définitions indisponibles pour de nouveaux éléments.")
                                         .arg(QString::fromStdString(c.requirement.id), QString::fromStdString(c.requirement.version)),
                                     "WARN");
        else if (!c.sameVersion())
            m_consoleDock->appendLog(tr("Bibliothèque %1 : version %2 installée, %3 utilisée par le projet.")
                                         .arg(QString::fromStdString(c.requirement.id), QString::fromStdString(c.installedVersion),
                                              QString::fromStdString(c.requirement.version)),
                                     "WARN");
    }
    if (!m_manifest->links().empty() && missing + changed == 0)
        m_consoleDock->appendLog(tr("%n fichier(s) lié(s) au projet, tous présents et inchangés.", nullptr,
                                    static_cast<int>(m_manifest->links().size())),
                                 "SYS");
    if (!m_manifest->saves().empty())
    {
        const auto& last = m_manifest->saves().back();
        m_consoleDock->appendLog(tr("Dernier enregistrement : %1 par %2 (%3, format %4).")
                                     .arg(QString::fromStdString(last.timestamp),
                                          last.author.empty() ? tr("auteur non renseigné") : QString::fromStdString(last.author),
                                          QString::fromStdString(last.application), QString::fromStdString(last.format)),
                                 "SYS");
    }
}

void MainWindow::onActionLinkedFiles()
{
    if (!m_manifest || !m_model)
        return;
    const QString projectFile = m_projectManager && m_projectManager->hasFilePath() ? m_projectManager->currentFilePath() : QString();

    QDialog dialog(this);
    dialog.setWindowTitle(tr("Fichiers liés et bibliothèques"));
    dialog.resize(860, 520);
    auto* layout = new QVBoxLayout(&dialog);
    layout->addWidget(new QLabel(tr("Fichiers reliés au projet (contrôlés à chaque ouverture, jamais ouverts automatiquement) :"), &dialog));
    auto* links = new QTableWidget(0, 4, &dialog);
    links->setHorizontalHeaderLabels({ tr("Rôle"), tr("Fichier"), tr("État"), tr("Modifié le") });
    links->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    links->setSelectionBehavior(QAbstractItemView::SelectRows);
    links->setEditTriggers(QAbstractItemView::NoEditTriggers);
    layout->addWidget(links, 3);
    layout->addWidget(new QLabel(tr("Bibliothèques utilisées par le projet :"), &dialog));
    auto* libraries = new QTableWidget(0, 4, &dialog);
    libraries->setHorizontalHeaderLabels({ tr("Bibliothèque"), tr("Version du projet"), tr("Sur ce poste"), tr("Définitions") });
    libraries->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    libraries->setEditTriggers(QAbstractItemView::NoEditTriggers);
    layout->addWidget(libraries, 2);
    auto* absolute = new QCheckBox(tr("Enregistrer aussi les chemins absolus (décocher pour un fichier à transmettre : ils peuvent "
                                      "révéler le nom d'utilisateur)"),
                                   &dialog);
    absolute->setChecked(storesAbsoluteLinkPaths());
    layout->addWidget(absolute);

    auto refresh = [&] {
        const auto checks = m_manifest->checkLinks(projectFile.toStdString());
        links->setRowCount(static_cast<int>(checks.size()));
        for (int r = 0; r < static_cast<int>(checks.size()); ++r)
        {
            const auto& c = checks[static_cast<std::size_t>(r)];
            const QString shown = QString::fromStdString(!c.resolvedPath.empty() ? c.resolvedPath
                                                         : (!c.link.absolutePath.empty() ? c.link.absolutePath : c.link.relativePath));
            links->setItem(r, 0, new QTableWidgetItem(QString::fromStdString(TSA::Project::linkRoleLabel(c.link.role))));
            links->setItem(r, 1, new QTableWidgetItem(shown));
            links->setItem(r, 2, new QTableWidgetItem(statusText(c.status)));
            links->setItem(r, 3, new QTableWidgetItem(QString::fromStdString(c.link.modified)));
        }
        m_manifest->collectLibraries(*m_model);
        const auto libs = m_manifest->checkLibraries(installedLibraries());
        libraries->setRowCount(static_cast<int>(libs.size()));
        for (int r = 0; r < static_cast<int>(libs.size()); ++r)
        {
            const auto& c = libs[static_cast<std::size_t>(r)];
            QStringList defs;
            for (const auto& d : c.requirement.definitions) defs << QString::fromStdString(d);
            libraries->setItem(r, 0, new QTableWidgetItem(QString::fromStdString(c.requirement.id)));
            libraries->setItem(r, 1, new QTableWidgetItem(QString::fromStdString(c.requirement.version)));
            libraries->setItem(r, 2, new QTableWidgetItem(!c.installed ? tr("absente")
                                                          : c.sameVersion() ? tr("installée")
                                                                            : tr("version %1").arg(QString::fromStdString(c.installedVersion))));
            libraries->setItem(r, 3, new QTableWidgetItem(defs.join(QStringLiteral(", "))));
        }
    };
    refresh();

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
    QPushButton* attach = buttons->addButton(tr("Joindre un document…"), QDialogButtonBox::ActionRole);
    QPushButton* remove = buttons->addButton(tr("Retirer le lien"), QDialogButtonBox::ActionRole);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(attach, &QPushButton::clicked, &dialog, [&] {
        const QString file = QFileDialog::getOpenFileName(&dialog, tr("Joindre un document au projet"));
        if (file.isEmpty()) return;
        registerLinkedFile(QStringLiteral("attachment"), file);
        refresh();
    });
    connect(remove, &QPushButton::clicked, &dialog, [&] {
        const int row = links->currentRow();
        if (row < 0 || row >= static_cast<int>(m_manifest->links().size())) return;
        const auto link = m_manifest->links()[static_cast<std::size_t>(row)];
        if (m_manifest->removeLink(link.role, link.absolutePath.empty() ? link.relativePath : link.absolutePath))
        {
            m_model->setUnsavedFlag(true);
            updateWindowTitle();
        }
        refresh();
    });
    connect(absolute, &QCheckBox::toggled, &dialog, [](bool on) { QSettings().setValue(kStoreAbsoluteLinkPathsKey, on); });
    dialog.exec();
}
