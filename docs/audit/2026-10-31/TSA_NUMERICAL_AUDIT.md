# Audit numérique et du moteur de calcul

## Conclusion

Le socle de calcul statique linéaire est **excellent** : sur des portiques 3D aléatoires sans rotule ou avec rotules sous charge uniforme, OpenSees et Fem3D concordent avec un solveur indépendant à environ **1e-10 près**.

Il existe en revanche **deux défauts critiques** dans la chaîne OpenSees, qui est le moteur par défaut : les rotules combinées à une charge non uniforme (BUG-148) et les efforts le long des barres en statique non linéaire (BUG-149). Tous deux passent le contrôle d'équilibre et le banc officiel.

Ces résultats ne remplacent pas une qualification réglementaire du logiciel.

## Méthode

1. **Lecture ligne à ligne** de Fem3D (éléments, condensation, assemblage, solveur, repères, sections, réponse), du générateur et du lecteur OpenSees, de `LoadResolver`, `ElementTransformation`, `SteelDesignEC3`, `ConcreteDesignEC2`. Conventions de signe et relations efforts-déformations rederivées à la main.
2. **Solveur indépendant** `preuves/scripts/frame3d.py`, écrit depuis la théorie (Euler-Bernoulli, rigidité directe, condensation, Gauss à 8 points). Seule la définition du repère local vient de TSA. Il a d'abord été validé sur 8 solutions fermées (`preuves/resultats/autovalidation_frame3d.txt`).
3. **Comparaison différentielle** par la chaîne réelle : modèle TSA → `AnalysisManager` → moteur → `ResultsModel`, avec `TSA_Benchmark.exe`.
4. **Cas analytiques ciblés** là où la lecture du code signalait un risque.

## Résultats de la comparaison différentielle

15 portiques 3D aléatoires à graine fixe, 2 moteurs, **12 678 comparaisons** (déplacements, rotations, réactions, efforts d'extrémité). Les barres sont inclinées, les sections tournées (β), avec ressorts, appuis articulés et charges nodales, uniformes, trapézoïdales, partielles et ponctuelles, en repère global ou local.

| Famille | Contenu | OpenSees | Fem3D | Écart relatif maximal |
|---|---|---|---|---|
| A | Sans rotule | 5/5 | 5/5 | 6,8e-11 |
| B | Rotules, charge uniforme sur ces barres | 5/5 | 5/5 | 1,2e-10 |
| C | Rotules, charges ponctuelles et trapézoïdales | **0/5** | 5/5 | OpenSees ×19,8 ; Fem3D 4,1e-11 |

| Essai complémentaire | Résultat |
|---|---|
| Famille A avec barres subdivisées (3 sous-éléments) | 1 152 vérifications réussies sur 3 portiques |
| Calcul OpenSees en N au lieu de kN | Correct : les 583 écarts du banc valent exactement ×1000, le banc comparant des N à une référence en kN |
| Banc officiel `tests/benchmarks` | 146/146 en 7 s, **mais aucun cas n'a de rotule, ni de charge sur barre en non linéaire** |
| Tests propres de Fem3D (`fem3d_tests`) | 49/49 |

## Vérification par thème

| Thème | Verdict | Preuve ou limite |
|---|---|---|
| Matrices élémentaires EB et Timoshenko | Correct | Relues terme à terme. Φ = 12EI/(GA_s L²) sur le bon plan |
| Matrice globale, assemblage | Correct | Familles A et B |
| Degrés de liberté, repères locaux | Correct | Repère d'OpenSees (`geomTransf`) et garde-fou de `Fem3DAdapter.cpp:124-135` |
| Conditions aux limites, ressorts | Correct | Familles A et B. Réaction = K_rf U_f + K_rr U_r − F_r |
| Blocages automatiques d'appuis | Correct et tracé | Choix utilisateur, listé avant calcul, efforts repris signalés |
| Chargements linéaires | Correct | Familles A et B. Gauss à 4 points exact pour les forces équivalentes |
| Chargements + rotules, OpenSees | **Faux** | BUG-148 |
| Transformations local / global | Correct | β, barres inclinées, poteaux quasi verticaux |
| Solveur direct OpenSees | Correct | BandGeneral et RCM par défaut |
| Solveur direct Fem3D | Correct, lent en grand | `SimplicialLDLT`, BUG-158 |
| Détection de mécanisme Fem3D | Correct | Pivot relatif et modes propres. Cas de mécanisme testés dans `fem3d_tests` |
| Détection de mécanisme OpenSees | Correct, indirecte | Texte du journal, garde 10 × envergure ou 10 rad |
| Solveurs itératifs | Sans objet dans TSA | Présents dans `TSALab/science` (SOLVER LAB), non audités |
| Statique non linéaire : déplacements, réactions | Correct | `RDM-11a`, `RDM-11b` |
| Statique non linéaire : efforts le long des barres | **Faux** | BUG-149 |
| Second ordre (P-Delta) en non linéaire | Correct à 0,5 % | `RDM-11b` subdivisé en 20 |
| Second ordre demandé en statique linéaire | **Incohérent** | BUG-150 |
| Modale | Correct sur 2 cas | `FEA-05`, `FEA-06`. Le code existe malgré ADR-022, voir l'écart de documentation |
| Dynamique temporelle | Absente de TSA | ADR-022. `TSALab/science` contient `Dynamics2D`, non audité |
| Coques | Correct sur les cas du banc | `FEA-01` à `FEA-04`, `FEA-07` (Navier, Mindlin). Non confronté au solveur indépendant |
| Câbles | Limité | Élastique des deux signes (BUG-151), corde droite sans module d'Ernst ni flèche |
| Sections fermées, ouvertes bisymétriques | Correct | `Section.cpp`, constantes vérifiées sur catalogue par les tests existants |
| Cornières | **Faux en flexion** | BUG-153 |
| Déformation d'effort tranchant | Négligée | `elasticBeamColumn` et Fem3D via TSA sont en Euler-Bernoulli. Timoshenko existe dans Fem3D mais `Fem3DAdapter.cpp:114` ne l'emploie pas |
| Poids propre | Règles incohérentes | BUG-156 |

## Vérification des formules réglementaires

Comparées à ma connaissance d'EN 1993-1-1 et d'EN 1992-1-1 : le texte officiel n'était pas disponible pendant l'audit. Une relecture sur la norme reste à faire.

| Point | Verdict |
|---|---|
| Facteurs α 0,13 / 0,21 / 0,34 / 0,49 / 0,76 | Correct |
| Tableau 6.2, choix de la courbe | Correct et prudent pour les profils creux |
| χ, Φ, λ̄, Nb,Rd | Correct |
| Limites de classes des parois | Correct |
| fctm, λ, η, εcu3 selon fck | Correct |
| As,min, As,max, section doublement armée | Correct |
| Flambement dans la note Legacy | **Axe fort seul**, BUG-152 |
| fy selon l'épaisseur | Non pris en compte (LOW) : `fk` constant, sensible au-delà de 40 mm |
| Déversement, interaction M-N, ELS, effort tranchant, assemblages | Non vérifiés par TSA. Les notes Simplifiée et Détaillée le disent, pas la note Legacy |
| β de flambement | 1,0 écrit en dur (`NDCGenerator.cpp:874`, `NDCProjectNote.cpp:1293`), faux pour un portique à nœuds déplaçables ou une console |

## Limites de cet audit numérique

- Aucune référence commerciale ni étalon NAFEMS n'a été rejouée.
- `Custom2D` a été vérifié en boîte noire uniquement (rotules, banc officiel). Ses sources `MetDeDeplacement` n'ont pas été relues.
- Le banc croisé Fem3D/OpenSees n'a produit **aucune** comparaison pendant l'audit : sa synthèse « 0 écart » ne vaut pas validation (BUG-161).
- Les moments répartis avec rotule n'ont pas été essayés.
