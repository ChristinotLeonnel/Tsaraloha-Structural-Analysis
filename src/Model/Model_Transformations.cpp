#include "Model.h"
#include "SelectionQuery.h"
#include "StructuralClipboard.h"

#include <functional>
#include <map>
#include <gp_Vec.hxx>

#include <algorithm>
#include <cmath>
#include <gp_Trsf.hxx>
#include <gp_Ax1.hxx>
#include <gp_Ax2.hxx>

namespace TSA::Model
{

namespace
{
ClipboardSelection selectionOf(const ElementSet& s)
{
    ClipboardSelection selection;
    selection.nodes = s.nodes;
    selection.beams = s.beams;
    selection.columns = s.columns;
    selection.slabs = s.slabs;
    selection.walls = s.walls;
    selection.trusses = s.trussMembers;
    selection.cables = s.cables;
    selection.foundations = s.foundations;
    return selection;
}

ElementSet elementSet(const std::set<int>& nodes, const std::set<int>& beams, const std::set<int>& columns, const std::set<int>& slabs,
                      const std::set<int>& cables, const std::set<int>& trusses)
{
    ElementSet s;
    s.nodes = nodes;
    s.beams = beams;
    s.columns = columns;
    s.slabs = slabs;
    s.cables = cables;
    s.trussMembers = trusses;
    return s;
}

/// Identifiants créés : nœuds, puis éléments par famille.
void appendCreated(const PasteResult& r, std::vector<int>& created)
{
    for (const auto* ids : { &r.nodeIds, &r.beamIds, &r.columnIds, &r.trussIds, &r.slabIds, &r.wallIds, &r.cableIds, &r.foundationIds })
        created.insert(created.end(), ids->begin(), ids->end());
}

void appendWarnings(const PasteResult& r, std::vector<std::string>* warnings)
{
    if (!warnings) return;
    warnings->insert(warnings->end(), r.warnings.begin(), r.warnings.end());
    if (!r.error.empty()) warnings->push_back(r.error);
}

/// Images d'une sélection par trsfOf(1) … trsfOf(repetitions), collées depuis un seul instantané de la sélection.
std::vector<int> copyImages(Model& m, const ElementSet& s, int repetitions, const std::function<gp_Trsf(int)>& trsfOf, const ClipboardOptions& options,
                            std::vector<std::string>* warnings)
{
    std::vector<int> created;
    if (repetitions < 1) return created;
    StructuralClipboard snapshot;
    snapshot.copyFrom(m, selectionOf(s), options);
    if (!snapshot.hasData()) return created;
    for (int step = 1; step <= repetitions; ++step)
    {
        const PasteResult r = snapshot.pasteImage(m, trsfOf(step));
        appendCreated(r, created);
        appendWarnings(r, warnings);
    }
    return created;
}

gp_Trsf translation(double dx, double dy, double dz, int step)
{
    gp_Trsf t;
    t.SetTranslation(gp_Vec(dx * step, dy * step, dz * step));
    return t;
}

gp_Trsf rotation(const gp_Pnt& center, const gp_Dir& axis, double angleRad, int step)
{
    gp_Trsf t;
    t.SetRotation(gp_Ax1(center, axis), angleRad * step);
    return t;
}
} // namespace


bool Model::moveNodes(const std::set<int>& nodeIds, double dx, double dy, double dz)
{
    if (nodeIds.empty())
        return false;

    for (int nid : nodeIds)
    {
        auto* n = getNode(nid);
        if (n)
        {
            n->setCoordinates(n->x() + dx, n->y() + dy, n->z() + dz);
            notifyNodeModified(nid);
        }
    }

    return true;
}

std::vector<int> Model::copyElements(const std::set<int>& nodeIds,
                                     const std::set<int>& beamIds,
                                     const std::set<int>& columnIds,
                                     const std::set<int>& slabIds,
                                     double dx, double dy, double dz, int repetitions,
                                     const std::set<int>& cableIds,
                                     const std::set<int>& trussIds)
{
    return copyElements(elementSet(nodeIds, beamIds, columnIds, slabIds, cableIds, trussIds), dx, dy, dz, repetitions, ClipboardOptions {});
}

std::vector<int> Model::copyElements(const ElementSet& elements, double dx, double dy, double dz, int repetitions, const ClipboardOptions& options,
                                     std::vector<std::string>* warnings)
{
    return copyImages(*this, elements, repetitions, [&](int step) { return translation(dx, dy, dz, step); }, options, warnings);
}

bool Model::rotateNodes(const std::set<int>& nodeIds, const gp_Pnt& center, const gp_Dir& axis, double angleRad)
{
    if (nodeIds.empty() || std::abs(angleRad) < 1e-7)
        return false;

    gp_Trsf trsf;
    trsf.SetRotation(gp_Ax1(center, axis), angleRad);

    for (int nid : nodeIds)
    {
        auto* n = getNode(nid);
        if (n)
        {
            gp_Pnt p(n->x(), n->y(), n->z());
            p.Transform(trsf);
            n->setCoordinates(p.X(), p.Y(), p.Z());
            notifyNodeModified(nid);
        }
    }
    return true;
}

std::vector<int> Model::copyAndRotateElements(const std::set<int>& nodeIds,
                                              const std::set<int>& beamIds,
                                              const std::set<int>& columnIds,
                                              const std::set<int>& slabIds,
                                              const gp_Pnt& center, const gp_Dir& axis,
                                              double angleRad, int repetitions,
                                              const std::set<int>& cableIds,
                                              const std::set<int>& trussIds)
{
    return copyAndRotateElements(elementSet(nodeIds, beamIds, columnIds, slabIds, cableIds, trussIds), center, axis, angleRad, repetitions, ClipboardOptions {});
}

std::vector<int> Model::copyAndRotateElements(const ElementSet& elements, const gp_Pnt& center, const gp_Dir& axis, double angleRad, int repetitions,
                                              const ClipboardOptions& options, std::vector<std::string>* warnings)
{
    if (std::abs(angleRad) < 1e-7)
        return {};
    return copyImages(*this, elements, repetitions, [&](int step) { return rotation(center, axis, angleRad, step); }, options, warnings);
}

std::vector<int> Model::mirrorCopyElements(const ElementSet& elements, const gp_Pnt& planePoint, const gp_Dir& planeNormal, const ClipboardOptions& options,
                                           std::vector<std::string>* warnings, double planeTol)
{
    std::vector<int> created;
    StructuralClipboard snapshot;
    snapshot.copyFrom(*this, selectionOf(elements), options);
    if (!snapshot.hasData()) return created;
    gp_Trsf trsf;
    trsf.SetMirror(gp_Ax2(planePoint, planeNormal));
    // Un nœud situé sur le plan est son propre symétrique (déplacement ≤ 2·tol) : partagé entre l'original et sa copie.
    PasteOptions paste;
    for (const auto& n : snapshot.nodes())
    {
        const gp_Pnt p(snapshot.originX() + n.relX, snapshot.originY() + n.relY, snapshot.originZ() + n.relZ);
        if (p.Distance(p.Transformed(trsf)) <= 2.0 * planeTol) paste.fixedNodes[n.originalId] = n.originalId;
    }
    const PasteResult r = snapshot.pasteImage(*this, trsf, paste);
    appendCreated(r, created);
    appendWarnings(r, warnings);
    return created;
}

} // namespace TSA::Model
