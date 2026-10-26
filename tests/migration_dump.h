#pragma once

// Relevé des propriétés de section, des termes de rigidité des éléments et des résultats d'un calcul OpenSees pour un
// projet .tsa (BUG-110, démonstration de la migration 1.x → 2.0). N'utilise que l'API antérieure à la migration
// (Section::rootRadius, area, iy…), pour être compilé à l'identique par l'ancien et le nouveau code et comparer les
// relevés au bit près (réels écrits en hexadécimal IEEE-754, %a).

#include "Analysis/Engine/AnalysisManager.h"
#include "Grid/GridManager.h"
#include "IO/TSAFile.h"
#include "Model/Load/LoadManager.h"
#include "Model/Model.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <cmath>
#include <cstdio>
#include <string>

namespace MigrationDump
{

inline QString hex(double v)
{
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%a", v);
    return QString::fromLatin1(buf);
}

/// Termes de la matrice de rigidité locale d'une barre 3D (Euler-Bernoulli) : EA/L, GJ/L, 12EI/L³, 6EI/L², 4EI/L, 2EI/L
/// pour les deux axes — les entrées de la matrice 12 × 12 sont ces termes au signe près.
template <class Element>
QJsonObject element(const TSA::Model::Model& m, const Element& e)
{
    const auto& s = e.section();
    const auto& mat = e.material();
    const auto* a = m.getNode(e.startNodeId());
    const auto* b = m.getNode(e.endNodeId());
    const double L = (a && b) ? std::hypot(b->x() - a->x(), b->y() - a->y(), b->z() - a->z()) : 0.0;
    const double E = mat.mechanical.youngModulus, G = E / (2.0 * (1.0 + mat.mechanical.poissonRatio));
    QJsonObject o { { "name", QString::fromStdString(s.name) }, { "r", hex(s.rootRadius()) }, { "A", hex(s.area()) }, { "Iy", hex(s.iy()) },
                    { "Iz", hex(s.iz()) }, { "It", hex(s.it()) }, { "Wy", hex(s.wy()) }, { "Wz", hex(s.wz()) } };
    if (L > 0.0)
    {
        QJsonArray k;
        for (double v : { E * s.area() / L, G * s.it() / L, 12 * E * s.iz() / (L * L * L), 6 * E * s.iz() / (L * L), 4 * E * s.iz() / L,
                          2 * E * s.iz() / L, 12 * E * s.iy() / (L * L * L), 6 * E * s.iy() / (L * L), 4 * E * s.iy() / L, 2 * E * s.iy() / L })
            k.append(hex(v));
        o.insert("k", k);
    }
    return o;
}

/// Relevé complet d'un projet : propriétés et rigidités de chaque barre ; calcul OpenSees statique du premier cas de
/// charge (poids propre compris), déplacements et réactions. `error` : motif d'un chargement ou d'un calcul impossible.
inline QJsonObject dumpProject(const TSA::Model::Model& m, const std::string& label)
{
    QJsonObject out { { "project", QString::fromStdString(label) } };
    QJsonObject beams, columns, trusses;
    for (const auto& [id, e] : m.beams()) beams.insert(QString::number(id), element(m, e));
    for (const auto& [id, e] : m.columns()) columns.insert(QString::number(id), element(m, e));
    for (const auto& [id, e] : m.trussMembers()) trusses.insert(QString::number(id), element(m, e));
    out.insert("beams", beams);
    out.insert("columns", columns);
    out.insert("trusses", trusses);

    if (m.loadManager().loadCases().empty() || (m.beams().empty() && m.columns().empty() && m.trussMembers().empty()))
        return out;
    TSA::Analysis::AnalysisEngineRegistry reg;
    TSA::Analysis::registerBuiltInEngines(reg);
    TSA::Analysis::AnalysisManager mgr(reg);
    TSA::Analysis::AnalysisContext ctx;
    ctx.engineId = "opensees";
    ctx.loadCaseIds = { m.loadManager().loadCases().begin()->first };
    ctx.common.includeSelfWeight = true;
    TSA::Grid::GridManager gm;
    const auto prep = mgr.prepare(m, &gm, ctx);
    if (!prep.canRun())
    {
        out.insert("analysis", QStringLiteral("préparation refusée"));
        return out;
    }
    const auto run = mgr.run(ctx, prep);
    if (!run.success)
    {
        out.insert("analysis", QStringLiteral("échec : ") + QString::fromStdString(run.message));
        return out;
    }
    QJsonObject disp, reac;
    for (const auto& [id, d] : run.results.allDisplacements())
        disp.insert(QString::number(id), QJsonArray { hex(d.ux), hex(d.uy), hex(d.uz), hex(d.rx), hex(d.ry), hex(d.rz) });
    for (const auto& [id, r] : run.results.allReactions())
        reac.insert(QString::number(id), QJsonArray { hex(r.rx), hex(r.ry), hex(r.rz), hex(r.mx), hex(r.my), hex(r.mz) });
    out.insert("analysis", QStringLiteral("ok"));
    out.insert("displacements", disp);
    out.insert("reactions", reac);
    return out;
}

/// Relève chaque fichier de `files` dans `outPath` (JSON, une entrée par fichier).
inline bool dumpFiles(const QStringList& files, const QString& outPath)
{
    QJsonArray all;
    for (const QString& f : files)
    {
        TSA::Model::Model m;
        TSA::Grid::GridManager gm;
        QString name, author;
        if (!TSA::IO::TSAProjectIO::loadProject(f, m, &gm, &name, &author))
        {
            all.append(QJsonObject { { "project", f }, { "error", QStringLiteral("chargement refusé") } });
            continue;
        }
        all.append(dumpProject(m, f.toStdString()));
    }
    QFile out(outPath);
    return out.open(QIODevice::WriteOnly) && out.write(QJsonDocument(all).toJson(QJsonDocument::Indented)) > 0;
}

/// Réenregistre chaque fichier dans `dir` au format courant (même nom) : fichiers 2.0 de la démonstration.
inline bool resaveFiles(const QStringList& files, const QString& dir)
{
    bool ok = true;
    for (const QString& f : files)
    {
        TSA::Model::Model m;
        TSA::Grid::GridManager gm;
        if (!TSA::IO::TSAProjectIO::loadProject(f, m, &gm)) continue;
        TSA::IO::TSAFileWriter writer;
        std::string err;
        ok = writer.saveToFile((dir + "/" + QFileInfo(f).fileName()).toStdString(), m, &gm, "Migration", "TSA", &err) && ok;
    }
    return ok;
}

} // namespace MigrationDump
