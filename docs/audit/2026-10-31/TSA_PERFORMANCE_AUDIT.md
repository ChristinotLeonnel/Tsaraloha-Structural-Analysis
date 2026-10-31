# Audit de performance

## Conclusion

Pour un modèle de 8 000 barres, l'édition, l'annulation et les fichiers restent fluides. Le premier vrai goulot est la **factorisation de Fem3D** : 117 s contre 18 s pour OpenSees à 29 200 barres. L'historique par instantanés complets devient perceptible, sans être gênant, autour de 30 000 barres.

Les performances d'affichage n'ont **pas** été mesurées, pour ne pas ouvrir de fenêtres sur le poste pendant l'audit.

## Conditions de mesure

- **Poste** : Windows 11, Intel i5-11400H (6 cœurs, 12 fils), RTX 3050 et Intel UHD.
- **Binaires** : Release MSVC 19.51, `/O2 /Ob2`, sans optimisation à l'édition de liens.
- **Moyen** : harnais `preuves/scripts/harness.cpp`, lié aux objets du build, sur l'API réelle.
- **Modèle** : portique 3D, poteaux HEB 200, poutres IPE 300, poids propre et une charge nodale.
- **Fiabilité** : deux passages pour les deux premières tailles, un seul pour 29 200 barres. D'autres applications étaient ouvertes.

## Mesures réelles

Les deux premières tailles donnent la plage des deux passages. Temps en millisecondes.

| Opération | 1 125 barres | 8 330 barres | 29 200 barres | Rapport 8 330 → 29 200 (×3,5 barres) |
|---|---:|---:|---:|---:|
| Construction du modèle | 1,7 à 1,8 | 12,7 | 43,5 | ×3,4 |
| Instantané d'historique | 2,6 à 2,9 | 24,3 à 24,5 | 82,4 | ×3,4 |
| Enregistrement `.tsa` | 92 à 95 | 424 à 481 | 1 927 | ×4,3 |
| Ouverture `.tsa` | 17 à 18 | 136 à 183 | 499 | ×3,1 |
| Édition d'un nœud (transaction) | 3,9 | 33 à 46 | 133 | ×3,3 |
| Annuler | 3,8 | 38 à 49 | 128 | ×3,0 |
| Rétablir | 3,0 à 3,1 | 35 à 44 | 125 | ×3,2 |
| Différentiel d'historique | 0,3 à 0,4 | 3,5 à 4,9 | 16,4 | ×3,9 |
| Préparation du calcul (Fem3D) | 11 à 12 | 115 à 155 | 1 079 | ×8 |
| Préparation du calcul (OpenSees) | 7 | 60 à 79 | 261 | ×3,8 |
| **Calcul Fem3D** | 72 à 74 | 4 638 à 4 660 | **117 301** | **×25** |
| **Calcul OpenSees** (processus complet) | 144 à 147 | 1 432 à 1 433 | **18 252** | **×13** |

| Mémoire privée | 1 125 barres | 8 330 barres | 29 200 barres |
|---|---:|---:|---:|
| Modèle | 0,5 à 0,8 Mio | 5,0 à 6,0 Mio | 20,6 Mio |
| Un instantané d'historique | 2,1 à 2,6 Mio | 16,0 à 17,8 Mio | 57,7 Mio |
| Fichier `.tsa` | 313 Kio | 771 Kio | 2 024 Kio |

| Suite d'essais | Durée mesurée |
|---|---:|
| 57 suites | 3 min 14 s |
| `demo` | 51,7 s |
| `views` (rendu OCCT) | 33,6 s |
| `bim` | 13,4 s |
| Banc officiel, 146 vérifications | 7 s |
| Fuzzing, par fichier | ≈ 0,11 à 0,18 s |

## Décomposition de Fem3D

| Étape | 8 330 barres | 29 200 barres | Part à 29 200 |
|---|---:|---:|---:|
| Conversion du modèle | 41 à 50 ms | 847 ms | < 1 % |
| **Assemblage et factorisation** | **4,0 à 4,1 s** | **81 à 98 s** | **environ 90 %** |
| Résolution et efforts, sans poids propre | 0,11 s | 0,75 s | < 1 % |
| Résolution et efforts, avec poids propre | 0,42 s | 4,85 s | ≈ 5 % |
| Remappage des résultats | 0,08 à 0,11 s | ≈ 0,4 à 0,5 s | < 1 % |

La conversion passe de 41-50 ms à 847 ms pour 3,5 fois plus de barres. C'est le coût quadratique d'`addElement`, visible mais secondaire. L'écart de 0,75 s à 4,85 s selon le poids propre vient de `fromLoadCase`, qui parcourt toutes les charges pour chaque barre.

**Sans cette décomposition, l'optimisation évidente aurait visé `fromLoadCase`, soit environ 3,5 % du temps.** La vraie cible est `SimplicialLDLT` : séquentiel, sans supernœuds ni BLAS.

## Goulots réels, par ordre d'intérêt

| Rang | Goulot | Levier | Gain | Nature du gain |
|---|---|---|---|---|
| 1 | Factorisation Fem3D | Solveur supernodal (CHOLMOD, Pardiso) ou numérotation METIS | Probablement ×5 à ×20 | **Estimation**, à mesurer |
| 2 | Boucle `fromLoadCase` | Index charges par élément | ≈ 4 s à 29 200 barres | Déduit de la mesure |
| 3 | `addElement` quadratique | Ensemble d'identifiants | ≈ 0,8 s à 29 200 barres | Déduit de la mesure |
| 4 | Instantané complet par édition | Copie à la demande ou journal d'opérations | Édition ×10 ou plus | **Estimation** |
| 5 | Préparation par captures répétées | Capture unique partagée entre validation et moteur | Environ ×2 sur la préparation | **Estimation** |

## Analyse qualitative

- **Complexité** : modèle, fichiers et historique sont linéaires dans les tailles mesurées, avec un surcoût sous-quadratique des `std::map`.
- **Structures de données** : `std::map<int, T>` partout, avec des objets de plusieurs centaines d'octets (environ 1,5 Kio par objet d'après la mesure de l'instantané). Défavorable au cache, mais sans impact visible à ces tailles.
- **Copies** : `createSnapshot` copie tout, chaque élément embarquant sa `Section` et son `Material`. C'est la cause de 57,7 Mio par instantané. Un partage de ces valeurs réduirait nettement la mémoire.
- **Budget de l'historique** : 50 niveaux à 57,7 Mio feraient 2,9 Gio. Le budget de 512 Mio limite cela, sur une estimation sous-évaluée (BUG-163).
- **Parallélisme** : le modèle est mono-thread. Le calcul travaille sur un instantané immuable, ce qui est sain. **Ne pas paralléliser le modèle ni les observateurs** : ils ne sont pas prévus pour cela. Les seuls candidats sûrs sont la factorisation (dans le solveur) et la boucle d'efforts de Fem3D, qui n'écrit que dans des résultats locaux.
- **OpenSees** : `waitForFinished(100)` en boucle, sans délai maximal (BUG-143 déjà connu).

## Non mesuré

| Sujet | Raison |
|---|---|
| Démarrage de `TSA.exe`, ouverture dans l'interface | Aurait ouvert des fenêtres sur le poste |
| Fréquence d'affichage 2D et 3D, utilisation GPU | Idem |
| Sélection, mise à jour des vues | Idem |
| Maillage Gmsh en grand | Non mesuré ici |
| Taille de 63 000 barres | Mesure arrêtée, Fem3D estimé à plus de 15 min, sans apport pour le classement |
| Consommation CPU par fonction | Aucun profileur disponible : la décomposition vient de chronomètres |
