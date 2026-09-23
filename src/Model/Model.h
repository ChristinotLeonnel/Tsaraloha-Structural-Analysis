#pragma once

#include "Node.h"
#include "Beam.h"
#include "Column.h"
#include "Slab.h"
#include "Wall.h"
#include "Foundation.h"
#include "TrussMember.h"
#include "../Coordinate/CoordinateSystem.h"

#include <map>
#include <vector>
#include <set>
#include <memory>
#include <gp_Pnt.hxx>
#include <gp_Dir.hxx>

namespace TSA::Model
{

class IModelObserver
{
public:
    virtual ~IModelObserver() = default;
    virtual void onNodeAdded(const Node& /*node*/) {}
    virtual void onNodeModified(const Node& /*node*/) {}
    virtual void onNodeRemoved(int /*nodeId*/) {}

    virtual void onBeamAdded(const Beam& /*beam*/) {}
    virtual void onBeamModified(const Beam& /*beam*/) {}
    virtual void onBeamRemoved(int /*beamId*/) {}

    virtual void onColumnAdded(const Column& /*column*/) {}
    virtual void onColumnModified(const Column& /*column*/) {}
    virtual void onColumnRemoved(int /*columnId*/) {}

    virtual void onSlabAdded(const Slab& /*slab*/) {}
    virtual void onSlabModified(const Slab& /*slab*/) {}
    virtual void onSlabRemoved(int /*slabId*/) {}

    virtual void onWallAdded(const Wall& /*wall*/) {}
    virtual void onWallModified(const Wall& /*wall*/) {}
    virtual void onWallRemoved(int /*wallId*/) {}

    virtual void onFoundationAdded(const Foundation& /*foundation*/) {}
    virtual void onFoundationModified(const Foundation& /*foundation*/) {}
    virtual void onFoundationRemoved(int /*foundationId*/) {}

    virtual void onTrussMemberAdded(const TrussMember& /*member*/) {}
    virtual void onTrussMemberModified(const TrussMember& /*member*/) {}
    virtual void onTrussMemberRemoved(int /*memberId*/) {}

    virtual void onModelCleared() {}
};

class Model
{
public:
    Model();
    ~Model();

    // Système de coordonnées et niveaux centralisés
    TSA::Coordinate::CoordinateSystem* coordinateSystem() { return m_coordinateSystem.get(); }
    const TSA::Coordinate::CoordinateSystem* coordinateSystem() const { return m_coordinateSystem.get(); }

    TSA::Coordinate::LevelManager* levelManager();
    const TSA::Coordinate::LevelManager* levelManager() const;

    // Observateurs
    void addObserver(IModelObserver* observer);
    void removeObserver(IModelObserver* observer);

    // Gestion des nœuds
    int addNode(double x, double y, double z, const std::string& levelId = "", const std::string& name = "");
    bool addNodeWithId(int id, double x, double y, double z, const std::string& levelId = "", const std::string& name = "");
    int addNodeAtGridIntersection(int ix, int iy, int iz);
    int addColumnBetweenLevels(int levelStartIndex, int levelEndIndex, double x, double y, double width = 0.30, double height = 0.30);
    bool removeNode(int nodeId);
    Node* getNode(int nodeId);
    const Node* getNode(int nodeId) const;
    const std::map<int, Node>& nodes() const { return m_nodes; }

    // Modification d'un niveau d'étage avec propagation instantanée aux objets attachés
    void onLevelElevationChanged(const std::string& levelId, double oldElevation, double newElevation);

    // Gestion des poutres
    int addBeam(int startNodeId, int endNodeId, double width = 0.30, double height = 0.50, const std::string& name = "");
    bool addBeamWithId(int id, int startNodeId, int endNodeId, double width = 0.30, double height = 0.50, const std::string& name = "");
    bool removeBeam(int beamId);
    Beam* getBeam(int beamId);
    const Beam* getBeam(int beamId) const;
    const std::map<int, Beam>& beams() const { return m_beams; }

    // Gestion des poteaux
    int addColumn(int startNodeId, int endNodeId, double width = 0.30, double height = 0.30, const std::string& name = "");
    bool addColumnWithId(int id, int startNodeId, int endNodeId, double width = 0.30, double height = 0.30, const std::string& name = "");
    bool removeColumn(int columnId);
    Column* getColumn(int columnId);
    const Column* getColumn(int columnId) const;
    const std::map<int, Column>& columns() const { return m_columns; }

    // Gestion des dalles
    int addSlab(const std::vector<int>& nodeIds, double thickness = 0.20, const std::string& name = "", SlabType type = SlabType::TwoWay);
    bool addSlabWithId(int id, const std::vector<int>& nodeIds, double thickness = 0.20, const std::string& name = "", SlabType type = SlabType::TwoWay);
    bool removeSlab(int slabId);
    Slab* getSlab(int slabId);
    const Slab* getSlab(int slabId) const;
    const std::map<int, Slab>& slabs() const { return m_slabs; }

    // Gestion des voiles (Walls)
    int addWall(int startNodeId, int endNodeId, double height = 3.0, double thickness = 0.20, const std::string& name = "");
    bool addWallWithId(int id, int startNodeId, int endNodeId, double height = 3.0, double thickness = 0.20, const std::string& name = "");
    bool removeWall(int wallId);
    Wall* getWall(int wallId);
    const Wall* getWall(int wallId) const;
    const std::map<int, Wall>& walls() const { return m_walls; }

    // Gestion des fondations
    int addFoundation(int nodeId, double widthA = 1.50, double lengthB = 1.50, double heightH = 0.50, const std::string& name = "", FoundationType type = FoundationType::IsolatedFooting);
    bool addFoundationWithId(int id, int nodeId, double widthA = 1.50, double lengthB = 1.50, double heightH = 0.50, const std::string& name = "", FoundationType type = FoundationType::IsolatedFooting);
    bool removeFoundation(int foundationId);
    Foundation* getFoundation(int foundationId);
    const Foundation* getFoundation(int foundationId) const;
    const std::map<int, Foundation>& foundations() const { return m_foundations; }

    // Gestion des treillis & contreventements
    int addTrussMember(int startNodeId, int endNodeId, double diameterOrWidth = 0.10, const std::string& name = "", TrussMemberRole role = TrussMemberRole::Diagonal);
    bool addTrussMemberWithId(int id, int startNodeId, int endNodeId, double diameterOrWidth = 0.10, const std::string& name = "", TrussMemberRole role = TrussMemberRole::Diagonal);
    bool removeTrussMember(int memberId);
    TrussMember* getTrussMember(int memberId);
    const TrussMember* getTrussMember(int memberId) const;
    const std::map<int, TrussMember>& trussMembers() const { return m_trussMembers; }

    // Transformations
    bool moveNodes(const std::set<int>& nodeIds, double dx, double dy, double dz);
    bool rotateNodes(const std::set<int>& nodeIds, const gp_Pnt& center, const gp_Dir& axis, double angleRad);
    std::vector<int> copyElements(const std::set<int>& nodeIds,
                                  const std::set<int>& beamIds,
                                  const std::set<int>& columnIds,
                                  const std::set<int>& slabIds,
                                  double dx, double dy, double dz, int repetitions = 1);
    std::vector<int> copyAndRotateElements(const std::set<int>& nodeIds,
                                          const std::set<int>& beamIds,
                                          const std::set<int>& columnIds,
                                          const std::set<int>& slabIds,
                                          const gp_Pnt& center, const gp_Dir& axis,
                                          double angleRad, int repetitions = 1);

    struct ModelStateSnapshot
    {
        std::map<int, Node> nodes;
        std::map<int, Beam> beams;
        std::map<int, Column> columns;
        std::map<int, Slab> slabs;
        std::map<int, Wall> walls;
        std::map<int, Foundation> foundations;
        std::map<int, TrussMember> trussMembers;
        int nextNodeId = 1;
        int nextBeamId = 1;
        int nextColumnId = 1;
        int nextSlabId = 1;
        int nextWallId = 1;
        int nextFoundationId = 1;
        int nextTrussMemberId = 1;
        std::string actionName;
    };

    // Historique Undo / Redo (Ctrl+Z / Ctrl+Y)
    void pushUndoState(const std::string& actionName = "");
    bool canUndo() const;
    bool canRedo() const;
    bool undo();
    bool redo();
    void clearUndoRedo();
    std::string lastUndoActionName() const;
    std::string lastRedoActionName() const;

    ModelStateSnapshot createSnapshot(const std::string& actionName = "") const;
    void restoreSnapshot(const ModelStateSnapshot& snapshot);

    // Notifications de modification
    void notifyNodeModified(int nodeId);
    void notifyBeamModified(int beamId);
    void notifyColumnModified(int columnId);
    void notifySlabModified(int slabId);
    void notifyWallModified(int wallId);
    void notifyFoundationModified(int foundationId);
    void notifyTrussMemberModified(int memberId);

    // Réinitialisation
    void clear();

    // Générateurs d'identifiants
    int nextNodeId() const { return m_nextNodeId; }
    int nextBeamId() const { return m_nextBeamId; }
    int nextColumnId() const { return m_nextColumnId; }
    int nextSlabId() const { return m_nextSlabId; }
    int nextWallId() const { return m_nextWallId; }
    int nextFoundationId() const { return m_nextFoundationId; }
    int nextTrussMemberId() const { return m_nextTrussMemberId; }

private:
    int m_nextNodeId = 1;
    int m_nextBeamId = 1;
    int m_nextColumnId = 1;
    int m_nextSlabId = 1;
    int m_nextWallId = 1;
    int m_nextFoundationId = 1;
    int m_nextTrussMemberId = 1;

    std::map<int, Node> m_nodes;
    std::map<int, Beam> m_beams;
    std::map<int, Column> m_columns;
    std::map<int, Slab> m_slabs;
    std::map<int, Wall> m_walls;
    std::map<int, Foundation> m_foundations;
    std::map<int, TrussMember> m_trussMembers;

    std::vector<IModelObserver*> m_observers;

    std::vector<ModelStateSnapshot> m_undoStack;
    std::vector<ModelStateSnapshot> m_redoStack;
    size_t m_maxUndoSteps = 50;

    std::shared_ptr<TSA::Coordinate::CoordinateSystem> m_coordinateSystem;
};

} // namespace TSA::Model
