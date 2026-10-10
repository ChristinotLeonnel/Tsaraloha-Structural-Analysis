#pragma once

// Maillage éléments finis de TSA (docs/MESH.md) : paramètres (globaux, locaux, zones de raffinement,
// profils), maillage produit, rapport de génération. Couche modèle : aucune dépendance à l'interface.
// Le maillage est produit par Gmsh (src/Mesh/Gmsh/GmshMeshEngine) ; ces types en sont indépendants.
//
// Cibles : un objet du modèle est désigné par sa référence stable « préfixe + id » (B12 poutre, C3 poteau,
// T4 treillis, K1 câble, SL2 dalle, W5 voile, F1 fondation = volume), comme dans TSA3D.

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace TSA::Mesh
{

enum class ElementOrder
{
    First = 1,
    Second = 2
};

/// Niveau de finesse : facteur appliqué à la taille globale (Personnalisé : 1).
enum class Fineness
{
    VeryCoarse,
    Coarse,
    Standard,
    Fine,
    VeryFine,
    Custom
};
double finenessFactor(Fineness f);

/// Répartition des divisions d'une barre.
enum class Distribution
{
    Uniform,      ///< divisions égales
    Progressive,  ///< tailles en progression géométrique depuis le nœud de départ
    Symmetric     ///< progression depuis les deux extrémités vers le milieu
};

/// Algorithmes 2D de Gmsh (option Mesh.Algorithm ; valeur = code Gmsh).
enum class Algorithm2D
{
    Automatic = 2,
    MeshAdapt = 1,
    Delaunay = 5,
    FrontalDelaunay = 6,
    FrontalDelaunayQuads = 8,
    PackingParallelograms = 9,
    QuasiStructuredQuad = 11
};

/// Algorithmes 3D de Gmsh (option Mesh.Algorithm3D ; valeur = code Gmsh).
enum class Algorithm3D
{
    Delaunay = 1,
    Frontal = 4,
    HXT = 10
};

/// Recombinaison des triangles en quadrangles (option Mesh.RecombinationAlgorithm).
enum class RecombinationAlgorithm
{
    Simple = 0,
    Blossom = 1,
    SimpleFullQuad = 2,
    BlossomFullQuad = 3
};

/// Éléments de volume : tétraèdres (non structuré) ou hexaèdres (transfini, volumes à 6 faces seulement).
enum class VolumeElement
{
    Tetrahedra,
    Hexahedra
};

enum class SurfaceShape
{
    Triangles,
    Quadrilaterals
};

enum class FeType
{
    Line2,
    Line3,
    Tri3,
    Tri6,
    Quad4,
    Quad8,
    Hex8,
    Hex20,
    Tet4,
    Tet10
};
const char* feTypeName(FeType t);
int feTypeDimension(FeType t);
int feTypeCornerCount(FeType t);
bool feTypeIsSecondOrder(FeType t);

struct QualityCriteria
{
    double maxAspectRatio = 5.0;       ///< rapport d'aspect normalisé (1 = élément idéal)
    double minAngleDeg = 20.0;         ///< angle minimal (triangles, quadrangles, faces)
    double maxAngleDeg = 150.0;
    double minScaledJacobian = 0.2;    ///< quadrangles, hexaèdres (1 = parfait, ≤ 0 = inversé)
    double maxGrowthRatio = 2.5;       ///< rapport de tailles entre éléments 1D voisins
    double minSICN = 0.1;              ///< qualité Gmsh « minSICN » (1 = parfait, < 0 = élément inversé)
};

/// Raffinement automatique autour d'un type d'entité.
struct AutoRefinement
{
    bool enabled = false;
    double size = 0.10;        ///< taille au voisinage (m)
    double radius = 0.50;      ///< rayon de la zone à taille constante (m)
};

struct GlobalMeshSettings
{
    bool mesh1D = true;
    bool mesh2D = true;
    bool mesh3D = false;
    double globalSize = 0.50;
    double minSize = 0.02;
    double maxSize = 5.0;
    Fineness fineness = Fineness::Standard;
    double refinementFactor = 1.0;     ///< facteur multiplicatif supplémentaire (< 1 : plus fin)
    double transitionRate = 0.3;       ///< croissance de la taille hors d'une zone (m de taille par m de distance)
    ElementOrder order = ElementOrder::First;
    std::string method = "gmsh";       ///< moteur de maillage (seul disponible : Gmsh)
    Algorithm2D algorithm2D = Algorithm2D::Automatic;
    Algorithm3D algorithm3D = Algorithm3D::Delaunay;
    VolumeElement volumeElement = VolumeElement::Tetrahedra;
    bool preferStructured = true;      ///< maillage transfini quand la géométrie le permet (surfaces à 3-4 côtés, volumes à 6 faces)
    bool recombine = false;            ///< triangles → quadrangles
    RecombinationAlgorithm recombination = RecombinationAlgorithm::Blossom;
    int smoothing = 1;                 ///< itérations de lissage (Mesh.Smoothing)
    bool optimize = true;              ///< optimisation des tétraèdres (Mesh.Optimize)
    bool boundaryElements = false;     ///< éléments de frontière (lignes des contours de surfaces, faces des volumes)
    bool physicalGroups = true;        ///< un groupe par objet source (et par frontière)
    double geometricTolerance = 1e-6;  ///< m
    bool keepCharacteristicPoints = true;   ///< nœuds aux charges ponctuelles, centres de semelles, coins
    int maxElements = 500000;          ///< garde-fou de ressources
    QualityCriteria quality;
    AutoRefinement nearSupports, nearPointLoads, nearSectionChanges, nearOpenings, nearConnections;

    /// Taille de référence effective (globale × finesse × facteur, bornée).
    double effectiveSize() const;
};

struct PortionSize
{
    double from = 0.0, to = 1.0;   ///< positions relatives sur la barre (0 = nœud de départ)
    double size = 0.1;
};

/// Paramètres locaux appliqués à des cibles (sans modifier les autres objets).
struct LocalMeshSetting
{
    std::string name;
    std::vector<std::string> targets;   ///< références (B12, SL2…)
    std::optional<double> size;         ///< taille locale (m)
    // 1D
    std::optional<int> divisions;
    std::optional<double> maxLength;
    Distribution distribution = Distribution::Uniform;
    double progression = 1.2;           ///< rapport entre deux divisions successives (Progressive, Symmetric)
    std::optional<double> endSize;      ///< raffinement aux extrémités (taille au nœud)
    std::vector<PortionSize> portions;  ///< tailles sur des portions de barre
    // 2D
    std::optional<int> divisionsU, divisionsV;
    std::optional<SurfaceShape> shape;
    std::optional<Algorithm2D> algorithm;
    std::optional<bool> structured;     ///< imposer / refuser le maillage transfini pour ces objets

    bool appliesTo(const std::string& ref) const;
};

/// Zone de raffinement locale (point, nœud, appui, charge) : taille constante dans le rayon, puis
/// transition progressive vers la taille environnante (GlobalMeshSettings::transitionRate).
struct RefinementZone
{
    enum class Kind
    {
        Point,
        Node,
        Support,
        Load
    };
    std::string name;
    Kind kind = Kind::Point;
    double x = 0, y = 0, z = 0;          ///< centre (Point ; renseigné pour les autres à la génération)
    int nodeId = 0;                      ///< Node / Support
    double radius = 0.5;
    std::optional<double> size;          ///< taille dans la zone
    std::optional<double> factor;        ///< ou facteur appliqué à la taille environnante
    std::vector<std::string> targets;    ///< vide : tous les objets
};

/// Profil : ensemble de paramètres réutilisable et modifiable.
struct MeshProfile
{
    std::string name;
    std::string category = "general";    ///< general, beam, slab, wall, bridge, volume
    std::string description;
    bool builtIn = false;
    GlobalMeshSettings global;
};

// --- Maillage produit -----------------------------------------------------------------------------

struct FeNode
{
    int id = 0;                 ///< 1..N
    double x = 0, y = 0, z = 0;
    int tsaNode = 0;            ///< nœud TSA confondu (0 : nœud du maillage)
    std::string source;         ///< objet ayant créé le nœud
};

struct FeElement
{
    int id = 0;
    FeType type = FeType::Line2;
    std::vector<int> nodes;     ///< ids de nœuds, ordre Gmsh (coins puis milieux d'arêtes)
    std::string source;         ///< référence de l'objet d'origine
    bool boundary = false;      ///< élément de frontière (ligne de contour, face de volume)
    double quality = 2.0;       ///< qualité Gmsh minSICN (2 : non calculée)
};

struct FeGroup
{
    std::string name;           ///< ex. « B12 », « SL2:contour »
    int dimension = 1;
    std::vector<int> elements;
    std::vector<int> nodes;     ///< groupe de nœuds (appuis, points de charge, interfaces) ; vide sinon
};

struct FeMesh
{
    std::vector<FeNode> nodes;          ///< nodes[i].id == i + 1
    std::vector<FeElement> elements;    ///< elements[i].id == i + 1
    std::vector<FeGroup> groups;

    bool empty() const { return elements.empty(); }
    const FeNode& node(int id) const { return nodes[std::size_t(id - 1)]; }
    std::size_t countOf(FeType t) const;
    std::size_t countByDimension(int dim) const;
    /// Positions relatives intérieures (0 < s < 1, triées) des nœuds d'une barre (éléments 1D de la source).
    std::vector<double> memberInteriorPositions(const std::string& ref) const;
};

/// Interface entre objets dont le maillage n'est pas conforme (nœuds non partagés).
struct MeshInterface
{
    std::string a, b;           ///< références des objets
    std::string description;
    bool conforming = true;
};

struct GenerationRecord
{
    std::string mesher;                 ///< « Gmsh »
    std::string mesherVersion;
    std::string generated;              ///< date ISO
    double durationMs = 0;
    std::string settingsHash;           ///< empreinte des paramètres utilisés
    std::string geometryHash;           ///< empreinte de la géométrie maillée
    std::string profile;                ///< profil utilisé
    std::vector<std::string> warnings, errors;
    std::vector<MeshInterface> interfaces;
    std::map<std::string, std::string> parameters;   ///< paramètres effectifs (lisibles)
    std::vector<std::string> engineLog;               ///< messages du moteur (avertissements, erreurs)
};

} // namespace TSA::Mesh
