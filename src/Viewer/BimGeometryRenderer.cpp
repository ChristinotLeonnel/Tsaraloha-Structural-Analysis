#include "BimGeometryRenderer.h"

#include "../BIM/Core/BimModel.h"

#include <Graphic3d_ArrayOfTriangles.hxx>
#include <Graphic3d_Group.hxx>
#include <Prs3d_Presentation.hxx>
#include <Prs3d_ShadingAspect.hxx>
#include <PrsMgr_PresentationManager.hxx>
#include <Select3D_SensitivePrimitiveArray.hxx>
#include <SelectMgr_Selection.hxx>
#include <gp_Trsf.hxx>

namespace TSA::Viewer
{

namespace
{
/// Maillage d'un produit : triangles ombrés (sommets dédoublés, normale du triangle), sélection par triangles.
class BimProductObject : public AIS_InteractiveObject
{
public:
    BimProductObject(int productId, TSA::BIM::MeshPtr mesh) : m_productId(productId), m_mesh(std::move(mesh))
    {
        const auto& m = *m_mesh;
        const auto triangles = static_cast<int>(m.triangleCount());
        m_triangles = new Graphic3d_ArrayOfTriangles(3 * triangles, 0, Graphic3d_ArrayFlags_VertexNormal);
        for (int t = 0; t < triangles; ++t)
        {
            gp_Pnt p[3];
            for (int k = 0; k < 3; ++k)
            {
                const std::size_t i = m.indices[3 * static_cast<std::size_t>(t) + k];
                p[k].SetCoord(m.positions[3 * i], m.positions[3 * i + 1], m.positions[3 * i + 2]);
            }
            const gp_Vec normal = gp_Vec(p[0], p[1]).Crossed(gp_Vec(p[0], p[2]));
            const gp_Dir dir = normal.SquareMagnitude() > 1e-24 ? gp_Dir(normal) : gp_Dir(0.0, 0.0, 1.0);   // triangle dégénéré
            for (const gp_Pnt& q : p) m_triangles->AddVertex(q, dir);
        }

        // Couleur du style IFC (sRGB), deux faces éclairées : les maillages IFC ne sont pas toujours orientés.
        myDrawer->SetShadingAspect(new Prs3d_ShadingAspect());
        myDrawer->ShadingAspect()->SetMaterial(Graphic3d_MaterialAspect(Graphic3d_NameOfMaterial_Plastified));
        myDrawer->ShadingAspect()->SetColor(Quantity_Color(m.color[0], m.color[1], m.color[2], Quantity_TOC_sRGB));
        myDrawer->ShadingAspect()->Aspect()->SetFaceCulling(Graphic3d_TypeOfBackfacingModel_DoubleSided);
        if (m.color[3] < 0.99f) SetTransparency(1.0 - m.color[3]);   // vitrages…

        gp_Trsf placement;
        placement.SetTranslation(gp_Vec(m.origin[0], m.origin[1], m.origin[2]));
        SetLocalTransformation(placement);

        const auto b = m.bounds();
        m_bounds.Update(b[0], b[1], b[2], b[3], b[4], b[5]);
    }

    int productId() const { return m_productId; }
    const TSA::BIM::MeshPtr& mesh() const { return m_mesh; }
    const Bnd_Box& bounds() const { return m_bounds; }

    bool AcceptDisplayMode(const int mode) const override { return mode == 0; }

    void Compute(const Handle(PrsMgr_PresentationManager)&, const Handle(Prs3d_Presentation)& prs, const int mode) override
    {
        if (mode != 0) return;
        Handle(Graphic3d_Group) group = prs->NewGroup();
        group->SetGroupPrimitivesAspect(myDrawer->ShadingAspect()->Aspect());
        group->AddPrimitiveArray(m_triangles);
    }

    void ComputeSelection(const Handle(SelectMgr_Selection)& selection, const int mode) override
    {
        if (mode != 0) return;
        Handle(SelectMgr_EntityOwner) owner = new SelectMgr_EntityOwner(this);
        Handle(Select3D_SensitivePrimitiveArray) sensitive = new Select3D_SensitivePrimitiveArray(owner);
        if (sensitive->InitTriangulation(m_triangles->Attributes(), m_triangles->Indices(), TopLoc_Location())) selection->Add(sensitive);
    }

    DEFINE_STANDARD_RTTI_INLINE(BimProductObject, AIS_InteractiveObject)

private:
    int m_productId = 0;
    TSA::BIM::MeshPtr m_mesh;   ///< partagé avec le modèle (identité : objet à reconstruire si le maillage change)
    Handle(Graphic3d_ArrayOfTriangles) m_triangles;
    Bnd_Box m_bounds;
};
} // namespace

bool BimGeometryRenderer::drawn(const TSA::BIM::PhysicalElement& e)
{
    return e.analytical.empty() && e.geometry && e.geometry->valid();
}

bool BimGeometryRenderer::sync(const TSA::BIM::BimModel& bim)
{
    if (m_context.IsNull()) return false;
    bool changed = false;
    std::map<int, Handle(AIS_InteractiveObject)> next;
    for (const auto& [id, e] : bim.elements())
    {
        if (!drawn(e)) continue;
        if (auto it = m_objects.find(id); it != m_objects.end())
        {
            const auto existing = Handle(BimProductObject)::DownCast(it->second);
            if (!existing.IsNull() && existing->mesh() == e.geometry)
            {
                next.emplace(id, it->second);
                m_objects.erase(it);
                continue;
            }
        }
        Handle(BimProductObject) object = new BimProductObject(id, e.geometry);
        if (m_visible) m_context->Display(object, 0, 0, false);
        next.emplace(id, object);
        changed = true;
    }
    // Restent : produits disparus, reliés au calcul depuis, ou dont le maillage a changé
    for (const auto& [id, object] : m_objects) m_context->Remove(object, false);
    changed = changed || !m_objects.empty();
    m_objects = std::move(next);
    return changed;
}

void BimGeometryRenderer::clear()
{
    if (!m_context.IsNull())
        for (const auto& [id, object] : m_objects) m_context->Remove(object, false);
    m_objects.clear();
}

void BimGeometryRenderer::setVisible(bool visible)
{
    if (visible == m_visible) return;
    m_visible = visible;
    if (m_context.IsNull()) return;
    for (const auto& [id, object] : m_objects)
    {
        if (visible) m_context->Display(object, 0, 0, false);
        else m_context->Erase(object, false);
    }
}

std::size_t BimGeometryRenderer::displayedCount() const
{
    if (m_context.IsNull()) return 0;
    std::size_t n = 0;
    for (const auto& [id, object] : m_objects)
        if (m_context->IsDisplayed(object)) ++n;
    return n;
}

Bnd_Box BimGeometryRenderer::bounds() const
{
    Bnd_Box box;
    for (const auto& [id, object] : m_objects)
        if (const auto product = Handle(BimProductObject)::DownCast(object); !product.IsNull()) box.Add(product->bounds());
    return box;
}

int BimGeometryRenderer::productOf(const Handle(SelectMgr_EntityOwner)& owner) const
{
    if (owner.IsNull() || !owner->HasSelectable()) return 0;
    const auto product = Handle(BimProductObject)::DownCast(owner->Selectable());
    if (product.IsNull()) return 0;
    const auto it = m_objects.find(product->productId());
    return it != m_objects.end() && it->second == product ? product->productId() : 0;
}

} // namespace TSA::Viewer
