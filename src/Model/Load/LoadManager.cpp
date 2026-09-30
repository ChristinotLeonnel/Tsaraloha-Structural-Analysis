#include "LoadManager.h"
#include "../Model.h"

#include <algorithm>

namespace TSA::Model
{

static constexpr double STANDARD_GRAVITY = 9.80665; // m/s²

LoadManager::LoadManager()
{
    resetToDefaults();
}

void LoadManager::clear()
{
    m_nodalLoads.clear();
    m_memberLoads.clear();
    m_loadCases.clear();
    m_combinations.clear();
    m_nextNodalLoadId = 1;
    m_nextMemberLoadId = 1;
    m_nextLoadCaseId = 1;
    m_nextCombinationId = 1;
    m_activeLoadCaseId = 1;
}

void LoadManager::resetToDefaults()
{
    clear();

    // 1. Cas de charge Eurocodes normalisés
    addLoadCase(LoadCase(1, "G", LoadCaseCategory::Dead, true, 1.0, "Charges permanentes et poids propre"));
    addLoadCase(LoadCase(2, "Q", LoadCaseCategory::Live, false, 1.0, "Charges d'exploitation"));
    addLoadCase(LoadCase(3, "W", LoadCaseCategory::Wind, false, 1.0, "Actions du vent"));
    addLoadCase(LoadCase(4, "S", LoadCaseCategory::Snow, false, 1.0, "Charges de neige"));
    addLoadCase(LoadCase(5, "E", LoadCaseCategory::Seismic, false, 1.0, "Action sismique"));

    // 2. Combinaisons types Eurocode EN 1990
    LoadCombination elu(1, "ELU Fondamental (1.35G + 1.50Q)", LoadCombinationType::ULS_Fundamental);
    elu.setFactor(1, 1.35); // 1.35 * G
    elu.setFactor(2, 1.50); // 1.50 * Q
    addCombination(elu);

    LoadCombination els(2, "ELS Caractéristique (1.00G + 1.00Q)", LoadCombinationType::SLS_Characteristic);
    els.setFactor(1, 1.00); // 1.00 * G
    els.setFactor(2, 1.00); // 1.00 * Q
    addCombination(els);

    LoadCombination sism(3, "ELU Sismique (1.00G + 0.30Q + 1.00E)", LoadCombinationType::ULS_Seismic);
    sism.setFactor(1, 1.00); // 1.00 * G
    sism.setFactor(2, 0.30); // psi2 * Q
    sism.setFactor(5, 1.00); // 1.00 * E
    addCombination(sism);

    m_activeLoadCaseId = 1;
}

// =============================================================================
// Cas de charge
// =============================================================================
int LoadManager::addLoadCase(const LoadCase& lc)
{
    int id = m_nextLoadCaseId++;
    while (m_loadCases.find(id) != m_loadCases.end())
    {
        id = m_nextLoadCaseId++;
    }

    LoadCase copy = lc;
    copy.setId(id);
    m_loadCases[id] = copy;
    return id;
}

bool LoadManager::addLoadCaseWithId(int id, const LoadCase& lc)
{
    if (m_loadCases.find(id) != m_loadCases.end())
    {
        return false;
    }
    LoadCase copy = lc;
    copy.setId(id);
    m_loadCases[id] = copy;
    m_nextLoadCaseId = std::max(m_nextLoadCaseId, id + 1);
    return true;
}

bool LoadManager::removeLoadCase(int id)
{
    auto it = m_loadCases.find(id);
    if (it == m_loadCases.end())
        return false;

    m_loadCases.erase(it);

    // Supprimer également les charges associées à ce cas
    for (auto lit = m_nodalLoads.begin(); lit != m_nodalLoads.end(); )
    {
        if (lit->second.loadCaseId() == id)
            lit = m_nodalLoads.erase(lit);
        else
            ++lit;
    }
    for (auto mit = m_memberLoads.begin(); mit != m_memberLoads.end(); )
    {
        if (mit->second.loadCaseId() == id)
            mit = m_memberLoads.erase(mit);
        else
            ++mit;
    }

    // Retirer ce cas des combinaisons
    for (auto& [cId, combo] : m_combinations)
    {
        combo.removeCase(id);
    }

    if (m_activeLoadCaseId == id)
    {
        m_activeLoadCaseId = m_loadCases.empty() ? 0 : m_loadCases.begin()->first;
    }
    return true;
}

LoadCase* LoadManager::getLoadCase(int id)
{
    auto it = m_loadCases.find(id);
    return (it != m_loadCases.end()) ? &it->second : nullptr;
}

const LoadCase* LoadManager::getLoadCase(int id) const
{
    auto it = m_loadCases.find(id);
    return (it != m_loadCases.end()) ? &it->second : nullptr;
}

// =============================================================================
// Combinaisons d'actions
// =============================================================================
int LoadManager::addCombination(const LoadCombination& combo)
{
    int id = m_nextCombinationId++;
    while (m_combinations.find(id) != m_combinations.end())
    {
        id = m_nextCombinationId++;
    }

    LoadCombination copy = combo;
    copy.setId(id);
    m_combinations[id] = copy;
    return id;
}

bool LoadManager::addCombinationWithId(int id, const LoadCombination& combo)
{
    if (m_combinations.find(id) != m_combinations.end())
    {
        return false;
    }
    LoadCombination copy = combo;
    copy.setId(id);
    m_combinations[id] = copy;
    m_nextCombinationId = std::max(m_nextCombinationId, id + 1);
    return true;
}

bool LoadManager::removeCombination(int id)
{
    return m_combinations.erase(id) > 0;
}

LoadCombination* LoadManager::getCombination(int id)
{
    auto it = m_combinations.find(id);
    return (it != m_combinations.end()) ? &it->second : nullptr;
}

const LoadCombination* LoadManager::getCombination(int id) const
{
    auto it = m_combinations.find(id);
    return (it != m_combinations.end()) ? &it->second : nullptr;
}

// =============================================================================
// Charges nodales
// =============================================================================
int LoadManager::addNodalLoad(const NodalLoad& load)
{
    int id = m_nextNodalLoadId++;
    while (m_nodalLoads.find(id) != m_nodalLoads.end())
    {
        id = m_nextNodalLoadId++;
    }

    NodalLoad copy = load;
    copy.setId(id);
    if (copy.name().empty())
    {
        copy.setName("NL" + std::to_string(id));
    }
    m_nodalLoads[id] = copy;
    return id;
}

bool LoadManager::addNodalLoadWithId(int id, const NodalLoad& load)
{
    if (m_nodalLoads.find(id) != m_nodalLoads.end())
    {
        return false;
    }
    NodalLoad copy = load;
    copy.setId(id);
    m_nodalLoads[id] = copy;
    m_nextNodalLoadId = std::max(m_nextNodalLoadId, id + 1);
    return true;
}

bool LoadManager::removeNodalLoad(int id)
{
    return m_nodalLoads.erase(id) > 0;
}

NodalLoad* LoadManager::getNodalLoad(int id)
{
    auto it = m_nodalLoads.find(id);
    return (it != m_nodalLoads.end()) ? &it->second : nullptr;
}

const NodalLoad* LoadManager::getNodalLoad(int id) const
{
    auto it = m_nodalLoads.find(id);
    return (it != m_nodalLoads.end()) ? &it->second : nullptr;
}

std::vector<NodalLoad> LoadManager::nodalLoadsForNode(int nodeId) const
{
    std::vector<NodalLoad> res;
    for (const auto& [id, nl] : m_nodalLoads)
    {
        if (nl.nodeId() == nodeId)
        {
            res.push_back(nl);
        }
    }
    return res;
}

std::vector<NodalLoad> LoadManager::nodalLoadsForCase(int loadCaseId) const
{
    std::vector<NodalLoad> res;
    for (const auto& [id, nl] : m_nodalLoads)
    {
        if (nl.loadCaseId() == loadCaseId)
        {
            res.push_back(nl);
        }
    }
    return res;
}

// =============================================================================
// Charges sur éléments
// =============================================================================
int LoadManager::addMemberLoad(const MemberLoad& load)
{
    int id = m_nextMemberLoadId++;
    while (m_memberLoads.find(id) != m_memberLoads.end())
    {
        id = m_nextMemberLoadId++;
    }

    MemberLoad copy = load;
    copy.setId(id);
    if (copy.name().empty())
    {
        copy.setName("ML" + std::to_string(id));
    }
    m_memberLoads[id] = copy;
    return id;
}

bool LoadManager::addMemberLoadWithId(int id, const MemberLoad& load)
{
    if (m_memberLoads.find(id) != m_memberLoads.end())
    {
        return false;
    }
    MemberLoad copy = load;
    copy.setId(id);
    m_memberLoads[id] = copy;
    m_nextMemberLoadId = std::max(m_nextMemberLoadId, id + 1);
    return true;
}

bool LoadManager::removeMemberLoad(int id)
{
    return m_memberLoads.erase(id) > 0;
}

MemberLoad* LoadManager::getMemberLoad(int id)
{
    auto it = m_memberLoads.find(id);
    return (it != m_memberLoads.end()) ? &it->second : nullptr;
}

const MemberLoad* LoadManager::getMemberLoad(int id) const
{
    auto it = m_memberLoads.find(id);
    return (it != m_memberLoads.end()) ? &it->second : nullptr;
}

std::vector<MemberLoad> LoadManager::memberLoadsForElement(int elementId) const
{
    std::vector<MemberLoad> res;
    for (const auto& [id, ml] : m_memberLoads)
    {
        if (ml.elementId() == elementId)
        {
            res.push_back(ml);
        }
    }
    return res;
}

std::vector<MemberLoad> LoadManager::memberLoadsForCase(int loadCaseId) const
{
    std::vector<MemberLoad> res;
    for (const auto& [id, ml] : m_memberLoads)
    {
        if (ml.loadCaseId() == loadCaseId)
        {
            res.push_back(ml);
        }
    }
    return res;
}

// =============================================================================
// Calculs de Poids Propre Automatique (Self-Weight)
// =============================================================================
double LoadManager::calculateElementLinearWeight(int elementId, const Model& model) const
{
    // Recherche dans les poutres
    const Beam* b = model.getBeam(elementId);
    if (b)
    {
        double area = b->section().area();       // m²
        double rho = b->material().mechanical.density; // kg/m³
        // q = rho * A * g en N/m => / 1000.0 en kN/m
        return (rho * area * STANDARD_GRAVITY) / 1000.0;
    }

    // Recherche dans les poteaux
    const Column* col = model.getColumn(elementId);
    if (col)
    {
        double area = col->section().area();
        double rho = col->material().mechanical.density;
        return (rho * area * STANDARD_GRAVITY) / 1000.0;
    }

    // Recherche dans les treillis
    const TrussMember* tr = model.getTrussMember(elementId);
    if (tr)
    {
        double area = tr->section().area();
        double rho = tr->material().mechanical.density;
        return (rho * area * STANDARD_GRAVITY) / 1000.0;
    }

    return 0.0;
}

double LoadManager::calculateElementTotalWeight(int elementId, const Model& model, double factor) const
{
    double q = calculateElementLinearWeight(elementId, model);
    double len = 0.0;

    const Beam* b = model.getBeam(elementId);
    if (b) len = b->length(model);

    const Column* col = model.getColumn(elementId);
    if (col) len = col->length(model);

    const TrussMember* tr = model.getTrussMember(elementId);
    if (tr) len = tr->length(model);

    return q * len * factor;
}

double LoadManager::calculateTotalStructuralWeight(const Model& model, double factor) const
{
    double totalWeight = 0.0;

    // 1. Poutres
    for (const auto& [id, b] : model.beams())
    {
        totalWeight += calculateElementTotalWeight(id, model, factor);
    }

    // 2. Poteaux
    for (const auto& [id, col] : model.columns())
    {
        totalWeight += calculateElementTotalWeight(id, model, factor);
    }

    // 3. Treillis
    for (const auto& [id, tr] : model.trussMembers())
    {
        totalWeight += calculateElementTotalWeight(id, model, factor);
    }

    // 4. Dalles (W = rho * Area * thickness * g / 1000)
    for (const auto& [id, slab] : model.slabs())
    {
        double area = slab.area(model);
        double th = slab.thickness();
        double rho = slab.material().mechanical.density;
        totalWeight += (rho * area * th * STANDARD_GRAVITY / 1000.0) * factor;
    }

    // 5. Voiles
    for (const auto& [id, wall] : model.walls())
    {
        double len = wall.length(model);
        double h = wall.height();
        double th = wall.thickness();
        double rho = wall.material().mechanical.density;
        totalWeight += (rho * len * h * th * STANDARD_GRAVITY / 1000.0) * factor;
    }

    return totalWeight;
}

double LoadManager::computeElementSelfWeight(const Model& model, int elementId)
{
    LoadManager lm;
    return lm.calculateElementLinearWeight(elementId, model);
}

double LoadManager::computeTotalModelSelfWeight(const Model& model)
{
    LoadManager lm;
    return lm.calculateTotalStructuralWeight(model);
}

// =============================================================================
// Snapshot & Undo/Redo
// =============================================================================
LoadManager::LoadSnapshot LoadManager::createSnapshot() const
{
    LoadSnapshot snap;
    snap.nodalLoads = m_nodalLoads;
    snap.memberLoads = m_memberLoads;
    snap.loadCases = m_loadCases;
    snap.combinations = m_combinations;
    snap.nextNodalLoadId = m_nextNodalLoadId;
    snap.nextMemberLoadId = m_nextMemberLoadId;
    snap.nextLoadCaseId = m_nextLoadCaseId;
    snap.nextCombinationId = m_nextCombinationId;
    snap.activeLoadCaseId = m_activeLoadCaseId;
    return snap;
}

void LoadManager::applySnapshot(const LoadManager::LoadSnapshot& snap)
{
    m_nodalLoads = snap.nodalLoads;
    m_memberLoads = snap.memberLoads;
    m_loadCases = snap.loadCases;
    m_combinations = snap.combinations;
    m_nextNodalLoadId = snap.nextNodalLoadId;
    m_nextMemberLoadId = snap.nextMemberLoadId;
    m_nextLoadCaseId = snap.nextLoadCaseId;
    m_nextCombinationId = snap.nextCombinationId;
    m_activeLoadCaseId = snap.activeLoadCaseId;
}

} // namespace TSA::Model
