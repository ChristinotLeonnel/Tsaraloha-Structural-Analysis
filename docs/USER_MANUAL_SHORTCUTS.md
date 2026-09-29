# Manuel de Référence Utilisateur — Raccourcis & Commandes TSA
**Conforme aux normes IEEE Std 1063-2001 (R2007) & ISO/IEC/IEEE 26514:2022**

---

## 1. Identification du Document & Contrôle (IEEE Std 1063 §5.2)

| Métadonnée | Valeur |
| :--- | :--- |
| **Titre du document** | Manuel de Référence des Raccourcis Clavier et Commandes Console |
| **Identifiant du document** | `TSA-MAN-USER-CMD-2026.09` |
| **Version du document** | 2.1.0 |
| **Produit logiciel cible** | Tsaraloha Structural Analysis (TSA) v2026.09 |
| **Date d'émission** | 29 Septembre 2026 |
| **Auteur / Émetteur** | Équipe d'Ingénierie TSA |
| **Statut du document** | Approuvé / Référence Officielle |
| **Classification** | Manuel de Référence Logicielle (Software User Documentation) |

### 1.1 Historique des Révisions (Clause 5.2.7)

| Version | Date | Auteur | Description des modifications |
| :--- | :--- | :--- | :--- |
| **1.0.0** | 2026-08-15 | Équipe TSA | Création initiale de la documentation des raccourcis. |
| **2.0.0** | 2026-09-10 | Équipe TSA | Restructuration autour de `CommandCatalog` et des commandes unifiées. |
| **2.1.0** | 2026-09-29 | Équipe TSA | Mise en conformité stricte avec **IEEE Std 1063**. Intégration de `Ctrl+A` (Sélection totale), `P` (Dock Propriétés), séparation formelle Snap Grille (`S`) vs Accrochage Objets OSNAP (`F3`), et raccourcis Numpad standardisés (`Num+1`, `Num+3`, `Num+5`, `Num+7`). |

---

## 2. Portée, Public Cible et Prérequis (IEEE Std 1063 §5.3 - §5.4)

### 2.1 Portée (Scope)
Ce manuel couvre l'intégralité des commandes d'interaction utilisateur de TSA accessibles par :
1. Raccourcis clavier globaux et contextuels ;
2. Commandes saisies dans le dock Console ;
3. Boutons du Ruban (Ribbon Bar) et barres d'outils ;
4. Menus déroulants et menus contextuels du viewport 3D.

### 2.2 Public Cible (Audience Profile)
- **Ingénieurs en Génie Civil & Structures** : Modélisation et analyse de structures tridimensionnelles.
- **Projeteurs & Dessinateurs CAO** : Saisie géométrique rapide au clavier et à la souris.
- **Développeurs & Intégrateurs** : Extension du catalogue de commandes et scripts d'automatisation.

### 2.3 Prérequis Système et Matériel
- **Système d'exploitation** : Microsoft Windows 10/11 x64 (ou plateforme compatible Qt 6).
- **Clavier** : Clavier standard 104/105 touches avec pavé numérique dédié (recommandé pour les orientations de caméra).
- **Souris** : Souris 3 boutons avec molette centrale cliquable (indispensable pour les rotations 3D, le pan et le zoom).

---

## 3. Conventions Notations & Ergonomie (IEEE Std 1063 §5.5)

### 3.1 Conventions Typographiques

| Style | Signification | Exemple |
| :--- | :--- | :--- |
| `Ctrl+<Touche>` | Raccourci clavier avec modificateur | `Ctrl+S`, `Ctrl+Shift+M` |
| `Num+<Chiffre>` | Touche située sur le pavé numérique | `Num+7`, `Num+1` |
| **Gras** | Élément d'interface graphique (bouton, onglet, menu) | **Fichier > Enregistrer**, **Ruban Structure** |
| `MAJUSCULES` | Commande textuelle de la console | `FIT`, `OSNAP`, `BEAM` |
| `cmd.*` | Identifiant canonique interne de la commande | `cmd.view.isometric`, `cmd.snap.object_snap` |

### 3.2 Conventions d'Interaction 3D (Souris & Caméra)

| Action Souris | Contexte Standard (OpenCASCADE) | Effet dans TSA |
| :--- | :--- | :--- |
| **Clic Gauche** | Sélection ou premier point de dessin | Sélectionne l'objet sous le curseur ou place le nœud. |
| **Clic-Glisser Gauche** | Mode boîte de sélection | Sélection rectangulaire d'éléments (fenêtre/capture). |
| **Bouton Central (Molette enfoncée)** | Navigation 3D | Rotation orbitale dynamique de la caméra autour du centre. |
| **Shift + Bouton Central** | Translation de vue (Pan) | Déplacement panoramique du plan de vue. |
| **Rotation Molette** | Zoom progressif | Zoom avant / arrière centré sur la position du curseur. |
| **Clic Droit** | Menu contextuel | Affiche les actions applicables à la sélection courante. |

---

## 4. Référence Exhaustive des Commandes (IEEE Std 1063 §5.6 - §5.7)

### 4.1 Fichier & Gestion de Projet

| Commande | Identifiant `cmd.*` | Raccourci | Console | Emplacement UI | Préconditions & Effet |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **Nouveau Projet** | `cmd.file.new` | `Ctrl+N` | `NEW` | Ruban > Fichier > Nouveau | Réinitialise le modèle après confirmation de sauvegarde. |
| **Ouvrir Projet** | `cmd.file.open` | `Ctrl+O` | `OPEN` | Ruban > Fichier > Ouvrir | Ouvre une archive de structure `.tsa`. |
| **Enregistrer** | `cmd.file.save` | `Ctrl+S` | `SAVE` | Ruban > Fichier > Enregistrer | Sauvegarde immédiate du modèle courant. |
| **Enregistrer Sous** | `cmd.file.save_as` | `Ctrl+Shift+S` | — | Ruban > Fichier > Enregistrer sous | Demande un nouvel emplacement de fichier `.tsa`. |
| **Quitter TSA** | `cmd.file.exit` | `Alt+F4` | `EXIT` | Fenêtre > Fermer | Ferme proprement l'application. |

### 4.2 Édition & Gestion de l'Historique

| Commande | Identifiant `cmd.*` | Raccourci | Console | Emplacement UI | Effet & Notes |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **Annuler (Undo)** | `cmd.edit.undo` | `Ctrl+Z` | `UNDO`, `U` | Ruban > Édition > Annuler | Restaure le snapshot précédent du modèle via `UndoManager`. |
| **Rétablir (Redo)** | `cmd.edit.redo` | `Ctrl+Y` *(ou `Ctrl+Shift+Z`)* | `REDO` | Ruban > Édition > Rétablir | Réapplique la commande annulée. |
| **Copier** | `cmd.edit.copy` | `Ctrl+C` | `COPY` | Ruban > Édition > Copier | Place les éléments sélectionnés dans le presse-papier structural. |
| **Coller 3D** | `cmd.edit.paste` | `Ctrl+V` | — | Ruban > Édition > Coller | Active le mode interactif de dépôt 3D au clic curseur. |
| **Supprimer** | `cmd.edit.delete` | `Suppr` *(Delete)* | `DEL`, `DELETE` | Ruban > Édition > Supprimer | Supprime les nœuds et barres sélectionnés (avec Undo). |
| **Tout Sélectionner** | `cmd.select.all` | `Ctrl+A` | `SELECTALL`, `ALL` | Ruban > Édition > Tout Sélectionner | Sélectionne tous les éléments du modèle structural. |
| **Mode Sélection** | `cmd.select.mode` | `Échap` *(Esc)* | `SELECT`, `ESC` | Ruban > Édition > Sélectionner | Quitte le mode de dessin actif et vide la sélection. |

### 4.3 Création Géométrique & Modes de Dessin 3D

| Commande | Identifiant `cmd.*` | Raccourci | Console | Emplacement UI | Comportement de saisie |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **Dessin Nœud** | `cmd.create.node` | `N` | `NODE`, `N` | Ruban > Modélisation > Nœud | Clics successifs sur la grille ou les plans. |
| **Dessin Poutre** | `cmd.create.beam` | `B` | `BEAM`, `B` | Ruban > Modélisation > Poutre | Saisie 2 points (nœud départ, nœud arrivée). |
| **Dessin Poteau** | `cmd.create.column` | `C` | `COLUMN`, `C` | Ruban > Modélisation > Poteau | Saisie 2 points ou point base avec hauteur d'étage. |
| **Dessin Câble** | `cmd.create.cable` | `Alt+C` | `CABLE` | Ruban > Modélisation > Câble | Saisie 2 points pour éléments de tension. |
| **Dessin Dalle** | `cmd.create.slab` | `L` | `SLAB`, `L` | Ruban > Modélisation > Dalle | Saisie polygonale fermée d'au moins 3 nœuds coplanaires. |
| **Dessin Voile** | `cmd.create.wall` | `W` | `WALL`, `W` | Ruban > Modélisation > Voile | Saisie de 2 points horizontaux définissant la ligne d'assise. |
| **Fondation** | `cmd.create.foundation` | — | `FOOTING` | Ruban > Modélisation > Semelle | Placement de semelle isolée sous un nœud support. |
| **Treillis** | `cmd.create.truss` | — | `TRUSS` | Ruban > Modélisation > Treillis | Générateur paramétrique de poutres en treillis. |

### 4.4 Transformations 3D

| Commande | Identifiant `cmd.*` | Raccourci | Console | Emplacement UI | Effet & Notes |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **Déplacement 3D** | `cmd.modify.move` | `M` | `MOVE`, `M` | Ruban > Outils > Déplacer | Déplacement interactif point-à-point avec magnétisme. |
| **Translation Numérique** | `cmd.modify.translate` | `Ctrl+Shift+M` | — | Ruban > Outils > Translation | Dialogue numérique avec incréments précis ($dX, dY, dZ$). |
| **Copie Numérique** | `cmd.modify.copy_dialog` | `Ctrl+D` | — | Ruban > Outils > Dupliquer | Répétition linéaire multiple selon un vecteur ($dX, dY, dZ$). |
| **Rotation 3D** | `cmd.modify.rotate` | `Ctrl+R` | — | Ruban > Outils > Rotation 3D | Rotation interactive autour d'un axe défini par 2 points. |
| **Centrer sur Origine** | `cmd.modify.move_origin`| — | — | Ruban > Outils > Vers Origine | Translation de la sélection pour faire coïncider son centre à $(0,0,0)$. |

### 4.5 Navigation & Orientation Caméra 3D

| Commande | Identifiant `cmd.*` | Raccourci | Console | Effet Caméra |
| :--- | :--- | :--- | :--- | :--- |
| **Cadrer Tout (Fit All)** | `cmd.view.fit_all` | `F` | `FIT` | Ajuste le zoom pour englober tous les objets du modèle. |
| **Cadrer Sélection** | `cmd.view.fit_selection` | `Shift+F` | `FITSEL`, `FS` | Centre et cadre la vue sur les objets sélectionnés. |
| **Zoom Avant** | `cmd.view.zoom_in` | `+` | `ZOOMIN`, `ZI` | Rapproche la caméra de la scène. |
| **Zoom Arrière** | `cmd.view.zoom_out` | `-` | `ZOOMOUT`, `ZO` | Éloigne la caméra de la scène. |
| **Vue Initiale (Home)** | `cmd.view.home` | `Home` | `HOME` | Réinitialise la caméra selon l'angle par défaut 3D. |
| **Réinitialiser Vue** | `cmd.view.reset` | `R` | `RESET` | Réaligne la vue sans modifier le niveau de zoom. |
| **Vue de Dessus (Top)** | `cmd.view.top` | `Num+7` | `TOP` | Vue orthogonale sur le plan XY ($+Z$). |
| **Vue de Dessous (Bottom)** | `cmd.view.bottom` | `Ctrl+Num+7` | `BOTTOM` | Vue orthogonale orientée vers le dessous ($-Z$). |
| **Vue de Face (Front)** | `cmd.view.front` | `Num+1` | `FRONT` | Vue en élévation XZ (regard selon $-Y$). |
| **Vue Arrière (Back)** | `cmd.view.back` | `Ctrl+Num+1` | `BACK` | Vue en élévation XZ (regard selon $+Y$). |
| **Vue Gauche (Left)** | `cmd.view.left` | `Num+3` | `LEFT` | Vue de profil YZ (regard selon $-X$). |
| **Vue Droite (Right)** | `cmd.view.right` | `Ctrl+Num+3` | `RIGHT` | Vue de profil YZ (regard selon $+X$). |
| **Vue Isométrique** | `cmd.view.iso` | `Num+5` | `ISO` | Projection axonométrique isométrique standard. |
| **Vue Précédente** | `cmd.view.prev` | `Alt+Left` | `PREV` | Restaure l'état précédent de la caméra (historique). |
| **Vue Suivante** | `cmd.view.next` | `Alt+Right` | `NEXT` | Avance dans l'historique des positions caméra. |

### 4.6 Plans de Travail (WorkPlane), Grilles & Accrochages (OSNAP)

| Commande | Identifiant `cmd.*` | Raccourci | Console | Emplacement UI | Effet & Rôle |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **Afficher Grille 3D** | `cmd.coord.grid_visible` | `G` *(ou `F7`)* | `GRID`, `G` | Ruban > Affichage > Grille | Bascule la visibilité de la grille paramétrique 3D. |
| **Magnétisme Grille** | `cmd.snap.grid` | `S` | `SNAP` | Ruban > Affichage > Snap | Active/désactive l'aimantation sur la grille. |
| **Accrochage Objets (OSNAP)** | `cmd.snap.object_snap` | `F3` | `OSNAP` | Ruban > Affichage > OSNAP | Active/désactive l'aimantation intelligente (nœuds, milieux, extrémités). |
| **Plan de Travail XY** | `cmd.coord.workplane_xy` | — | `WPXY` | Dock Vue > WorkPlane | Positionne le plan actif sur l'horizontale ($Z=0$). |
| **Plan de Travail XZ** | `cmd.coord.workplane_xz` | — | `WPXZ` | Dock Vue > WorkPlane | Positionne le plan actif sur l'élévation longitudinale. |
| **Plan de Travail YZ** | `cmd.coord.workplane_yz` | — | `WPYZ` | Dock Vue > WorkPlane | Positionne le plan actif sur l'élévation transversale. |
| **Plan de Travail Étage** | `cmd.coord.workplane_level` | — | `WPLEVEL` | Dock Vue > WorkPlane | Aligne le plan actif sur l'étage/niveau sélectionné. |
| **Gestionnaire d'Étages** | `cmd.coord.manage_levels` | `Ctrl+L` | `LEVELS` | Ruban > Structure > Étages | Boîte de dialogue de gestion des niveaux altimétriques. |

### 4.7 Système d'Isolation 3D & Affichage

| Commande | Identifiant `cmd.*` | Raccourci | Console | Description & Effet |
| :--- | :--- | :--- | :--- | :--- |
| **Isoler Sélection** | `cmd.isolate.selection` | `I` | `ISOLATE` | Masque tous les éléments du modèle sauf la sélection active. |
| **Isoler par Type** | `cmd.isolate.same_type` | `Alt+I` | — | Conserve visibles uniquement les éléments du même type (ex. toutes les poutres). |
| **Isoler sur Plan** | `cmd.isolate.workplane` | `Alt+W` | — | Filtre et isole les éléments géométriquement proches du plan de travail. |
| **Isoler par Coupe** | `cmd.isolate.section` | `Ctrl+I` | — | Isole la tranche spatiale comprise entre deux cotes altimétriques. |
| **Isoler par Projection** | `cmd.isolate.projection` | `Ctrl+Shift+I`| — | Isole les éléments dont la projection tombe dans un rectangle de vue. |
| **Masquer Sélection** | `cmd.isolate.hide` | `H` | `HIDE` | Masque temporairement les éléments actuellement sélectionnés. |
| **Estomper / Fantôme** | `cmd.isolate.ghost` | `Shift+I` | `GHOST` | Alterne entre masquage strict et transparence (effet fantôme PBR). |
| **Annuler Isolation** | `cmd.isolate.undo` | `Ctrl+H` | `UNDO_ISO` | Restaure l'état d'affichage antérieur au dernier filtrage. |
| **Tout Afficher** | `cmd.isolate.show_all` | `Alt+H` | `SHOWALL` | Désactive tous les filtres d'isolation et réaffiche la structure complète. |

### 4.8 Fenêtres, Docks & Environnement

| Commande | Identifiant `cmd.*` | Raccourci | Console | Effet |
| :--- | :--- | :--- | :--- | :--- |
| **Panneau Propriétés** | `cmd.properties.panel` | `P` | `PROP`, `P` | Affiche ou masque le panneau latéral droit des propriétés métier. |
| **Basculer Thème** | `cmd.settings.theme` | `Ctrl+T` | `THEME` | Alterne dynamiquement entre le thème Sombre (Dark) et Clair (Light). |
| **Plein Écran** | `cmd.view.fullscreen` | `F11` | `FULLSCREEN` | Bascule l'affichage en plein écran sans bordure. |
| **Aide & Raccourcis** | `cmd.help.shortcuts` | `F1` | `HELP`, `?` | Ouvre la fenêtre récapitulative des raccourcis et commandes. |
| **Calcul Statique** | `cmd.analysis.solve` | `F5` | `SOLVE` | Lance l'analyse de structure par éléments finis. |

---

## 5. Vérification & Traçabilité (IEEE Std 1063 §4.2)

### 5.1 Traçabilité du Code Source
Chaque commande spécifiée dans ce manuel est rattachée :
1. À sa définition dans `src/Commands/CommandCatalog.cpp` ;
2. À son action Qt `QAction` dans `src/UI/MainWindow_Actions.cpp` ;
3. À son slot métier dans `src/UI/MainWindow.cpp` ou `src/Viewer/OccView.cpp`.

### 5.2 Outil de Contrôle Automatisé
La cohérence de ce document est contrôlée en continu via l'outil :
```powershell
python tools/check_shortcuts.py --strict
```
En cas de divergence entre l'implémentation C++ et cette documentation, l'outil lève une erreur bloquante et stoppe le pipeline d'intégration.
