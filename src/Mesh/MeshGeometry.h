#pragma once

#include "../Model/SurfaceFormulation.h"

// Géométrie maillable extraite du modèle TSA : barres (1D), surfaces (dalles avec ouvertures, voiles :
// 2D), volumes (semelles : parallélépipèdes, 3D), points caractéristiques (appuis, charges ponctuelles,
// changements de section, connexions) et empreinte servant à détecter un maillage obsolète.

#include "MeshTypes.h"

#include <string>
#include <vector>

namespace TSA::Model
{
class Model;
class Slab;
class Wall;
}

namespace TSA::Mesh
{

struct P3
{
    double x = 0, y = 0, z = 0;
    P3 operator+(const P3& o) const { return { x + o.x, y + o.y, z + o.z }; }
    P3 operator-(const P3& o) const { return { x - o.x, y - o.y, z - o.z }; }
    P3 operator*(double k) const { return { x * k, y * k, z * k }; }
    double dot(const P3& o) const { return x * o.x + y * o.y + z * o.z; }
    P3 cross(const P3& o) const { return { y * o.z - z * o.y, z * o.x - x * o.z, x * o.y - y * o.x }; }
    double norm() const;
    P3 normalized() const;
};
double distance(const P3& a, const P3& b);
/// Distance d'un point au segment [a, b].
double distanceToSegment(const P3& p, const P3& a, const P3& b);

/// Repère d'une surface plane : P = origin + u·U + v·V, n normale.
struct PlaneFrame
{
    P3 origin, U { 1, 0, 0 }, V { 0, 1, 0 }, N { 0, 0, 1 };
    P3 toGlobal(double u, double v) const { return origin + U * u + V * v; }
    void toLocal(const P3& p, double* u, double* v) const;
};
/// Repère local d'une dalle (origine au premier nœud, U vers le deuxième, N de Newell) ; utilisé pour les ouvertures.
PlaneFrame slabFrame(const std::vector<P3>& corners);

struct MemberGeom
{
    std::string ref;            ///< B12, C3, T4, K1
    int family = 0;             ///< 0 poutre, 1 poteau, 2 treillis, 3 câble (ordre de StructuralElementKind)
    int id = 0;
    int nodeI = 0, nodeJ = 0;
    P3 a, b;
    double length = 0;
    std::string section;
    std::string material;
    std::vector<double> pointLoads;   ///< positions absolues des charges ponctuelles (m depuis le nœud I)
};

struct SurfaceGeom
{
    enum class Kind
    {
        Slab,
        Wall
    };
    std::string ref;            ///< SL2, W5
    Kind kind = Kind::Slab;
    int id = 0;
    std::vector<P3> outer;      ///< contour (sommets)
    std::vector<int> cornerNodes;   ///< nœud TSA de chaque sommet (0 si aucun)
    std::vector<std::vector<P3>> holes;
    PlaneFrame frame;
    double area = 0;
    std::string material;
    double thickness = 0;
    TSA::Model::SurfaceFormulation formulation = TSA::Model::SurfaceFormulation::Shell;
};

struct VolumeGeom
{
    std::string ref;            ///< F1
    int id = 0;
    int nodeId = 0;
    P3 nodePos;                 ///< centre de la face supérieure
    P3 min, max;                ///< boîte alignée sur les axes
    std::string material;
};

struct CharacteristicPoint
{
    enum class Kind
    {
        Support,
        PointLoad,
        SectionChange,
        Connection
    };
    Kind kind = Kind::Support;
    P3 p;
    int nodeId = 0;
    std::string target;         ///< barre porteuse (charge ponctuelle)
};

struct MeshGeometry
{
    std::vector<MemberGeom> members;
    std::vector<SurfaceGeom> surfaces;
    std::vector<VolumeGeom> volumes;
    std::vector<CharacteristicPoint> points;
    std::map<int, P3> tsaNodes;
    std::string hash;           ///< empreinte (SHA-256) de tout ce qui influence le maillage

    static MeshGeometry extract(const TSA::Model::Model& model);
    const MemberGeom* member(const std::string& ref) const;
    const SurfaceGeom* surface(const std::string& ref) const;
    const VolumeGeom* volume(const std::string& ref) const;
    /// Toutes les références maillables.
    std::vector<std::string> refs() const;
};

/// Ouvertures d'une dalle en coordonnées globales (affichage, maillage).
std::vector<std::vector<P3>> slabOpenings3D(const TSA::Model::Model& model, const TSA::Model::Slab& slab);
/// Vérifie qu'une ouverture est contenue dans la dalle et ne recoupe pas les autres ; message vide = valide.
std::string validateSlabOpening(const TSA::Model::Model& model, const TSA::Model::Slab& slab, const std::vector<std::pair<double, double>>& polygon,
                                int ignoreIndex = -1);

/// Ouverture d'un voile (repère local : u le long du voile depuis le nœud de départ, v vers le haut) : contenue dans le
/// rectangle du voile, sans recouper le contour ni les autres ouvertures ; message vide = valide.
std::string validateWallOpening(const TSA::Model::Model& model, const TSA::Model::Wall& wall, const std::vector<std::pair<double, double>>& polygon,
                                int ignoreIndex = -1);

/// Point dans un polygone 2D (règle pair-impair).
bool pointInPolygon(double u, double v, const std::vector<std::pair<double, double>>& poly);

} // namespace TSA::Mesh
