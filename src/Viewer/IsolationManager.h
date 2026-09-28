#pragma once

// Système d'isolation : masquer / isoler des entités du viewer 3D sans les supprimer du modèle.
// Header-only : aucun ajout à CMakeLists.txt nécessaire.

#include <AIS_InteractiveContext.hxx>
#include <AIS_InteractiveObject.hxx>
#include <AIS_ListOfInteractive.hxx>
#include <Bnd_Box.hxx>
#include <gp_Ax3.hxx>
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>

#include <algorithm>
#include <set>
#include <vector>

namespace TSA::Viewer
{

class IsolationManager
{
public:
    /// Objets à ne jamais masquer (grille, trièdre du plan, gizmo, flèche de projection...).
    void protect(const Handle(AIS_InteractiveObject)& obj)
    {
        if (!obj.IsNull())
            m_protected.insert(obj.get());
    }
    void unprotect(const Handle(AIS_InteractiveObject)& obj)
    {
        if (!obj.IsNull())
            m_protected.erase(obj.get());
    }

    bool isActive() const { return !m_hidden.empty(); }

    /// "Isoler" : ne garde affichée que la sélection courante.
    void isolateSelection(const Handle(AIS_InteractiveContext)& ctx)
    {
        std::vector<Handle(AIS_InteractiveObject)> keep = selected(ctx);
        if (keep.empty())
            return;
        AIS_ListOfInteractive shown;
        ctx->DisplayedObjects(shown);
        for (const Handle(AIS_InteractiveObject)& obj : shown)
        {
            if (isProtected(obj) || std::find(keep.begin(), keep.end(), obj) != keep.end())
                continue;
            hide(ctx, obj);
        }
        ctx->UpdateCurrentViewer();
    }

    /// "Masquer" : cache la sélection courante.
    void hideSelection(const Handle(AIS_InteractiveContext)& ctx)
    {
        for (const Handle(AIS_InteractiveObject)& obj : selected(ctx))
            if (!isProtected(obj))
                hide(ctx, obj);
        ctx->UpdateCurrentViewer();
    }

    /// Isolation automatique pour la projection : cache tout objet situé ENTIÈREMENT du côté
    /// caméra du plan (il gênerait la vue). 'flip' = même valeur que pour la projection.
    void isolateForProjection(const Handle(AIS_InteractiveContext)& ctx, const gp_Ax3& plane, bool flip = false,
                              double tol = 1e-6)
    {
        gp_Vec n(plane.Direction());
        if (flip)
            n.Reverse();
        AIS_ListOfInteractive shown;
        ctx->DisplayedObjects(shown);
        for (const Handle(AIS_InteractiveObject)& obj : shown)
        {
            if (isProtected(obj))
                continue;
            const Bnd_Box& box = obj->BoundingBox();
            if (box.IsVoid() || box.IsOpen())
                continue;
            double x0, y0, z0, x1, y1, z1;
            box.Get(x0, y0, z0, x1, y1, z1);
            double minDist = 1e300;
            for (int i = 0; i < 8; ++i)
            {
                const gp_Pnt c((i & 1) ? x1 : x0, (i & 2) ? y1 : y0, (i & 4) ? z1 : z0);
                minDist = std::min(minDist, gp_Vec(plane.Location(), c).Dot(n));
            }
            if (minDist > tol)
                hide(ctx, obj);
        }
        ctx->UpdateCurrentViewer();
    }

    /// "Tout afficher" : quitte le mode isolation.
    void restoreAll(const Handle(AIS_InteractiveContext)& ctx)
    {
        for (const Handle(AIS_InteractiveObject)& obj : m_hidden)
            ctx->Display(obj, false);
        m_hidden.clear();
        ctx->UpdateCurrentViewer();
    }

    /// À appeler si un objet est supprimé du modèle pendant l'isolation.
    void forget(const Handle(AIS_InteractiveObject)& obj)
    {
        m_hidden.erase(std::remove(m_hidden.begin(), m_hidden.end(), obj), m_hidden.end());
        unprotect(obj);
    }

private:
    bool isProtected(const Handle(AIS_InteractiveObject)& o) const { return m_protected.count(o.get()) != 0; }

    void hide(const Handle(AIS_InteractiveContext)& ctx, const Handle(AIS_InteractiveObject)& obj)
    {
        ctx->Erase(obj, false);
        m_hidden.push_back(obj);
    }

    static std::vector<Handle(AIS_InteractiveObject)> selected(const Handle(AIS_InteractiveContext)& ctx)
    {
        std::vector<Handle(AIS_InteractiveObject)> out;
        for (ctx->InitSelected(); ctx->MoreSelected(); ctx->NextSelected())
            out.push_back(ctx->SelectedInteractive());
        return out;
    }

    std::vector<Handle(AIS_InteractiveObject)> m_hidden;
    std::set<const AIS_InteractiveObject*> m_protected;
};

} // namespace TSA::Viewer
