// Échange openBIM (IFC) : l'interface relie les actions aux services de src/BIM/IFC, sans logique
// BIM propre (aucune propriété IFC codée ici).

#include "MainWindow.h"
#include "../BIM/IFC/IfcExporter.h"
#include "../BIM/IFC/IfcImporter.h"
#include "../Coordinate/LevelManager.h"
#include "../Model/Model.h"
#include "../Project/ProjectManager.h"
#include "../Viewer/OccView.h"
#include "Dock/LogConsoleDock.h"
#include "ModelTree/ModelTreeWidget.h"
#include "Ruler/ViewportContainer.h"

#include <QApplication>
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
    QApplication::setOverrideCursor(Qt::WaitCursor);
    const auto r = TSA::BIM::Ifc::IfcExporter::exportToFile(*m_model, path.toStdString(), opt);
    QApplication::restoreOverrideCursor();
    if (!r.ok)
    {
        QMessageBox::critical(this, tr("Export IFC"), QString::fromStdString(r.summary()));
        return;
    }
    if (m_consoleDock)
    {
        m_consoleDock->appendLog(tr("IFC exporté : %1 — %2").arg(path, QString::fromStdString(r.summary())), "BIM");
        for (const auto& w : r.warnings) m_consoleDock->appendLog(QString::fromStdString(w), "WARN");
    }
    if (m_statusInfo) m_statusInfo->setText(tr("IFC exporté"));
    QString msg = tr("Fichier IFC 4.3 enregistré :\n%1\n\n%2").arg(path, QString::fromStdString(r.summary()));
    if (!r.warnings.empty()) msg += "\n\n" + tr("Points à vérifier :") + "\n" + joinWarnings(r.warnings);
    msg += "\n\n" + tr("Implémentation partielle : validez le fichier avec un outil tiers avant diffusion (aucune certification revendiquée).");
    QMessageBox::information(this, tr("Export IFC"), msg);
}

void MainWindow::onActionImportIfc()
{
    if (!m_model) return;
    const QString path = QFileDialog::getOpenFileName(this, tr("Importer un fichier IFC"), QString(),
                                                      tr("IFC (*.ifc);;Tous les fichiers (*.*)"));
    if (path.isEmpty()) return;

    // L'import crée un nouveau projet (même parcours que Fichier > Nouveau, avec enregistrement proposé)
    const auto before = m_model->revision();
    onActionNew();
    if (m_model->revision() == before && !m_model->nodes().empty()) return;   // nouveau projet annulé

    QApplication::setOverrideCursor(Qt::WaitCursor);
    const auto r = TSA::BIM::Ifc::IfcImporter::importFile(path.toStdString(), *m_model);
    // Reconstruction complète des vues (onModelCleared) puis état initial de l'historique
    m_model->restoreSnapshot(m_model->createSnapshot("Import IFC"));
    m_model->clearUndoRedo();
    QApplication::restoreOverrideCursor();

    if (!r.ok)
    {
        QMessageBox::critical(this, tr("Import IFC"), QString::fromStdString(r.summary()));
        return;
    }

    const QString name = QFileInfo(path).completeBaseName();
    if (m_viewportContainer && m_model->levelManager())
        m_viewportContainer->updateLevelsList(m_model->levelManager()->elevationList(), m_model->levelManager()->levelNames());
    if (m_modelTree)
    {
        m_modelTree->setProjectName(name + " (IFC)");
        m_modelTree->refreshAll();
    }
    if (m_occView)
    {
        m_occView->rebuildGrid();
        m_occView->fitModel();
    }
    if (m_consoleDock)
    {
        m_consoleDock->appendLog(tr("IFC importé : %1 — %2").arg(path, QString::fromStdString(r.summary())), "BIM");
        for (const auto& w : r.warnings) m_consoleDock->appendLog(QString::fromStdString(w), "WARN");
    }
    if (m_statusInfo) m_statusInfo->setText(tr("IFC importé"));
    QString msg = tr("%1\n\n%2").arg(path, QString::fromStdString(r.summary()));
    if (!r.warnings.empty()) msg += "\n\n" + tr("Points à vérifier :") + "\n" + joinWarnings(r.warnings);
    msg += "\n\n" + tr("Vérifiez le modèle analytique (appuis, relâchements, charges) avant tout calcul.");
    QMessageBox::information(this, tr("Import IFC"), msg);
}
