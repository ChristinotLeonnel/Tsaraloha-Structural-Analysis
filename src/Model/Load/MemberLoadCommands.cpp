#include "MemberLoadCommands.h"

#include "LoadManager.h"
#include "../Model.h"

#include <algorithm>
#include <cmath>

namespace TSA::Model
{

double memberLoadTargetLength(const Model& model, int elementId, MemberTargetType type)
{
    switch (type)
    {
    case MemberTargetType::Column:
        if (const auto* c = model.getColumn(elementId)) return c->length(model);
        break;
    case MemberTargetType::Truss:
        if (const auto* t = model.getTrussMember(elementId)) return t->length(model);
        break;
    case MemberTargetType::Cable:
        break;   // charges sur câble : non prises en charge par les moteurs
    default:
        if (const auto* b = model.getBeam(elementId)) return b->length(model);
        break;
    }
    return -1.0;
}

bool hasEquivalentMemberLoad(const Model& model, const MemberLoad& load)
{
    const double L = memberLoadTargetLength(model, load.elementId(), load.targetType());
    if (L < 0.0) return false;
    const auto range = load.appliedRange(L);
    auto same = [](double a, double b) { return std::abs(a - b) <= 1e-9 * std::max(1.0, std::abs(a)); };
    for (const auto& [id, other] : model.loadManager().memberLoads())
    {
        if (other.elementId() != load.elementId() || other.targetType() != load.targetType()
            || other.loadCaseId() != load.loadCaseId() || other.type() != load.type()
            || other.direction() != load.direction() || other.coordSystem() != load.coordSystem())
            continue;
        const auto r = other.appliedRange(L);
        if (same(other.q1(), load.q1()) && same(other.intensityAt(r.second, L), load.intensityAt(range.second, L))
            && same(r.first, range.first) && same(r.second, range.second))
            return true;
    }
    return false;
}

std::vector<int> applyMemberLoad(Model& model, const MemberLoad& prototype,
                                 const std::vector<MemberLoadTarget>& targets,
                                 const std::string& undoLabel, std::string* error)
{
    auto fail = [&](const std::string& message) {
        if (error) *error = message;
        return std::vector<int>{};
    };
    if (targets.empty()) return fail("Aucune barre à charger.");

    // Toutes les cibles sont vérifiées avant toute modification (aucune application partielle).
    for (const auto& t : targets)
    {
        const double L = memberLoadTargetLength(model, t.elementId, t.type);
        if (L < 0.0) return fail("La barre #" + std::to_string(t.elementId) + " n'existe plus.");
        MemberLoad probe = prototype;
        probe.setElementId(t.elementId);
        probe.setTargetType(t.type);
        if (const std::string why = probe.validate(L); !why.empty())
            return fail("Barre #" + std::to_string(t.elementId) + " : " + why);
    }
    if (!model.loadManager().getLoadCase(prototype.loadCaseId()))
        return fail("Le cas de charge choisi n'existe pas.");

    model.pushUndoState(undoLabel);
    std::vector<int> ids;
    ids.reserve(targets.size());
    for (const auto& t : targets)
    {
        MemberLoad ml = prototype;
        ml.setId(0);
        ml.setElementId(t.elementId);
        ml.setTargetType(t.type);
        const int id = model.loadManager().addMemberLoad(ml);
        model.notifyMemberLoadAdded(id);
        ids.push_back(id);
    }
    if (error) error->clear();
    return ids;
}

int removeLoads(Model& model, const std::set<int>& nodalLoadIds, const std::set<int>& memberLoadIds,
                const std::string& undoLabel)
{
    std::vector<int> nodal, member;
    for (int id : nodalLoadIds)
        if (model.loadManager().getNodalLoad(id)) nodal.push_back(id);
    for (int id : memberLoadIds)
        if (model.loadManager().getMemberLoad(id)) member.push_back(id);
    if (nodal.empty() && member.empty()) return 0;

    model.pushUndoState(undoLabel);
    for (int id : nodal)
    {
        model.loadManager().removeNodalLoad(id);
        model.notifyNodalLoadRemoved(id);
    }
    for (int id : member)
    {
        model.loadManager().removeMemberLoad(id);
        model.notifyMemberLoadRemoved(id);
    }
    return static_cast<int>(nodal.size() + member.size());
}

} // namespace TSA::Model
