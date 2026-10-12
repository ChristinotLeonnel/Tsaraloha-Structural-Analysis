#pragma once

// Transmission du maillage généré (Gmsh) au calcul, partagée par les fenêtres de TSA et de TSALab (docs/MESH.md).
// Le maillage n'est transmis que si l'option du calcul le demande, s'il est à jour et compatible avec le moteur
// (adaptateur OpenSees), sans conversion silencieuse : maillage obsolète ou non transmissible → choix explicite de
// l'utilisateur (régénérer, calculer sans maillage, annuler).

#include <QString>

#include <functional>

class QWidget;

namespace TSA::Analysis
{
struct AnalysisContext;
class AnalysisEngine;
}

namespace TSA::UI
{

class MeshController;

/// Journal de l'application : (texte, type « INFO » / « WARN »).
using MeshLogFn = std::function<void(const QString&, const QString&)>;

/// Remplit ctx.memberSubdivisions, ctx.surfaces et ctx.meshLabel à partir du maillage de `mesh` (vidés sinon).
/// Faux si l'utilisateur annule le calcul. `mesh` peut être nul (aucun maillage : calcul sans maillage).
bool applyMeshToAnalysis(MeshController* mesh, TSA::Analysis::AnalysisContext& ctx, const TSA::Analysis::AnalysisEngine& engine, QWidget* parent,
                         const MeshLogFn& log = {});

} // namespace TSA::UI
