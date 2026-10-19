#include "CommandCatalog.h"
#include <QtGlobal>   // QT_TRANSLATE_NOOP : libellés extraits par lupdate, traduits à l'affichage
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
  registerCommand({"cmd.file.new", QT_TRANSLATE_NOOP("CommandCatalog", "Nouveau projet"), QT_TRANSLATE_NOOP("CommandCatalog", "Créer un nouveau projet vierge"), "Ctrl+N",
                   ":/icons/common/file_new.svg", CommandCategory::File});
  registerCommand({"cmd.file.open", QT_TRANSLATE_NOOP("CommandCatalog", "Ouvrir un projet"), QT_TRANSLATE_NOOP("CommandCatalog", "Ouvrir un projet existant (.tsa)"), "Ctrl+O",
                   ":/icons/common/file_open.svg", CommandCategory::File});
  registerCommand({"cmd.file.save", QT_TRANSLATE_NOOP("CommandCatalog", "Enregistrer"), QT_TRANSLATE_NOOP("CommandCatalog", "Enregistrer le projet actuel"), "Ctrl+S",
                   ":/icons/common/file_save.svg", CommandCategory::File});
  registerCommand({"cmd.file.save_as", QT_TRANSLATE_NOOP("CommandCatalog", "Enregistrer sous"), QT_TRANSLATE_NOOP("CommandCatalog", "Enregistrer le projet sous un nouveau nom"), "Ctrl+Shift+S",
                   ":/icons/common/file_save_as.svg", CommandCategory::File});
  registerCommand({"cmd.file.save_as_template", QT_TRANSLATE_NOOP("CommandCatalog", "Enregistrer comme template"), QT_TRANSLATE_NOOP("CommandCatalog", "Enregistrer le projet comme modèle réutilisable (grille, niveaux, plans de travail, réglages) pour créer de nouveaux projets"), "",
                   ":/icons/common/save_as_template.svg", CommandCategory::File});
  registerCommand({"cmd.file.close", QT_TRANSLATE_NOOP("CommandCatalog", "Fermer le projet"), QT_TRANSLATE_NOOP("CommandCatalog", "Fermer le projet (demande d'enregistrement si modifié) et revenir au Start Center"), "Ctrl+W; Ctrl+F4",
                   "", CommandCategory::File});
  registerCommand({"cmd.file.exit", QT_TRANSLATE_NOOP("CommandCatalog", "Quitter"), QT_TRANSLATE_NOOP("CommandCatalog", "Quitter TSA (Alt+F4 reste géré par Windows)"), "",
                   "", CommandCategory::File});
  registerCommand({"cmd.file.export_diagnostic", QT_TRANSLATE_NOOP("CommandCatalog", "Exporter le diagnostic"), QT_TRANSLATE_NOOP("CommandCatalog", "Exporter un rapport de diagnostic technique"), "",
                   ":/icons/common/diagnostic.svg", CommandCategory::File});
  registerCommand({"cmd.model.topology", QT_TRANSLATE_NOOP("CommandCatalog", "Topologie et numérotation"), QT_TRANSLATE_NOOP("CommandCatalog", "Paramètres du projet : stratégies de numérotation des nœuds et des éléments"), "",
                   ":/icons/modeling/topology.svg", CommandCategory::Model});
  registerCommand({"cmd.model.clean", QT_TRANSLATE_NOOP("CommandCatalog", "Nettoyer le modèle"), QT_TRANSLATE_NOOP("CommandCatalog", "Détecter et corriger les défauts géométriques du modèle"), "",
                   ":/icons/modeling/model_clean.svg", CommandCategory::Model});
  registerCommand({"cmd.edit.undo", QT_TRANSLATE_NOOP("CommandCatalog", "Annuler"), QT_TRANSLATE_NOOP("CommandCatalog", "Annuler la dernière modification"), "Ctrl+Z",
                   ":/icons/edit/undo.svg", CommandCategory::Edit});
  registerCommand({"cmd.edit.redo", QT_TRANSLATE_NOOP("CommandCatalog", "Rétablir"), QT_TRANSLATE_NOOP("CommandCatalog", "Rétablir la modification annulée"), "Ctrl+Y; Ctrl+Shift+Z",
                   ":/icons/edit/redo.svg", CommandCategory::Edit});
  registerCommand({"cmd.edit.copy", QT_TRANSLATE_NOOP("CommandCatalog", "Copier"), QT_TRANSLATE_NOOP("CommandCatalog", "Copier les éléments sélectionnés dans le presse-papier"), "Ctrl+C; Ctrl+Ins",
                   ":/icons/edit/copy.svg", CommandCategory::Edit});
  registerCommand({"cmd.edit.paste", QT_TRANSLATE_NOOP("CommandCatalog", "Coller"), QT_TRANSLATE_NOOP("CommandCatalog", "Coller les éléments du presse-papier en 3D"), "Ctrl+V; Shift+Ins",
                   ":/icons/edit/paste.svg", CommandCategory::Edit});
  registerCommand({"cmd.edit.cut", QT_TRANSLATE_NOOP("CommandCatalog", "Couper"), QT_TRANSLATE_NOOP("CommandCatalog", "Copier les éléments sélectionnés dans le presse-papier puis les supprimer"), "Ctrl+X",
                   ":/icons/edit/cut.svg", CommandCategory::Edit});
  registerCommand({"cmd.edit.paste_original", QT_TRANSLATE_NOOP("CommandCatalog", "Coller à la position d'origine"), QT_TRANSLATE_NOOP("CommandCatalog", "Coller les éléments du presse-papier aux coordonnées du projet source"), "Ctrl+Shift+V",
                   ":/icons/edit/paste_in_place.svg", CommandCategory::Edit});
  registerCommand({"cmd.edit.delete", QT_TRANSLATE_NOOP("CommandCatalog", "Supprimer"), QT_TRANSLATE_NOOP("CommandCatalog", "Supprimer les éléments, charges ou cotations sélectionnés"), "Del",
                   ":/icons/edit/delete.svg", CommandCategory::Edit});
  registerCommand({"cmd.edit.repeat", QT_TRANSLATE_NOOP("CommandCatalog", "Répéter la dernière commande"), QT_TRANSLATE_NOOP("CommandCatalog", "Relancer la dernière commande de modélisation, de modification, de charge ou d'appui"), "Ctrl+Return",
                   "", CommandCategory::Edit});
  registerCommand({"cmd.select.mode", QT_TRANSLATE_NOOP("CommandCatalog", "Mode sélection"), QT_TRANSLATE_NOOP("CommandCatalog", "Revenir au mode sélection et annuler l'outil en cours"), "Esc",
                   ":/icons/edit/select.svg", CommandCategory::Selection});
  registerCommand({"cmd.select.all", QT_TRANSLATE_NOOP("CommandCatalog", "Tout sélectionner"), QT_TRANSLATE_NOOP("CommandCatalog", "Sélectionner tous les éléments"), "Ctrl+A",
                   ":/icons/edit/select_all.svg", CommandCategory::Selection});
  registerCommand({"cmd.select.invert", QT_TRANSLATE_NOOP("CommandCatalog", "Inverser la sélection"), QT_TRANSLATE_NOOP("CommandCatalog", "Sélectionner les éléments non sélectionnés"), "Ctrl+Alt+I",
                   "", CommandCategory::Selection});
  registerCommand({"cmd.select.nodes", QT_TRANSLATE_NOOP("CommandCatalog", "Sélectionner les nœuds"), QT_TRANSLATE_NOOP("CommandCatalog", "Sélectionner tous les nœuds"), "Ctrl+Alt+N",
                   "", CommandCategory::Selection});
  registerCommand({"cmd.select.beams", QT_TRANSLATE_NOOP("CommandCatalog", "Sélectionner les poutres"), QT_TRANSLATE_NOOP("CommandCatalog", "Sélectionner toutes les poutres"), "Ctrl+Alt+B",
                   "", CommandCategory::Selection});
  registerCommand({"cmd.select.columns", QT_TRANSLATE_NOOP("CommandCatalog", "Sélectionner les poteaux"), QT_TRANSLATE_NOOP("CommandCatalog", "Sélectionner tous les poteaux"), "Ctrl+Alt+P",
                   "", CommandCategory::Selection});
  registerCommand({"cmd.select.slabs", QT_TRANSLATE_NOOP("CommandCatalog", "Sélectionner les dalles"), QT_TRANSLATE_NOOP("CommandCatalog", "Sélectionner toutes les dalles"), "",
                   "", CommandCategory::Selection});
  registerCommand({"cmd.select.walls", QT_TRANSLATE_NOOP("CommandCatalog", "Sélectionner les voiles"), QT_TRANSLATE_NOOP("CommandCatalog", "Sélectionner tous les voiles"), "",
                   "", CommandCategory::Selection});
  registerCommand({"cmd.select.foundations", QT_TRANSLATE_NOOP("CommandCatalog", "Sélectionner les fondations"), QT_TRANSLATE_NOOP("CommandCatalog", "Sélectionner toutes les fondations"), "",
                   "", CommandCategory::Selection});
  registerCommand({"cmd.select.truss", QT_TRANSLATE_NOOP("CommandCatalog", "Sélectionner les treillis"), QT_TRANSLATE_NOOP("CommandCatalog", "Sélectionner toutes les barres de treillis"), "",
                   "", CommandCategory::Selection});
  registerCommand({"cmd.select.cables", QT_TRANSLATE_NOOP("CommandCatalog", "Sélectionner les câbles"), QT_TRANSLATE_NOOP("CommandCatalog", "Sélectionner tous les câbles"), "",
                   "", CommandCategory::Selection});
  registerCommand({"cmd.select.same_section", QT_TRANSLATE_NOOP("CommandCatalog", "Même section"), QT_TRANSLATE_NOOP("CommandCatalog", "Sélectionner les éléments de même section que la sélection"), "Ctrl+Alt+S",
                   "", CommandCategory::Selection});
  registerCommand({"cmd.select.same_material", QT_TRANSLATE_NOOP("CommandCatalog", "Même matériau"), QT_TRANSLATE_NOOP("CommandCatalog", "Sélectionner les éléments de même matériau que la sélection"), "Ctrl+Alt+M",
                   "", CommandCategory::Selection});
  registerCommand({"cmd.select.active_level", QT_TRANSLATE_NOOP("CommandCatalog", "Éléments du niveau actif"), QT_TRANSLATE_NOOP("CommandCatalog", "Sélectionner les éléments du niveau actif"), "",
                   "", CommandCategory::Selection});
  registerCommand({"cmd.select.active_workplane", QT_TRANSLATE_NOOP("CommandCatalog", "Éléments du plan de travail"), QT_TRANSLATE_NOOP("CommandCatalog", "Sélectionner les éléments du plan de travail actif"), "",
                   "", CommandCategory::Selection});
  registerCommand({"cmd.isolate.selection", QT_TRANSLATE_NOOP("CommandCatalog", "Isoler la sélection"), QT_TRANSLATE_NOOP("CommandCatalog", "Masquer tout sauf la sélection"), "I",
                   "", CommandCategory::Selection});
  registerCommand({"cmd.isolate.same_type", QT_TRANSLATE_NOOP("CommandCatalog", "Isoler par type"), QT_TRANSLATE_NOOP("CommandCatalog", "Isoler les éléments du même type que la sélection"), "Alt+I",
                   "", CommandCategory::Selection});
  registerCommand({"cmd.isolate.workplane", QT_TRANSLATE_NOOP("CommandCatalog", "Isoler le plan de travail"), QT_TRANSLATE_NOOP("CommandCatalog", "Isoler les éléments du plan de travail actif"), "Alt+W",
                   ":/icons/modeling/isolate_workplane.svg", CommandCategory::Selection});
  registerCommand({"cmd.isolate.hide", QT_TRANSLATE_NOOP("CommandCatalog", "Masquer la sélection"), QT_TRANSLATE_NOOP("CommandCatalog", "Masquer les éléments sélectionnés"), "H",
                   "", CommandCategory::Selection});
  registerCommand({"cmd.isolate.invert", QT_TRANSLATE_NOOP("CommandCatalog", "Inverser l'isolation"), QT_TRANSLATE_NOOP("CommandCatalog", "Échanger objets visibles et objets masqués"), "",
                   "", CommandCategory::Selection});
  registerCommand({"cmd.isolate.undo", QT_TRANSLATE_NOOP("CommandCatalog", "Isolation précédente"), QT_TRANSLATE_NOOP("CommandCatalog", "Annuler la dernière isolation"), "Ctrl+H",
                   "", CommandCategory::Selection});
  registerCommand({"cmd.isolate.show_all", QT_TRANSLATE_NOOP("CommandCatalog", "Tout afficher"), QT_TRANSLATE_NOOP("CommandCatalog", "Mettre fin à l'isolation et réafficher tous les objets"), "Alt+H",
                   "", CommandCategory::Selection});
  registerCommand({"cmd.create.node", QT_TRANSLATE_NOOP("CommandCatalog", "Dessiner un nœud"), QT_TRANSLATE_NOOP("CommandCatalog", "Mode dessin : nœud structural"), "N",
                   ":/icons/structure/node.svg", CommandCategory::Create});
  registerCommand({"cmd.create.node_dialog", QT_TRANSLATE_NOOP("CommandCatalog", "Nouveau nœud (coordonnées)"), QT_TRANSLATE_NOOP("CommandCatalog", "Créer un nœud par saisie de ses coordonnées"), "Ctrl+Shift+N",
                   "", CommandCategory::Create});
  registerCommand({"cmd.create.beam", QT_TRANSLATE_NOOP("CommandCatalog", "Dessiner une poutre"), QT_TRANSLATE_NOOP("CommandCatalog", "Mode dessin : poutre"), "B",
                   ":/icons/structure/beam.svg", CommandCategory::Create});
  registerCommand({"cmd.create.column", QT_TRANSLATE_NOOP("CommandCatalog", "Dessiner un poteau"), QT_TRANSLATE_NOOP("CommandCatalog", "Mode dessin : poteau"), "C",
                   ":/icons/structure/column.svg", CommandCategory::Create});
  registerCommand({"cmd.create.bar", QT_TRANSLATE_NOOP("CommandCatalog", "Outil barres"), QT_TRANSLATE_NOOP("CommandCatalog", "Fenêtre de tracé des éléments filaires"), "",
                   ":/icons/structure/bar.svg", CommandCategory::Create});
  registerCommand({"cmd.create.cable", QT_TRANSLATE_NOOP("CommandCatalog", "Dessiner un câble"), QT_TRANSLATE_NOOP("CommandCatalog", "Mode dessin : câble / hauban"), "Alt+C",
                   ":/icons/structure/cable.svg", CommandCategory::Create});
  registerCommand({"cmd.create.slab", QT_TRANSLATE_NOOP("CommandCatalog", "Dessiner une dalle"), QT_TRANSLATE_NOOP("CommandCatalog", "Mode dessin : dalle"), "L",
                   ":/icons/structure/slab.svg", CommandCategory::Create});
  registerCommand({"cmd.create.wall", QT_TRANSLATE_NOOP("CommandCatalog", "Dessiner un voile"), QT_TRANSLATE_NOOP("CommandCatalog", "Mode dessin : voile"), "W",
                   ":/icons/structure/wall.svg", CommandCategory::Create});
  registerCommand({"cmd.create.truss", QT_TRANSLATE_NOOP("CommandCatalog", "Treillis paramétrique"), QT_TRANSLATE_NOOP("CommandCatalog", "Générer un treillis paramétrique (Warren, Pratt, Howe)"), "",
                   ":/icons/structure/truss.svg", CommandCategory::Create});
  registerCommand({"cmd.create.foundation", QT_TRANSLATE_NOOP("CommandCatalog", "Semelle / fondation"), QT_TRANSLATE_NOOP("CommandCatalog", "Créer une semelle ou une fondation"), "",
                   ":/icons/structure/footing.svg", CommandCategory::Create});
  registerCommand({"cmd.create.cube", QT_TRANSLATE_NOOP("CommandCatalog", "Cube structurel 3D"), QT_TRANSLATE_NOOP("CommandCatalog", "Générer un cube structurel paramétrique"), "",
                   "", CommandCategory::Create});
  registerCommand({"cmd.create.presets", QT_TRANSLATE_NOOP("CommandCatalog", "Paramètres de modélisation"), QT_TRANSLATE_NOOP("CommandCatalog", "Sections, matériaux et options utilisés par les outils de dessin"), "",
                   ":/icons/modeling/modeling_settings.svg", CommandCategory::Create});
  registerCommand({"cmd.support.fixed", QT_TRANSLATE_NOOP("CommandCatalog", "Encastrement"), QT_TRANSLATE_NOOP("CommandCatalog", "Affecter un encastrement (6 DDL bloqués) aux nœuds sélectionnés"), "A, E",
                   "", CommandCategory::Create});
  registerCommand({"cmd.support.pinned", QT_TRANSLATE_NOOP("CommandCatalog", "Articulation"), QT_TRANSLATE_NOOP("CommandCatalog", "Affecter une articulation (rotule 3D) aux nœuds sélectionnés"), "A, A",
                   "", CommandCategory::Create});
  registerCommand({"cmd.support.roller", QT_TRANSLATE_NOOP("CommandCatalog", "Appui simple"), QT_TRANSLATE_NOOP("CommandCatalog", "Affecter un appui simple (rouleau) aux nœuds sélectionnés"), "A, S",
                   "", CommandCategory::Create});
  registerCommand({"cmd.tool.draw_beam_chain", QT_TRANSLATE_NOOP("CommandCatalog", "Poutres en chaîne"), QT_TRANSLATE_NOOP("CommandCatalog", "Outil de dessin : poutres enchaînées"), "",
                   "", CommandCategory::Create});
  registerCommand({"cmd.tool.draw_beam_rectangle", QT_TRANSLATE_NOOP("CommandCatalog", "Rectangle de poutres"), QT_TRANSLATE_NOOP("CommandCatalog", "Outil de dessin : quatre poutres en rectangle"), "",
                   "", CommandCategory::Create});
  registerCommand({"cmd.tool.draw_portal", QT_TRANSLATE_NOOP("CommandCatalog", "Portique"), QT_TRANSLATE_NOOP("CommandCatalog", "Outil de dessin : portique (deux poteaux, une traverse)"), "",
                   "", CommandCategory::Create});
  registerCommand({"cmd.tool.draw_x_bracing", QT_TRANSLATE_NOOP("CommandCatalog", "Croix de contreventement"), QT_TRANSLATE_NOOP("CommandCatalog", "Outil de dessin : croix de Saint-André"), "",
                   "", CommandCategory::Create});
  registerCommand({"cmd.tool.draw_beam_arc", QT_TRANSLATE_NOOP("CommandCatalog", "Poutres en arc"), QT_TRANSLATE_NOOP("CommandCatalog", "Outil de dessin : arc discrétisé en poutres"), "",
                   "", CommandCategory::Create});
  registerCommand({"cmd.tool.draw_grid_columns", QT_TRANSLATE_NOOP("CommandCatalog", "Poteaux sur la grille"), QT_TRANSLATE_NOOP("CommandCatalog", "Outil de dessin : poteaux aux intersections de la grille"), "",
                   "", CommandCategory::Create});
  registerCommand({"cmd.modify.move", QT_TRANSLATE_NOOP("CommandCatalog", "Déplacer (3D)"), QT_TRANSLATE_NOOP("CommandCatalog", "Déplacer la sélection point à point"), "M",
                   ":/icons/structure/struct_move.svg", CommandCategory::Modify});
  registerCommand({"cmd.modify.copy_3d", QT_TRANSLATE_NOOP("CommandCatalog", "Copier (3D)"), QT_TRANSLATE_NOOP("CommandCatalog", "Copier la sélection point à point"), "Ctrl+Shift+D",
                   "", CommandCategory::Modify});
  registerCommand({"cmd.modify.copy", QT_TRANSLATE_NOOP("CommandCatalog", "Copie numérique"), QT_TRANSLATE_NOOP("CommandCatalog", "Copie numérique avec répétition (dialogue)"), "Ctrl+D",
                   ":/icons/edit/copy_array.svg", CommandCategory::Modify});
  registerCommand({"cmd.modify.translate", QT_TRANSLATE_NOOP("CommandCatalog", "Translation numérique"), QT_TRANSLATE_NOOP("CommandCatalog", "Translation par dialogue (dX, dY, dZ)"), "Ctrl+Shift+M",
                   ":/icons/move.svg", CommandCategory::Modify});
  registerCommand({"cmd.modify.rotate", QT_TRANSLATE_NOOP("CommandCatalog", "Rotation (3D)"), QT_TRANSLATE_NOOP("CommandCatalog", "Rotation de la sélection autour d'un axe"), "Ctrl+R",
                   ":/icons/edit/rotate.svg", CommandCategory::Modify});
  registerCommand({"cmd.modify.mirror", QT_TRANSLATE_NOOP("CommandCatalog", "Symétrie"), QT_TRANSLATE_NOOP("CommandCatalog", "Symétrie / copie miroir (console : MIRROR, MI)"), "",
                   ":/icons/edit/mirror.svg", CommandCategory::Modify});
  registerCommand({"cmd.modify.split_bars", QT_TRANSLATE_NOOP("CommandCatalog", "Diviser les barres"), QT_TRANSLATE_NOOP("CommandCatalog", "Diviser les barres sélectionnées en N tronçons (console : SPLIT)"), "",
                   ":/icons/structure/struct_split.svg", CommandCategory::Modify});
  registerCommand({"cmd.modify.merge_nodes", QT_TRANSLATE_NOOP("CommandCatalog", "Fusionner les nœuds"), QT_TRANSLATE_NOOP("CommandCatalog", "Fusionner les nœuds confondus (console : MERGE)"), "",
                   ":/icons/structure/struct_merge.svg", CommandCategory::Modify});
  registerCommand({"cmd.modify.move_origin", QT_TRANSLATE_NOOP("CommandCatalog", "Déplacer l'origine"), QT_TRANSLATE_NOOP("CommandCatalog", "Déplacer l'origine 3D"), "",
                   ":/icons/edit/move_origin.svg", CommandCategory::Modify});
  registerCommand({"cmd.modify.viewport_input", QT_TRANSLATE_NOOP("CommandCatalog", "Saisie dans la vue 3D"), QT_TRANSLATE_NOOP("CommandCatalog", "Piloter les outils directement dans la vue 3D (sinon par dialogue)"), "",
                   ":/icons/tools/input_viewport.svg", CommandCategory::Modify});
  registerCommand({"cmd.tool.move", QT_TRANSLATE_NOOP("CommandCatalog", "Outil déplacer"), QT_TRANSLATE_NOOP("CommandCatalog", "Outil de modification : déplacer"), "",
                   "", CommandCategory::Modify});
  registerCommand({"cmd.tool.copy", QT_TRANSLATE_NOOP("CommandCatalog", "Outil copier"), QT_TRANSLATE_NOOP("CommandCatalog", "Outil de modification : copier"), "",
                   "", CommandCategory::Modify});
  registerCommand({"cmd.tool.array_linear", QT_TRANSLATE_NOOP("CommandCatalog", "Réseau linéaire"), QT_TRANSLATE_NOOP("CommandCatalog", "Outil de modification : réseau linéaire"), "",
                   "", CommandCategory::Modify});
  registerCommand({"cmd.tool.rotate", QT_TRANSLATE_NOOP("CommandCatalog", "Outil rotation"), QT_TRANSLATE_NOOP("CommandCatalog", "Outil de modification : rotation"), "",
                   "", CommandCategory::Modify});
  registerCommand({"cmd.tool.array_polar", QT_TRANSLATE_NOOP("CommandCatalog", "Réseau polaire"), QT_TRANSLATE_NOOP("CommandCatalog", "Outil de modification : réseau polaire"), "",
                   "", CommandCategory::Modify});
  registerCommand({"cmd.tool.mirror", QT_TRANSLATE_NOOP("CommandCatalog", "Outil symétrie"), QT_TRANSLATE_NOOP("CommandCatalog", "Outil de modification : symétrie"), "",
                   "", CommandCategory::Modify});
  registerCommand({"cmd.tool.scale", QT_TRANSLATE_NOOP("CommandCatalog", "Échelle"), QT_TRANSLATE_NOOP("CommandCatalog", "Outil de modification : mise à l'échelle"), "",
                   "", CommandCategory::Modify});
  registerCommand({"cmd.tool.split", QT_TRANSLATE_NOOP("CommandCatalog", "Diviser"), QT_TRANSLATE_NOOP("CommandCatalog", "Outil de modification : diviser une barre"), "",
                   "", CommandCategory::Modify});
  registerCommand({"cmd.tool.split_at", QT_TRANSLATE_NOOP("CommandCatalog", "Couper en un point"), QT_TRANSLATE_NOOP("CommandCatalog", "Outil de modification : couper une barre en un point"), "",
                   "", CommandCategory::Modify});
  registerCommand({"cmd.tool.intersect", QT_TRANSLATE_NOOP("CommandCatalog", "Intersection"), QT_TRANSLATE_NOOP("CommandCatalog", "Outil de modification : couper deux barres à leur intersection"), "",
                   "", CommandCategory::Modify});
  registerCommand({"cmd.tool.extend", QT_TRANSLATE_NOOP("CommandCatalog", "Prolonger"), QT_TRANSLATE_NOOP("CommandCatalog", "Outil de modification : prolonger jusqu'à une limite (AutoCAD : EX)"), "E, X",
                   "", CommandCategory::Modify});
  registerCommand({"cmd.tool.trim", QT_TRANSLATE_NOOP("CommandCatalog", "Ajuster"), QT_TRANSLATE_NOOP("CommandCatalog", "Outil de modification : ajuster à une limite (AutoCAD : TR)"), "T, R",
                   "", CommandCategory::Modify});
  registerCommand({"cmd.tool.offset", QT_TRANSLATE_NOOP("CommandCatalog", "Décaler"), QT_TRANSLATE_NOOP("CommandCatalog", "Outil de modification : copie décalée (AutoCAD : O)"), "O, F",
                   "", CommandCategory::Modify});
  registerCommand({"cmd.tool.merge_nodes", QT_TRANSLATE_NOOP("CommandCatalog", "Outil fusion de nœuds"), QT_TRANSLATE_NOOP("CommandCatalog", "Outil de modification : fusionner des nœuds"), "",
                   "", CommandCategory::Modify});
  registerCommand({"cmd.tool.dim_aligned", QT_TRANSLATE_NOOP("CommandCatalog", "Cotation alignée"), QT_TRANSLATE_NOOP("CommandCatalog", "Cotation en vraie grandeur entre deux points (AutoCAD : DAL)"), "D, A",
                   "", CommandCategory::Annotation});
  registerCommand({"cmd.tool.dim_linear", QT_TRANSLATE_NOOP("CommandCatalog", "Cotation linéaire"), QT_TRANSLATE_NOOP("CommandCatalog", "Cotation suivant X, Y ou Z selon le curseur (AutoCAD : DLI)"), "D, L",
                   "", CommandCategory::Annotation});
  registerCommand({"cmd.tool.dim_horizontal", QT_TRANSLATE_NOOP("CommandCatalog", "Cotation horizontale"), QT_TRANSLATE_NOOP("CommandCatalog", "Cotation de la composante horizontale"), "D, H",
                   "", CommandCategory::Annotation});
  registerCommand({"cmd.tool.dim_x", QT_TRANSLATE_NOOP("CommandCatalog", "Cotation suivant X"), QT_TRANSLATE_NOOP("CommandCatalog", "Cotation projetée sur X"), "D, X",
                   "", CommandCategory::Annotation});
  registerCommand({"cmd.tool.dim_y", QT_TRANSLATE_NOOP("CommandCatalog", "Cotation suivant Y"), QT_TRANSLATE_NOOP("CommandCatalog", "Cotation projetée sur Y"), "D, Y",
                   "", CommandCategory::Annotation});
  registerCommand({"cmd.tool.dim_z", QT_TRANSLATE_NOOP("CommandCatalog", "Cotation suivant Z"), QT_TRANSLATE_NOOP("CommandCatalog", "Cotation projetée sur Z"), "D, Z",
                   "", CommandCategory::Annotation});
  registerCommand({"cmd.tool.dim_angular", QT_TRANSLATE_NOOP("CommandCatalog", "Cotation angulaire"), QT_TRANSLATE_NOOP("CommandCatalog", "Angle entre deux bras (AutoCAD : DAN)"), "D, N",
                   "", CommandCategory::Annotation});
  registerCommand({"cmd.tool.dim_level", QT_TRANSLATE_NOOP("CommandCatalog", "Cotation de niveau"), QT_TRANSLATE_NOOP("CommandCatalog", "Niveau d'un point par rapport à la référence"), "D, V",
                   "", CommandCategory::Annotation});
  registerCommand({"cmd.tool.dim_chain", QT_TRANSLATE_NOOP("CommandCatalog", "Cotation en chaîne"), QT_TRANSLATE_NOOP("CommandCatalog", "Cotes successives sur une ligne commune (AutoCAD : DCO)"), "D, C",
                   "", CommandCategory::Annotation});
  registerCommand({"cmd.tool.dim_cumulative", QT_TRANSLATE_NOOP("CommandCatalog", "Cotation cumulée"), QT_TRANSLATE_NOOP("CommandCatalog", "Cotes depuis une origine commune (AutoCAD : DBA)"), "D, B",
                   "", CommandCategory::Annotation});
  registerCommand({"cmd.dim.edit", QT_TRANSLATE_NOOP("CommandCatalog", "Modifier la cotation"), QT_TRANSLATE_NOOP("CommandCatalog", "Modifier la cotation sélectionnée"), "D, E",
                   ":/icons/dimensions/dim_edit.svg", CommandCategory::Annotation});
  registerCommand({"cmd.dim.delete", QT_TRANSLATE_NOOP("CommandCatalog", "Supprimer les cotations"), QT_TRANSLATE_NOOP("CommandCatalog", "Supprimer les cotations sélectionnées"), "",
                   ":/icons/dimensions/dim_delete.svg", CommandCategory::Annotation});
  registerCommand({"cmd.dim.visible", QT_TRANSLATE_NOOP("CommandCatalog", "Afficher les cotations"), QT_TRANSLATE_NOOP("CommandCatalog", "Afficher ou masquer les cotations"), "",
                   ":/icons/dimensions/dim_visibility.svg", CommandCategory::Annotation});
  registerCommand({"cmd.dim.style", QT_TRANSLATE_NOOP("CommandCatalog", "Style des cotations"), QT_TRANSLATE_NOOP("CommandCatalog", "Unités, précision, tailles et couleurs des cotations"), "",
                   ":/icons/dimensions/dim_style.svg", CommandCategory::Annotation});
  registerCommand({"cmd.dim.clean", QT_TRANSLATE_NOOP("CommandCatalog", "Supprimer les cotations invalides"), QT_TRANSLATE_NOOP("CommandCatalog", "Supprimer les cotations dont un nœud a été supprimé"), "",
                   ":/icons/dimensions/dim_clean.svg", CommandCategory::Annotation});
  registerCommand({"cmd.tools.measure", QT_TRANSLATE_NOOP("CommandCatalog", "Mesurer une distance"), QT_TRANSLATE_NOOP("CommandCatalog", "Mesurer une distance 3D (AutoCAD : DI)"), "D, I",
                   "", CommandCategory::Annotation});
  registerCommand({"cmd.display.grid", QT_TRANSLATE_NOOP("CommandCatalog", "Afficher la grille"), QT_TRANSLATE_NOOP("CommandCatalog", "Afficher ou masquer la grille 3D"), "G; F7",
                   ":/icons/modeling/grid_visible.svg", CommandCategory::Snap});
  registerCommand({"cmd.snap.grid", QT_TRANSLATE_NOOP("CommandCatalog", "Magnétisme grille"), QT_TRANSLATE_NOOP("CommandCatalog", "Activer ou désactiver le magnétisme de la grille (Snap)"), "S",
                   ":/icons/modeling/snap_grid.svg", CommandCategory::Snap});
  registerCommand({"cmd.snap.object_snap", QT_TRANSLATE_NOOP("CommandCatalog", "Accrochage objets"), QT_TRANSLATE_NOOP("CommandCatalog", "Activer ou désactiver l'accrochage aux objets (OSNAP)"), "F3",
                   ":/icons/modeling/snap_object.svg", CommandCategory::Snap});
  registerCommand({"cmd.display.levels", QT_TRANSLATE_NOOP("CommandCatalog", "Afficher les plans d'étages"), QT_TRANSLATE_NOOP("CommandCatalog", "Afficher ou masquer les plans d'étages"), "",
                   ":/icons/modeling/levels_visible.svg", CommandCategory::Snap});
  registerCommand({"cmd.display.grid_labels", QT_TRANSLATE_NOOP("CommandCatalog", "Afficher les libellés d'axes"), QT_TRANSLATE_NOOP("CommandCatalog", "Afficher ou masquer les libellés des axes de grille"), "",
                   "", CommandCategory::Snap});
  registerCommand({"cmd.display.rulers", QT_TRANSLATE_NOOP("CommandCatalog", "Afficher les règles"), QT_TRANSLATE_NOOP("CommandCatalog", "Afficher ou masquer les règles graduées"), "",
                   ":/icons/view/rulers.svg", CommandCategory::Snap});
  registerCommand({"cmd.coord.axis_z", QT_TRANSLATE_NOOP("CommandCatalog", "Plan horizontal Z"), QT_TRANSLATE_NOOP("CommandCatalog", "Bandeau de la vue : plans horizontaux (niveaux)"), "Alt+Z",
                   "", CommandCategory::Snap});
  registerCommand({"cmd.coord.axis_x", QT_TRANSLATE_NOOP("CommandCatalog", "Coupe verticale X"), QT_TRANSLATE_NOOP("CommandCatalog", "Bandeau de la vue : coupes perpendiculaires à X"), "Alt+X",
                   "", CommandCategory::Snap});
  registerCommand({"cmd.coord.axis_y", QT_TRANSLATE_NOOP("CommandCatalog", "Coupe verticale Y"), QT_TRANSLATE_NOOP("CommandCatalog", "Bandeau de la vue : coupes perpendiculaires à Y"), "Alt+Y",
                   "", CommandCategory::Snap});
  registerCommand({"cmd.coord.workplane_xy", QT_TRANSLATE_NOOP("CommandCatalog", "Plan de travail XY"), QT_TRANSLATE_NOOP("CommandCatalog", "Plan de travail global horizontal XY"), "",
                   ":/icons/modeling/workplane_xy.svg", CommandCategory::Snap});
  registerCommand({"cmd.coord.workplane_xz", QT_TRANSLATE_NOOP("CommandCatalog", "Plan de travail XZ"), QT_TRANSLATE_NOOP("CommandCatalog", "Plan de travail global vertical XZ"), "",
                   ":/icons/modeling/workplane_xz.svg", CommandCategory::Snap});
  registerCommand({"cmd.coord.workplane_yz", QT_TRANSLATE_NOOP("CommandCatalog", "Plan de travail YZ"), QT_TRANSLATE_NOOP("CommandCatalog", "Plan de travail global vertical YZ"), "",
                   ":/icons/modeling/workplane_yz.svg", CommandCategory::Snap});
  registerCommand({"cmd.coord.workplane_next", QT_TRANSLATE_NOOP("CommandCatalog", "Plan de travail suivant"), QT_TRANSLATE_NOOP("CommandCatalog", "Plan parallèle suivant passant par des intersections de la grille (niveau, axe)"), "Alt+PgUp",
                   ":/icons/modeling/workplane_next.svg", CommandCategory::Snap});
  registerCommand({"cmd.coord.workplane_prev", QT_TRANSLATE_NOOP("CommandCatalog", "Plan de travail précédent (grille)"), QT_TRANSLATE_NOOP("CommandCatalog", "Plan parallèle précédent passant par des intersections de la grille (niveau, axe)"), "Alt+PgDown",
                   ":/icons/modeling/workplane_prev.svg", CommandCategory::Snap});
  registerCommand({"cmd.coord.workplane_next_intersection", QT_TRANSLATE_NOOP("CommandCatalog", "Intersection suivante"), QT_TRANSLATE_NOOP("CommandCatalog", "Origine du plan de travail sur l'intersection suivante de la grille"), "Ctrl+Alt+PgUp",
                   ":/icons/modeling/intersection_next.svg", CommandCategory::Snap});
  registerCommand({"cmd.coord.workplane_prev_intersection", QT_TRANSLATE_NOOP("CommandCatalog", "Intersection précédente"), QT_TRANSLATE_NOOP("CommandCatalog", "Origine du plan de travail sur l'intersection précédente de la grille"), "Ctrl+Alt+PgDown",
                   ":/icons/modeling/intersection_prev.svg", CommandCategory::Snap});
  registerCommand({"cmd.coord.workplane_align_intersection", QT_TRANSLATE_NOOP("CommandCatalog", "Aligner sur l'intersection"), QT_TRANSLATE_NOOP("CommandCatalog", "Origine du plan de travail sur l'intersection de la grille la plus proche du curseur"), "Alt+Home",
                   ":/icons/modeling/intersection_align.svg", CommandCategory::Snap});
  registerCommand({"cmd.coord.workplane_back", QT_TRANSLATE_NOOP("CommandCatalog", "Revenir au plan de travail précédent"), QT_TRANSLATE_NOOP("CommandCatalog", "Plan de travail précédent (historique)"), "Alt+Backspace",
                   ":/icons/modeling/workplane_back.svg", CommandCategory::Snap});
  registerCommand({"cmd.coord.workplane_level", QT_TRANSLATE_NOOP("CommandCatalog", "Plan de travail sur l'étage"), QT_TRANSLATE_NOOP("CommandCatalog", "Plan de travail sur l'étage actif"), "",
                   ":/icons/modeling/workplane_level.svg", CommandCategory::Snap});
  registerCommand({"cmd.coord.workplane_custom", QT_TRANSLATE_NOOP("CommandCatalog", "Plan de travail personnalisé"), QT_TRANSLATE_NOOP("CommandCatalog", "Définir un plan de travail personnalisé"), "",
                   ":/icons/modeling/workplane_custom.svg", CommandCategory::Snap});
  registerCommand({"cmd.coord.workplane_visible", QT_TRANSLATE_NOOP("CommandCatalog", "Afficher le plan de travail"), QT_TRANSLATE_NOOP("CommandCatalog", "Afficher ou masquer le plan de travail 3D"), "",
                   ":/icons/modeling/workplane_visible.svg", CommandCategory::Snap});
  registerCommand({"cmd.view.normal_to_plane", QT_TRANSLATE_NOOP("CommandCatalog", "Vue normale au plan"), QT_TRANSLATE_NOOP("CommandCatalog", "Orienter la caméra perpendiculairement au plan de travail"), "Num+0",
                   ":/icons/view/view_normal.svg", CommandCategory::Snap});
  registerCommand({"cmd.struct.levels", QT_TRANSLATE_NOOP("CommandCatalog", "Étages et niveaux"), QT_TRANSLATE_NOOP("CommandCatalog", "Gestionnaire des étages et niveaux"), "Ctrl+L",
                   ":/icons/structure/levels.svg", CommandCategory::Structure});
  registerCommand({"cmd.struct.grid_dialog", QT_TRANSLATE_NOOP("CommandCatalog", "Gestionnaire de grilles"), QT_TRANSLATE_NOOP("CommandCatalog", "Créer, modifier et activer les grilles d'axes"), "Ctrl+G",
                   ":/icons/modeling/grid_manager.svg", CommandCategory::Structure});
  registerCommand({"cmd.struct.new_grid", QT_TRANSLATE_NOOP("CommandCatalog", "Nouvelle grille 3D"), QT_TRANSLATE_NOOP("CommandCatalog", "Créer une nouvelle grille d'axes"), "Ctrl+Shift+G",
                   ":/icons/modeling/grid_new.svg", CommandCategory::Structure});
  registerCommand({"cmd.view.fit_all", QT_TRANSLATE_NOOP("CommandCatalog", "Zoom étendu"), QT_TRANSLATE_NOOP("CommandCatalog", "Cadrer tout le modèle (AutoCAD : Z, E)"), "F; Z, E",
                   ":/icons/view/fit_all.svg", CommandCategory::View});
  registerCommand({"cmd.view.fit_selection", QT_TRANSLATE_NOOP("CommandCatalog", "Zoom sur la sélection"), QT_TRANSLATE_NOOP("CommandCatalog", "Cadrer la sélection"), "Shift+F",
                   ":/icons/view/fit_selection.svg", CommandCategory::View});
  registerCommand({"cmd.view.zoom_in", QT_TRANSLATE_NOOP("CommandCatalog", "Zoom avant"), QT_TRANSLATE_NOOP("CommandCatalog", "Agrandir la vue"), "+",
                   ":/icons/view/zoom_in.svg", CommandCategory::View});
  registerCommand({"cmd.view.zoom_out", QT_TRANSLATE_NOOP("CommandCatalog", "Zoom arrière"), QT_TRANSLATE_NOOP("CommandCatalog", "Réduire la vue"), "-",
                   ":/icons/view/zoom_out.svg", CommandCategory::View});
  registerCommand({"cmd.view.zoom_window", QT_TRANSLATE_NOOP("CommandCatalog", "Zoom fenêtre"), QT_TRANSLATE_NOOP("CommandCatalog", "Zoomer sur un rectangle tracé (AutoCAD : Z, W)"), "Z, W",
                   ":/icons/view/zoom_window.svg", CommandCategory::View});
  registerCommand({"cmd.view.prev", QT_TRANSLATE_NOOP("CommandCatalog", "Vue précédente"), QT_TRANSLATE_NOOP("CommandCatalog", "Revenir à la vue précédente (AutoCAD : Z, P)"), "Alt+Left; Z, P",
                   ":/icons/view/view_previous.svg", CommandCategory::View});
  registerCommand({"cmd.view.next", QT_TRANSLATE_NOOP("CommandCatalog", "Vue suivante"), QT_TRANSLATE_NOOP("CommandCatalog", "Revenir à la vue suivante"), "Alt+Right",
                   ":/icons/view/view_next.svg", CommandCategory::View});
  registerCommand({"cmd.view.home", QT_TRANSLATE_NOOP("CommandCatalog", "Vue d'accueil"), QT_TRANSLATE_NOOP("CommandCatalog", "Vue initiale du modèle"), "Home",
                   ":/icons/view/view_home.svg", CommandCategory::View});
  registerCommand({"cmd.view.reset", QT_TRANSLATE_NOOP("CommandCatalog", "Réinitialiser la vue"), QT_TRANSLATE_NOOP("CommandCatalog", "Réinitialiser l'orientation de la caméra"), "R",
                   ":/icons/view/view_reset.svg", CommandCategory::View});
  registerCommand({"cmd.view.top", QT_TRANSLATE_NOOP("CommandCatalog", "Vue de dessus"), QT_TRANSLATE_NOOP("CommandCatalog", "Caméra vers -Z"), "Num+7",
                   ":/icons/view/view_top.svg", CommandCategory::View});
  registerCommand({"cmd.view.bottom", QT_TRANSLATE_NOOP("CommandCatalog", "Vue de dessous"), QT_TRANSLATE_NOOP("CommandCatalog", "Caméra vers +Z"), "Ctrl+Num+7",
                   ":/icons/view/view_bottom.svg", CommandCategory::View});
  registerCommand({"cmd.view.front", QT_TRANSLATE_NOOP("CommandCatalog", "Vue de face"), QT_TRANSLATE_NOOP("CommandCatalog", "Caméra vers +Y"), "Num+1",
                   ":/icons/view/view_front.svg", CommandCategory::View});
  registerCommand({"cmd.view.back", QT_TRANSLATE_NOOP("CommandCatalog", "Vue arrière"), QT_TRANSLATE_NOOP("CommandCatalog", "Caméra vers -Y"), "Ctrl+Num+1",
                   ":/icons/view/view_back.svg", CommandCategory::View});
  registerCommand({"cmd.view.left", QT_TRANSLATE_NOOP("CommandCatalog", "Vue de gauche"), QT_TRANSLATE_NOOP("CommandCatalog", "Caméra vers +X"), "Num+3",
                   ":/icons/view/view_left.svg", CommandCategory::View});
  registerCommand({"cmd.view.right", QT_TRANSLATE_NOOP("CommandCatalog", "Vue de droite"), QT_TRANSLATE_NOOP("CommandCatalog", "Caméra vers -X"), "Ctrl+Num+3",
                   ":/icons/view/view_side.svg", CommandCategory::View});
  registerCommand({"cmd.view.iso", QT_TRANSLATE_NOOP("CommandCatalog", "Vue isométrique"), QT_TRANSLATE_NOOP("CommandCatalog", "Vue 3D isométrique"), "Num+5",
                   ":/icons/view/view_iso.svg", CommandCategory::View});
  registerCommand({"cmd.view.3d", QT_TRANSLATE_NOOP("CommandCatalog", "Vue 3D axonométrique"), QT_TRANSLATE_NOOP("CommandCatalog", "Revenir à la vue 3D axonométrique"), "",
                   "", CommandCategory::View});
  registerCommand({"cmd.view.xy", QT_TRANSLATE_NOOP("CommandCatalog", "Plan XY"), QT_TRANSLATE_NOOP("CommandCatalog", "Vue d'étage (plan XY)"), "",
                   ":/icons/view/plan_xy.svg", CommandCategory::View});
  registerCommand({"cmd.view.xz", QT_TRANSLATE_NOOP("CommandCatalog", "Plan XZ"), QT_TRANSLATE_NOOP("CommandCatalog", "Élévation de face (plan XZ)"), "",
                   ":/icons/view/plan_xz.svg", CommandCategory::View});
  registerCommand({"cmd.view.yz", QT_TRANSLATE_NOOP("CommandCatalog", "Plan YZ"), QT_TRANSLATE_NOOP("CommandCatalog", "Coupe latérale / pignon (plan YZ)"), "",
                   ":/icons/view/plan_yz.svg", CommandCategory::View});
  registerCommand({"cmd.view.rotate_left", QT_TRANSLATE_NOOP("CommandCatalog", "Pivoter la vue 2D à gauche"), QT_TRANSLATE_NOOP("CommandCatalog", "Rotation de la vue 2D de -15°"), "",
                   ":/icons/view/rotate_ccw.svg", CommandCategory::View});
  registerCommand({"cmd.view.rotate_right", QT_TRANSLATE_NOOP("CommandCatalog", "Pivoter la vue 2D à droite"), QT_TRANSLATE_NOOP("CommandCatalog", "Rotation de la vue 2D de +15°"), "",
                   ":/icons/view/rotate_cw.svg", CommandCategory::View});
  registerCommand({"cmd.view.section_cut", QT_TRANSLATE_NOOP("CommandCatalog", "Coupes de la structure"), QT_TRANSLATE_NOOP("CommandCatalog", "Plan de coupe dynamique 3D"), "",
                   ":/icons/view/section_cut.svg", CommandCategory::View});
  registerCommand({"cmd.view.coord_system", QT_TRANSLATE_NOOP("CommandCatalog", "Repère local / global"), QT_TRANSLATE_NOOP("CommandCatalog", "Afficher le repère local ou global"), "",
                   "", CommandCategory::View});
  registerCommand({"cmd.view.fullscreen", QT_TRANSLATE_NOOP("CommandCatalog", "Plein écran"), QT_TRANSLATE_NOOP("CommandCatalog", "Basculer en mode plein écran"), "F11",
                   ":/icons/fullscreen.svg", CommandCategory::View});
  registerCommand({"cmd.view.display_physical", QT_TRANSLATE_NOOP("CommandCatalog", "Modèle physique"), QT_TRANSLATE_NOOP("CommandCatalog", "Représentation : sections volumiques"), "Alt+1",
                   ":/icons/view/display_physical.svg", CommandCategory::View});
  registerCommand({"cmd.view.display_analytical", QT_TRANSLATE_NOOP("CommandCatalog", "Modèle filaire analytique"), QT_TRANSLATE_NOOP("CommandCatalog", "Représentation : axes des barres et nœuds"), "Alt+2",
                   ":/icons/view/display_analytical.svg", CommandCategory::View});
  registerCommand({"cmd.view.display_fe", QT_TRANSLATE_NOOP("CommandCatalog", "Modèle éléments finis"), QT_TRANSLATE_NOOP("CommandCatalog", "Représentation : maillage réellement transmis au moteur"), "Alt+3",
                   ":/icons/view/display_fe.svg", CommandCategory::View});
  registerCommand({"cmd.view.display_overlay", QT_TRANSLATE_NOOP("CommandCatalog", "Superposition"), QT_TRANSLATE_NOOP("CommandCatalog", "Représentation : sections translucides et axes"), "Alt+4",
                   ":/icons/view/view_transparent.svg", CommandCategory::View});
  registerCommand({"cmd.view.render_textured", QT_TRANSLATE_NOOP("CommandCatalog", "Rendu texturé"), QT_TRANSLATE_NOOP("CommandCatalog", "Volumes avec la finition des matériaux (couleur, brillance, texture)"), "Alt+5",
                   ":/icons/view/view_3d.svg", CommandCategory::View});
  registerCommand({"cmd.view.render_shaded", QT_TRANSLATE_NOOP("CommandCatalog", "Rendu ombré"), QT_TRANSLATE_NOOP("CommandCatalog", "Volumes ombrés en couleur unie, sans texture"), "Alt+6",
                   ":/icons/view/view_shaded.svg", CommandCategory::View});
  registerCommand({"cmd.view.render_wireframe", QT_TRANSLATE_NOOP("CommandCatalog", "Rendu filaire"), QT_TRANSLATE_NOOP("CommandCatalog", "Arêtes des volumes seulement"), "Alt+7",
                   ":/icons/view/view_wireframe.svg", CommandCategory::View});
  registerCommand({"cmd.view.render_edges", QT_TRANSLATE_NOOP("CommandCatalog", "Arêtes des volumes"), QT_TRANSLATE_NOOP("CommandCatalog", "Dessiner les arêtes des faces sur les volumes ombrés"), "Alt+8",
                   "", CommandCategory::View});
  registerCommand({"cmd.view.render_lightweight", QT_TRANSLATE_NOOP("CommandCatalog", "Mode léger"), QT_TRANSLATE_NOOP("CommandCatalog", "PC modestes : sans texture ni arête, facettisation plus grossière"), "Alt+9",
                   "", CommandCategory::View});
  registerCommand({"cmd.display.nodes", QT_TRANSLATE_NOOP("CommandCatalog", "Afficher les nœuds"), QT_TRANSLATE_NOOP("CommandCatalog", "Afficher ou masquer les nœuds"), "",
                   ":/icons/modeling/nodes_visible.svg", CommandCategory::Display});
  registerCommand({"cmd.display.node_labels", QT_TRANSLATE_NOOP("CommandCatalog", "Afficher les numéros de nœuds"), QT_TRANSLATE_NOOP("CommandCatalog", "Afficher ou masquer les étiquettes des nœuds"), "",
                   ":/icons/modeling/node_labels.svg", CommandCategory::Display});
  registerCommand({"cmd.display.supports", QT_TRANSLATE_NOOP("CommandCatalog", "Afficher les appuis"), QT_TRANSLATE_NOOP("CommandCatalog", "Afficher ou masquer les symboles d'appuis"), "",
                   ":/icons/modeling/supports_visible.svg", CommandCategory::Display});
  registerCommand({"cmd.display.support_labels", QT_TRANSLATE_NOOP("CommandCatalog", "Afficher les étiquettes d'appuis"), QT_TRANSLATE_NOOP("CommandCatalog", "Afficher ou masquer les étiquettes d'appuis"), "",
                   ":/icons/modeling/support_labels.svg", CommandCategory::Display});
  registerCommand({"cmd.display.loads", QT_TRANSLATE_NOOP("CommandCatalog", "Afficher les charges"), QT_TRANSLATE_NOOP("CommandCatalog", "Afficher ou masquer les charges 3D"), "Ctrl+Shift+L",
                   ":/icons/modeling/loads_visible.svg", CommandCategory::Display});
  registerCommand({"cmd.display.surface_loads", QT_TRANSLATE_NOOP("CommandCatalog", "Afficher les charges surfaciques"), QT_TRANSLATE_NOOP("CommandCatalog", "Afficher ou masquer les charges des dalles et voiles"), "",
                   ":/icons/modeling/surface_loads_visible.svg", CommandCategory::Display});
  registerCommand({"cmd.display.forces", QT_TRANSLATE_NOOP("CommandCatalog", "Afficher les forces"), QT_TRANSLATE_NOOP("CommandCatalog", "Afficher ou masquer les forces"), "",
                   "", CommandCategory::Display});
  registerCommand({"cmd.display.moments", QT_TRANSLATE_NOOP("CommandCatalog", "Afficher les moments"), QT_TRANSLATE_NOOP("CommandCatalog", "Afficher ou masquer les moments"), "",
                   "", CommandCategory::Display});
  registerCommand({"cmd.display.load_values", QT_TRANSLATE_NOOP("CommandCatalog", "Afficher les valeurs des charges"), QT_TRANSLATE_NOOP("CommandCatalog", "Afficher ou masquer les valeurs des charges"), "",
                   ":/icons/modeling/load_values.svg", CommandCategory::Display});
  registerCommand({"cmd.properties.panel", QT_TRANSLATE_NOOP("CommandCatalog", "Panneau Propriétés"), QT_TRANSLATE_NOOP("CommandCatalog", "Afficher ou masquer le panneau Propriétés"), "P",
                   ":/icons/view/properties.svg", CommandCategory::Properties});
  registerCommand({"cmd.library.custom", QT_TRANSLATE_NOOP("CommandCatalog", "Bibliothèque personnalisée"), QT_TRANSLATE_NOOP("CommandCatalog", "Bibliothèque personnalisée de sections et matériaux"), "Ctrl+B",
                   ":/icons/common/library.svg", CommandCategory::Libraries});
  registerCommand({"cmd.library.manager", QT_TRANSLATE_NOOP("CommandCatalog", "Gestionnaire TSALib"), QT_TRANSLATE_NOOP("CommandCatalog", "Gestionnaire d'extensions TSALib"), "",
                   ":/icons/common/extension_manager.svg", CommandCategory::Libraries});
  registerCommand({"cmd.section.i", QT_TRANSLATE_NOOP("CommandCatalog", "Profilé en I/H"), QT_TRANSLATE_NOOP("CommandCatalog", "Créer un profilé IPE / HEA / HEB"), "",
                   "", CommandCategory::Libraries});
  registerCommand({"cmd.section.rect", QT_TRANSLATE_NOOP("CommandCatalog", "Section rectangulaire"), QT_TRANSLATE_NOOP("CommandCatalog", "Créer une section rectangulaire"), "",
                   "", CommandCategory::Libraries});
  registerCommand({"cmd.section.circ", QT_TRANSLATE_NOOP("CommandCatalog", "Section circulaire"), QT_TRANSLATE_NOOP("CommandCatalog", "Créer une section circulaire"), "",
                   "", CommandCategory::Libraries});
  registerCommand({"cmd.material.concrete", QT_TRANSLATE_NOOP("CommandCatalog", "Béton armé"), QT_TRANSLATE_NOOP("CommandCatalog", "Matériau béton armé (C25/30)"), "",
                   "", CommandCategory::Libraries});
  registerCommand({"cmd.material.steel", QT_TRANSLATE_NOOP("CommandCatalog", "Acier structural"), QT_TRANSLATE_NOOP("CommandCatalog", "Matériau acier (S355)"), "",
                   "", CommandCategory::Libraries});
  registerCommand({"cmd.loads.point", QT_TRANSLATE_NOOP("CommandCatalog", "Force et couple"), QT_TRANSLATE_NOOP("CommandCatalog", "Charge nodale : force et moment"), "Q, N",
                   ":/icons/load_point.svg", CommandCategory::Loads});
  registerCommand({"cmd.loads.distributed", QT_TRANSLATE_NOOP("CommandCatalog", "Charge uniforme"), QT_TRANSLATE_NOOP("CommandCatalog", "Charge répartie uniforme sur barre"), "Q, U",
                   ":/icons/load_distributed.svg", CommandCategory::Loads});
  registerCommand({"cmd.loads.trapezoidal", QT_TRANSLATE_NOOP("CommandCatalog", "Charge trapézoïdale"), QT_TRANSLATE_NOOP("CommandCatalog", "Charge répartie trapézoïdale sur barre"), "Q, T",
                   "", CommandCategory::Loads});
  registerCommand({"cmd.loads.bar_point", QT_TRANSLATE_NOOP("CommandCatalog", "Force ponctuelle sur barre"), QT_TRANSLATE_NOOP("CommandCatalog", "Charge concentrée sur une barre"), "Q, P",
                   "", CommandCategory::Loads});
  registerCommand({"cmd.loads.surface", QT_TRANSLATE_NOOP("CommandCatalog", "Charge surfacique"), QT_TRANSLATE_NOOP("CommandCatalog", "Charge répartie sur surface"), "Q, S",
                   "", CommandCategory::Loads});
  registerCommand({"cmd.loads.surface_supports", QT_TRANSLATE_NOOP("CommandCatalog", "Appuis linéiques des surfaces"), QT_TRANSLATE_NOOP("CommandCatalog", "Appuis répartis sur les bords des dalles et voiles"), "",
                   ":/icons/modeling/surface_supports.svg", CommandCategory::Loads});
  registerCommand({"cmd.loads.self_weight", QT_TRANSLATE_NOOP("CommandCatalog", "Poids propre"), QT_TRANSLATE_NOOP("CommandCatalog", "Paramètres du poids propre"), "Q, G",
                   "", CommandCategory::Loads});
  registerCommand({"cmd.loadcases.manager", QT_TRANSLATE_NOOP("CommandCatalog", "Cas de charges et combinaisons"), QT_TRANSLATE_NOOP("CommandCatalog", "Gestionnaire des cas de charges et combinaisons"), "Q, C",
                   "", CommandCategory::LoadCases});
  registerCommand({"cmd.mesh.settings", QT_TRANSLATE_NOOP("CommandCatalog", "Paramètres du maillage"), QT_TRANSLATE_NOOP("CommandCatalog", "Panneau de paramétrage du maillage (Gmsh)"), "",
                   ":/icons/analysis/mesh_settings.svg", CommandCategory::Mesh});
  registerCommand({"cmd.mesh.preview", QT_TRANSLATE_NOOP("CommandCatalog", "Aperçu du maillage"), QT_TRANSLATE_NOOP("CommandCatalog", "Maillage prévisionnel sans remplacer celui du projet"), "",
                   ":/icons/analysis/mesh_preview.svg", CommandCategory::Mesh});
  registerCommand({"cmd.mesh.regenerate", QT_TRANSLATE_NOOP("CommandCatalog", "Régénérer le maillage"), QT_TRANSLATE_NOOP("CommandCatalog", "Régénérer avec les paramètres enregistrés"), "",
                   ":/icons/analysis/mesh_regenerate.svg", CommandCategory::Mesh});
  registerCommand({"cmd.mesh.quality", QT_TRANSLATE_NOOP("CommandCatalog", "Vérifier la qualité du maillage"), QT_TRANSLATE_NOOP("CommandCatalog", "Contrôle de qualité et liste des problèmes"), "",
                   ":/icons/analysis/mesh_quality.svg", CommandCategory::Mesh});
  registerCommand({"cmd.mesh.show", QT_TRANSLATE_NOOP("CommandCatalog", "Afficher le maillage"), QT_TRANSLATE_NOOP("CommandCatalog", "Afficher le maillage du projet dans la vue 3D"), "",
                   ":/icons/analysis/mesh_show.svg", CommandCategory::Mesh});
  registerCommand({"cmd.mesh.view.wireframe", QT_TRANSLATE_NOOP("CommandCatalog", "Maillage filaire"), QT_TRANSLATE_NOOP("CommandCatalog", "Arêtes uniques du maillage, sans faces"), "",
                   "", CommandCategory::Mesh});
  registerCommand({"cmd.mesh.view.faces_edges", QT_TRANSLATE_NOOP("CommandCatalog", "Maillage : faces et arêtes"), QT_TRANSLATE_NOOP("CommandCatalog", "Faces des éléments et arêtes uniques"), "",
                   "", CommandCategory::Mesh});
  registerCommand({"cmd.mesh.view.faces", QT_TRANSLATE_NOOP("CommandCatalog", "Maillage : faces seules"), QT_TRANSLATE_NOOP("CommandCatalog", "Faces des éléments sans arêtes"), "",
                   "", CommandCategory::Mesh});
  registerCommand({"cmd.mesh.view.shrink", QT_TRANSLATE_NOOP("CommandCatalog", "Maillage : éléments réduits"), QT_TRANSLATE_NOOP("CommandCatalog", "Éléments réduits autour de leur centre"), "",
                   "", CommandCategory::Mesh});
  registerCommand({"cmd.mesh.view.edges", QT_TRANSLATE_NOOP("CommandCatalog", "Arêtes du maillage"), QT_TRANSLATE_NOOP("CommandCatalog", "Afficher ou masquer les arêtes du maillage"), "",
                   "", CommandCategory::Mesh});
  registerCommand({"cmd.mesh.view.transparent", QT_TRANSLATE_NOOP("CommandCatalog", "Faces du maillage transparentes"), QT_TRANSLATE_NOOP("CommandCatalog", "Faces semi-transparentes"), "",
                   "", CommandCategory::Mesh});
  registerCommand({"cmd.mesh.view.mesh_only", QT_TRANSLATE_NOOP("CommandCatalog", "Maillage seul"), QT_TRANSLATE_NOOP("CommandCatalog", "Masquer la géométrie du modèle, garder le maillage"), "",
                   "", CommandCategory::Mesh});
  registerCommand({"cmd.mesh.view.nodes", QT_TRANSLATE_NOOP("CommandCatalog", "Nœuds du maillage"), QT_TRANSLATE_NOOP("CommandCatalog", "Afficher ou masquer les nœuds du maillage"), "",
                   "", CommandCategory::Mesh});
  registerCommand({"cmd.mesh.view.node_labels", QT_TRANSLATE_NOOP("CommandCatalog", "Numéros des nœuds du maillage"), QT_TRANSLATE_NOOP("CommandCatalog", "Identifiants des nœuds du maillage"), "",
                   "", CommandCategory::Mesh});
  registerCommand({"cmd.mesh.view.element_labels", QT_TRANSLATE_NOOP("CommandCatalog", "Numéros des éléments du maillage"), QT_TRANSLATE_NOOP("CommandCatalog", "Identifiants des éléments du maillage"), "",
                   "", CommandCategory::Mesh});
  registerCommand({"cmd.mesh.view.node_size", QT_TRANSLATE_NOOP("CommandCatalog", "Taille des nœuds du maillage"), QT_TRANSLATE_NOOP("CommandCatalog", "Échelle des marqueurs de nœuds"), "",
                   "", CommandCategory::Mesh});
  registerCommand({"cmd.mesh.view.line_width", QT_TRANSLATE_NOOP("CommandCatalog", "Épaisseur des traits du maillage"), QT_TRANSLATE_NOOP("CommandCatalog", "Épaisseur des arêtes et éléments 1D"), "",
                   "", CommandCategory::Mesh});
  registerCommand({"cmd.mesh.view.show_1d", QT_TRANSLATE_NOOP("CommandCatalog", "Éléments 1D du maillage"), QT_TRANSLATE_NOOP("CommandCatalog", "Afficher ou masquer les éléments 1D"), "",
                   "", CommandCategory::Mesh});
  registerCommand({"cmd.mesh.view.show_2d", QT_TRANSLATE_NOOP("CommandCatalog", "Éléments 2D du maillage"), QT_TRANSLATE_NOOP("CommandCatalog", "Afficher ou masquer les éléments 2D"), "",
                   "", CommandCategory::Mesh});
  registerCommand({"cmd.mesh.view.show_3d", QT_TRANSLATE_NOOP("CommandCatalog", "Éléments 3D du maillage"), QT_TRANSLATE_NOOP("CommandCatalog", "Afficher ou masquer les éléments 3D"), "",
                   "", CommandCategory::Mesh});
  registerCommand({"cmd.mesh.view.interior", QT_TRANSLATE_NOOP("CommandCatalog", "Faces intérieures des volumes"), QT_TRANSLATE_NOOP("CommandCatalog", "Afficher aussi les faces partagées entre volumes"), "",
                   "", CommandCategory::Mesh});
  registerCommand({"cmd.mesh.view.cut", QT_TRANSLATE_NOOP("CommandCatalog", "Coupe du maillage"), QT_TRANSLATE_NOOP("CommandCatalog", "Masquer les éléments d'un côté d'un plan"), "",
                   "", CommandCategory::Mesh});
  registerCommand({"cmd.mesh.view.isolate", QT_TRANSLATE_NOOP("CommandCatalog", "Isoler des groupes du maillage"), QT_TRANSLATE_NOOP("CommandCatalog", "Afficher seulement les groupes choisis"), "",
                   "", CommandCategory::Mesh});
  registerCommand({"cmd.mesh.view.all_groups", QT_TRANSLATE_NOOP("CommandCatalog", "Afficher tous les groupes du maillage"), QT_TRANSLATE_NOOP("CommandCatalog", "Annuler l'isolation des groupes"), "",
                   "", CommandCategory::Mesh});
  registerCommand({"cmd.mesh.view.pick", QT_TRANSLATE_NOOP("CommandCatalog", "Sélection dans le maillage"), QT_TRANSLATE_NOOP("CommandCatalog", "Sélectionner un nœud ou un élément du maillage"), "",
                   "", CommandCategory::Mesh});
  registerCommand({"cmd.mesh.view.fit", QT_TRANSLATE_NOOP("CommandCatalog", "Cadrer sur le maillage"), QT_TRANSLATE_NOOP("CommandCatalog", "Cadrer la vue sur le maillage affiché"), "",
                   "", CommandCategory::Mesh});
  registerCommand({"cmd.mesh.view.results", QT_TRANSLATE_NOOP("CommandCatalog", "Déplacements sur le maillage"), QT_TRANSLATE_NOOP("CommandCatalog", "Déplacements calculés sur le maillage (couleurs, déformée)"), "",
                   "", CommandCategory::Mesh});
  registerCommand({"cmd.mesh.view.deform_scale", QT_TRANSLATE_NOOP("CommandCatalog", "Facteur de déformation du maillage"), QT_TRANSLATE_NOOP("CommandCatalog", "Amplification des déplacements affichés"), "",
                   "", CommandCategory::Mesh});
  registerCommand({"cmd.mesh.view.animate", QT_TRANSLATE_NOOP("CommandCatalog", "Animer la déformée du maillage"), QT_TRANSLATE_NOOP("CommandCatalog", "Animation de la déformée"), "",
                   "", CommandCategory::Mesh});
  registerCommand({"cmd.mesh.local", QT_TRANSLATE_NOOP("CommandCatalog", "Maillage local de la sélection"), QT_TRANSLATE_NOOP("CommandCatalog", "Paramètres de maillage des objets sélectionnés"), "",
                   ":/icons/analysis/mesh_local.svg", CommandCategory::Mesh});
  registerCommand({"cmd.mesh.zone", QT_TRANSLATE_NOOP("CommandCatalog", "Raffinement local"), QT_TRANSLATE_NOOP("CommandCatalog", "Zone de raffinement autour des nœuds sélectionnés"), "",
                   ":/icons/analysis/mesh_zone.svg", CommandCategory::Mesh});
  registerCommand({"cmd.mesh.export", QT_TRANSLATE_NOOP("CommandCatalog", "Exporter le maillage"), QT_TRANSLATE_NOOP("CommandCatalog", "Exporter le maillage (msh, vtk, unv, inp, bdf, mesh)"), "",
                   ":/icons/analysis/mesh_export.svg", CommandCategory::Mesh});
  registerCommand({"cmd.mesh.import", QT_TRANSLATE_NOOP("CommandCatalog", "Importer un maillage"), QT_TRANSLATE_NOOP("CommandCatalog", "Importer un maillage (Gmsh et formats lus par Gmsh)"), "",
                   ":/icons/analysis/mesh_import.svg", CommandCategory::Mesh});
  registerCommand({"cmd.mesh.clear", QT_TRANSLATE_NOOP("CommandCatalog", "Supprimer le maillage"), QT_TRANSLATE_NOOP("CommandCatalog", "Supprimer le maillage du projet (paramètres conservés)"), "",
                   ":/icons/analysis/mesh_clear.svg", CommandCategory::Mesh});
  registerCommand({"cmd.model.slab_openings", QT_TRANSLATE_NOOP("CommandCatalog", "Ouvertures de la dalle"), QT_TRANSLATE_NOOP("CommandCatalog", "Trémies de la dalle sélectionnée"), "",
                   ":/icons/modeling/slab_openings.svg", CommandCategory::Mesh});
  registerCommand({"cmd.analysis.mesh", QT_TRANSLATE_NOOP("CommandCatalog", "Générer le maillage"), QT_TRANSLATE_NOOP("CommandCatalog", "Générer le maillage du projet avec Gmsh (enregistré, qualité contrôlée)"), "",
                   ":/icons/mesh_generate.svg", CommandCategory::Mesh});
  registerCommand({"cmd.analysis.solve", QT_TRANSLATE_NOOP("CommandCatalog", "Lancer le calcul"), QT_TRANSLATE_NOOP("CommandCatalog", "Lancer le calcul structurel (validations habituelles)"), "F5",
                   ":/icons/analysis_run.svg", CommandCategory::Analysis});
  registerCommand({"cmd.analysis.config", QT_TRANSLATE_NOOP("CommandCatalog", "Paramètres d'analyse"), QT_TRANSLATE_NOOP("CommandCatalog", "Moteur, portée, type d'analyse"), "Ctrl+F5",
                   "", CommandCategory::Analysis});
  registerCommand({"cmd.results.deformed", QT_TRANSLATE_NOOP("CommandCatalog", "Afficher la déformée"), QT_TRANSLATE_NOOP("CommandCatalog", "Afficher ou masquer la déformée 3D"), "F9",
                   ":/icons/analysis/deformed_visible.svg", CommandCategory::Results});
  registerCommand({"cmd.results.reactions", QT_TRANSLATE_NOOP("CommandCatalog", "Afficher les réactions"), QT_TRANSLATE_NOOP("CommandCatalog", "Afficher ou masquer les réactions d'appui"), "Shift+F9",
                   ":/icons/analysis/reactions.svg", CommandCategory::Results});
  registerCommand({"cmd.results.diagram_mz", QT_TRANSLATE_NOOP("CommandCatalog", "Diagramme Mz"), QT_TRANSLATE_NOOP("CommandCatalog", "Diagramme du moment Mz"), "",
                   ":/icons/analysis/diagram_mz.svg", CommandCategory::Results});
  registerCommand({"cmd.results.diagram_my", QT_TRANSLATE_NOOP("CommandCatalog", "Diagramme My"), QT_TRANSLATE_NOOP("CommandCatalog", "Diagramme du moment My"), "",
                   ":/icons/analysis/diagram_my.svg", CommandCategory::Results});
  registerCommand({"cmd.results.diagram_mx", QT_TRANSLATE_NOOP("CommandCatalog", "Diagramme Mx"), QT_TRANSLATE_NOOP("CommandCatalog", "Diagramme de torsion Mx"), "",
                   ":/icons/analysis/diagram_mx.svg", CommandCategory::Results});
  registerCommand({"cmd.results.diagram_vz", QT_TRANSLATE_NOOP("CommandCatalog", "Diagramme Vz"), QT_TRANSLATE_NOOP("CommandCatalog", "Diagramme de l'effort tranchant Vz"), "",
                   ":/icons/analysis/diagram_vz.svg", CommandCategory::Results});
  registerCommand({"cmd.results.diagram_vy", QT_TRANSLATE_NOOP("CommandCatalog", "Diagramme Vy"), QT_TRANSLATE_NOOP("CommandCatalog", "Diagramme de l'effort tranchant Vy"), "",
                   ":/icons/analysis/diagram_vy.svg", CommandCategory::Results});
  registerCommand({"cmd.results.diagram_n", QT_TRANSLATE_NOOP("CommandCatalog", "Diagramme N"), QT_TRANSLATE_NOOP("CommandCatalog", "Diagramme de l'effort normal"), "",
                   ":/icons/analysis/diagram_n.svg", CommandCategory::Results});
  registerCommand({"cmd.results.diagram_deflection", QT_TRANSLATE_NOOP("CommandCatalog", "Flèches"), QT_TRANSLATE_NOOP("CommandCatalog", "Diagramme des flèches"), "",
                   ":/icons/analysis/diagram_deflection.svg", CommandCategory::Results});
  registerCommand({"cmd.results.diagram_none", QT_TRANSLATE_NOOP("CommandCatalog", "Masquer les diagrammes"), QT_TRANSLATE_NOOP("CommandCatalog", "Masquer les diagrammes d'efforts"), "",
                   ":/icons/analysis/diagram_none.svg", CommandCategory::Results});
  registerCommand({"cmd.results.fit_model", QT_TRANSLATE_NOOP("CommandCatalog", "Cadrer le modèle"), QT_TRANSLATE_NOOP("CommandCatalog", "Cadrer le modèle (résultats)"), "",
                   ":/icons/analysis/fit_model.svg", CommandCategory::Results});
  registerCommand({"cmd.results.fit_results", QT_TRANSLATE_NOOP("CommandCatalog", "Cadrer les résultats"), QT_TRANSLATE_NOOP("CommandCatalog", "Cadrer les résultats affichés"), "",
                   ":/icons/analysis/fit_results.svg", CommandCategory::Results});
  registerCommand({"cmd.results.fit_deformed", QT_TRANSLATE_NOOP("CommandCatalog", "Cadrer la déformée"), QT_TRANSLATE_NOOP("CommandCatalog", "Cadrer la déformée"), "",
                   ":/icons/analysis/fit_deformed.svg", CommandCategory::Results});
  registerCommand({"cmd.results.displacements", QT_TRANSLATE_NOOP("CommandCatalog", "Déplacements"), QT_TRANSLATE_NOOP("CommandCatalog", "Déformée et déplacements"), "",
                   ":/icons/results_disp.svg", CommandCategory::Results});
  registerCommand({"cmd.results.forces", QT_TRANSLATE_NOOP("CommandCatalog", "Efforts internes"), QT_TRANSLATE_NOOP("CommandCatalog", "Diagrammes des efforts (M, N, V)"), "",
                   ":/icons/results_forces.svg", CommandCategory::Results});
  registerCommand({"cmd.results.stresses", QT_TRANSLATE_NOOP("CommandCatalog", "Contraintes"), QT_TRANSLATE_NOOP("CommandCatalog", "Contrainte normale de fibre extrême σ = N/A ± M/W (poutres)"), "",
                   ":/icons/results_stress.svg", CommandCategory::Results});
  registerCommand({"cmd.report.ndc", QT_TRANSLATE_NOOP("CommandCatalog", "Note de calcul"), QT_TRANSLATE_NOOP("CommandCatalog", "Ouvrir la note de calcul"), "F8",
                   "", CommandCategory::Documentation});
  registerCommand({"cmd.report.templates", QT_TRANSLATE_NOOP("CommandCatalog", "Documents par templates"), QT_TRANSLATE_NOOP("CommandCatalog", "Rapports et notes produits par templates : aperçu, options, export, gestion"), "",
                   ":/icons/file/report_templates.svg", CommandCategory::Documentation});
  registerCommand({"cmd.bim.import_ifc", QT_TRANSLATE_NOOP("CommandCatalog", "Importer IFC"), QT_TRANSLATE_NOOP("CommandCatalog", "Importer un modèle IFC"), "Ctrl+I",
                   ":/icons/file/ifc_import.svg", CommandCategory::Bim});
  registerCommand({"cmd.bim.import_tsa3d", QT_TRANSLATE_NOOP("CommandCatalog", "Importer TSA3D"), QT_TRANSLATE_NOOP("CommandCatalog", "Créer un projet depuis un fichier TSA3D (format d'échange)"), "",
                   ":/icons/file/tsa3d_import.svg", CommandCategory::Bim});
  registerCommand({"cmd.bim.export_tsa3d", QT_TRANSLATE_NOOP("CommandCatalog", "Exporter TSA3D"), QT_TRANSLATE_NOOP("CommandCatalog", "Exporter le modèle au format d'échange TSA3D"), "",
                   ":/icons/file/tsa3d_export.svg", CommandCategory::Bim});
  registerCommand({"cmd.bim.import_module", QT_TRANSLATE_NOOP("CommandCatalog", "Importer via un module"), QT_TRANSLATE_NOOP("CommandCatalog", "Convertir un fichier avec un module puis l'importer (TSA3D)"), "",
                   ":/icons/file/module_import.svg", CommandCategory::Bim});
  registerCommand({"cmd.bim.export_ifc", QT_TRANSLATE_NOOP("CommandCatalog", "Exporter IFC"), QT_TRANSLATE_NOOP("CommandCatalog", "Exporter le modèle en IFC"), "Ctrl+E",
                   ":/icons/file/ifc_export.svg", CommandCategory::Bim});
  registerCommand({"cmd.window.results", QT_TRANSLATE_NOOP("CommandCatalog", "Panneau Résultats"), QT_TRANSLATE_NOOP("CommandCatalog", "Afficher ou masquer le panneau Résultats structuraux 3D"), "Ctrl+1",
                   "", CommandCategory::Workspace});
  registerCommand({"cmd.window.properties", QT_TRANSLATE_NOOP("CommandCatalog", "Fenêtre Propriétés"), QT_TRANSLATE_NOOP("CommandCatalog", "Afficher ou masquer la fenêtre Propriétés"), "Ctrl+2",
                   "", CommandCategory::Workspace});
  registerCommand({"cmd.window.model_browser", QT_TRANSLATE_NOOP("CommandCatalog", "Navigateur du modèle"), QT_TRANSLATE_NOOP("CommandCatalog", "Afficher ou masquer le navigateur du modèle"), "Ctrl+3",
                   "", CommandCategory::Workspace});
  registerCommand({"cmd.window.work_planes", QT_TRANSLATE_NOOP("CommandCatalog", "Plans de travail et vues"), QT_TRANSLATE_NOOP("CommandCatalog", "Afficher ou masquer le panneau des plans de travail"), "Ctrl+4",
                   "", CommandCategory::Workspace});
  registerCommand({"cmd.window.visibility", QT_TRANSLATE_NOOP("CommandCatalog", "Calques et visibilité"), QT_TRANSLATE_NOOP("CommandCatalog", "Afficher ou masquer le panneau Calques et visibilité"), "Ctrl+5",
                   "", CommandCategory::Workspace});
  registerCommand({"cmd.window.elements", QT_TRANSLATE_NOOP("CommandCatalog", "Éléments structuraux"), QT_TRANSLATE_NOOP("CommandCatalog", "Afficher ou masquer le panneau des éléments"), "Ctrl+6",
                   "", CommandCategory::Workspace});
  registerCommand({"cmd.window.analysis_data", QT_TRANSLATE_NOOP("CommandCatalog", "Données d'analyse"), QT_TRANSLATE_NOOP("CommandCatalog", "Afficher ou masquer le panneau des données d'analyse"), "Ctrl+7",
                   "", CommandCategory::Workspace});
  registerCommand({"cmd.window.console", QT_TRANSLATE_NOOP("CommandCatalog", "Console et messages"), QT_TRANSLATE_NOOP("CommandCatalog", "Afficher ou masquer la console"), "F2",
                   "", CommandCategory::Workspace});
  registerCommand({"cmd.ai.assistant", QT_TRANSLATE_NOOP("CommandCatalog", "Assistant IA"), QT_TRANSLATE_NOOP("CommandCatalog", "Ouvrir l'assistant de co-ingénierie"), "Ctrl+Shift+I",
                   "", CommandCategory::Tools});
  registerCommand({"cmd.ai.config", QT_TRANSLATE_NOOP("CommandCatalog", "Configuration IA"), QT_TRANSLATE_NOOP("CommandCatalog", "Paramètres de l'assistant IA"), "",
                   ":/icons/common/ai_settings.svg", CommandCategory::Tools});
  registerCommand({"cmd.ai.check", QT_TRANSLATE_NOOP("CommandCatalog", "Vérifier la structure (IA)"), QT_TRANSLATE_NOOP("CommandCatalog", "Vérification de la structure par l'assistant"), "",
                   ":/icons/common/ai_check.svg", CommandCategory::Tools});
  registerCommand({"cmd.ai.analyze", QT_TRANSLATE_NOOP("CommandCatalog", "Analyser le modèle (IA)"), QT_TRANSLATE_NOOP("CommandCatalog", "Analyse du modèle par l'assistant"), "",
                   ":/icons/model_tree.svg", CommandCategory::Tools});
  registerCommand({"cmd.ai.explain", QT_TRANSLATE_NOOP("CommandCatalog", "Expliquer avec l'IA"), QT_TRANSLATE_NOOP("CommandCatalog", "Explication des résultats par l'assistant"), "",
                   ":/icons/common/ai_explain.svg", CommandCategory::Tools});
  registerCommand({"cmd.settings.theme", QT_TRANSLATE_NOOP("CommandCatalog", "Thème sombre / clair"), QT_TRANSLATE_NOOP("CommandCatalog", "Basculer entre le thème sombre et le thème clair"), "Ctrl+T",
                   ":/icons/common/theme_dark.svg", CommandCategory::Settings});
  registerCommand({"cmd.settings.language", QT_TRANSLATE_NOOP("CommandCatalog", "Langue de l'interface"), QT_TRANSLATE_NOOP("CommandCatalog", "Choisir la langue de l'interface : English, Français, Malagasy (appliquée au redémarrage)"), "",
                   ":/icons/common/language.svg", CommandCategory::Settings});
  registerCommand({"cmd.tools.search", QT_TRANSLATE_NOOP("CommandCatalog", "Rechercher une commande"), QT_TRANSLATE_NOOP("CommandCatalog", "Retrouver une commande, un panneau ou un paramètre par son nom, en anglais, en français ou en malgache"), "Ctrl+F",
                   ":/icons/common/search.svg", CommandCategory::Help});
  registerCommand({"cmd.help.shortcuts", QT_TRANSLATE_NOOP("CommandCatalog", "Liste des raccourcis"), QT_TRANSLATE_NOOP("CommandCatalog", "Afficher la liste des raccourcis clavier"), "F1",
                   ":/icons/common/shortcuts.svg", CommandCategory::Help});
  registerCommand({"cmd.help.shortcut_editor", QT_TRANSLATE_NOOP("CommandCatalog", "Personnaliser les raccourcis"), QT_TRANSLATE_NOOP("CommandCatalog", "Ouvrir l'éditeur des raccourcis clavier"), "Ctrl+F1",
                   ":/icons/common/keyboard_settings.svg", CommandCategory::Help});
  registerCommand({"cmd.help.modules", QT_TRANSLATE_NOOP("CommandCatalog", "Modules"), QT_TRANSLATE_NOOP("CommandCatalog", "Gérer les modules installés (état, approbation)"), "",
                   ":/icons/common/modules.svg", CommandCategory::Help});
  registerCommand({"cmd.help.full", QT_TRANSLATE_NOOP("CommandCatalog", "Aide complète"), QT_TRANSLATE_NOOP("CommandCatalog", "Ouvrir le centre d'aide"), "Shift+F1",
                   "", CommandCategory::Help});
  registerCommand({"cmd.help.online_docs", QT_TRANSLATE_NOOP("CommandCatalog", "Documentation en ligne"), QT_TRANSLATE_NOOP("CommandCatalog", "Ouvrir la documentation en ligne"), "",
                   ":/icons/common/online_docs.svg", CommandCategory::Help});
  registerCommand({"cmd.help.report_problem", QT_TRANSLATE_NOOP("CommandCatalog", "Signaler un problème"), QT_TRANSLATE_NOOP("CommandCatalog", "Signaler un problème"), "",
                   ":/icons/common/report_problem.svg", CommandCategory::Help});
  registerCommand({"cmd.help.about", QT_TRANSLATE_NOOP("CommandCatalog", "À propos"), QT_TRANSLATE_NOOP("CommandCatalog", "Informations de version"), "",
                   "", CommandCategory::Help});
}

} // namespace TSA::Commands
