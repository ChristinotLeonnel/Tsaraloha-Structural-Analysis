#include "MeshAnalysisBridge.h"

#include "../../Analysis/Engine/AnalysisContext.h"
#include "../../Analysis/Engine/AnalysisEngine.h"
#include "MeshController.h"

#include <QMessageBox>
#include <QPushButton>
#include <QStringList>

namespace TSA::UI
{

using TSA::Mesh::SolverCheckItem;

bool applyMeshToAnalysis(MeshController* mesh, TSA::Analysis::AnalysisContext& ctx, const TSA::Analysis::AnalysisEngine& engine, QWidget* parent,
                         const MeshLogFn& log)
{
    ctx.memberSubdivisions.clear();
    ctx.surfaces = {};
    if (!mesh || !mesh->hasMesh() || !ctx.common.useGeneratedMesh) return true;
    const QString engineName = QString::fromStdString(engine.info().name);
    if (mesh->stale())
    {
        QMessageBox box(QMessageBox::Warning, QObject::tr("Maillage obsolète"),
                        QObject::tr("La géométrie a été modifiée depuis la génération du maillage : il ne correspond plus au modèle."), QMessageBox::NoButton,
                        parent);
        auto* regen = box.addButton(QObject::tr("Régénérer le maillage"), QMessageBox::AcceptRole);
        auto* without = box.addButton(QObject::tr("Calculer sans maillage (un élément par barre)"), QMessageBox::DestructiveRole);
        box.addButton(QMessageBox::Cancel);
        box.exec();
        if (box.clickedButton() == regen) mesh->generate(false);
        return box.clickedButton() == without;
    }
    const auto compat = mesh->solverCompatibility(engine.info().name, engine.capabilities());
    QStringList lines;
    for (const auto& i : compat.items)
    {
        if (i.level == SolverCheckItem::Level::Ok) continue;
        QString objs;
        for (std::size_t k = 0; k < i.objects.size() && k < 8; ++k) objs += (k ? ", " : " (") + QString::fromStdString(i.objects[k]);
        if (!objs.isEmpty()) objs += i.objects.size() > 8 ? QStringLiteral(", …)") : QStringLiteral(")");
        lines << QStringLiteral("• [%1] %2%3")
                     .arg(i.level == SolverCheckItem::Level::Blocking ? QObject::tr("bloquant") : QObject::tr("avertissement"),
                          QString::fromStdString(i.message), objs);
    }
    if (log)
    {
        log(QObject::tr("Maillage → %1 : %2").arg(engineName, QString::fromStdString(compat.summary())), QStringLiteral("INFO"));
        for (const auto& l : lines) log(l, QStringLiteral("WARN"));
    }
    if (compat.blocking())
    {
        QStringList strategies;
        for (const auto& s : compat.strategies) strategies << QStringLiteral("• ") + QString::fromStdString(s);
        QMessageBox box(QMessageBox::Warning, QObject::tr("Maillage non transmissible à %1").arg(engineName),
                        QObject::tr("%1\n\nStratégies compatibles :\n%2").arg(lines.join('\n'), strategies.join('\n')), QMessageBox::NoButton, parent);
        auto* without = box.addButton(QObject::tr("Calculer sans maillage (un élément par barre)"), QMessageBox::AcceptRole);
        box.addButton(QMessageBox::Cancel);
        box.exec();
        return box.clickedButton() == without;
    }
    if (!lines.isEmpty())
    {
        if (QMessageBox::warning(parent, QObject::tr("Maillage et %1").arg(engineName),
                                 QObject::tr("Le maillage est transmis en partie :\n\n%1\n\nLancer le calcul ?").arg(lines.join('\n')),
                                 QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes) != QMessageBox::Yes)
            return false;
    }
    ctx.memberSubdivisions = compat.memberSubdivisions;
    ctx.surfaces = compat.surfaces;
    ctx.surfaces.meshStamp = mesh->project().record.generated + "|" + mesh->project().record.settingsHash;
    ctx.meshLabel = QObject::tr("maillage %1 du %2")
                        .arg(QString::fromStdString(mesh->project().record.mesher), QString::fromStdString(mesh->project().record.generated))
                        .toStdString();
    return true;
}

} // namespace TSA::UI
