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
| Ouvrir (Ctrl+O), projet récent | nouvel onglet ; si le fichier est déjà ouvert, son onglet est activé (pas de second exemplaire) ; en cas d'échec du chargement, l'onglet vide est retiré et l'onglet précédent revient au premier plan |
| Glisser-déposer un fichier `.tsa` (fenêtre ou vue 3D) | comme Ouvrir : rien n'est remplacé, aucune confirmation n'est donc nécessaire ; fichier corrompu ou verrouillé par un autre programme : message explicite, onglets conservés ; fichier non `.tsa` : refusé |
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
- **Fenêtres flottantes** (panneau d'outil comme « Éléments Filaires », panneaux détachés) : elles appartiennent à
  leur document. Quand l'onglet passe en arrière-plan, elles sont masquées ; elles reviennent, sans être activées,
  quand il repasse au premier plan (`MainWindow::setDocumentForeground`). Le bouton « Ajouter » d'un projet caché ne
  peut donc pas rester sous les yeux de l'utilisateur pendant qu'il travaille sur un autre projet. L'outil
  reprend seulement si l'utilisateur clique dans son panneau.
- **Vue 3D** : une vue réaffichée se réajuste à la taille de la fenêtre, même si celle-ci a été redimensionnée
  pendant que l'onglet était caché. Il n'y a plus de bande noire.
- **Répéter la dernière commande (Ctrl+Entrée)** :
  - TSA retient l'identifiant de la dernière commande de travail (création, modification, charges, appuis),
    commun à toute l'application ;
  - la commande est rejouée **dans le document au premier plan** ;
  - elle ne vise jamais l'action d'un onglet caché, ni celle d'un onglet fermé (`ShortcutManager::repeatTarget`) ;
  - si aucun document n'est visible, rien n'est déclenché.
- **Mémoire** : fermer un onglet détruit son `MainWindow` (`deleteLater`), avec son modèle, ses résultats, sa vue
  et ses caches.

### Serveur d'automatisation (MCP)

Chaque document a son serveur, mais un seul écoute : celui du document au premier plan. Le canal a un nom fixe,
pour rester compatible avec les clients existants.

- **Changement d'onglet** : le serveur du document quitté est arrêté et ses connexions sont fermées. Le pont MCP se
  reconnecte au document actif à la requête suivante. Aucune requête ne peut atteindre un document caché ou fermé.
- **Chaque requête** vérifie que le document est encore ouvert et actif. Chaque réponse porte
  `document {id, name, path}`. L'identifiant est stable pendant toute la vie de l'onglet.
- **`list_documents`** donne les documents ouverts. Une requête peut préciser `document_id` : s'il ne désigne pas le
  document actif, elle reçoit une erreur explicite et **rien n'est exécuté**. Aucune commande ne vise un document
  en arrière-plan.
- **Unicité du canal** : sous Windows, `QLocalServer::listen` réussit même si un autre serveur écoute déjà sous le
  même nom. Le démarrage vérifie donc d'abord qu'aucun serveur ne répond (autre document, autre instance de TSA).
  Voir `docs/MCP.md`.

## 2. Presse-papier structurel

`StructuralClipboard::shared()` (`src/Model/StructuralClipboard.*`) est un presse-papier unique pour toute
l'application. On peut copier dans un onglet et coller dans un autre, même après avoir fermé le projet source :
le contenu est une copie par valeur, sans référence vers ce projet.

**Contenu copié :**

| Toujours | En option (Édition > Options du presse-papier, mémorisé) | Jamais |
| :--- | :--- | :--- |
| nœuds (positions relatives), poutres, poteaux, voiles, barres de treillis, câbles, dalles complètes (ouvertures, formulation, appuis linéaires), fondations sélectionnées (dimensions, matériau, portance), cotations sélectionnées, sections, matériaux, orientations, relâchements, métadonnées BIM (un produit par produit source) | appuis des nœuds (activé par défaut) ; charges nodales, sur barres et surfaciques des éléments copiés, avec leurs cas et les combinaisons pertinentes (désactivé par défaut) | résultats et réglages de calcul : le projet cible doit être recalculé |

**Politique par type d'objet :**

| Objet | Règle |
| :--- | :--- |
| Fondation | copiée si elle est sélectionnée ; son nœud porteur est copié avec elle |
| Cotation (annotation) | copiée si elle est sélectionnée. Un ancrage sur un nœud copié est relié au nœud collé. Un ancrage sur un nœud non copié est figé à la position du nœud, puis translaté avec le collage. La cotation reste cohérente et n'a jamais de référence invalide. |
| Cas de charge | rapproché par nom et catégorie. Une copie renommée lors d'un collage précédent (« Q (copie) ») est réutilisée. Sinon le cas est créé ; si un cas de même nom mais d'autre catégorie existe, il est créé sous « Q (copie) », avec un avertissement. |
| Combinaison | copiée si elle utilise au moins un cas des charges copiées ; ses autres cas sont emportés (sans charge) pour qu'elle reste complète. Une combinaison identique (même nom, type et facteurs) déjà présente est réutilisée. Une combinaison homonyme au contenu différent est copiée sous « ELU 1 (copie) », avec un avertissement. Une combinaison qui référence un cas absent du projet source n'est pas copiée : avertissement précis à la copie. |

**Fusion des nœuds coïncidents** (Édition > Options du presse-papier) :
- **Nœuds séparés** (défaut) : comportement historique, pour des modèles indépendants.
- **Fusionner les nœuds coïncidents** : un nœud collé réutilise un nœud existant si la **distance réelle** est
  inférieure à la tolérance. La tolérance est en mètres, l'unité de longueur du modèle (1 mm par défaut, réglable
  dans « Tolérance de fusion… »). La recherche se fait sur une grille de cellules de la taille de la tolérance.

Avant toute modification, `analyzePaste` analyse le collage :

| Situation | Traitement |
| :--- | :--- |
| Appui copié + nœud existant libre, ou appuis identiques | compatible ; l'appui est conservé, aucune propriété n'est perdue |
| Deux appuis non libres et différents | **conflit** : rien n'est collé, et une boîte propose « Ne pas fusionner ces nœuds » (défaut), « Conserver les appuis du projet », « Utiliser les appuis copiés » ou « Annuler » |
| Élément dont deux nœuds seraient fusionnés en un seul (longueur nulle, sommet de dalle doublé) | **bloquant** : collage refusé, modèle inchangé |
| Élément reliant deux nœuds existants déjà reliés par un élément de la même famille | doublon. Une boîte propose « Ne pas coller les doublons » (défaut : élément et ses charges non collés) ou « Les coller quand même » (avertissement : rigidité doublée) |
| Charges nodales d'un nœud fusionné | ajoutées au nœud existant, avec un avertissement |

**Dépendances** : elles sont rapprochées au collage. Les tables de correspondance source → cible sont exposées
dans `PasteResult` : `nodeMap`, `elementMap`, `loadCaseMap`, `mergedNodeIds`. Les identifiants des objets collés
sont toujours nouveaux, sans collision avec les existants.

**Commandes :**

| Commande | Raccourci | Effet |
| :--- | :--- | :--- |
| Copier | Ctrl+C | sélection → presse-papier |
| Couper | Ctrl+X | copie, puis suppression annulable |
| Coller au point désigné | Ctrl+V | le coin minimal de la sélection copiée est placé au point cliqué dans la vue |
| Coller à la position d'origine | Ctrl+Maj+V | mêmes coordonnées que dans le projet source |

**Tout ou rien** (`pasteUndoable`) :
1. Le contenu est d'abord validé : chaque élément doit avoir ses nœuds, chaque charge son cas de charge.
2. Les conflits sont analysés. Un conflit à trancher ou un élément dégénéré arrête tout : rien n'est modifié, et
   l'historique n'est pas touché.
3. Le collage se fait ensuite dans une transaction d'édition. Il forme une seule étape d'annulation : Ctrl+Z annule
   le collage entier, y compris un appui remplacé sur un nœud fusionné.
4. Si un élément ne peut pas être recréé, la transaction est annulée. Le modèle revient à son état d'avant le
   collage, sans entrée Annuler ni Rétablir, et un message indique la dépendance manquante.

Les avertissements (renommages, doublons ignorés, combinaisons non copiées) sont écrits dans la console, et leur
nombre est rappelé dans la barre d'état.

## 3. Tests

**Suite `display`, test 375** : copier-coller entre projets, avec le projet source détruit avant le collage.

**Suite `multidoc`** :

| Test | Objet |
| :--- | :--- |
| 379 | fusion des nœuds : portique collé sur lui-même (séparés, fusionnés, doublons ignorés ou conservés), poutre reliée à un nœud existant, modèles indépendants, distance comparée à la tolérance |
| 380 | conflits d'appuis : décision demandée sans rien modifier ; séparer, conserver, remplacer ; compatibilité appui + nœud libre ; Annuler |
| 381 | élément dégénéré : refusé, modèle et historique inchangés |
| 382 | fondations et cotations : propriétés, nœud porteur, ancrages reliés ou figés, Annuler |
| 383 | combinaisons : pertinence, facteurs, cas emportés, réutilisation, homonymes renommés, référence absente |
| 384 | Annuler / Rétablir avec fusion, identifiants, sections, matériaux, orientations, aucun réglage de calcul transféré |
| 385 | ouverture : fichier absent (« introuvable ») ou verrouillé (« inaccessible ») |
| 386 | MCP et plusieurs documents, par de vraies connexions locales |
| 387 | profils d'affichage (`docs/DISPLAY_SCALES.md` § 7) |

**Suite `shortcuts`, test 378** : répétition de la dernière commande avec plusieurs documents (onglet caché, allers
et retours, onglet d'origine fermé, aucun document visible).

**Scénarios dans l'application réelle** (`tools/ui_tests/`) :
- `multidoc_scenario.ps1` couvre :
  - noms identiques ;
  - résultats propres à chaque onglet ;
  - fichier déjà ouvert ;
  - nouveau projet ;
  - copier-coller d'un onglet à l'autre ;
  - Ctrl+Tab ;
  - fermeture avec confirmation.
- `dragdrop_repeat_scenario.ps1` couvre :
  - un vrai glisser-déposer depuis l'Explorateur : même nom, troisième projet, déjà ouvert, corrompu, verrouillé,
    non `.tsa` ;
  - Ctrl+Entrée après changement d'onglet, après plusieurs changements et après fermeture de l'onglet d'origine ;
  - le panneau flottant d'un outil, qui ne reste pas sur l'autre projet.

## 4. Limites

| Priorité | Limite |
| :--- | :--- |
| P2 | Les charges nodales d'un nœud fusionné s'ajoutent à celles du nœud existant : un portique collé sur lui-même avec ses charges les double (un avertissement le signale). |
| P2 | Collage d'une cotation seule : ses ancrages sont figés (aucun nœud n'est copié avec elle). |
| P3 | Le presse-papier structurel n'est pas relié au presse-papier Windows : on ne peut pas coller entre deux instances de TSA. |
| P3 | MCP : les commandes agissent sur le document actif. Viser un autre document demande d'activer son onglet dans l'application (choix de sécurité : aucune modification d'un projet que l'utilisateur ne voit pas). |
