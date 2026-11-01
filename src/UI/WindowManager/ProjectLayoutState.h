#pragma once

#include <QJsonObject>
#include <QList>
#include <QStringList>

class QDockWidget;
class QTabBar;

namespace TSA::UI
{
class WindowManager;
class WorkspaceView;

/// Disposition de la fenêtre propre à un projet (bloc UIST du .tsa, docs/PROJECT_STATE_PLAN.md §3.1), JSON « schema » 1 :
/// état des panneaux (binaire Qt en base64), panneaux ouverts et fermés (listes lisibles : repli si Qt refuse l'état
/// binaire), espace de travail (WorkspaceView::stateJson) et onglet actif du ruban (titre et indice ; barre d'onglets
/// du ruban, RibbonBar::tabBar : le ruban appartient à la couche application, ce composant à la couche widgets).
/// @param temporarilyHidden panneaux masqués par un mode contextuel (Blueprint, focus) : enregistrés comme ouverts, sans
///        être réaffichés (jamais un masquage temporaire dans la disposition enregistrée).
QJsonObject captureProjectLayout(const WindowManager& windows, const WorkspaceView* workspace, const QTabBar* ribbonTabs,
                                 const QList<const QDockWidget*>& temporarilyHidden = {});

/// Applique, dans l'ordre : état binaire des panneaux, visibilité des listes lisibles (seulement les panneaux qui y
/// figurent : un panneau apparu depuis garde sa visibilité), espace de travail, onglet du ruban (par titre, sinon par
/// indice). Renvoie les avertissements (version inconnue, état binaire refusé, onglet introuvable) ; jamais bloquant.
QStringList applyProjectLayout(WindowManager& windows, WorkspaceView* workspace, QTabBar* ribbonTabs, const QJsonObject& state);

} // namespace TSA::UI
