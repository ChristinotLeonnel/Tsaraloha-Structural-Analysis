#pragma once

// Présentations OCCT des modes de représentation (ModelDisplayMode.h) qui s'ajoutent aux objets du
// modèle sans les remplacer. Objets non sélectionnables (la sélection reste sur les éléments du modèle),
// dans le calque supérieur (jamais masqués par les solides, pas de conflit de profondeur), une
// présentation par famille : quelques objets OCCT quel que soit le nombre de barres.
//
//  - AnalyticalModelRenderer   : axes analytiques nœud à nœud (mode Superposition).
//  - FiniteElementMeshRenderer : maillage du solveur (mode Éléments finis), à partir de SolverMesh.

#include "ModelDisplayMode.h"

#include <AIS_InteractiveContext.hxx>
#include <AIS_Shape.hxx>

#include <map>
#include <vector>

namespace TSA::Viewer
{

class AnalyticalModelRenderer
{
public:
    explicit AnalyticalModelRenderer(const Handle(AIS_InteractiveContext)& context) : m_context(context) {}
    ~AnalyticalModelRenderer() { clear(); }

    /// Axes à tracer, par famille (segments nœud à nœud des éléments affichés).
    void rebuild(const std::map<TSA::Analysis::StructuralElementKind, std::vector<std::pair<gp_Pnt, gp_Pnt>>>& axes);
    void clear();
    bool isShown() const { return !m_objects.empty(); }

private:
    Handle(AIS_InteractiveContext) m_context;
    std::vector<Handle(AIS_Shape)> m_objects;
};

class FiniteElementMeshRenderer
{
public:
    explicit FiniteElementMeshRenderer(const Handle(AIS_InteractiveContext)& context) : m_context(context) {}
    ~FiniteElementMeshRenderer() { clear(); }

    void rebuild(const SolverMeshGeometry& geometry);
    void clear();
    bool isShown() const { return !m_objects.empty(); }

private:
    Handle(AIS_InteractiveContext) m_context;
    std::vector<Handle(AIS_Shape)> m_objects;
};

} // namespace TSA::Viewer
