// Échange openBIM (IFC) : l'interface relie les actions aux services de src/BIM/IFC, sans logique
// BIM propre (aucune propriété IFC codée ici).
//
// Import et export s'exécutent dans un thread de travail, derrière une fenêtre de progression modale (annulable pour
// l'import) : l'interface reste dessinée, et aucune autre commande ne peut démarrer pendant ce temps. Le thread ne
// touche jamais au projet ouvert : l'import remplit un modèle temporaire, ouvert ensuite dans un nouvel onglet (les
// projets ouverts ne sont jamais remplacés) ; l'export lit une copie (instantané).

#include "MainWindow.h"
#include "../BIM/Core/BimModel.h"
#include "../BIM/IFC/IfcExporter.h"
#include "../BIM/IFC/IfcImporter.h"
#include "../Coordinate/LevelManager.h"
#include "../Model/Model.h"
#include "../Project/ProjectManager.h"
#include "../Viewer/OccView.h"
#include "Dialogs/IfcDialogs.h"
#include "Dialogs/TaskProgressDialog.h"
#include "Dock/LogConsoleDock.h"
#include "ModelTree/ModelTreeWidget.h"
#include "Ruler/ViewportContainer.h"
#include "Widgets/ProjectStatusOverlay.h"

#include <QApplication>
#include <QMetaMethod>
#include <QFileDialog>
#include <QFileInfo>
#include <QLabel>
#include <QMessageBox>

namespace
{
QString joinWarnings(const std::vector<std::string>& warnings, int max = 8)
{
    QStringList lines;
    for (std::size_t i = 0; i < warnings.size() && static_cast<int>(i) < max; ++i) lines << QString::fromStdString(warnings[i]);
    if (static_cast<int>(warnings.size()) > max) lines << QObject::tr("… %1 autre(s), voir la console.").arg(warnings.size() - max);
    return lines.join("\n");
}
} // namespace

void MainWindow::onActionExportIfc()
{
    if (!m_model) return;
    const QString base = (m_projectManager && m_projectManager->hasFilePath())
        ? QFileInfo(m_projectManager->currentFilePath()).completeBaseName()
        : QStringLiteral("Projet");
    QString path = QFileDialog::getSaveFileName(this, tr("Exporter au format IFC"), base + ".ifc",
                                                tr("IFC 4.3 (*.ifc);;Tous les fichiers (*.*)"));
    if (path.isEmpty()) return;
    if (!path.endsWith(".ifc", Qt::CaseInsensitive)) path += ".ifc";

    TSA::BIM::Ifc::IfcExportOptions opt;
    opt.projectName = base.toStdString();

    // Copie du projet (instantané) : le thread de travail ne lit jamais le modèle vivant, dont la couche BIM est
    // synchronisée à la demande par l'interface.
    TSA::Model::Model copy;
    copy.restoreSnapshot(m_model->createSnapshot("Export IFC"));

    TSA::UI::TaskProgressDialog dialog(tr("Export IFC"), tr("Écriture de %1…").arg(QFileInfo(path).fileName()), this);
    dialog.setRange(0, 0);              // durée inconnue (écriture puis relecture indépendante)
    dialog.setCancelButton(nullptr);    // l'écriture atomique n'est pas interruptible ; rapide
    TSA::UI::TaskProgress progress;
    TSA::BIM::Ifc::IfcExportReport r;
    const std::string target = path.toStdString();
    const std::string failure = TSA::UI::runTaskInWorker(dialog, progress, [&] { r = TSA::BIM::Ifc::IfcExporter::exportToFile(copy, target, opt); });
    if (!failure.empty())
    {
        r.ok = false;
        r.error = failure;
    }

    if (!r.ok)
    {
        if (m_consoleDock) m_consoleDock->appendLog(tr("Export IFC échoué : %1").arg(QString::fromStdString(r.error)), "ERROR");
        QMessageBox::critical(this, tr("Export IFC"), QString::fromStdString(r.summary()));
        return;
    }
    if (m_consoleDock)
    {
        m_consoleDock->appendLog(tr("IFC exporté : %1 — %2").arg(path, QString::fromStdString(r.summary())), "BIM");
        for (const auto& w : r.warnings) m_consoleDock->appendLog(QString::fromStdString(w), "WARN");
    }
    registerLinkedFile(QStringLiteral("export-ifc"), path);
    if (m_statusInfo) m_statusInfo->setText(tr("IFC exporté"));
    QString msg = tr("Fichier IFC 4.3 enregistré :\n%1\n\n%2").arg(path, QString::fromStdString(r.summary()));
    if (r.verifiedElements < 0)
        msg += "\n\n" + tr("Relecture indépendante non effectuée dans cette version : validez le fichier avec un outil tiers avant diffusion.");
    if (!r.warnings.empty()) msg += "\n\n" + tr("Points à vérifier :") + "\n" + joinWarnings(r.warnings);
    msg += "\n\n" + tr("Aucune certification buildingSMART n'est revendiquée.");
    QMessageBox::information(this, tr("Export IFC"), msg);
}

void MainWindow::onActionImportIfc()
{
    if (!m_model) return;
    const QString path = QFileDialog::getOpenFileName(this, tr("Importer un fichier IFC"), QString(),
                                                      tr("IFC (*.ifc);;Tous les fichiers (*.*)"));
    if (path.isEmpty()) return;

    TSA::UI::IfcImportOptionsDialog optionsDialog(path, this);
    if (optionsDialog.exec() != QDialog::Accepted) return;
    TSA::BIM::Ifc::IfcImportOptions options = optionsDialog.options();

    // Lecture dans un modèle temporaire : le projet ouvert n'est remplacé qu'après un import réussi
    // (un fichier illisible, sans élément exploitable ou un import annulé le laissent intact).
    TSA::UI::TaskProgress progress;
    options.progress = [&progress](int percent, const std::string& stage) {
        progress.publish(percent, stage);
        return !progress.cancel.load();
    };
    auto imported = std::make_shared<TSA::Model::Model>();
    TSA::BIM::Ifc::IfcImportReport r;
    TSA::UI::TaskProgressDialog dialog(tr("Import IFC"), tr("Lecture de %1…").arg(QFileInfo(path).fileName()), this);
    const std::string source = path.toStdString();
    const std::string failure = TSA::UI::runTaskInWorker(dialog, progress, [&] { r = TSA::BIM::Ifc::IfcImporter::importFile(source, *imported, options); });
    if (!failure.empty())
    {
        r.ok = false;
        r.error = "erreur pendant l'import : " + failure;
    }

    if (r.canceled || (progress.cancel && !r.ok))
    {
        // Interruption demandée : pas une erreur, aucune boîte critique (comme pour le calcul).
        if (m_consoleDock) m_consoleDock->appendLog(tr("Import IFC annulé : %1 — le projet ouvert n'a pas été modifié.").arg(path), "WARN");
        if (m_statusInfo) m_statusInfo->setText(tr("Import IFC annulé"));
        return;
    }
    if (!r.ok)
    {
        if (m_consoleDock)
        {
            m_consoleDock->appendLog(tr("Import IFC échoué : %1 — %2").arg(path, QString::fromStdString(r.summary())), "ERROR");
            for (const auto& w : r.warnings) m_consoleDock->appendLog(QString::fromStdString(w), "WARN");
        }
        QString msg = QString::fromStdString(r.summary());
        if (!r.warnings.empty()) msg += "\n\n" + tr("Détail :") + "\n" + joinWarnings(r.warnings);
        msg += "\n\n" + tr("Le projet ouvert n'a pas été modifié.");
        QMessageBox::critical(this, tr("Import IFC"), msg);
        return;
    }

    // Projet importé ouvert dans un nouvel onglet par l'hôte (comme Fichier > Ouvrir) : ce document et son historique
    // restent intacts. Sans hôte (fenêtre seule), installé ici seulement si le document est vide.
    if (isSignalConnected(QMetaMethod::fromSignal(&MainWindow::importedProjectReady)))
    {
        emit importedProjectReady(imported, path, std::make_shared<const TSA::BIM::Ifc::IfcImportReport>(r));
        return;
    }
    if (!m_model->nodes().empty() || !m_model->bim().elements().empty() || hasUnsavedChanges())
    {
        QMessageBox::warning(this, tr("Import IFC"), tr("Ouvrez un projet vide pour recevoir l'import : le projet ouvert n'a pas été modifié."));
        return;
    }
    adoptImportedProject(*imported, path, r);
}

void MainWindow::adoptImportedProject(const TSA::Model::Model& imported, const QString& path, const TSA::BIM::Ifc::IfcImportReport& r)
{
    if (!m_model) return;
    // Reconstruction complète des vues (onModelCleared) puis état initial de l'historique ; projet sans fichier,
    // à enregistrer (fermeture : enregistrement proposé)
    m_model->restoreSnapshot(imported.createSnapshot("Import IFC"));
    m_model->clearUndoRedo();
    m_model->setModified(true);
    registerLinkedFile(QStringLiteral("import-ifc"), path);   // source de l'import, contrôlée à chaque ouverture

    const QString name = QFileInfo(path).completeBaseName();
    if (m_projectManager) m_projectManager->setProjectName(name);
    updateWindowTitle();
    if (m_projectStatusWidget)
    {
        m_projectStatusWidget->setModel(m_model);
        m_projectStatusWidget->setProjectInfo(name, QString());
        m_projectStatusWidget->refreshStatus();
    }
    if (m_viewportContainer && m_model->levelManager())
        m_viewportContainer->updateLevelsList(m_model->levelManager()->elevationList(), m_model->levelManager()->levelNames());
    if (m_modelTree)
    {
        m_modelTree->setProjectName(name + " (IFC)");
        m_modelTree->refreshAll();
    }
    if (m_occView)
    {
        // Objets importés montrés, bouton « Objets IFC importés » accordé (sans relancer son action)
        m_occView->setBimGeometryVisible(true);
        syncDisplayActionsFromView();
        m_occView->rebuildGrid();
        // Onglet neuf : la vue OCCT n'existe qu'à son premier affichage ; le cadrage sur le projet importé est alors fait
        // à la place du cadrage général (qui englobe la grille du gabarit et réduit le bâtiment à un coin de la vue).
        m_occView->fitModelWhenReady();
    }
    if (m_consoleDock)
    {
        m_consoleDock->appendLog(tr("IFC importé : %1 — %2").arg(path, QString::fromStdString(r.summary())), "BIM");
        for (const auto& w : r.warnings) m_consoleDock->appendLog(QString::fromStdString(w), "WARN");
    }
    if (m_statusInfo) m_statusInfo->setText(tr("IFC importé : %1 produit(s)").arg(r.products));

    TSA::UI::IfcImportReportDialog report(path, r, this);
    report.exec();
}
