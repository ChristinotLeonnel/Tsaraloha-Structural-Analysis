# Projets récents et aperçus du dernier état

## Ce que voit l'utilisateur

Au démarrage (sans fichier en argument), la zone centrale affiche **Projets récents** : une carte
par projet, avec l'aperçu du dernier état réel du modèle, le nom du fichier, « Modifié il y a… » et
le dossier. Survol : bordure, légère ombre, « Ouvrir → ». Clic ou Entrée : ouvrir. Clic droit :
Ouvrir, Renommer, Dupliquer, Afficher dans le dossier, Actualiser l'aperçu, Retirer de la liste,
Supprimer (Corbeille), Propriétés. Retour à l'accueil : Ruban › Accueil › Projets récents, ou
Fichier › Projets récents. Échap revient au modèle.

## Fonctionnement

- **Un seul viewport** : `QStackedWidget` central = page d'accueil (`UI/Home/StartPage`) ou
  `ViewportContainer`. OCCT n'est initialisé qu'au premier affichage du viewport.
- **Capture** : `OccView::captureViewImage` (rendu OCCT hors écran du vrai viewport : mêmes calques,
  même caméra, mode 2D compris), 480×270. Déclenchée après 2,5 s sans activité, uniquement si la
  révision du modèle ou la caméra ont changé ; aussi à l'enregistrement, au changement de projet et
  à la fermeture. L'écriture disque se fait dans un thread secondaire.
- **Cache** (`Project/ModelPreviewCache`) : `<AppData>/previews/<sha1(chemin)>.png` + `.json`
  (`ProjectPreviewMetadata` : caméra, état d'affichage, révision, effectifs, format source,
  version). Ignoré si le fichier a été modifié après la capture (autre poste, autre instance).
- **Repli** : miniature embarquée dans le `.tsa` (chunk `THMB`, écrite à chaque enregistrement) — un
  projet reçu d'ailleurs a donc aussi un aperçu.
- **Caméra** : `OccView::cameraState()` / `applyCameraState()` ; restaurée à la réouverture si le
  fichier n'a pas changé depuis. L'état d'affichage (`viewState`) est mémorisé mais pas encore
  restauré (il est lié aux actions de l'interface).
- **Liste** (`Project/RecentProjects`) : QSettings, 16 entrées, chemins normalisés, fichiers absents
  masqués.

## Préparation BIM et IA

`sourceFormat` (TSA aujourd'hui ; IFC, DXF… plus tard) : l'aperçu reste celui du modèle structural
TSA, pas une icône de format. L'IA de co-ingénierie travaille sur les données TSA réelles ; l'aperçu
et ses métadonnées sont disponibles pour un futur modèle « vision », sans remplacer ces données.

## Limites

- Pas d'onglets multi-documents dans TSA : pas de « Ouvrir dans un nouvel onglet ».
- Le cube de navigation et le trièdre font partie du viewport : ils apparaissent dans l'aperçu.
