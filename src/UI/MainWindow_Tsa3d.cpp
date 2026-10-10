// Échange TSA3D (docs/TSA3D.md) : l'interface relie les actions aux services de src/IO/Tsa3d (lecture,
// validation, import, export), sans logique de format propre. Import via un module (docs/SDK.md) : le
// convertisseur du module s'exécute dans un processus séparé et produit un TSA3D, importé comme un fichier.

#include "MainWindow.h"

#include "../Analysis/ResultsModel.h"
#include "../Coordinate/LevelManager.h"
#include "../IO/Tsa3d/Tsa3d.h"
#include "../Modules/ModuleRegistry.h"
#include "../Model/Model.h"
#include "../Project/ProjectManager.h"
#include "../Viewer/OccView.h"
#include "Common/EcosystemApplication.h"
#include "Dialogs/ModulesDialog.h"
#include "Dock/LogConsoleDock.h"
#include "ModelTree/ModelTreeWidget.h"
#include "Ruler/ViewportContainer.h"

#include <QApplication>
#include <QFileDialog>
#include <QFileInfo>
#include <QLabel>
#include <QMessageBox>
#include <QTemporaryDir>

#include <algorithm>

namespace
{
QString firstLines(const QStringList& lines, int max = 12)
{
    QStringList out = lines.mid(0, max);
    if (lines.size() > max) out << QObject::tr("… %1 autre(s), voir la console.").arg(lines.size() - max);
    return out.join('\n');
}
} // namespace

void MainWindow::onActionExportTsa3d()
{
    if (!m_model) return;
    const QString base = (m_projectManager && m_projectManager->hasFilePath()) ? QFileInfo(m_projectManager->currentFilePath()).completeBaseName()
                                                                               : QStringLiteral("Projet");
    QString path = QFileDialog::getSaveFileName(this, tr("Exporter au format TSA3D"), base + ".tsa3d",
                                                tr("TSA3D (*.tsa3d);;JSON (*.json);;Tous les fichiers (*.*)"));
    if (path.isEmpty()) return;
    if (QFileInfo(path).suffix().isEmpty()) path += ".tsa3d";

    TSA::IO::Tsa3d::ExportOptions opt;
    opt.projectName = base;
    if (m_resultsModel && m_resultsModel->isValid())
    {
        opt.results = m_resultsModel.get();
        opt.includeMesh = true;
        opt.includeResults = QMessageBox::question(this, tr("Export TSA3D"),
                                                   tr("Joindre les déplacements et réactions du dernier calcul ?\n"
                                                      "Ils seront marqués « calculés, non validés » ; le maillage du solveur est joint.")) ==
                             QMessageBox::Yes;
    }
    const auto ex = TSA::IO::Tsa3d::exportModel(*m_model, opt);
    QString err;
    if (!TSA::IO::Tsa3d::writeFile(path, ex.document, &err))
    {
        QMessageBox::critical(this, tr("Export TSA3D"), err);
        return;
    }
    const auto check = TSA::IO::Tsa3d::validate(ex.document);   // contrôle de ce qui vient d'être écrit
    if (m_consoleDock)
    {
        m_consoleDock->appendLog(tr("TSA3D exporté : %1").arg(path), "INFO");
        for (const auto& l : ex.report.lines()) m_consoleDock->appendLog(l, "WARN");
        for (const auto& l : check.lines())
            if (l.startsWith("ERREUR")) m_consoleDock->appendLog(l, "ERROR");
    }
    if (m_statusInfo) m_statusInfo->setText(tr("TSA3D exporté"));
    QString msg = tr("Fichier TSA3D %1 enregistré :\n%2").arg(TSA::IO::Tsa3d::versionString(), path);
    if (!ex.report.issues.empty()) msg += "\n\n" + tr("Données non représentées :") + "\n" + firstLines(ex.report.lines());
    QMessageBox::information(this, tr("Export TSA3D"), msg);
}

void MainWindow::onActionImportTsa3d()
{
    if (!m_model) return;
    const QString path = QFileDialog::getOpenFileName(this, tr("Importer un fichier TSA3D"), QString(),
                                                      tr("TSA3D (*.tsa3d *.json);;Tous les fichiers (*.*)"));
    if (path.isEmpty()) return;
    importTsa3dFile(path, path);
}

void MainWindow::onActionImportViaModule()
{
    if (!m_model) return;
    auto& registry = TSA::Modules::ModuleRegistry::instance();
    const auto importers = registry.importers();
    if (importers.empty())
    {
        QMessageBox::information(this, tr("Importer via un module"),
                                 tr("Aucun module actif ne fournit de convertisseur d'import.\nVoir Aide > Modules."));
        return;
    }
    // Filtres de la boîte d'ouverture : un par convertisseur (le filtre choisi désigne le convertisseur).
    QStringList filters;
    for (const auto& c : importers) filters << tr("%1 — %2 (*.%3)").arg(c.title, c.moduleId, c.extensions.join(" *."));
    QString chosen;
    const QString input = QFileDialog::getOpenFileName(this, tr("Importer via un module"), QString(), filters.join(";;"), &chosen);
    if (input.isEmpty()) return;
    const int index = std::max<qsizetype>(0, filters.indexOf(chosen));
    const auto& conv = importers[size_t(index)];

    // Conversion dans un processus séparé vers un fichier TSA3D temporaire, puis import TSA3D habituel
    // (validation complète : le module n'écrit jamais dans le modèle).
    QTemporaryDir tmp;
    if (!tmp.isValid())
    {
        QMessageBox::critical(this, tr("Importer via un module"), tr("Dossier temporaire indisponible."));
        return;
    }
    const QString output = tmp.filePath(QStringLiteral("converted.tsa3d"));
    QString log;
    QApplication::setOverrideCursor(Qt::WaitCursor);
    const bool ok = registry.runConverter(conv, input, output, &log);
    QApplication::restoreOverrideCursor();
    if (m_consoleDock)
        for (const auto& l : log.split('\n', Qt::SkipEmptyParts)) m_consoleDock->appendLog(QStringLiteral("[%1] %2").arg(conv.id, l), ok ? "INFO" : "ERROR");
    if (!ok)
    {
        QMessageBox::critical(this, tr("Importer via un module"), tr("Conversion échouée (%1) :\n\n%2").arg(conv.id, log.trimmed()));
        return;
    }
    importTsa3dFile(output, input);
}

void MainWindow::onActionModules()
{
    TSA::UI::ModulesDialog dlg(TSA::Modules::ModuleRegistry::instance(), TSA::UI::EcosystemApplication::moduleHostServices(), this);
    dlg.exec();
}

void MainWindow::importTsa3dFile(const QString& path, const QString& sourceLabel)
{

    // Lecture et validation AVANT de créer le projet : un fichier invalide ne modifie rien.
    QJsonObject doc;
    TSA::IO::Tsa3d::Report report;
    if (!TSA::IO::Tsa3d::readFile(path, &doc, &report))
    {
        QMessageBox::critical(this, tr("Import TSA3D"), firstLines(report.lines()));
        return;
    }
    report = TSA::IO::Tsa3d::validate(doc);
    if (report.hasErrors())
    {
        if (m_consoleDock)
            for (const auto& l : report.lines()) m_consoleDock->appendLog(l, l.startsWith("ERREUR") ? "ERROR" : "WARN");
        QMessageBox::critical(this, tr("Import TSA3D"), tr("Fichier refusé (%1 erreur(s)) :\n\n%2").arg(report.count(TSA::IO::Tsa3d::Severity::Error)).arg(firstLines(report.lines())));
        return;
    }

    const auto before = m_model->revision();
    onActionNew();
    if (m_model->revision() == before && !m_model->nodes().empty()) return;   // nouveau projet annulé

    QApplication::setOverrideCursor(Qt::WaitCursor);
    const auto r = TSA::IO::Tsa3d::importDocument(doc, *m_model);
    m_model->clearUndoRedo();
    QApplication::restoreOverrideCursor();
    if (!r.ok)
    {
        QMessageBox::critical(this, tr("Import TSA3D"), firstLines(r.report.lines()));
        return;
    }
    const QString name = doc.value("metadata").toObject().value("name").toString(QFileInfo(sourceLabel).completeBaseName());
    if (m_viewportContainer && m_model->levelManager())
        m_viewportContainer->updateLevelsList(m_model->levelManager()->elevationList(), m_model->levelManager()->levelNames());
    if (m_modelTree)
    {
        m_modelTree->setProjectName(name + " (TSA3D)");
        m_modelTree->refreshAll();
    }
    if (m_occView)
    {
        m_occView->rebuildGrid();
        m_occView->fitModel();
    }
    const QString summary = tr("%1 nœuds, %2 barres, %3 surfaces, %4 fondations, %5 cas, %6 charges, %7 combinaisons")
                                .arg(r.nodes).arg(r.members).arg(r.surfaces).arg(r.foundations).arg(r.loadCases).arg(r.loads).arg(r.combinations);
    if (m_consoleDock)
    {
        m_consoleDock->appendLog(tr("TSA3D importé : %1 — %2").arg(sourceLabel, summary), "INFO");
        for (const auto& l : r.report.lines()) m_consoleDock->appendLog(l, l.startsWith("AVERT") ? "WARN" : "INFO");
    }
    if (m_statusInfo) m_statusInfo->setText(tr("TSA3D importé : %1").arg(summary));
    QString msg = tr("%1\n\n%2").arg(path, summary);
    const int warnings = r.report.count(TSA::IO::Tsa3d::Severity::Warning);
    if (warnings) msg += "\n\n" + tr("Points à vérifier (%1) :").arg(warnings) + "\n" + firstLines(r.report.lines());
    msg += "\n\n" + tr("Champs et extensions non interprétés : conservés dans le projet et restitués à l'export TSA3D.");
    QMessageBox::information(this, tr("Import TSA3D"), msg);
}
