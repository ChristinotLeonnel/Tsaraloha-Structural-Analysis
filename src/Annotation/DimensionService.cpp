#include "DimensionService.h"

#include "DimensionGeometry.h"
#include "../Model/Model.h"

namespace TSA::Annotation
{

namespace
{
bool validate(const TSA::Model::Model& model, const Dimension& d, std::string* error)
{
    for (const auto& a : d.anchors)
    {
        if (a.nodeId > 0 && !a.orphaned && !model.getNode(a.nodeId))
        {
            if (error) *error = "le nœud " + std::to_string(a.nodeId) + " n'existe pas";
            return false;
        }
    }
    const DimensionLayout layout = layoutFor(model, d, model.dimensions().style);
    if (!layout.valid)
    {
        if (error) *error = layout.error;
        return false;
    }
    return true;
}
} // namespace

int addDimension(TSA::Model::Model& model, Dimension dim, std::string* error)
{
    // Point fixe de secours = position actuelle du nœud associé.
    for (auto& a : dim.anchors)
    {
        a.orphaned = false;
        if (const auto* n = a.nodeId > 0 ? model.getNode(a.nodeId) : nullptr) a.point = { n->x(), n->y(), n->z() };
    }
    if (!validate(model, dim, error)) return 0;
    model.pushLabelUndoState("Cotation");
    auto& set = model.dimensionsForEdit();
    dim.id = set.nextId++;
    const int id = dim.id;
    set.items[id] = std::move(dim);
    model.notifyDimensionsChanged({ id });
    if (error) error->clear();
    return id;
}

bool updateDimension(TSA::Model::Model& model, const Dimension& dim, std::string* error)
{
    if (!model.dimensions().items.count(dim.id))
    {
        if (error) *error = "cotation introuvable";
        return false;
    }
    if (!validate(model, dim, error)) return false;
    model.pushLabelUndoState("Modification de cotation");
    model.dimensionsForEdit().items[dim.id] = dim;
    model.notifyDimensionsChanged({ dim.id });
    return true;
}

int removeDimensions(TSA::Model::Model& model, const std::set<int>& ids)
{
    std::vector<int> existing;
    for (int id : ids)
        if (model.dimensions().items.count(id)) existing.push_back(id);
    if (existing.empty()) return 0;
    model.pushLabelUndoState("Suppression de cotation(s)");
    for (int id : existing) model.dimensionsForEdit().items.erase(id);
    model.notifyDimensionsChanged(existing);
    return static_cast<int>(existing.size());
}

int removeInvalidDimensions(TSA::Model::Model& model)
{
    std::set<int> ids;
    for (const auto& [id, d] : model.dimensions().items)
    {
        bool invalid = false;
        resolveAnchors(model, d, &invalid);
        if (invalid || d.hasInvalidReference()) ids.insert(id);
    }
    return removeDimensions(model, ids);
}

bool reassociateAnchor(TSA::Model::Model& model, int dimensionId, int anchorIndex, int nodeId, std::string* error)
{
    auto it = model.dimensions().items.find(dimensionId);
    const auto* node = model.getNode(nodeId);
    if (it == model.dimensions().items.end() || anchorIndex < 0 || anchorIndex >= static_cast<int>(it->second.anchors.size()) || !node)
    {
        if (error) *error = "cotation, ancrage ou nœud introuvable";
        return false;
    }
    Dimension d = it->second;
    auto& a = d.anchors[anchorIndex];
    a.nodeId = nodeId;
    a.orphaned = false;
    a.point = { node->x(), node->y(), node->z() };
    return updateDimension(model, d, error);
}

void setDimensionStyle(TSA::Model::Model& model, const DimensionStyle& style)
{
    if (model.dimensions().style == style) return;
    model.pushLabelUndoState("Style des cotations");
    model.dimensionsForEdit().style = style;
    model.notifyDimensionsChanged({});
}

std::vector<int> dimensionsReferencingNode(const TSA::Model::Model& model, int nodeId)
{
    std::vector<int> ids;
    for (const auto& [id, d] : model.dimensions().items)
        for (const auto& a : d.anchors)
            if (a.nodeId == nodeId)
            {
                ids.push_back(id);
                break;
            }
    return ids;
}

} // namespace TSA::Annotation
