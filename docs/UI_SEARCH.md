# Recherche globale des commandes

Le champ **Rechercher une commande** de la barre de titre (`Ctrl+F`, raccourci personnalisable `cmd.tools.search`)
retrouve une commande, un panneau ou un paramètre par son nom, quel que soit l'onglet du ruban qui la contient.

## Utilisation

- `Ctrl+F` ou un clic donne le focus au champ. La liste se met à jour à chaque frappe, sans bloquer la saisie : la
  recherche part 50 ms après la dernière touche.
- `↑` `↓` `Pg.Préc` `Pg.Suiv` choisissent un résultat, `Entrée` l'exécute.
- `Échap` ferme la liste ; un second `Échap` efface la recherche, un troisième rend le focus à la vue. Ces touches
  sont réservées au champ : `Échap` ne déclenche pas « Mode sélection » pendant la saisie.
- Le bouton ⓧ efface la recherche.

Chaque résultat affiche :

- l'icône, le nom dans la langue de l'interface et le raccourci actif ;
- sa nature : commande, panneau, paramètre ou console ;
- son emplacement dans le ruban (« Modèle › Éléments filaires ») et sa description.

## Ce qui est retrouvé

| Source | Exemples | Exécution |
|---|---|---|
| Catalogue des commandes (`CommandCatalog`) : toutes les commandes du ruban, des menus et des raccourcis, outils de dessin, de modification et de cotation compris | Dessiner une poutre, Plan de travail XY, Générer le maillage, Lancer le calcul, Importer IFC, Langue de l'interface | L'**action existante** du document au premier plan est déclenchée, comme par un clic dans le ruban. |
| Panneaux du document | Console, Navigateur du modèle, Données d'analyse | Le panneau est affiché et mis au premier plan. |
| Registre d'automatisation (`CommandRegistry`), commandes des plugins et des extensions comprises | `model.create_node`, `model.create_beam` | La console s'ouvre avec la commande pré-remplie ; on complète ses paramètres et on valide par Entrée. |

Une commande **indisponible** n'est jamais présentée comme exécutée. Une infobulle sous le champ en donne la raison :
- au Start Center, la commande demande un projet ouvert ;
- l'action est désactivée dans l'état actuel du projet (sélection, maillage ou résultats requis, rien à annuler) ;
- la commande n'existe pas dans cette fenêtre.

## Correspondance

Chaque entrée est cherchée dans :

1. son nom dans la langue affichée ;
2. ses noms dans les **deux autres langues** (traductions des `.ts`) : « beam », « poutre » et « andry mitsivalana »
   trouvent la même commande quelle que soit la langue de l'interface ;
3. des **synonymes** du métier (`src/Commands/CommandSynonyms.cpp`) : « IPE », « HEA », « solive », « girder »
   trouvent « Dessiner une poutre » ;
4. son identifiant (`cmd.create.beam`) et sa description.

La comparaison ne tient compte ni de la casse ni des accents (« modele » trouve « Modèle »). Elle unifie les
ligatures (œ → oe) et les apostrophes. Le classement va du plus au moins pertinent :

1. nom exact ;
2. début du nom ;
3. début d'un mot ;
4. fragment ;
5. faute de frappe : une lettre tolérée à partir de 4 lettres, deux à partir de 7, transposition comprise.

Pour une requête de plusieurs mots, chaque mot doit être trouvé, sauf les mots courts (« de », « the », « ny »…). Un
identifiant n'apparaît qu'une fois, et un panneau qui a déjà sa commande au catalogue n'est pas répété.

## Performances

L'index compte environ 400 entrées. Il est relu à chaque ouverture du champ, pour suivre le document actif et les
extensions chargées. Une requête sur 6 000 entrées prend moins de 25 ms (test 457).

## Architecture

| Fichier | Couche | Rôle |
|---|---|---|
| `src/Commands/CommandSearchIndex` | modèle | Normalisation, distance de frappe, pertinence ; aucun widget. |
| `src/Commands/CommandSynonyms` | modèle | Synonymes du métier, en français, anglais et malgache. |
| `src/UI/Search/GlobalSearchWidget` | widgets | Champ, liste, clavier, accessibilité, thème. |
| `src/UI/Shell/AppShell_Search.cpp` | application | Entrées dans le contexte actuel, emplacements du ruban, exécution et explications. |

Tests : suite `i18n`, tests 455 à 458.
