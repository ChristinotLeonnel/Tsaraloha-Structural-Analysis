// tsa-mesh : maillage d'un projet TSA par Gmsh en ligne de commande (docs/MESH.md). Utilise les paramètres
// enregistrés dans le projet (chunk MESH) ou une taille imposée ; écrit le maillage (format déduit de
// l'extension : msh, vtk, unv, inp, bdf, mesh) et affiche le rapport et la synthèse de qualité.
//
//   tsa-mesh <projet.tsa | modèle.tsa3d> <sortie.msh> [taille globale m] [--3d] [--quads] [--order2]
//
// Code de sortie : 0 succès, 1 échec (projet illisible, Gmsh indisponible, maillage refusé), 2 usage.

#include "IO/TSAFile.h"
#include "IO/Tsa3d/Tsa3d.h"
#include "Mesh/MeshProject.h"
#include "Mesh/MeshQuality.h"
#include "Model/Model.h"

#include <QCoreApplication>
#include <QStringList>

#include <cstdio>

#ifdef _WIN32
#include <windows.h>
#endif

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
#ifdef _WIN32
    SetConsoleOutputCP(65001);   // UTF-8
#endif
    QStringList args = app.arguments().mid(1);
    const bool volumes = args.removeAll(QStringLiteral("--3d")) > 0;
    const bool quads = args.removeAll(QStringLiteral("--quads")) > 0;
    const bool order2 = args.removeAll(QStringLiteral("--order2")) > 0;
    if (args.size() < 2)
    {
        std::fprintf(stderr, "Usage : tsa-mesh <projet.tsa> <sortie.msh> [taille globale m] [--3d] [--quads] [--order2]\n");
        return 2;
    }
    const auto caps = TSA::Mesh::meshEngineCapabilities();
    if (!caps.available)
    {
        std::fprintf(stderr, "Maillage indisponible : %s\n", caps.unavailableReason.c_str());
        return 1;
    }
    std::printf("Moteur : %s %s\n", caps.name.c_str(), caps.version.c_str());
    TSA::Model::Model model;
    std::string err;
    if (args[0].endsWith(QStringLiteral(".tsa3d"), Qt::CaseInsensitive))
    {
        // Fichier d'échange TSA3D (docs/TSA3D.md) : importé (validé) dans un modèle, puis maillé.
        QJsonObject doc;
        TSA::IO::Tsa3d::Report report;
        if (!TSA::IO::Tsa3d::readFile(args[0], &doc, &report) || !TSA::IO::Tsa3d::importDocument(doc, model).ok)
        {
            std::fprintf(stderr, "Fichier TSA3D refusé : %s\n", report.lines().join(QStringLiteral(" ; ")).toUtf8().constData());
            return 1;
        }
    }
    else
    {
        TSA::IO::TSAFileReader reader;
        if (!reader.loadFromFile(args[0].toStdString(), model, nullptr, "", nullptr, nullptr, nullptr, &err))
        {
            std::fprintf(stderr, "Projet illisible : %s\n", err.c_str());
            return 1;
        }
    }
    TSA::Mesh::MeshProject project = TSA::Mesh::MeshProject::load(model);
    auto& s = project.settings;
    if (args.size() > 2) s.global.globalSize = args[2].replace(',', '.').toDouble();
    if (volumes) s.global.mesh3D = true;
    if (quads) s.global.recombine = true;
    if (order2) s.global.order = TSA::Mesh::ElementOrder::Second;
    const auto geometry = TSA::Mesh::MeshGeometry::extract(model);
    TSA::Mesh::GenerationOptions o;
    o.writeMshTo = args[1].endsWith(QStringLiteral(".msh"), Qt::CaseInsensitive) ? args[1].toStdString() : std::string();
    const auto r = TSA::Mesh::MeshGenerator::generate(geometry, s, o);
    for (const auto& w : r.record.warnings) std::printf("avertissement : %s\n", w.c_str());
    for (const auto& e : r.record.errors) std::fprintf(stderr, "ERREUR : %s\n", e.c_str());
    if (!r.ok) return 1;
    if (o.writeMshTo.empty() && !TSA::Mesh::MeshGenerator::exportMesh(r.mesh, args[1].toStdString(), &err))
    {
        std::fprintf(stderr, "Écriture impossible : %s\n", err.c_str());
        return 1;
    }
    const auto q = TSA::Mesh::analyzeMesh(r.mesh, s.global.quality, r.record.interfaces);
    std::printf("%zu nœuds, %zu éléments (1D %zu, 2D %zu, 3D %zu), %zu groupes, %.0f ms\n", r.mesh.nodes.size(), q.elements, q.elements1D, q.elements2D,
                q.elements3D, r.mesh.groups.size(), r.record.durationMs);
    std::printf("qualité : %d erreur(s), %d avertissement(s), minSICN min %.3f, ≈ %zu DDL\n", q.errors, q.warnings, q.jacobian.count ? q.jacobian.min : 1.0,
                q.estimatedDofs);
    std::printf("%s écrit\n", args[1].toUtf8().constData());
    return 0;
}
