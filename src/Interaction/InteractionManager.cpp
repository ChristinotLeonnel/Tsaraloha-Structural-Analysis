#include "InteractionManager.h"

namespace TSA::Interaction
{

InteractionManager::InteractionManager(QObject* parent)
    : QObject(parent)
{
}

void InteractionManager::setMode(InteractionMode mode)
{
    if (m_mode != mode)
    {
        m_mode = mode;
        resetDrawingState();
        emit modeChanged(m_mode);
        emit promptChanged(promptText());
    }
}

bool InteractionManager::isDrawingMode() const noexcept
{
    switch (m_mode)
    {
    case InteractionMode::DrawNode:
    case InteractionMode::DrawBar:
    case InteractionMode::DrawBeam:
    case InteractionMode::DrawColumn:
    case InteractionMode::DrawSlab:
    case InteractionMode::DrawWall:
    case InteractionMode::DrawFoundation:
    case InteractionMode::DrawTruss:
        return true;
    default:
        return false;
    }
}

bool InteractionManager::isTransformMode() const noexcept
{
    switch (m_mode)
    {
    case InteractionMode::Move3D:
    case InteractionMode::Copy3D:
    case InteractionMode::Rotate3D:
    case InteractionMode::MoveOrigin3D:
    case InteractionMode::Paste3D:
        return true;
    default:
        return false;
    }
}

void InteractionManager::setStartPoint(const gp_Pnt& pt, int nodeId)
{
    m_startPoint = pt;
    m_startNodeId = nodeId;
    m_hasStartPoint = true;
    emit promptChanged(promptText());
}

void InteractionManager::addSlabPoint(const gp_Pnt& pt, int nodeId)
{
    m_slabPoints.push_back(pt);
    m_slabNodeIds.push_back(nodeId);
    emit promptChanged(promptText());
}

void InteractionManager::resetDrawingState()
{
    m_hasStartPoint = false;
    m_startPoint = gp_Pnt(0, 0, 0);
    m_startNodeId = 0;
    m_slabPoints.clear();
    m_slabNodeIds.clear();
    emit drawingStateReset();
}

QString InteractionManager::promptText() const
{
    switch (m_mode)
    {
    case InteractionMode::Select:
        return tr("Sélection : Cliquez pour sélectionner des éléments (Shift = cumul, Clic Droit = annuler)");
    case InteractionMode::DrawNode:
        return tr("Dessin Nœud : Cliquez dans l'espace 3D ou sur une grille pour créer un nœud (Échap = Annuler)");
    case InteractionMode::DrawBar:
    case InteractionMode::DrawBeam:
    case InteractionMode::DrawColumn:
        return m_hasStartPoint
            ? tr("Deuxième point : Cliquez le second point d'extrémité de la barre (Échap = Annuler)")
            : tr("Premier point : Cliquez l'origine de la barre (Échap = Annuler)");
    case InteractionMode::DrawSlab:
        return tr("Contour Dalle : Cliquez les sommets de la dalle (%1 points). Entrée ou Double-clic = Valider").arg(m_slabPoints.size());
    case InteractionMode::DrawWall:
        return m_hasStartPoint
            ? tr("Deuxième point : Cliquez le second point du voile (Échap = Annuler)")
            : tr("Premier point : Cliquez le premier point de base du voile (Échap = Annuler)");
    case InteractionMode::DrawFoundation:
        return tr("Créer Semelle : Cliquez sur un nœud de base pour y créer une fondation (Échap = Annuler)");
    case InteractionMode::DrawTruss:
        return m_hasStartPoint
            ? tr("Deuxième point : Cliquez la fin du membre de treillis (Échap = Annuler)")
            : tr("Premier point : Cliquez l'origine du membre de treillis (Échap = Annuler)");
    case InteractionMode::Move3D:
    case InteractionMode::Copy3D:
        return m_hasStartPoint
            ? tr("Point cible : Cliquez le point d'arrivée pour la transformation (Échap = Annuler)")
            : tr("Point de base : Cliquez le point de référence de départ (Échap = Annuler)");
    case InteractionMode::Rotate3D:
        return m_hasStartPoint
            ? tr("Point de direction : Cliquez le point définissant l'angle de rotation (Échap = Annuler)")
            : tr("Centre de rotation : Cliquez le centre du pivot (Échap = Annuler)");
    case InteractionMode::MoveOrigin3D:
        return tr("Nouvelle Origine : Cliquez le point devenant la nouvelle origine (0,0,0) (Échap = Annuler)");
    case InteractionMode::Paste3D:
        return tr("Coller 3D : Cliquez le point d'insertion pour les éléments du presse-papier (Échap = Annuler)");
    }
    return QString();
}

} // namespace TSA::Interaction
