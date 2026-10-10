#include "CommandCatalog.h"
#include <ProductIdentity.h>

#ifndef NDEBUG
#include <iostream>
#endif

namespace TSA::Commands {

CommandCatalog &CommandCatalog::instance() {
  static CommandCatalog s_instance;
  return s_instance;
}

CommandCatalog::CommandCatalog() { initializeStandardCatalog(); }

void CommandCatalog::registerCommand(const CommandDescriptor &desc) {
#ifndef NDEBUG
  // Un raccourci partage par deux commandes est "ambigu" pour Qt : aucune des
  // deux ne se declenche. On le signale des l'enregistrement.
  if (!desc.shortcut.empty()) {
    auto norm = [](const std::string &s) {
      std::string out;
      for (char c : s) {
        if (c != ' ') {
          out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
        }
      }
      return out;
    };
    std::string normDesc = norm(desc.shortcut);
    for (const auto &[otherId, other] : m_commands) {
      if (otherId != desc.id && norm(other.shortcut) == normDesc) {
        std::cerr << "[CommandCatalog] Raccourci en conflit '" << desc.shortcut
                  << "' : " << otherId << " <-> " << desc.id << std::endl;
      }
    }
  }
#endif
  m_commands[desc.id] = desc;
}

const CommandDescriptor *
CommandCatalog::findCommand(const std::string &id) const {
  auto it = m_commands.find(id);
  if (it != m_commands.end()) {
    return &it->second;
  }
  return nullptr;
}

std::vector<CommandDescriptor>
CommandCatalog::commandsInCategory(CommandCategory category) const {
  std::vector<CommandDescriptor> result;
  for (const auto &[id, desc] : m_commands) {
    if (desc.category == category) {
      result.push_back(desc);
    }
  }
  return result;
}

void CommandCatalog::initializeStandardCatalog() {
  // Source unique des commandes raccourcissables : libellé, description, raccourci(s) par défaut
  // (plusieurs séparés par « ; »), catégorie. Les raccourcis effectifs sont ceux de
  // ShortcutManager (shortcut.txt utilisateur, valeurs par défaut ci-dessous).
  registerCommand({"cmd.file.new", "Nouveau projet", "Créer un nouveau projet vierge", "Ctrl+N",
                   ":/icons/common/file_new.svg", CommandCategory::File});
  registerCommand({"cmd.file.open", "Ouvrir un projet", "Ouvrir un projet existant (.tsa)", "Ctrl+O",
                   ":/icons/common/file_open.svg", CommandCategory::File});
  registerCommand({"cmd.file.save", "Enregistrer", "Enregistrer le projet actuel", "Ctrl+S",
                   ":/icons/common/file_save.svg", CommandCategory::File});
  registerCommand({"cmd.file.save_as", "Enregistrer sous", "Enregistrer le projet sous un nouveau nom", "Ctrl+Shift+S",
                   ":/icons/common/file_save_as.svg", CommandCategory::File});
  registerCommand({"cmd.file.close", "Fermer le projet", "Fermer le projet (demande d'enregistrement si modifié) et revenir au Start Center", "Ctrl+W; Ctrl+F4",
                   "", CommandCategory::File});
  registerCommand({"cmd.file.exit", "Quitter", "Quitter TSA (Alt+F4 reste géré par Windows)", "",
                   "", CommandCategory::File});
  registerCommand({"cmd.file.export_diagnostic", "Exporter le diagnostic", "Exporter un rapport de diagnostic technique", "",
                   ":/icons/common/diagnostic.svg", CommandCategory::File});
  registerCommand({"cmd.model.topology", "Topologie et numérotation", "Paramètres du projet : stratégies de numérotation des nœuds et des éléments", "",
                   "", CommandCategory::Model});
  registerCommand({"cmd.model.clean", "Nettoyer le modèle", "Détecter et corriger les défauts géométriques du modèle", "",
                   "", CommandCategory::Model});
  registerCommand({"cmd.edit.undo", "Annuler", "Annuler la dernière modification", "Ctrl+Z",
                   ":/icons/edit/undo.svg", CommandCategory::Edit});
  registerCommand({"cmd.edit.redo", "Rétablir", "Rétablir la modification annulée", "Ctrl+Y; Ctrl+Shift+Z",
                   ":/icons/edit/redo.svg", CommandCategory::Edit});
  registerCommand({"cmd.edit.copy", "Copier", "Copier les éléments sélectionnés dans le presse-papier", "Ctrl+C; Ctrl+Ins",
                   ":/icons/edit/copy.svg", CommandCategory::Edit});
  registerCommand({"cmd.edit.paste", "Coller", "Coller les éléments du presse-papier en 3D", "Ctrl+V; Shift+Ins",
                   ":/icons/edit/paste.svg", CommandCategory::Edit});
  registerCommand({"cmd.edit.delete", "Supprimer", "Supprimer les éléments, charges ou cotations sélectionnés", "Del",
                   ":/icons/edit/delete.svg", CommandCategory::Edit});
  registerCommand({"cmd.edit.repeat", "Répéter la dernière commande", "Relancer la dernière commande de modélisation, de modification, de charge ou d'appui", "Ctrl+Return",
                   "", CommandCategory::Edit});
  registerCommand({"cmd.select.mode", "Mode sélection", "Revenir au mode sélection et annuler l'outil en cours", "Esc",
                   ":/icons/edit/select.svg", CommandCategory::Selection});
  registerCommand({"cmd.select.all", "Tout sélectionner", "Sélectionner tous les éléments", "Ctrl+A",
                   ":/icons/edit/select_all.svg", CommandCategory::Selection});
  registerCommand({"cmd.select.invert", "Inverser la sélection", "Sélectionner les éléments non sélectionnés", "Ctrl+Alt+I",
                   "", CommandCategory::Selection});
  registerCommand({"cmd.select.nodes", "Sélectionner les nœuds", "Sélectionner tous les nœuds", "Ctrl+Alt+N",
                   "", CommandCategory::Selection});
  registerCommand({"cmd.select.beams", "Sélectionner les poutres", "Sélectionner toutes les poutres", "Ctrl+Alt+B",
                   "", CommandCategory::Selection});
  registerCommand({"cmd.select.columns", "Sélectionner les poteaux", "Sélectionner tous les poteaux", "Ctrl+Alt+P",
                   "", CommandCategory::Selection});
  registerCommand({"cmd.select.slabs", "Sélectionner les dalles", "Sélectionner toutes les dalles", "",
                   "", CommandCategory::Selection});
  registerCommand({"cmd.select.walls", "Sélectionner les voiles", "Sélectionner tous les voiles", "",
                   "", CommandCategory::Selection});
  registerCommand({"cmd.select.foundations", "Sélectionner les fondations", "Sélectionner toutes les fondations", "",
                   "", CommandCategory::Selection});
  registerCommand({"cmd.select.truss", "Sélectionner les treillis", "Sélectionner toutes les barres de treillis", "",
                   "", CommandCategory::Selection});
  registerCommand({"cmd.select.cables", "Sélectionner les câbles", "Sélectionner tous les câbles", "",
                   "", CommandCategory::Selection});
  registerCommand({"cmd.select.same_section", "Même section", "Sélectionner les éléments de même section que la sélection", "Ctrl+Alt+S",
                   "", CommandCategory::Selection});
  registerCommand({"cmd.select.same_material", "Même matériau", "Sélectionner les éléments de même matériau que la sélection", "Ctrl+Alt+M",
                   "", CommandCategory::Selection});
  registerCommand({"cmd.select.active_level", "Éléments du niveau actif", "Sélectionner les éléments du niveau actif", "",
                   "", CommandCategory::Selection});
  registerCommand({"cmd.select.active_workplane", "Éléments du plan de travail", "Sélectionner les éléments du plan de travail actif", "",
                   "", CommandCategory::Selection});
  registerCommand({"cmd.isolate.selection", "Isoler la sélection", "Masquer tout sauf la sélection", "I",
                   "", CommandCategory::Selection});
  registerCommand({"cmd.isolate.same_type", "Isoler par type", "Isoler les éléments du même type que la sélection", "Alt+I",
                   "", CommandCategory::Selection});
  registerCommand({"cmd.isolate.workplane", "Isoler le plan de travail", "Isoler les éléments du plan de travail actif", "Alt+W",
                   ":/icons/view/view_top.svg", CommandCategory::Selection});
  registerCommand({"cmd.isolate.hide", "Masquer la sélection", "Masquer les éléments sélectionnés", "H",
                   "", CommandCategory::Selection});
  registerCommand({"cmd.isolate.invert", "Inverser l'isolation", "Échanger objets visibles et objets masqués", "",
                   "", CommandCategory::Selection});
  registerCommand({"cmd.isolate.undo", "Isolation précédente", "Annuler la dernière isolation", "Ctrl+H",
                   "", CommandCategory::Selection});
  registerCommand({"cmd.isolate.show_all", "Tout afficher", "Mettre fin à l'isolation et réafficher tous les objets", "Alt+H",
                   "", CommandCategory::Selection});
  registerCommand({"cmd.create.node", "Dessiner un nœud", "Mode dessin : nœud structural", "N",
                   ":/icons/structure/node.svg", CommandCategory::Create});
  registerCommand({"cmd.create.node_dialog", "Nouveau nœud (coordonnées)", "Créer un nœud par saisie de ses coordonnées", "Ctrl+Shift+N",
                   "", CommandCategory::Create});
  registerCommand({"cmd.create.beam", "Dessiner une poutre", "Mode dessin : poutre", "B",
                   ":/icons/structure/beam.svg", CommandCategory::Create});
  registerCommand({"cmd.create.column", "Dessiner un poteau", "Mode dessin : poteau", "C",
                   ":/icons/structure/column.svg", CommandCategory::Create});
  registerCommand({"cmd.create.bar", "Outil barres", "Fenêtre de tracé des éléments filaires", "",
                   ":/icons/structure/bar.svg", CommandCategory::Create});
  registerCommand({"cmd.create.cable", "Dessiner un câble", "Mode dessin : câble / hauban", "Alt+C",
                   ":/icons/structure/cable.svg", CommandCategory::Create});
  registerCommand({"cmd.create.slab", "Dessiner une dalle", "Mode dessin : dalle", "L",
                   ":/icons/structure/slab.svg", CommandCategory::Create});
  registerCommand({"cmd.create.wall", "Dessiner un voile", "Mode dessin : voile", "W",
                   ":/icons/structure/wall.svg", CommandCategory::Create});
  registerCommand({"cmd.create.truss", "Treillis paramétrique", "Générer un treillis paramétrique (Warren, Pratt, Howe)", "",
                   ":/icons/structure/truss.svg", CommandCategory::Create});
  registerCommand({"cmd.create.foundation", "Semelle / fondation", "Créer une semelle ou une fondation", "",
                   ":/icons/structure/footing.svg", CommandCategory::Create});
  registerCommand({"cmd.create.cube", "Cube structurel 3D", "Générer un cube structurel paramétrique", "",
                   "", CommandCategory::Create});
  registerCommand({"cmd.create.presets", "Paramètres de modélisation", "Sections, matériaux et options utilisés par les outils de dessin", "",
                   ":/icons/structure/presets.svg", CommandCategory::Create});
  registerCommand({"cmd.support.fixed", "Encastrement", "Affecter un encastrement (6 DDL bloqués) aux nœuds sélectionnés", "A, E",
                   "", CommandCategory::Create});
  registerCommand({"cmd.support.pinned", "Articulation", "Affecter une articulation (rotule 3D) aux nœuds sélectionnés", "A, A",
                   "", CommandCategory::Create});
  registerCommand({"cmd.support.roller", "Appui simple", "Affecter un appui simple (rouleau) aux nœuds sélectionnés", "A, S",
                   "", CommandCategory::Create});
  registerCommand({"cmd.tool.draw_beam_chain", "Poutres en chaîne", "Outil de dessin : poutres enchaînées", "",
                   "", CommandCategory::Create});
  registerCommand({"cmd.tool.draw_beam_rectangle", "Rectangle de poutres", "Outil de dessin : quatre poutres en rectangle", "",
                   "", CommandCategory::Create});
  registerCommand({"cmd.tool.draw_portal", "Portique", "Outil de dessin : portique (deux poteaux, une traverse)", "",
                   "", CommandCategory::Create});
  registerCommand({"cmd.tool.draw_x_bracing", "Croix de contreventement", "Outil de dessin : croix de Saint-André", "",
                   "", CommandCategory::Create});
  registerCommand({"cmd.tool.draw_beam_arc", "Poutres en arc", "Outil de dessin : arc discrétisé en poutres", "",
                   "", CommandCategory::Create});
  registerCommand({"cmd.tool.draw_grid_columns", "Poteaux sur la grille", "Outil de dessin : poteaux aux intersections de la grille", "",
                   "", CommandCategory::Create});
  registerCommand({"cmd.modify.move", "Déplacer (3D)", "Déplacer la sélection point à point", "M",
                   ":/icons/structure/struct_move.svg", CommandCategory::Modify});
  registerCommand({"cmd.modify.copy_3d", "Copier (3D)", "Copier la sélection point à point", "Ctrl+Shift+D",
                   "", CommandCategory::Modify});
  registerCommand({"cmd.modify.copy", "Copie numérique", "Copie numérique avec répétition (dialogue)", "Ctrl+D",
                   ":/icons/structure/struct_copy.svg", CommandCategory::Modify});
  registerCommand({"cmd.modify.translate", "Translation numérique", "Translation par dialogue (dX, dY, dZ)", "Ctrl+Shift+M",
                   ":/icons/move.svg", CommandCategory::Modify});
  registerCommand({"cmd.modify.rotate", "Rotation (3D)", "Rotation de la sélection autour d'un axe", "Ctrl+R",
                   ":/icons/edit/rotate.svg", CommandCategory::Modify});
  registerCommand({"cmd.modify.mirror", "Symétrie", "Symétrie / copie miroir (console : MIRROR, MI)", "",
                   ":/icons/edit/mirror.svg", CommandCategory::Modify});
  registerCommand({"cmd.modify.split_bars", "Diviser les barres", "Diviser les barres sélectionnées en N tronçons (console : SPLIT)", "",
                   ":/icons/structure/struct_split.svg", CommandCategory::Modify});
  registerCommand({"cmd.modify.merge_nodes", "Fusionner les nœuds", "Fusionner les nœuds confondus (console : MERGE)", "",
                   ":/icons/structure/struct_merge.svg", CommandCategory::Modify});
  registerCommand({"cmd.modify.move_origin", "Déplacer l'origine", "Déplacer l'origine 3D", "",
                   ":/icons/structure/struct_move.svg", CommandCategory::Modify});
  registerCommand({"cmd.modify.viewport_input", "Saisie dans la vue 3D", "Piloter les outils directement dans la vue 3D (sinon par dialogue)", "",
                   "", CommandCategory::Modify});
  registerCommand({"cmd.tool.move", "Outil déplacer", "Outil de modification : déplacer", "",
                   "", CommandCategory::Modify});
  registerCommand({"cmd.tool.copy", "Outil copier", "Outil de modification : copier", "",
                   "", CommandCategory::Modify});
  registerCommand({"cmd.tool.array_linear", "Réseau linéaire", "Outil de modification : réseau linéaire", "",
                   "", CommandCategory::Modify});
  registerCommand({"cmd.tool.rotate", "Outil rotation", "Outil de modification : rotation", "",
                   "", CommandCategory::Modify});
  registerCommand({"cmd.tool.array_polar", "Réseau polaire", "Outil de modification : réseau polaire", "",
                   "", CommandCategory::Modify});
  registerCommand({"cmd.tool.mirror", "Outil symétrie", "Outil de modification : symétrie", "",
                   "", CommandCategory::Modify});
  registerCommand({"cmd.tool.scale", "Échelle", "Outil de modification : mise à l'échelle", "",
                   "", CommandCategory::Modify});
  registerCommand({"cmd.tool.split", "Diviser", "Outil de modification : diviser une barre", "",
                   "", CommandCategory::Modify});
  registerCommand({"cmd.tool.split_at", "Couper en un point", "Outil de modification : couper une barre en un point", "",
                   "", CommandCategory::Modify});
  registerCommand({"cmd.tool.intersect", "Intersection", "Outil de modification : couper deux barres à leur intersection", "",
                   "", CommandCategory::Modify});
  registerCommand({"cmd.tool.extend", "Prolonger", "Outil de modification : prolonger jusqu'à une limite (AutoCAD : EX)", "E, X",
                   "", CommandCategory::Modify});
  registerCommand({"cmd.tool.trim", "Ajuster", "Outil de modification : ajuster à une limite (AutoCAD : TR)", "T, R",
                   "", CommandCategory::Modify});
  registerCommand({"cmd.tool.offset", "Décaler", "Outil de modification : copie décalée (AutoCAD : O)", "O, F",
                   "", CommandCategory::Modify});
  registerCommand({"cmd.tool.merge_nodes", "Outil fusion de nœuds", "Outil de modification : fusionner des nœuds", "",
                   "", CommandCategory::Modify});
  registerCommand({"cmd.tool.dim_aligned", "Cotation alignée", "Cotation en vraie grandeur entre deux points (AutoCAD : DAL)", "D, A",
                   "", CommandCategory::Annotation});
  registerCommand({"cmd.tool.dim_linear", "Cotation linéaire", "Cotation suivant X, Y ou Z selon le curseur (AutoCAD : DLI)", "D, L",
                   "", CommandCategory::Annotation});
  registerCommand({"cmd.tool.dim_horizontal", "Cotation horizontale", "Cotation de la composante horizontale", "D, H",
                   "", CommandCategory::Annotation});
  registerCommand({"cmd.tool.dim_x", "Cotation suivant X", "Cotation projetée sur X", "D, X",
                   "", CommandCategory::Annotation});
  registerCommand({"cmd.tool.dim_y", "Cotation suivant Y", "Cotation projetée sur Y", "D, Y",
                   "", CommandCategory::Annotation});
  registerCommand({"cmd.tool.dim_z", "Cotation suivant Z", "Cotation projetée sur Z", "D, Z",
                   "", CommandCategory::Annotation});
  registerCommand({"cmd.tool.dim_angular", "Cotation angulaire", "Angle entre deux bras (AutoCAD : DAN)", "D, N",
                   "", CommandCategory::Annotation});
  registerCommand({"cmd.tool.dim_level", "Cotation de niveau", "Niveau d'un point par rapport à la référence", "D, V",
                   "", CommandCategory::Annotation});
  registerCommand({"cmd.tool.dim_chain", "Cotation en chaîne", "Cotes successives sur une ligne commune (AutoCAD : DCO)", "D, C",
                   "", CommandCategory::Annotation});
  registerCommand({"cmd.tool.dim_cumulative", "Cotation cumulée", "Cotes depuis une origine commune (AutoCAD : DBA)", "D, B",
                   "", CommandCategory::Annotation});
  registerCommand({"cmd.dim.edit", "Modifier la cotation", "Modifier la cotation sélectionnée", "D, E",
                   "", CommandCategory::Annotation});
  registerCommand({"cmd.dim.delete", "Supprimer les cotations", "Supprimer les cotations sélectionnées", "",
                   "", CommandCategory::Annotation});
  registerCommand({"cmd.dim.visible", "Afficher les cotations", "Afficher ou masquer les cotations", "",
                   "", CommandCategory::Annotation});
  registerCommand({"cmd.dim.style", "Style des cotations", "Unités, précision, tailles et couleurs des cotations", "",
                   "", CommandCategory::Annotation});
  registerCommand({"cmd.dim.clean", "Supprimer les cotations invalides", "Supprimer les cotations dont un nœud a été supprimé", "",
                   "", CommandCategory::Annotation});
  registerCommand({"cmd.tools.measure", "Mesurer une distance", "Mesurer une distance 3D (AutoCAD : DI)", "D, I",
                   "", CommandCategory::Annotation});
  registerCommand({"cmd.display.grid", "Afficher la grille", "Afficher ou masquer la grille 3D", "G; F7",
                   ":/icons/view/grid.svg", CommandCategory::Snap});
  registerCommand({"cmd.snap.grid", "Magnétisme grille", "Activer ou désactiver le magnétisme de la grille (Snap)", "S",
                   ":/icons/snap.svg", CommandCategory::Snap});
  registerCommand({"cmd.snap.object_snap", "Accrochage objets", "Activer ou désactiver l'accrochage aux objets (OSNAP)", "F3",
                   ":/icons/view/snap.svg", CommandCategory::Snap});
  registerCommand({"cmd.display.levels", "Afficher les plans d'étages", "Afficher ou masquer les plans d'étages", "",
                   "", CommandCategory::Snap});
  registerCommand({"cmd.display.grid_labels", "Afficher les libellés d'axes", "Afficher ou masquer les libellés des axes de grille", "",
                   "", CommandCategory::Snap});
  registerCommand({"cmd.display.rulers", "Afficher les règles", "Afficher ou masquer les règles graduées", "",
                   ":/icons/view/rulers.svg", CommandCategory::Snap});
  registerCommand({"cmd.coord.axis_z", "Plan horizontal Z", "Bandeau de la vue : plans horizontaux (niveaux)", "Alt+Z",
                   "", CommandCategory::Snap});
  registerCommand({"cmd.coord.axis_x", "Coupe verticale X", "Bandeau de la vue : coupes perpendiculaires à X", "Alt+X",
                   "", CommandCategory::Snap});
  registerCommand({"cmd.coord.axis_y", "Coupe verticale Y", "Bandeau de la vue : coupes perpendiculaires à Y", "Alt+Y",
                   "", CommandCategory::Snap});
  registerCommand({"cmd.coord.workplane_xy", "Plan de travail XY", "Plan de travail global horizontal XY", "",
                   ":/icons/view/view_top.svg", CommandCategory::Snap});
  registerCommand({"cmd.coord.workplane_xz", "Plan de travail XZ", "Plan de travail global vertical XZ", "",
                   ":/icons/view/view_front.svg", CommandCategory::Snap});
  registerCommand({"cmd.coord.workplane_yz", "Plan de travail YZ", "Plan de travail global vertical YZ", "",
                   ":/icons/view/view_side.svg", CommandCategory::Snap});
  registerCommand({"cmd.coord.workplane_level", "Plan de travail sur l'étage", "Plan de travail sur l'étage actif", "",
                   ":/icons/structure/levels.svg", CommandCategory::Snap});
  registerCommand({"cmd.coord.workplane_custom", "Plan de travail personnalisé", "Définir un plan de travail personnalisé", "",
                   "", CommandCategory::Snap});
  registerCommand({"cmd.coord.workplane_visible", "Afficher le plan de travail", "Afficher ou masquer le plan de travail 3D", "",
                   "", CommandCategory::Snap});
  registerCommand({"cmd.view.normal_to_plane", "Vue normale au plan", "Orienter la caméra perpendiculairement au plan de travail", "Num+0",
                   "", CommandCategory::Snap});
  registerCommand({"cmd.struct.levels", "Étages et niveaux", "Gestionnaire des étages et niveaux", "Ctrl+L",
                   ":/icons/structure/levels.svg", CommandCategory::Structure});
  registerCommand({"cmd.struct.grid_dialog", "Gestionnaire de grilles", "Créer, modifier et activer les grilles d'axes", "Ctrl+G",
                   ":/icons/grid/grid_manager.svg", CommandCategory::Structure});
  registerCommand({"cmd.struct.new_grid", "Nouvelle grille 3D", "Créer une nouvelle grille d'axes", "Ctrl+Shift+G",
                   "", CommandCategory::Structure});
  registerCommand({"cmd.view.fit_all", "Zoom étendu", "Cadrer tout le modèle (AutoCAD : Z, E)", "F; Z, E",
                   ":/icons/view/fit_all.svg", CommandCategory::View});
  registerCommand({"cmd.view.fit_selection", "Zoom sur la sélection", "Cadrer la sélection", "Shift+F",
                   ":/icons/view/fit_all.svg", CommandCategory::View});
  registerCommand({"cmd.view.zoom_in", "Zoom avant", "Agrandir la vue", "+",
                   ":/icons/view/zoom_in.svg", CommandCategory::View});
  registerCommand({"cmd.view.zoom_out", "Zoom arrière", "Réduire la vue", "-",
                   ":/icons/view/zoom_out.svg", CommandCategory::View});
  registerCommand({"cmd.view.zoom_window", "Zoom fenêtre", "Zoomer sur un rectangle tracé (AutoCAD : Z, W)", "Z, W",
                   ":/icons/view/zoom_window.svg", CommandCategory::View});
  registerCommand({"cmd.view.prev", "Vue précédente", "Revenir à la vue précédente (AutoCAD : Z, P)", "Alt+Left; Z, P",
                   ":/icons/edit/undo.svg", CommandCategory::View});
  registerCommand({"cmd.view.next", "Vue suivante", "Revenir à la vue suivante", "Alt+Right",
                   ":/icons/edit/redo.svg", CommandCategory::View});
  registerCommand({"cmd.view.home", "Vue d'accueil", "Vue initiale du modèle", "Home",
                   ":/icons/view/view_3d.svg", CommandCategory::View});
  registerCommand({"cmd.view.reset", "Réinitialiser la vue", "Réinitialiser l'orientation de la caméra", "R",
                   ":/icons/view/view_iso.svg", CommandCategory::View});
  registerCommand({"cmd.view.top", "Vue de dessus", "Caméra vers -Z", "Num+7",
                   ":/icons/view/view_top.svg", CommandCategory::View});
  registerCommand({"cmd.view.bottom", "Vue de dessous", "Caméra vers +Z", "Ctrl+Num+7",
                   ":/icons/view/view_top.svg", CommandCategory::View});
  registerCommand({"cmd.view.front", "Vue de face", "Caméra vers +Y", "Num+1",
                   ":/icons/view/view_front.svg", CommandCategory::View});
  registerCommand({"cmd.view.back", "Vue arrière", "Caméra vers -Y", "Ctrl+Num+1",
                   ":/icons/view/view_front.svg", CommandCategory::View});
  registerCommand({"cmd.view.left", "Vue de gauche", "Caméra vers +X", "Num+3",
                   ":/icons/view/view_side.svg", CommandCategory::View});
  registerCommand({"cmd.view.right", "Vue de droite", "Caméra vers -X", "Ctrl+Num+3",
                   ":/icons/view/view_side.svg", CommandCategory::View});
  registerCommand({"cmd.view.iso", "Vue isométrique", "Vue 3D isométrique", "Num+5",
                   ":/icons/view/view_3d.svg", CommandCategory::View});
  registerCommand({"cmd.view.3d", "Vue 3D axonométrique", "Revenir à la vue 3D axonométrique", "",
                   "", CommandCategory::View});
  registerCommand({"cmd.view.xy", "Plan XY", "Vue d'étage (plan XY)", "",
                   ":/icons/view/view_top.svg", CommandCategory::View});
  registerCommand({"cmd.view.xz", "Plan XZ", "Élévation de face (plan XZ)", "",
                   ":/icons/view/view_front.svg", CommandCategory::View});
  registerCommand({"cmd.view.yz", "Plan YZ", "Coupe latérale / pignon (plan YZ)", "",
                   ":/icons/view/view_side.svg", CommandCategory::View});
  registerCommand({"cmd.view.rotate_left", "Pivoter la vue 2D à gauche", "Rotation de la vue 2D de -15°", "",
                   "", CommandCategory::View});
  registerCommand({"cmd.view.rotate_right", "Pivoter la vue 2D à droite", "Rotation de la vue 2D de +15°", "",
                   "", CommandCategory::View});
  registerCommand({"cmd.view.section_cut", "Coupes de la structure", "Plan de coupe dynamique 3D", "",
                   ":/icons/view/section_cut.svg", CommandCategory::View});
  registerCommand({"cmd.view.coord_system", "Repère local / global", "Afficher le repère local ou global", "",
                   "", CommandCategory::View});
  registerCommand({"cmd.view.fullscreen", "Plein écran", "Basculer en mode plein écran", "F11",
                   ":/icons/fullscreen.svg", CommandCategory::View});
  registerCommand({"cmd.view.display_physical", "Modèle physique", "Représentation : sections volumiques", "Alt+1",
                   ":/icons/view/view_shaded.svg", CommandCategory::View});
  registerCommand({"cmd.view.display_analytical", "Modèle filaire analytique", "Représentation : axes des barres et nœuds", "Alt+2",
                   ":/icons/view/display_analytical.svg", CommandCategory::View});
  registerCommand({"cmd.view.display_fe", "Modèle éléments finis", "Représentation : maillage réellement transmis au moteur", "Alt+3",
                   ":/icons/view/display_fe.svg", CommandCategory::View});
  registerCommand({"cmd.view.display_overlay", "Superposition", "Représentation : sections translucides et axes", "Alt+4",
                   ":/icons/view/view_transparent.svg", CommandCategory::View});
  registerCommand({"cmd.display.nodes", "Afficher les nœuds", "Afficher ou masquer les nœuds", "",
                   "", CommandCategory::Display});
  registerCommand({"cmd.display.node_labels", "Afficher les numéros de nœuds", "Afficher ou masquer les étiquettes des nœuds", "",
                   "", CommandCategory::Display});
  registerCommand({"cmd.display.supports", "Afficher les appuis", "Afficher ou masquer les symboles d'appuis", "",
                   "", CommandCategory::Display});
  registerCommand({"cmd.display.support_labels", "Afficher les étiquettes d'appuis", "Afficher ou masquer les étiquettes d'appuis", "",
                   "", CommandCategory::Display});
  registerCommand({"cmd.display.loads", "Afficher les charges", "Afficher ou masquer les charges 3D", "Ctrl+Shift+L",
                   "", CommandCategory::Display});
  registerCommand({"cmd.display.forces", "Afficher les forces", "Afficher ou masquer les forces", "",
                   "", CommandCategory::Display});
  registerCommand({"cmd.display.moments", "Afficher les moments", "Afficher ou masquer les moments", "",
                   "", CommandCategory::Display});
  registerCommand({"cmd.display.load_values", "Afficher les valeurs des charges", "Afficher ou masquer les valeurs des charges", "",
                   "", CommandCategory::Display});
  registerCommand({"cmd.properties.panel", "Panneau Propriétés", "Afficher ou masquer le panneau Propriétés", "P",
                   ":/icons/view/properties.svg", CommandCategory::Properties});
  registerCommand({"cmd.library.custom", "Bibliothèque personnalisée", "Bibliothèque personnalisée de sections et matériaux", "Ctrl+B",
                   "", CommandCategory::Libraries});
  registerCommand({"cmd.library.manager", "Gestionnaire TSALib", "Gestionnaire d'extensions TSALib", "",
                   ":/icons/extension_manager.svg", CommandCategory::Libraries});
  registerCommand({"cmd.section.i", "Profilé en I/H", "Créer un profilé IPE / HEA / HEB", "",
                   "", CommandCategory::Libraries});
  registerCommand({"cmd.section.rect", "Section rectangulaire", "Créer une section rectangulaire", "",
                   "", CommandCategory::Libraries});
  registerCommand({"cmd.section.circ", "Section circulaire", "Créer une section circulaire", "",
                   "", CommandCategory::Libraries});
  registerCommand({"cmd.material.concrete", "Béton armé", "Matériau béton armé (C25/30)", "",
                   "", CommandCategory::Libraries});
  registerCommand({"cmd.material.steel", "Acier structural", "Matériau acier (S355)", "",
                   "", CommandCategory::Libraries});
  registerCommand({"cmd.loads.point", "Force et couple", "Charge nodale : force et moment", "Q, N",
                   ":/icons/load_point.svg", CommandCategory::Loads});
  registerCommand({"cmd.loads.distributed", "Charge uniforme", "Charge répartie uniforme sur barre", "Q, U",
                   ":/icons/load_distributed.svg", CommandCategory::Loads});
  registerCommand({"cmd.loads.trapezoidal", "Charge trapézoïdale", "Charge répartie trapézoïdale sur barre", "Q, T",
                   "", CommandCategory::Loads});
  registerCommand({"cmd.loads.bar_point", "Force ponctuelle sur barre", "Charge concentrée sur une barre", "Q, P",
                   "", CommandCategory::Loads});
  registerCommand({"cmd.loads.surface", "Charge surfacique", "Charge répartie sur surface", "Q, S",
                   "", CommandCategory::Loads});
  registerCommand({"cmd.loads.self_weight", "Poids propre", "Paramètres du poids propre", "Q, G",
                   "", CommandCategory::Loads});
  registerCommand({"cmd.loadcases.manager", "Cas de charges et combinaisons", "Gestionnaire des cas de charges et combinaisons", "Q, C",
                   "", CommandCategory::LoadCases});
  registerCommand({"cmd.analysis.mesh", "Générer le maillage EF", "Estimation du maillage (aucun maillage n'est généré : voir docs/DISPLAY_MODES.md)", "",
                   ":/icons/analysis_mesh.svg", CommandCategory::Mesh});
  registerCommand({"cmd.analysis.solve", "Lancer le calcul", "Lancer le calcul structurel (validations habituelles)", "F5",
                   ":/icons/analysis_run.svg", CommandCategory::Analysis});
  registerCommand({"cmd.analysis.config", "Paramètres d'analyse", "Moteur, portée, type d'analyse", "Ctrl+F5",
                   "", CommandCategory::Analysis});
  registerCommand({"cmd.results.deformed", "Afficher la déformée", "Afficher ou masquer la déformée 3D", "F9",
                   "", CommandCategory::Results});
  registerCommand({"cmd.results.reactions", "Afficher les réactions", "Afficher ou masquer les réactions d'appui", "Shift+F9",
                   "", CommandCategory::Results});
  registerCommand({"cmd.results.diagram_mz", "Diagramme Mz", "Diagramme du moment Mz", "",
                   "", CommandCategory::Results});
  registerCommand({"cmd.results.diagram_my", "Diagramme My", "Diagramme du moment My", "",
                   "", CommandCategory::Results});
  registerCommand({"cmd.results.diagram_mx", "Diagramme Mx", "Diagramme de torsion Mx", "",
                   "", CommandCategory::Results});
  registerCommand({"cmd.results.diagram_vz", "Diagramme Vz", "Diagramme de l'effort tranchant Vz", "",
                   "", CommandCategory::Results});
  registerCommand({"cmd.results.diagram_vy", "Diagramme Vy", "Diagramme de l'effort tranchant Vy", "",
                   "", CommandCategory::Results});
  registerCommand({"cmd.results.diagram_n", "Diagramme N", "Diagramme de l'effort normal", "",
                   "", CommandCategory::Results});
  registerCommand({"cmd.results.diagram_deflection", "Flèches", "Diagramme des flèches", "",
                   "", CommandCategory::Results});
  registerCommand({"cmd.results.diagram_none", "Masquer les diagrammes", "Masquer les diagrammes d'efforts", "",
                   "", CommandCategory::Results});
  registerCommand({"cmd.results.fit_model", "Cadrer le modèle", "Cadrer le modèle (résultats)", "",
                   "", CommandCategory::Results});
  registerCommand({"cmd.results.fit_results", "Cadrer les résultats", "Cadrer les résultats affichés", "",
                   "", CommandCategory::Results});
  registerCommand({"cmd.results.fit_deformed", "Cadrer la déformée", "Cadrer la déformée", "",
                   "", CommandCategory::Results});
  registerCommand({"cmd.results.displacements", "Déplacements", "Déformée et déplacements", "",
                   ":/icons/results_disp.svg", CommandCategory::Results});
  registerCommand({"cmd.results.forces", "Efforts internes", "Diagrammes des efforts (M, N, V)", "",
                   ":/icons/results_forces.svg", CommandCategory::Results});
  registerCommand({"cmd.results.stresses", "Contraintes", "Contraintes de Von Mises", "",
                   ":/icons/results_stress.svg", CommandCategory::Results});
  registerCommand({"cmd.report.ndc", "Note de calcul", "Ouvrir la note de calcul", "F8",
                   "", CommandCategory::Documentation});
  registerCommand({"cmd.bim.import_ifc", "Importer IFC", "Importer un modèle IFC", "Ctrl+I",
                   "", CommandCategory::Bim});
  registerCommand({"cmd.bim.export_ifc", "Exporter IFC", "Exporter le modèle en IFC", "Ctrl+E",
                   "", CommandCategory::Bim});
  registerCommand({"cmd.window.results", "Panneau Résultats", "Afficher ou masquer le panneau Résultats structuraux 3D", "Ctrl+1",
                   "", CommandCategory::Workspace});
  registerCommand({"cmd.window.properties", "Fenêtre Propriétés", "Afficher ou masquer la fenêtre Propriétés", "Ctrl+2",
                   "", CommandCategory::Workspace});
  registerCommand({"cmd.window.model_browser", "Navigateur du modèle", "Afficher ou masquer le navigateur du modèle", "Ctrl+3",
                   "", CommandCategory::Workspace});
  registerCommand({"cmd.window.work_planes", "Plans de travail et vues", "Afficher ou masquer le panneau des plans de travail", "Ctrl+4",
                   "", CommandCategory::Workspace});
  registerCommand({"cmd.window.visibility", "Calques et visibilité", "Afficher ou masquer le panneau Calques et visibilité", "Ctrl+5",
                   "", CommandCategory::Workspace});
  registerCommand({"cmd.window.elements", "Éléments structuraux", "Afficher ou masquer le panneau des éléments", "Ctrl+6",
                   "", CommandCategory::Workspace});
  registerCommand({"cmd.window.analysis_data", "Données d'analyse", "Afficher ou masquer le panneau des données d'analyse", "Ctrl+7",
                   "", CommandCategory::Workspace});
  registerCommand({"cmd.window.console", "Console et messages", "Afficher ou masquer la console", "F2",
                   "", CommandCategory::Workspace});
  registerCommand({"cmd.ai.assistant", "Assistant IA", "Ouvrir l'assistant de co-ingénierie", "Ctrl+Shift+I",
                   "", CommandCategory::Tools});
  registerCommand({"cmd.ai.config", "Configuration IA", "Paramètres de l'assistant IA", "",
                   "", CommandCategory::Tools});
  registerCommand({"cmd.ai.check", "Vérifier la structure (IA)", "Vérification de la structure par l'assistant", "",
                   "", CommandCategory::Tools});
  registerCommand({"cmd.ai.analyze", "Analyser le modèle (IA)", "Analyse du modèle par l'assistant", "",
                   "", CommandCategory::Tools});
  registerCommand({"cmd.ai.explain", "Expliquer avec l'IA", "Explication des résultats par l'assistant", "",
                   "", CommandCategory::Tools});
  registerCommand({"cmd.settings.theme", "Thème sombre / clair", "Basculer entre le thème sombre et le thème clair", "Ctrl+T",
                   ":/icons/common/theme_dark.svg", CommandCategory::Settings});
  registerCommand({"cmd.help.shortcuts", "Liste des raccourcis", "Afficher la liste des raccourcis clavier", "F1",
                   ":/icons/common/shortcuts.svg", CommandCategory::Help});
  registerCommand({"cmd.help.shortcut_editor", "Personnaliser les raccourcis", "Ouvrir l'éditeur des raccourcis clavier", "Ctrl+F1",
                   "", CommandCategory::Help});
  registerCommand({"cmd.help.full", "Aide complète", "Ouvrir le centre d'aide", "Shift+F1",
                   "", CommandCategory::Help});
  registerCommand({"cmd.help.online_docs", "Documentation en ligne", "Ouvrir la documentation en ligne", "",
                   "", CommandCategory::Help});
  registerCommand({"cmd.help.report_problem", "Signaler un problème", "Signaler un problème", "",
                   "", CommandCategory::Help});
  registerCommand({"cmd.help.about", "À propos", "Informations de version", "",
                   "", CommandCategory::Help});
}

} // namespace TSA::Commands
