# Plan de correction

Ordre fixé par la règle de la mission : la fiabilité scientifique passe avant la vitesse de développement. Les charges sont des **estimations** d'ordre de grandeur. Chaque correction suit le processus d'AGENTS.md : branche dédiée, test écrit d'abord, banc complet, TSA puis TSALab.

## Phase 0 — Mise en sécurité : à faire avant tout usage réel avec rotules

Objectif : qu'aucun résultat faux ne soit produit sans message. Ce sont des refus, pas des corrections de fond.

| N° | Action | Défaut | Charge |
|---|---|---|---|
| 0.1 | Refuser, avec OpenSees, toute charge ponctuelle, trapézoïdale, partielle ou moment réparti sur une barre à rotule, en orientant vers Fem3D | BUG-148 | 0,5 j |
| 0.2 | Corriger le facteur de charge du lecteur (une ligne, puis fonction partagée) | BUG-149 | 0,5 j |
| 0.3 | Refuser P-Delta et corotationnel en statique linéaire | BUG-150 | 0,5 j |
| 0.4 | Avertir pour tout câble à N ≤ 0 | BUG-151 | 0,5 j |
| 0.5 | Vérifier les deux axes dans la note Legacy, afficher ses limites | BUG-152 | 0,5 j |
| 0.6 | Avertir pour toute cornière fléchie | BUG-153 | 0,5 j |
| 0.7 | Ajouter au banc les cas de cet audit, en attendus d'échec documentés s'il le faut | — | 1 j |

**Critère de sortie** : les cas `HINGE`, `GERBER`, `PIN2`, `NLIN`, `PDELTA-LIN` sont soit justes, soit refusés avec un message. Aucun ne produit de valeur fausse.

## Phase 1 — Corrections de fond du calcul

| N° | Action | Défaut | Charge |
|---|---|---|---|
| 1.1 | Condenser les forces équivalentes et les charges ponctuelles pour OpenSees ; adapter la reconstruction des efforts | BUG-148 | 3 à 5 j |
| 1.2 | Axes principaux pour les cornières, ou refus motivé | BUG-153 | 2 j |
| 1.3 | Règle unique du poids propre, coefficient appliqué ou retiré | BUG-156 | 1 j |
| 1.4 | Câbles sans compression : itération ou matériau dédié, à décider | BUG-151 | 3 à 5 j |
| 1.5 | Comparaison Fem3D / OpenSees rendue rapide d'accès pour contrôle croisé | — | 1 j |

## Phase 2 — Robustesse

| N° | Action | Défaut | Charge |
|---|---|---|---|
| 2.1 | Contrôle référentiel à la lecture, garde à `ModelValidator.cpp:576` | BUG-154 | 1 j |
| 2.2 | Frontières d'exception : `prepare`, `run`, corps de threads, `notify` | BUG-155 | 1 à 2 j |
| 2.3 | Sauvegarde automatique et récupération | BUG-157 | 3 j |
| 2.4 | Validation commune avant tout moteur | ARC-04 | 2 j |
| 2.5 | Défauts LOW de lecture et d'exécution | BUG-159 à 166 | 2 j |

## Phase 3 — Filet de validation

| N° | Action | Charge |
|---|---|---|
| 3.1 | Solveur indépendant et portiques aléatoires dans la suite `benchmarks` | 1 à 2 j |
| 3.2 | OpenSees en CI, CI bloquante | 2 j |
| 3.3 | Fuzzing nocturne de l'ouverture et du calcul | 1 j |
| 3.4 | ASan sous Windows, puis Linux, sur les suites sans OpenGL | 2 j |
| 3.5 | Mesure de couverture de code | 1 j |
| 3.6 | Validation Linux complète | 2 j |
| 3.7 | Contrôles locaux des efforts : M = 0 aux rotules, V = dM/dx | 1 j |

## Phase 4 — Performance

N'engager qu'après les phases précédentes, et mesurer avant et après.

| N° | Action | Gain mesuré ou estimé | Charge |
|---|---|---|---|
| 4.1 | Solveur creux supernodal pour Fem3D | Estimation ×5 à ×20 | 2 à 3 j |
| 4.2 | Index des charges et des identifiants | 4 à 5 s à 29 200 barres | 0,5 j |
| 4.3 | Partage de `Section` et `Material`, instantanés plus légers | Mémoire de l'historique | 3 à 5 j |
| 4.4 | Mesure réelle de l'affichage et du démarrage | Non mesuré | 1 j |

## Phase 5 — Maintenabilité

| N° | Action | Charge |
|---|---|---|
| 5.1 | Matrice de dépendances dans `check_layers.py`, exceptions gelées | 1 j |
| 5.2 | Une seule note de calcul : retrait de Legacy après accord | 3 j |
| 5.3 | Une seule définition de chaque règle métier (ARC-03) | 2 j |
| 5.4 | Mise à jour de `.claude/current-state.md` et `.claude/architecture.md` | 0,5 j |
| 5.5 | Extraction progressive d'`OccView` et de `MainWindow`, à l'occasion des évolutions | Continu |

## Décisions à prendre par le responsable du projet

| Sujet | Choix |
|---|---|
| ADR-022 et analyse modale | Assumer et documenter le modal, ou le retirer |
| Moteur par défaut | Garder OpenSees, ou passer à Fem3D pour les barres, qui réussit tous les cas à rotule |
| Note Legacy | Corriger, ou retirer |
| Câbles | Limiter le domaine d'emploi, ou développer le calcul sans compression |
| Licences Gmsh et OpenSees | Mode de diffusion |
| Résultats déjà produits | Revoir les notes de calcul de projets réels avec rotules ou non linéaire |

Le dernier point n'est pas du développement : **tout résultat déjà produit avec OpenSees sur une barre à rotule portant une charge non uniforme doit être considéré comme non fiable.**
