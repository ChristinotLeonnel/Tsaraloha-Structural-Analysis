#include "PropertyAssignment.h"

#include "../Model/Model.h"
#include "../UndoRedo/EditTransaction.h"

namespace TSA::Interaction
{

int assignSection(TSA::Model::Model& model, const std::set<int>& beamIds, const std::set<int>& columnIds, const TSA::Model::Section& section,
                  const std::string& action)
{
    TSA::UndoRedo::EditTransaction tx(model, action);
    int modified = 0;
    for (int id : beamIds)
        if (auto* beam = model.getBeam(id))
        {
            beam->setSection(section);
            model.notifyBeamModified(id);
            ++modified;
        }
    for (int id : columnIds)
        if (auto* column = model.getColumn(id))
        {
            column->setSection(section);
            model.notifyColumnModified(id);
            ++modified;
        }
    if (modified == 0)
    {
        tx.discardUnchanged();
        return 0;
    }
    tx.commit();
    return modified;
}

bool setSlabOpenings(TSA::Model::Model& model, int slabId, const std::vector<TSA::Model::SlabOpening>& openings, const std::string& action)
{
    auto* slab = model.getSlab(slabId);
    if (!slab) return false;
    TSA::UndoRedo::EditTransaction tx(model, action);
    slab->setOpenings(openings);
    model.notifySlabModified(slabId);
    tx.commit();
    return true;
}

bool setWallOpenings(TSA::Model::Model& model, int wallId, const std::vector<TSA::Model::SurfaceOpening>& openings, const std::string& action)
{
    auto* wall = model.getWall(wallId);
    if (!wall) return false;
    TSA::UndoRedo::EditTransaction tx(model, action);
    wall->setOpenings(openings);
    model.notifyWallModified(wallId);
    tx.commit();
    return true;
}

} // namespace TSA::Interaction
