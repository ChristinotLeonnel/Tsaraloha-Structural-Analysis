#include "Model.h"
#include "ModelDiff.h"
#include "../UndoRedo/UndoManager.h"
#include "../Diagnostics/Logger.h"

namespace TSA::Model
{

void Model::pushUndoState(const std::string& actionName)
{
    if (!actionName.empty())
    {
        TSA::Diagnostics::Logger::instance().setLastCommand(actionName);
        TSA_LOG_INFO("Model", "ActionStarted", actionName);
    }
    if (m_undoManager)
    {
        m_undoManager->pushState(*this, actionName);
    }
}

bool Model::canUndo() const
{
    return m_undoManager ? m_undoManager->canUndo() : false;
}

bool Model::canRedo() const
{
    return m_undoManager ? m_undoManager->canRedo() : false;
}

bool Model::undo()
{
    return m_undoManager ? m_undoManager->undo(*this) : false;
}

bool Model::redo()
{
    return m_undoManager ? m_undoManager->redo(*this) : false;
}

void Model::clearUndoRedo()
{
    if (m_undoManager)
    {
        m_undoManager->clear();
    }
}

std::string Model::lastUndoActionName() const
{
    return m_undoManager ? m_undoManager->lastUndoActionName() : "";
}

std::string Model::lastRedoActionName() const
{
    return m_undoManager ? m_undoManager->lastRedoActionName() : "";
}

TSA::UndoRedo::UndoManager* Model::undoManager()
{
    return m_undoManager.get();
}

const TSA::UndoRedo::UndoManager* Model::undoManager() const
{
    return m_undoManager.get();
}

Model::ModelStateSnapshot Model::createSnapshot(const std::string& actionName) const
{
    ModelStateSnapshot snap;
    snap.nodes = m_nodes;
    snap.beams = m_beams;
    snap.columns = m_columns;
    snap.slabs = m_slabs;
    snap.walls = m_walls;
    snap.foundations = m_foundations;
    snap.trussMembers = m_trussMembers;
    snap.cables = m_cables;
    snap.loadSnapshot = m_loadManager.createSnapshot();
    snap.calculationSnapshots = m_calculationSnapshots;
    snap.definitionReferences = m_definitionReferences;
    snap.nextNodeId = m_nextNodeId;
    snap.nextBeamId = m_nextBeamId;
    snap.nextColumnId = m_nextColumnId;
    snap.nextSlabId = m_nextSlabId;
    snap.nextWallId = m_nextWallId;
    snap.nextFoundationId = m_nextFoundationId;
    snap.nextTrussMemberId = m_nextTrussMemberId;
    snap.nextCableId = m_nextCableId;
    snap.actionName = actionName;
    return snap;
}

void Model::applySnapshotData(const Model::ModelStateSnapshot& snapshot)
{
    m_nodes = snapshot.nodes;
    m_beams = snapshot.beams;
    m_columns = snapshot.columns;
    m_slabs = snapshot.slabs;
    m_walls = snapshot.walls;
    m_foundations = snapshot.foundations;
    m_trussMembers = snapshot.trussMembers;
    m_cables = snapshot.cables;
    m_loadManager.applySnapshot(snapshot.loadSnapshot);
    m_calculationSnapshots = snapshot.calculationSnapshots;
    m_definitionReferences = snapshot.definitionReferences;
    m_nextNodeId = snapshot.nextNodeId;
    m_nextBeamId = snapshot.nextBeamId;
    m_nextColumnId = snapshot.nextColumnId;
    m_nextSlabId = snapshot.nextSlabId;
    m_nextWallId = snapshot.nextWallId;
    m_nextFoundationId = snapshot.nextFoundationId;
    m_nextTrussMemberId = snapshot.nextTrussMemberId;
    m_nextCableId = snapshot.nextCableId;
    m_isModified = true;
}

void Model::notifyModelDiffApplied(const ModelDiff& diff)
{
    for (auto* obs : m_observers)
    {
        obs->onModelDiffApplied(diff);
    }
}

void Model::restoreSnapshot(const Model::ModelStateSnapshot& snapshot)
{
    applySnapshotData(snapshot);

    for (auto* obs : m_observers)
    {
        obs->onModelCleared();
    }
}

void Model::setCalculationSnapshot(const std::string& key, const TSA::ExtensionSystem::MechanicalSnapshot& snapshot, const TSA::ExtensionSystem::DefinitionReference& ref)
{
    m_calculationSnapshots[key] = snapshot;
    if (ref.isValid())
    {
        m_definitionReferences[key] = ref;
    }
    m_isModified = true;
}

const TSA::ExtensionSystem::MechanicalSnapshot* Model::getCalculationSnapshot(const std::string& key) const
{
    auto it = m_calculationSnapshots.find(key);
    return (it != m_calculationSnapshots.end()) ? &it->second : nullptr;
}

const TSA::ExtensionSystem::DefinitionReference* Model::getDefinitionReference(const std::string& key) const
{
    auto it = m_definitionReferences.find(key);
    return (it != m_definitionReferences.end()) ? &it->second : nullptr;
}

bool Model::hasCalculationSnapshot(const std::string& key) const
{
    return m_calculationSnapshots.find(key) != m_calculationSnapshots.end();
}

void Model::removeCalculationSnapshot(const std::string& key)
{
    m_calculationSnapshots.erase(key);
    m_definitionReferences.erase(key);
    m_isModified = true;
}

void Model::clearCalculationSnapshots()
{
    m_calculationSnapshots.clear();
    m_definitionReferences.clear();
    m_isModified = true;
}

void Model::clear()
{
    m_calculationSnapshots.clear();
    m_definitionReferences.clear();
    m_cables.clear();
    m_trussMembers.clear();
    m_foundations.clear();
    m_walls.clear();
    m_slabs.clear();
    m_columns.clear();
    m_beams.clear();
    m_nodes.clear();
    m_nextNodeId = 1;
    m_nextBeamId = 1;
    m_nextColumnId = 1;
    m_nextSlabId = 1;
    m_nextWallId = 1;
    m_nextFoundationId = 1;
    m_nextTrussMemberId = 1;
    m_nextCableId = 1;
    m_loadManager.resetToDefaults();
    m_isModified = false;
    clearUndoRedo();

    if (m_workPlaneManager)
    {
        m_workPlaneManager->resetToDefault();
    }

    for (auto* obs : m_observers)
    {
        obs->onModelCleared();
    }
}

} // namespace TSA::Model
