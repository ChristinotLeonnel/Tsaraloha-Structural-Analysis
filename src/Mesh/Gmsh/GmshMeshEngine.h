#pragma once

// Adaptateur Gmsh (SDK 4.15, API C++ « gmsh.h_cwrap » sur l'API C) — docs/MESH.md.
//
// Seul composant de TSA qui appelle Gmsh. Responsabilités :
//   - cycle de vie : initialisation / fermeture par opération, sous verrou (l'API Gmsh a un état global et
//     n'est pas réentrante) ; DLL chargée en différé : absence détectée et signalée, jamais de plantage ;
//   - construction de la géométrie (noyau « geo ») depuis MeshGeometry : arêtes et sommets partagés par
//     construction (conformité barre / bord de dalle / dalles voisines / pied de voile), points et lignes
//     plongés (nœud TSA dans une dalle, voile posé sur une dalle), semelles découpées en 4 volumes autour
//     du nœud d'appui ;
//   - paramètres : tailles (champs MathEval / Constant / Threshold / Distance / Restrict / Min), contraintes
//     transfinies (divisions, progressions, maillage structuré), recombinaison, algorithmes, ordre ;
//   - extraction vers FeMesh avec correspondance explicite (étiquettes Gmsh → identifiants TSA, jamais
//     utilisées comme identifiants universels), groupes physiques, qualité « minSICN » ;
//   - import / export de fichiers de maillage (msh, vtk, unv, inp, bdf, mesh…).

#include "../MeshGenerator.h"

namespace TSA::Mesh::GmshMeshEngine
{

/// Capacités réelles ; available = faux si la DLL est absente ou si TSA a été compilé sans le SDK.
MesherCapabilities capabilities();
GenerationResult generate(const MeshGeometry& geometry, const MeshSettingsSet& settings, const GenerationOptions& options);
bool exportMesh(const FeMesh& mesh, const std::string& path, std::string* error);
bool importMesh(const std::string& path, FeMesh* mesh, std::vector<std::string>* warnings, std::string* error);

/// Type d'élément Gmsh → type TSA (faux si non pris en charge par le modèle de maillage TSA).
bool fromGmshType(int gmshType, FeType* type);
int toGmshType(FeType type);

} // namespace TSA::Mesh::GmshMeshEngine
