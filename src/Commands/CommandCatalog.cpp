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
  // --- 1. FILE ---
  registerCommand({"cmd.file.new", "Nouveau Projet",
                   "Créer un nouveau projet vierge", "Ctrl+N",
                   ":/icons/common/file_new.svg", CommandCategory::File});
  registerCommand({"cmd.file.open", "Ouvrir",
                   "Ouvrir un projet existant (.tsa)", "Ctrl+O",
                   ":/icons/common/file_open.svg", CommandCategory::File});
  registerCommand({"cmd.file.save", "Enregistrer",
                   "Enregistrer le projet actuel", "Ctrl+S",
                   ":/icons/common/file_save.svg", CommandCategory::File});
  registerCommand({"cmd.file.save_as", "Enregistrer sous",
                   "Enregistrer sous un nouveau nom", "Ctrl+Shift+S",
                   ":/icons/common/file_save_as.svg", CommandCategory::File});
  registerCommand({"cmd.file.export_diagnostic", "Exporter Diagnostic",
                   "Générer un rapport de diagnostic complet", "",
                   ":/icons/common/diagnostic.svg", CommandCategory::File});
  registerCommand({"cmd.file.exit", "Quitter", std::string("Quitter ") + TSA::Product::kName, "Alt+F4",
                   ":/icons/common/exit.svg", CommandCategory::File});

  // --- 2. EDIT ---
  registerCommand({"cmd.edit.undo", "Annuler", "Annuler la dernière action",
                   "Ctrl+Z", ":/icons/edit/undo.svg", CommandCategory::Edit});
  registerCommand({"cmd.edit.redo", "Rétablir",
                   "Rétablir la dernière action annulée", "Ctrl+Y",
                   ":/icons/edit/redo.svg", CommandCategory::Edit});
  registerCommand({"cmd.edit.copy", "Copier",
                   "Copier les éléments sélectionnés dans le presse-papier",
                   "Ctrl+C", ":/icons/edit/copy.svg", CommandCategory::Edit});
  registerCommand({"cmd.edit.paste", "Coller",
                   "Coller les éléments du presse-papier", "Ctrl+V",
                   ":/icons/edit/paste.svg", CommandCategory::Edit});
  registerCommand({"cmd.edit.delete", "Supprimer",
                   "Supprimer les éléments sélectionnés", "Del",
                   ":/icons/edit/delete.svg", CommandCategory::Edit});

  // --- 3. CREATE / DRAW ---
  registerCommand({"cmd.create.node", "Nœud", "Créer un nœud structural en 3D",
                   "N", ":/icons/structure/node.svg", CommandCategory::Create});
  registerCommand({"cmd.create.beam", "Poutre",
                   "Créer une poutre horizontale ou inclinée", "B",
                   ":/icons/structure/beam.svg", CommandCategory::Create});
  registerCommand({"cmd.create.column", "Poteau",
                   "Créer un poteau vertical ou incliné", "C",
                   ":/icons/structure/column.svg", CommandCategory::Create});
  registerCommand({"cmd.create.cable", "Câble",
                   "Créer un câble structural ou hauban", "Alt+C",
                   ":/icons/structure/cable.svg", CommandCategory::Create});
  registerCommand({"cmd.create.bar", "Barre",
                   "Créer une barre générique (Robot SA style)", "",
                   ":/icons/structure/bar.svg", CommandCategory::Create});
  registerCommand({"cmd.create.truss", "Treillis",
                   "Créer une barre de treillis ou diagonale", "",
                   ":/icons/structure/truss.svg", CommandCategory::Create});
  registerCommand({"cmd.create.slab", "Dalle",
                   "Créer un panneau de dalle ou plancher", "L",
                   ":/icons/structure/slab.svg", CommandCategory::Create});
  registerCommand({"cmd.create.wall", "Voile", "Créer un voile ou mur porteur",
                   "W", ":/icons/structure/wall.svg", CommandCategory::Create});
  registerCommand({"cmd.create.foundation", "Fondation",
                   "Créer une semelle isolée ou filante", "",
                   ":/icons/structure/footing.svg", CommandCategory::Create});
  registerCommand({"cmd.create.presets", "Structures Types",
                   "Générer un portique, treillis ou tour type", "",
                   ":/icons/structure/presets.svg", CommandCategory::Create});

  // --- 4. MODIFY ---
  registerCommand({"cmd.modify.move", "Déplacer 3D",
                   "Déplacer les éléments sélectionnés point à point", "M",
                   ":/icons/structure/struct_move.svg",
                   CommandCategory::Modify});
  registerCommand({"cmd.modify.copy", "Copier 3D",
                   "Copier les éléments sélectionnés par translation", "Ctrl+D",
                   ":/icons/structure/struct_copy.svg",
                   CommandCategory::Modify});
  registerCommand({"cmd.modify.rotate", "Rotation 3D",
                   "Faire pivoter les éléments autour d'un axe", "Ctrl+R",
                   ":/icons/edit/rotate.svg", CommandCategory::Modify});
  registerCommand({"cmd.modify.translate", "Translation Numérique",
                   "Déplacer les éléments par incréments dX, dY, dZ",
                   "Ctrl+Shift+M", ":/icons/move.svg",
                   CommandCategory::Modify});
  registerCommand({"cmd.modify.mirror", "Symétrie (Miroir)",
                   "Copier ou retourner la sélection par symétrie / plan X, Y ou Z", "",
                   ":/icons/edit/mirror.svg", CommandCategory::Modify});
  registerCommand({"cmd.modify.split_bars", "Diviser les barres",
                   "Diviser les poutres et poteaux sélectionnés en N tronçons égaux", "",
                   ":/icons/structure/struct_split.svg", CommandCategory::Modify});
  registerCommand({"cmd.modify.merge_nodes", "Fusionner les nœuds confondus",
                   "Fusionner les nœuds géométriquement confondus du modèle", "",
                   ":/icons/structure/struct_merge.svg", CommandCategory::Modify});
  registerCommand({"cmd.modify.move_origin", "Déplacer vers Origine",
                   "Repositionner la sélection sur l'origine (0,0,0)", "",
                   ":/icons/structure/struct_move.svg",
                   CommandCategory::Modify});

  // --- 5. SELECTION ---
  registerCommand({"cmd.select.mode", "Sélectionner",
                   "Activer le mode sélection souris standard", "Esc",
                   ":/icons/edit/select.svg", CommandCategory::Selection});
  registerCommand({"cmd.select.all", "Tout Sélectionner",
                   "Sélectionner tous les éléments du modèle", "Ctrl+A",
                   ":/icons/edit/select_all.svg", CommandCategory::Selection});
  registerCommand({"cmd.select.clear", "Effacer Sélection",
                   "Désélectionner tous les éléments", "", "",
                   CommandCategory::Selection});
  registerCommand({"cmd.select.invert", "Inverser la Sélection",
                   "Sélectionner tous les éléments non sélectionnés", "Ctrl+Alt+I", "",
                   CommandCategory::Selection});
  registerCommand({"cmd.select.by_type", "Sélectionner par Type",
                   "Sélectionner tous les éléments d'un type (nœuds, poutres, poteaux...)", "", "",
                   CommandCategory::Selection});
  registerCommand({"cmd.select.same_section", "Même Section",
                   "Sélectionner les barres de même section que la sélection", "", "",
                   CommandCategory::Selection});
  registerCommand({"cmd.select.same_material", "Même Matériau",
                   "Sélectionner les éléments de même matériau que la sélection", "", "",
                   CommandCategory::Selection});
  registerCommand({"cmd.select.active_level", "Éléments du Niveau Actif",
                   "Sélectionner les éléments situés sur le niveau actif", "", "",
                   CommandCategory::Selection});
  registerCommand({"cmd.select.active_workplane", "Éléments du Plan de Travail",
                   "Sélectionner les éléments entièrement contenus dans le plan de travail actif", "", "",
                   CommandCategory::Selection});

  // --- 6. PROPERTIES ---
  registerCommand({"cmd.properties.panel", "Panneau Propriétés",
                   "Inspecter les propriétés de la sélection", "P",
                   ":/icons/view/properties.svg", CommandCategory::Properties});

  // --- 7. LIBRARIES ---
  registerCommand({"cmd.library.sections", "Catalogue Profilés",
                   "Consulter la bibliothèque de sections TSALib", "",
                   ":/icons/structure/library.svg",
                   CommandCategory::Libraries});
  registerCommand({"cmd.library.materials", "Catalogue Matériaux",
                   "Consulter les matériaux Eurocodes / ASTM", "",
                   ":/icons/structure/materials.svg",
                   CommandCategory::Libraries});
  registerCommand({"cmd.library.cables", "Catalogue Câbles",
                   "Consulter les torons et câbles clos TSALib", "",
                   ":/icons/structure/cable_library.svg",
                   CommandCategory::Libraries});
  registerCommand({"cmd.library.manager", "Gestionnaire d'Extensions",
                   "Gérer les packs et bibliothèques .tsalib", "",
                   ":/icons/extension_manager.svg",
                   CommandCategory::Libraries});

  // --- 8. STRUCTURE (Grilles & Niveaux) ---
  registerCommand({"cmd.struct.grid_dialog", "Gestionnaire de Grilles",
                   "Créer et modifier les grilles 3D", "",
                   ":/icons/grid/grid_manager.svg",
                   CommandCategory::Structure});
  registerCommand({"cmd.struct.levels", "Gestionnaire d'Étages",
                   "Définir les étages et niveaux de référence", "Ctrl+L",
                   ":/icons/structure/levels.svg", CommandCategory::Structure});

  // --- 9. LOADS ---
  registerCommand({"cmd.loads.point", "Charge Ponctuelle",
                   "Appliquer une force ponctuelle sur un nœud", "",
                   ":/icons/load_point.svg", CommandCategory::Loads});
  registerCommand({"cmd.loads.distributed", "Charge Répartie",
                   "Appliquer une charge linéique sur une barre", "",
                   ":/icons/load_distributed.svg", CommandCategory::Loads});
  registerCommand({"cmd.loads.moment", "Moment",
                   "Appliquer un moment fléchissant", "",
                   ":/icons/load_moment.svg", CommandCategory::Loads});

  // --- 10. ANALYSIS ---
  registerCommand({"cmd.analysis.mesh", "Générer Maillage",
                   "Générer le maillage éléments finis 1D/2D", "",
                   ":/icons/analysis_mesh.svg", CommandCategory::Analysis});
  registerCommand({"cmd.analysis.solve", "Calcul Statique",
                   "Lancer la résolution statique linéaire [K]{u}={F}", "F5",
                   ":/icons/analysis_run.svg", CommandCategory::Analysis});

  // --- 11. RESULTS ---
  registerCommand({"cmd.results.displacements", "Déplacements",
                   "Afficher la déformée et les flèches", "",
                   ":/icons/results_disp.svg", CommandCategory::Results});
  registerCommand({"cmd.results.forces", "Efforts Internes",
                   "Afficher les diagrammes de moments et tranchants", "",
                   ":/icons/results_forces.svg", CommandCategory::Results});
  registerCommand({"cmd.results.stresses", "Contraintes",
                   "Afficher les cartes de contraintes de Von Mises", "",
                   ":/icons/results_stress.svg", CommandCategory::Results});

  // --- 12. VIEW & DISPLAY ---
  registerCommand({"cmd.view.fit_all", "Tout Ajuster",
                   "Cadrer toute la scène 3D", "F", ":/icons/view/fit_all.svg",
                   CommandCategory::View});
  registerCommand({"cmd.view.fit_selection", "Cadrer Sélection",
                   "Cadrer la vue sur les éléments sélectionnés", "Shift+F",
                   ":/icons/view/fit_all.svg", CommandCategory::View});
  registerCommand({"cmd.view.zoom_in", "Zoom Avant", "Agrandir la vue", "+",
                   ":/icons/view/zoom_in.svg", CommandCategory::View});
  registerCommand({"cmd.view.zoom_out", "Zoom Arrière", "Réduire la vue", "-",
                   ":/icons/view/zoom_out.svg", CommandCategory::View});
  registerCommand({"cmd.view.zoom_window", "Zoom Fenêtre",
                   "Agrandir une région rectangulaire de la vue", "",
                   ":/icons/view/zoom_window.svg", CommandCategory::View});
  registerCommand({"cmd.view.prev", "Vue Précédente",
                   "Restaurer l'orientation et zoom caméra précédents",
                   "Alt+Left", ":/icons/edit/undo.svg", CommandCategory::View});
  registerCommand({"cmd.view.next", "Vue Suivante",
                   "Rétablir l'orientation et zoom caméra suivants",
                   "Alt+Right", ":/icons/edit/redo.svg",
                   CommandCategory::View});
  registerCommand({"cmd.view.home", "Vue Initiale",
                   "Réinitialiser la caméra en vue d'accueil 3D", "Home",
                   ":/icons/view/view_3d.svg", CommandCategory::View});
  registerCommand({"cmd.view.reset", "Réinitialiser Vue",
                   "Réinitialiser l'orientation de caméra 3D", "R",
                   ":/icons/view/view_iso.svg", CommandCategory::View});
  registerCommand({"cmd.view.top", "Vue de Dessus",
                   "Orienter la caméra vue de dessus (+Z)", "Num+7",
                   ":/icons/view/view_top.svg", CommandCategory::View});
  registerCommand({"cmd.view.bottom", "Vue de Dessous",
                   "Orienter la caméra vue de dessous (-Z)", "Ctrl+Num+7",
                   ":/icons/view/view_top.svg", CommandCategory::View});
  registerCommand({"cmd.view.front", "Vue de Face",
                   "Orienter la caméra vue de face (+Y)", "Num+1",
                   ":/icons/view/view_front.svg", CommandCategory::View});
  registerCommand({"cmd.view.back", "Vue Arrière",
                   "Orienter la caméra vue arrière (-Y)", "Ctrl+Num+1",
                   ":/icons/view/view_front.svg", CommandCategory::View});
  registerCommand({"cmd.view.left", "Vue Gauche",
                   "Orienter la caméra vue gauche (-X)", "Num+3",
                   ":/icons/view/view_side.svg", CommandCategory::View});
  registerCommand({"cmd.view.right", "Vue Droite",
                   "Orienter la caméra vue droite (+X)", "Ctrl+Num+3",
                   ":/icons/view/view_side.svg", CommandCategory::View});
  registerCommand({"cmd.view.iso", "Vue 3D Isométrique",
                   "Basculer en vue 3D axonométrique", "Num+5",
                   ":/icons/view/view_3d.svg", CommandCategory::View});
  registerCommand({"cmd.view.xy", "Plan (XY)", "Basculer en vue de dessus", "",
                   ":/icons/view/view_top.svg", CommandCategory::View});
  registerCommand({"cmd.view.xz", "Façade (XZ)", "Basculer en vue de face", "",
                   ":/icons/view/view_front.svg", CommandCategory::View});
  registerCommand({"cmd.view.yz", "Pignon (YZ)", "Basculer en vue latérale", "",
                   ":/icons/view/view_side.svg", CommandCategory::View});
  registerCommand({"cmd.view.section_cut", "Plan de Coupe",
                   "Activer le plan de coupe dynamique 3D", "",
                   ":/icons/view/section_cut.svg", CommandCategory::View});
  registerCommand({"cmd.view.fullscreen", "Plein Écran",
                   "Basculer en mode plein écran", "F11",
                   ":/icons/fullscreen.svg", CommandCategory::View});
  registerCommand({"cmd.coord.workplane_xy", "Plan de Travail XY",
                   "Activer le plan de travail horizontal XY", "Alt+Z",
                   ":/icons/view/view_top.svg", CommandCategory::View});
  registerCommand({"cmd.coord.workplane_xz", "Plan de Travail XZ",
                   "Activer le plan de travail vertical frontal XZ", "Alt+Y",
                   ":/icons/view/view_front.svg", CommandCategory::View});
  registerCommand({"cmd.coord.workplane_yz", "Plan de Travail YZ",
                   "Activer le plan de travail vertical latéral YZ", "Alt+X",
                   ":/icons/view/view_side.svg", CommandCategory::View});
  registerCommand({"cmd.coord.workplane_level", "Plan de Travail Étage",
                   "Aligner le plan de travail sur l'étage actif", "",
                   ":/icons/structure/levels.svg", CommandCategory::View});
  registerCommand({"cmd.snap.grid", "Magnétisme Grille",
                   "Activer ou désactiver l'accrochage magnétique à la grille",
                   "S", ":/icons/snap.svg", CommandCategory::View});
  registerCommand({"cmd.snap.object_snap", "Accrochage Objets (OSNAP)",
                   "Activer/désactiver l'accrochage magnétique intelligent",
                   "F3", ":/icons/view/snap.svg", CommandCategory::View});
  registerCommand({"cmd.display.grid", "Afficher Grille",
                   "Afficher ou masquer la grille 3D", "G",
                   ":/icons/view/grid.svg", CommandCategory::Display});
  registerCommand({"cmd.display.rulers", "Afficher Règles",
                   "Afficher ou masquer les règles de projection", "",
                   ":/icons/view/rulers.svg", CommandCategory::Display});

  // --- 13. SETTINGS & HELP ---
  registerCommand({"cmd.settings.theme", "Basculer Thème",
                   "Alterner entre le thème sombre et clair", "Ctrl+T",
                   ":/icons/common/theme_dark.svg", CommandCategory::Settings});
  registerCommand({"cmd.help.help", std::string("Aide Complète ") + TSA::Product::kName,
                   "Ouvrir le centre d'aide, guide et documentation", "",
                   ":/icons/common/help.svg", CommandCategory::Settings});
  registerCommand({"cmd.help.shortcuts", "Raccourcis Clavier",
                   "Afficher la liste des raccourcis", "F1",
                   ":/icons/common/shortcuts.svg", CommandCategory::Settings});
  registerCommand({"cmd.help.about", std::string("À Propos de ") + TSA::Product::kName,
                   "Afficher les informations de version et crédits", "",
                   ":/icons/common/about.svg", CommandCategory::Settings});

  // --- 14. ISOLATION 3D --- (menu Affichage ▸ Isolation 3D, OccView::isolateElements…)
  registerCommand({"cmd.isolate.selection", "Isoler la Sélection",
                   "Masquer tous les objets sauf la sélection", "I", "",
                   CommandCategory::View});
  registerCommand({"cmd.isolate.same_type", "Isoler par Type",
                   "Isoler les objets du même type que la sélection", "Alt+I",
                   "", CommandCategory::View});
  registerCommand({"cmd.isolate.workplane", "Isoler sur Plan de Travail",
                   "Isoler les objets situés sur le plan de travail actif",
                   "Alt+W", ":/icons/view/view_top.svg",
                   CommandCategory::View});
  registerCommand({"cmd.isolate.hide", "Masquer la Sélection",
                   "Masquer les éléments sélectionnés", "H", "",
                   CommandCategory::View});
  registerCommand({"cmd.isolate.invert", "Inverser l'Isolation",
                   "Échanger objets visibles et objets masqués", "", "",
                   CommandCategory::View});
  registerCommand({"cmd.isolate.undo", "Isolation Précédente",
                   "Annuler la dernière isolation", "Ctrl+H", "",
                   CommandCategory::View});
  registerCommand({"cmd.isolate.show_all", "Tout Afficher",
                   "Mettre fin à l'isolation et réafficher tous les objets",
                   "Alt+H", "", CommandCategory::View});
}

} // namespace TSA::Commands
