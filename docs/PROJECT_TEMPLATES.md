# Templates de projets et plans de travail

## Templates de projets

**Enregistrer comme template…** (menu de l'application, recherche : « template ») enregistre le projet ouvert comme
modèle réutilisable, sur le principe des gabarits AutoCAD.

- Un template est un fichier projet `.tsa` ordinaire, rangé dans le dossier des templates de l'utilisateur
  (`<données de l'application>/project-templates`), avec une fiche `<nom>.json` :
  `{ "format": "tsa-project-template/1", "name", "description", "created" }`.
- Il reprend tout le contenu du projet : grille et axes (coordonnées réelles, espacements irréguliers), niveaux,
  plans de travail enregistrés, objets, cas de charge, réglages d'analyse et de maillage, vues nommées. Un `.tsa`
  n'enregistre aucun résultat de calcul.
- Le projet ouvert n'est que lu : son fichier, son nom et son état « modifié » ne changent pas.
- Un nom déjà pris n'est jamais écrasé en silence : le remplacement est demandé. L'écriture passe par un fichier
  temporaire, donc un échec laisse le template précédent intact.

**Nouveau projet** propose, sous les gabarits intégrés (« Structure générale », « Modèle vide »), les templates
enregistrés avec leur description. Le template est lu et vérifié dans un modèle à part, puis écrit dans le fichier
du nouveau projet sous le nom choisi, avant d'être ouvert normalement. Le nouveau projet est une copie indépendante :
le fichier du template n'est que lu, et un fichier existant n'est jamais écrasé. Un template sans fiche reste
proposé, sous le nom de son fichier.

Code : `src/Project/ProjectTemplates.{h,cpp}`. Tests : suite `templates_wp` (468 à 475).

## Plans de travail dérivés de la géométrie réelle

Le sélecteur de plans (barre au-dessus de la vue 3D) et l'arbre du modèle proposent les plans réellement définis
dans le projet. Aucune liste de hauteurs n'est codée dans le programme.

| Plans | Source de vérité |
|---|---|
| Z (niveaux) | Les niveaux du modèle (`LevelManager`), à défaut les altitudes de la grille active, à défaut les Z des nœuds. |
| X, Y (coupes) | Les axes de la grille active, plus les coordonnées des nœuds. |

- Chaque axe garde son identifiant attaché à sa coordonnée réelle. Des axes saisis dans le désordre (A = 0, B = 4,
  C = 5, D = 3,2, E = 7) sont triés par coordonnée sans perdre leur nom : le plan « D (X = 3.20 m) » est à 3,2 m.
- Les coordonnées X et Y ne sont jamais proposées comme niveaux. Il y a un plan Z par niveau, jamais un par
  intersection. Deux niveaux proches ne sont pas fusionnés.
- Les positions d'une grille sont mesurées depuis son origine : les plans ajoutent l'origine. Une grille tournée ou
  cylindrique ne définit pas de plans X = cte ou Y = cte ; ses altitudes restent proposées.
- Une modification de la grille reconstruit le repère du modèle (`src/Grid/GridCoordinateSync.{h,cpp}`) et
  actualise le sélecteur sans déplacer le plan actif. Si ce plan a disparu, le plus proche est retenu. Les éléments
  structurels ne sont jamais déplacés.
- Les niveaux sont des objets du projet (étages importés d'IFC ou de TSA3D compris). Ils ne sont remplacés par les
  altitudes de la grille que sur une action explicite : validation de la fenêtre Grille, nouveau projet.
- Compatibilité : les anciennes versions imposaient à tout projet les niveaux fixes 0 ; 3 ; 6,5 ; 10 m (« RDC (Z=0.00) »…),
  quelle que soit sa grille. À l'ouverture, ces niveaux sont remplacés par ceux de la grille ; tout autre niveau est
  conservé.
