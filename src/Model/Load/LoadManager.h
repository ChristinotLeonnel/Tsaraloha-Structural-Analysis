#pragma once

#include "LoadEnums.h"
#include "NodalLoad.h"
#include "MemberLoad.h"
#include "LoadCase.h"
#include "LoadCombination.h"

#include <map>
#include <vector>
#include <memory>

namespace TSA::Model
{

class Model;

/**
 * @brief Gestionnaire centralisé des charges, cas de charges et combinaisons d'actions.
 * Intégré au modèle structural de TSA, assure la persistance, l'accès unifié
 * et la synchronisation avec le solveur OpenSees et le viewport 3D.
 */
class LoadManager
{
public:
    struct LoadSnapshot
    {
        std::map<int, NodalLoad> nodalLoads;
        std::map<int, MemberLoad> memberLoads;
        std::map<int, LoadCase> loadCases;
        std::map<int, LoadCombination> combinations;
        int nextNodalLoadId = 1;
        int nextMemberLoadId = 1;
        int nextLoadCaseId = 1;
        int nextCombinationId = 1;
        int activeLoadCaseId = 1;
    };

    LoadManager();
    ~LoadManager() = default;

    // Réinitialisation avec cas Eurocodes par défaut
    void resetToDefaults();
    void initializeEurocodeDefaults() { resetToDefaults(); }
    void clear();

    // =========================================================================
    // Cas de charge (Load Cases / Load Patterns)
    // =========================================================================
    int addLoadCase(const LoadCase& lc);
    bool addLoadCaseWithId(int id, const LoadCase& lc);
    bool removeLoadCase(int id);
    LoadCase* getLoadCase(int id);
    const LoadCase* getLoadCase(int id) const;
    const std::map<int, LoadCase>& loadCases() const noexcept { return m_loadCases; }

    int activeLoadCaseId() const noexcept { return m_activeLoadCaseId; }
    void setActiveLoadCaseId(int id) noexcept { m_activeLoadCaseId = id; }

    // =========================================================================
    // Combinaisons d'actions (Load Combinations)
    // =========================================================================
    int addCombination(const LoadCombination& combo);
    bool addCombinationWithId(int id, const LoadCombination& combo);
    bool removeCombination(int id);
    LoadCombination* getCombination(int id);
    const LoadCombination* getCombination(int id) const;
    const std::map<int, LoadCombination>& combinations() const noexcept { return m_combinations; }
    const std::map<int, LoadCombination>& loadCombinations() const noexcept { return m_combinations; }

    // =========================================================================
    // Charges nodales (Forces & Moments)
    // =========================================================================
    int addNodalLoad(const NodalLoad& load);
    bool addNodalLoadWithId(int id, const NodalLoad& load);
    bool removeNodalLoad(int id);
    NodalLoad* getNodalLoad(int id);
    const NodalLoad* getNodalLoad(int id) const;
    const std::map<int, NodalLoad>& nodalLoads() const noexcept { return m_nodalLoads; }

    std::vector<NodalLoad> nodalLoadsForNode(int nodeId) const;
    std::vector<NodalLoad> nodalLoadsForCase(int loadCaseId) const;

    // =========================================================================
    // Charges sur éléments (Uniforme, Linéaire, Ponctuelle sur barre)
    // =========================================================================
    int addMemberLoad(const MemberLoad& load);
    bool addMemberLoadWithId(int id, const MemberLoad& load);
    bool removeMemberLoad(int id);
    MemberLoad* getMemberLoad(int id);
    const MemberLoad* getMemberLoad(int id) const;
    const std::map<int, MemberLoad>& memberLoads() const noexcept { return m_memberLoads; }

    std::vector<MemberLoad> memberLoadsForElement(int elementId) const;
    std::vector<MemberLoad> memberLoadsForCase(int loadCaseId) const;

    std::vector<NodalLoad> getNodalLoadsForNode(int nodeId) const { return nodalLoadsForNode(nodeId); }
    std::vector<NodalLoad> getNodalLoadsForCase(int loadCaseId) const { return nodalLoadsForCase(loadCaseId); }
    std::vector<MemberLoad> getMemberLoadsForElement(int elementId) const { return memberLoadsForElement(elementId); }
    std::vector<MemberLoad> getMemberLoadsForCase(int loadCaseId) const { return memberLoadsForCase(loadCaseId); }

    // =========================================================================
    // Poids propre automatique (Self-Weight)
    // =========================================================================
    /**
     * @brief Calcule la charge linéique uniforme de poids propre q = rho * A * g (kN/m)
     * pour une barre à partir de son matériau et de sa section.
     */
    double calculateElementLinearWeight(int elementId, const Model& model) const;

    /**
     * @brief Calcule le poids propre total (kN) d'un élément donné.
     */
    double calculateElementTotalWeight(int elementId, const Model& model, double factor = 1.0) const;

    /**
     * @brief Calcule le poids propre total (kN) de toute la structure.
     */
    double calculateTotalStructuralWeight(const Model& model, double factor = 1.0) const;

    static double computeElementSelfWeight(const Model& model, int elementId);
    static double computeTotalModelSelfWeight(const Model& model);

    // =========================================================================
    // Snapshot & Undo/Redo
    // =========================================================================
    LoadSnapshot createSnapshot() const;
    void applySnapshot(const LoadSnapshot& snapshot);

private:
    std::map<int, NodalLoad> m_nodalLoads;
    std::map<int, MemberLoad> m_memberLoads;
    std::map<int, LoadCase> m_loadCases;
    std::map<int, LoadCombination> m_combinations;

    int m_nextNodalLoadId = 1;
    int m_nextMemberLoadId = 1;
    int m_nextLoadCaseId = 1;
    int m_nextCombinationId = 1;
    int m_activeLoadCaseId = 1;
};

} // namespace TSA::Model
