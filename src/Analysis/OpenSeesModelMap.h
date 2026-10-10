#pragma once

// Correspondance unique TSA ↔ OpenSees, construite depuis le snapshot de calcul.
// Le générateur de script (OpenSeesAnalysisBuilder) et le lecteur de résultats
// (OpenSeesResultsReader) utilisent la même instance : aucun des deux ne « devine » un tag.
//
//  - Nœuds        : tag OpenSees = id du nœud TSA.
//  - Éléments     : tag unique 1..N (SnapshotElement::tag) ↔ (famille, id TSA).
//  - Ressorts     : nœud auxiliaire entièrement fixé (tag > max id nœud TSA) + élément
//                   zeroLength (tag > N) ; leurs réactions sont reportées sur le nœud TSA.
//  - DDL          : modèle BasicBuilder -ndm 3 -ndf 6 → [UX UY UZ RX RY RZ] pour tous les nœuds.

#include "AnalysisTypes.h"
#include "CalculationSnapshot.h"
#include "ElementTransformation.h"
#include "ResultsModel.h"

#include <map>
#include <string>
#include <vector>

namespace TSA::Analysis
{

struct AnalysisParameters;

struct OpsElementEntry
{
    ElementKey key;
    int tag = 0;
    std::string opsClass;        ///< "elasticBeamColumn", "truss", "corotTruss"
    int nodeI = 0;
    int nodeJ = 0;
    int transfTag = 0;           ///< 0 si pas de geomTransf (treillis, câbles)
    Vec3 vecxz {};               ///< vecteur transmis à geomTransf (axe z local TSA)
    LocalAxes axes;              ///< axes locaux tels qu'OpenSees les calcule
    bool isBeamColumn() const { return opsClass == "elasticBeamColumn"; }
    /// Réponse « basicStiffness » disponible dans OpenSees 3.8.0 (ElasticBeam3d, Truss).
    bool hasBasicStiffness() const { return opsClass == "elasticBeamColumn" || opsClass == "truss"; }
};

class OpenSeesModelMap
{
public:
    static OpenSeesModelMap build(const CalculationSnapshot& snapshot, const AnalysisParameters& params);

    int ndm() const { return 3; }
    int ndf() const { return 6; }

    const std::vector<OpsElementEntry>& elements() const { return m_elements; }
    const OpsElementEntry* byTag(int tag) const;
    const OpsElementEntry* byKey(const ElementKey& key) const;

    const std::vector<SpringSupportInfo>& springs() const { return m_springs; }
    /// Nœud TSA porteur du ressort dont le nœud auxiliaire est auxTag (0 si inconnu).
    int tsaNodeOfAux(int auxTag) const;

    // Dispositions des recorders (ordre des colonnes des fichiers OpenSees)
    const std::vector<int>& structuralNodeTags() const { return m_structuralNodeTags; }
    const std::vector<int>& reactionNodeTags() const { return m_reactionNodeTags; }   ///< nœuds fixés puis auxiliaires
    const std::vector<int>& beamColumnTags() const { return m_beamColumnTags; }       ///< localForce : 12 valeurs
    const std::vector<int>& axialTags() const { return m_axialTags; }                 ///< basicForce : 1 valeur
    const std::vector<int>& allElementTags() const { return m_allElementTags; }       ///< globalForce : 12 valeurs
    const std::vector<int>& basicStiffnessBeamTags() const { return m_kbBeamTags; }   ///< 36 valeurs
    const std::vector<int>& basicStiffnessTrussTags() const { return m_kbTrussTags; } ///< 1 valeur

    /// Nombre d'équations attendu (DDL non fixés, nœuds structuraux) : estimation a priori
    /// servant à décider l'extraction de K_global ; la valeur exacte vient de nodeDOFs.
    int estimatedFreeDofs() const { return m_estimatedFreeDofs; }

    /// Incohérences détectées (tags dupliqués, nœuds manquants, axes indéfinis…). Vide = cohérent.
    std::vector<std::string> validate(const CalculationSnapshot& snapshot) const;

    /// Maillage tel qu'écrit dans le script : un nœud OpenSees par nœud du snapshot, nœuds auxiliaires
    /// des ressorts, un élément fini par barre (aucune subdivision), éléments zeroLength des ressorts.
    SolverMesh solverMesh(const CalculationSnapshot& snapshot) const;

private:
    std::vector<OpsElementEntry> m_elements;
    std::map<int, std::size_t> m_indexByTag;
    std::map<ElementKey, std::size_t> m_indexByKey;
    std::vector<SpringSupportInfo> m_springs;
    std::map<int, int> m_auxToTsa;
    std::vector<int> m_structuralNodeTags;
    std::vector<int> m_reactionNodeTags;
    std::vector<int> m_beamColumnTags;
    std::vector<int> m_axialTags;
    std::vector<int> m_allElementTags;
    std::vector<int> m_kbBeamTags;
    std::vector<int> m_kbTrussTags;
    int m_estimatedFreeDofs = 0;
};

} // namespace TSA::Analysis
