#pragma once

// Contrôle de qualité d'un maillage (docs/MESH.md §G) : rapport d'aspect, angles, jacobien normalisé
// (quadrangles, hexaèdres), éléments dégénérés ou inversés, nœuds isolés ou confondus, connexions
// incorrectes (nœud inconnu ou répété, arête partagée par plus de deux éléments d'une même surface),
// incompatibilités entre régions (nœud d'une région au milieu d'une arête d'une autre), croissance des
// tailles 1D, nombre d'éléments et de degrés de liberté. Une génération réussie n'est PAS un critère de
// qualité ni de convergence : ce contrôle et l'étude de convergence le sont.

#include "MeshTypes.h"

#include <string>
#include <vector>

namespace TSA::Mesh
{

struct QualityIssue
{
    enum class Severity
    {
        Error,
        Warning,
        Info
    };
    Severity severity = Severity::Warning;
    std::string kind;           ///< « rapport d'aspect », « élément inversé »…
    std::string message;
    int element = 0;            ///< élément concerné (0 : aucun)
    int node = 0;               ///< nœud concerné (0 : aucun)
    double x = 0, y = 0, z = 0; ///< position pour la localisation
    std::string source;
    double value = 0;
};

struct MetricStats
{
    double min = 0, max = 0, mean = 0;
    std::size_t count = 0;
};

struct QualityReport
{
    std::vector<QualityIssue> issues;
    MetricStats aspect, minAngle, maxAngle, jacobian, growth;
    std::size_t nodes = 0, elements = 0, elements1D = 0, elements2D = 0, elements3D = 0;
    std::size_t estimatedDofs = 0;   ///< 6 DDL par nœud de barre ou de coque, 3 par nœud de volume
    int errors = 0, warnings = 0;
    std::vector<double> perElementQuality;   ///< 1 = parfait, 0 = inacceptable (index = id - 1)

    bool acceptable() const { return errors == 0; }
};

QualityReport analyzeMesh(const FeMesh& mesh, const QualityCriteria& criteria, const std::vector<MeshInterface>& interfaces = {});

const char* severityName(QualityIssue::Severity s);

} // namespace TSA::Mesh
