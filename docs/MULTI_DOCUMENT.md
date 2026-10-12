# Projets multiples et copier-coller entre projets — TSA

## 1. Onglets

Chaque projet ouvert a son onglet, sous la barre de titre, comme dans un logiciel de CAO. Chaque onglet est un
`MainWindow` complet et indépendant, géré par `AppShell` (`src/UI/Shell/AppShell.*`). Il a ses propres :
- modèle et historique Annuler / Rétablir ;
- maillage, contexte et résultats d'analyse ;
- vue 3D et sélection ;
- panneaux ;
- état d'enregistrement.

Aucune donnée n'est partagée entre projets, sauf le presse-papier structurel (§ 2), partagé volontairement.

| Action | Comportement |
| :--- | :--- |
| Nouveau projet (Ctrl+N) | nouvel onglet ; les projets ouverts restent intacts |
| Ouvrir (Ctrl+O), projet récent, glisser-déposer | nouvel onglet ; si le fichier est déjà ouvert, son onglet est activé (pas de second exemplaire) ; en cas d'échec du chargement, l'onglet vide est retiré |
| Fermer le projet (Ctrl+W, croix de l'onglet) | confirmation si le projet est modifié (Enregistrer / Ne pas enregistrer / Annuler) ; refus si un calcul est en cours ; l'onglet voisin est activé ; le dernier onglet ramène au Start Center |
| Fermer tous les projets, Enregistrer tous les projets | menu de l'application et menu contextuel des onglets |
| Quitter | confirmation pour chaque projet modifié ; « Annuler » interrompt la fermeture |
| Ctrl+Tab / Ctrl+Maj+Tab, Ctrl+Pg.Suiv / Ctrl+Pg.Préc | onglet suivant / précédent |
| Menu contextuel d'un onglet | Enregistrer, Fermer, Fermer les autres projets, Fermer tous, Enregistrer tous, Copier le chemin, Ouvrir le dossier |

- **Nom de l'onglet** : le nom du fichier, suivi de « ● » si le projet est modifié. Deux fichiers de même nom dans
  des dossiers différents sont distingués par leur dossier (« Reservoir.tsa — d1 »). L'infobulle donne le chemin
  complet.
- **Barre de titre** : ses actions (Enregistrer, Enregistrer sous, Annuler, Rétablir) et son titre suivent l'onglet
  actif.
- **Serveur d'automatisation (MCP)** : il a un nom fixe et ne peut donc écouter que pour un seul projet. Il suit
  l'onglet actif (`MainWindow::setAutomationActive`).
- **Mémoire** : fermer un onglet détruit son `MainWindow` (`deleteLater`), avec son modèle, ses résultats, sa vue
  et ses caches.

## 2. Presse-papier structurel

`StructuralClipboard::shared()` (`src/Model/StructuralClipboard.*`) est un presse-papier unique pour toute
l'application. On peut copier dans un onglet et coller dans un autre, même après avoir fermé le projet source :
le contenu est une copie par valeur, sans référence vers ce projet.

**Contenu copié :**

| Toujours | En option (Édition > Options du presse-papier, mémorisé) | Jamais |
| :--- | :--- | :--- |
| nœuds (positions relatives), poutres, poteaux, voiles, barres de treillis, câbles, dalles complètes (ouvertures, formulation, appuis linéaires), sections, matériaux, orientations, relâchements, métadonnées BIM (un produit par produit source) | appuis des nœuds (activé par défaut) ; charges nodales, sur barres et surfaciques des éléments copiés (désactivé par défaut) | résultats de calcul : le projet cible doit être recalculé |

**Dépendances** : elles sont rapprochées au collage. Les tables de correspondance source → cible sont exposées
dans `PasteResult` : `nodeMap`, `elementMap`, `loadCaseMap`.
- **Nœuds** : ils sont recréés.
- **Sections et matériaux** : ils sont copiés avec l'élément.
- **Cas de charge** : ils sont rapprochés par nom et catégorie. Un cas absent du projet cible y est créé
  (`createdLoadCaseIds`).

**Commandes :**

| Commande | Raccourci | Effet |
| :--- | :--- | :--- |
| Copier | Ctrl+C | sélection → presse-papier |
| Couper | Ctrl+X | copie, puis suppression annulable |
| Coller au point désigné | Ctrl+V | le coin minimal de la sélection copiée est placé au point cliqué dans la vue |
| Coller à la position d'origine | Ctrl+Maj+V | mêmes coordonnées que dans le projet source |

**Tout ou rien** (`pasteUndoable`) :
1. Le contenu est d'abord validé : chaque élément doit avoir ses nœuds, chaque charge son cas de charge.
2. Le collage se fait ensuite dans une transaction d'édition. Il forme une seule étape d'annulation : Ctrl+Z annule
   le collage entier.
3. Si un élément ne peut pas être recréé, la transaction est annulée. Le modèle revient à son état d'avant le
   collage, sans entrée Annuler ni Rétablir, et un message indique la dépendance manquante.

## 3. Tests

- **Test 375** (suite `display`) :
  - le projet source est détruit avant le collage ;
  - collage dans un projet qui a déjà du contenu et un cas « G » d'identifiant différent ;
  - vérifiés : positions, sections, matériaux, orientations, appuis, voile, treillis, dalle ;
  - charges : valeurs identiques, cas rapprochés ou créés ;
  - collage à l'origine ;
  - Annuler et Rétablir du collage entier ;
  - dépendance manquante : modèle et historique inchangés.
- **Scénario dans l'application réelle** : `tools/ui_tests/multidoc_scenario.ps1`. Il couvre :
  - noms identiques ;
  - résultats propres à chaque onglet ;
  - fichier déjà ouvert ;
  - nouveau projet ;
  - copier-coller d'un onglet à l'autre ;
  - Ctrl+Tab ;
  - fermeture avec confirmation ;
  - fermeture de l'application.

## 4. Limites

- Fondations et cotations : elles ne sont pas copiées.
- Nœuds coïncidents : le collage ne les fusionne pas. Un collage à l'origine dans le projet source superpose donc
  des éléments aux originaux. Les outils de nettoyage du modèle les détectent.
- Charges : copier une charge emporte son cas de charge, mais pas les combinaisons du projet source.
- Presse-papier Windows : le presse-papier structurel n'y est pas relié. On ne peut pas coller entre deux
  instances de TSA.
