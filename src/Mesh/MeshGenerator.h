#pragma once

// Façade de génération de maillage (docs/MESH.md). Le moteur de maillage est Gmsh (SDK, API C++),
// encapsulé dans GmshMeshEngine : aucun autre module de TSA n'inclut l'API Gmsh. Cette façade fournit
// les capacités réelles du moteur (l'interface ne propose que ce qu'il sait faire), la génération,
// l'import / export de maillages, une estimation rapide et l'empreinte des paramètres.

#include "MeshGeometry.h"
#include "MeshTypes.h"

#include <functional>
#include <string>
#include <vector>

namespace TSA::Mesh
{

/// Ce que le moteur sait réellement faire.
struct MesherCapabilities
{
    bool available = false;            ///< SDK présent et initialisable
    std::string unavailableReason;
    std::string id = "gmsh";
    std::string name = "Gmsh";
    std::string version;               ///< version de la bibliothèque chargée
    std::string license = "GNU GPL v2 ou ultérieure (avec exceptions), voir licenses/Gmsh";
    bool supports1D = true, supports2D = true, supports3D = true;
    bool secondOrder = true;
    std::vector<Algorithm2D> algorithms2D;
    std::vector<Algorithm3D> algorithms3D;
    std::vector<RecombinationAlgorithm> recombinationAlgorithms;
    bool tetrahedra = true;
    bool structuredHexahedra = true;   ///< hexaèdres transfinis : volumes à 6 faces uniquement
    std::string structuredRestriction = "surfaces à 3 ou 4 côtés sans ouverture, volumes à 6 faces";
    std::vector<std::string> exportFormats;   ///< extensions (msh, vtk, unv, inp, bdf, mesh)
    std::vector<std::string> importFormats;
};
MesherCapabilities meshEngineCapabilities();
const char* algorithm2DName(Algorithm2D a);
const char* algorithm3DName(Algorithm3D a);
const char* recombinationName(RecombinationAlgorithm a);

struct MeshSettingsSet
{
    GlobalMeshSettings global;
    std::vector<LocalMeshSetting> locals;
    std::vector<RefinementZone> zones;
    std::string profile;
};

struct MeshEstimate
{
    std::size_t nodes = 0, elements1D = 0, elements2D = 0, elements3D = 0;
    std::size_t elements() const { return elements1D + elements2D + elements3D; }
    std::vector<std::string> notes;
};

struct GenerationOptions
{
    /// Consulté entre les étapes (géométrie, 1D, 2D, 3D, extraction) : Gmsh ne s'interrompt pas au milieu d'une étape.
    std::function<bool()> cancelled;
    std::function<void(int percent, const std::string& status)> progress;
    /// Objets à mailler (vide : tous ceux des dimensions activées).
    std::vector<std::string> only;
    /// Fichier .msh à écrire en plus (export natif Gmsh, groupes physiques compris) ; vide : aucun.
    std::string writeMshTo;
};

struct GenerationResult
{
    bool ok = false;
    bool cancelled = false;
    FeMesh mesh;
    GenerationRecord record;
};

class MeshGenerator
{
public:
    static GenerationResult generate(const MeshGeometry& geometry, const MeshSettingsSet& settings, const GenerationOptions& options = {});
    /// Estimation rapide (sans moteur) à partir des longueurs, aires et volumes et des tailles imposées.
    static MeshEstimate estimate(const MeshGeometry& geometry, const MeshSettingsSet& settings);
    static std::string settingsHash(const MeshSettingsSet& settings);
    /// Export (format déduit de l'extension) avec groupes physiques nommés d'après les références TSA.
    static bool exportMesh(const FeMesh& mesh, const std::string& path, std::string* error);
    /// Import : les groupes physiques nommés d'après des références TSA sont rattachés à ces objets.
    static bool importMesh(const std::string& path, FeMesh* mesh, std::vector<std::string>* warnings, std::string* error);
};

/// Taille de référence effective pour un objet (taille locale, sinon globale × finesse × facteur ; bornée).
double effectiveSizeFor(const MeshSettingsSet& settings, const std::string& ref);
/// Paramètres locaux applicables à un objet (le dernier défini l'emporte), nullptr sinon.
const LocalMeshSetting* localSettingFor(const MeshSettingsSet& settings, const std::string& ref);

} // namespace TSA::Mesh
