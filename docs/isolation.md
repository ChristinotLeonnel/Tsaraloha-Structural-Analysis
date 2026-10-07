# Isolation 3D

Commandes du menu **Affichage ▸ Isolation 3D** (catalogue `cmd.isolate.*`, raccourcis dans
`docs/shortcuts.txt`). Elles agissent sur l'unique passe de visibilité du viewport,
`OccView::updateElementIsolation()`, partagée avec l'isolation du plan de travail, le mode 2D et le
filtre des familles (dock Calques & Visibilité) : il n'existe pas d'autre mécanique de visibilité.

| Commande | Raccourci | Effet |
| :--- | :--- | :--- |
| Isoler la sélection | I | seuls les éléments sélectionnés (et leurs nœuds) restent visibles |
| Isoler par type | Alt+I | isole toutes les familles présentes dans la sélection (poutres, poteaux…) |
| Isoler le plan de travail | Alt+W | bascule l'isolation de proximité du plan de travail actif |
| Masquer la sélection | H | masque les éléments sélectionnés |
| Inverser l'isolation | — | isole exactement ce qui était masqué |
| Isolation précédente | Ctrl+H | revient à l'état d'isolation précédent (pile) |
| Tout afficher | Alt+H | met fin à l'isolation et au masquage par éléments |

API (`OccView`) : `isolateElements(ElementSet)`, `hideElements(ElementSet)`,
`invertElementIsolation()`, `undoElementIsolation()`, `showAllElements()`. L'isolation est un état
d'affichage : elle ne modifie pas le modèle et n'entre pas dans l'historique Annuler / Rétablir.
