# Torsion dans TSA — modèle, charges, appuis, validation et limites

Ce document fait le point sur l'état de la torsion dans TSA. Il distingue ce qui est calculé et vérifié de ce qui
ne l'est pas. **TSA calcule la torsion uniforme de Saint-Venant.** Le gauchissement (torsion non uniforme de
Vlasov) n'est pas calculé : voir §6. Avant tout usage d'ingénierie sur des profils ouverts dont le gauchissement est
empêché, lisez les §6 et §7.

## 1. Modèle mécanique

- **Élément** : `elasticBeamColumn` d'OpenSees (ElasticBeam3d), 6 DDL par nœud, rigidité de torsion G·J avec
  G = E / 2(1 + ν) et J = `Section::it()`. La torsion est découplée de la flexion : le centre de cisaillement est
  confondu avec le centre de gravité.
- **Repères** : le moment nodal est donné en repère global. Le moment réparti est donné en repère local (x = axe
  de la barre, de i vers j) ou global. Les rotations sont lues en global sur les nœuds et en local le long des
  barres (`StationForces::rx`).
- **Signe** : règle de la main droite. Un couple positif autour de +x donne θx > 0. Le moment intérieur suit la
  convention de la face positive : console encastrée en x = 0 sous couple réparti m, T(x) = m(L − x).
- **Unités** : kN, m, kNm, kNm/m, kN/m² (E, G), m⁴ (J), m⁶ (Iw), rad.

## 2. Constante de torsion J (`Section::it()`)

| Section | Formule | Vérification |
| :--- | :--- | :--- |
| IPE, HEA, HEB normalisés (désignation et dimensions EN 10365) | El Darwish & Johnston, avec congés de rayon r (EN 10365) | 65 profils des fiches British Steel 21.07.26 : écart ≤ 0,36 % ; 20 profils de TSA de même rayon : ≤ 0,5 % (test 332) |
| Profil en I modifié ou reconstitué soudé | parois minces (2·b·tf³ + (h − tf)·tw³)/3 | exact sans congé ; sous-estime J de 12 à 29 % pour un profil laminé |
| UPN | parois minces, épaisseur d'aile moyenne | **non vérifié** : ailes inclinées et congés ignorés, aucune référence publiée exploitée |
| T, cornière | parois minces | **non vérifié** |
| Rectangle plein | a·b³·[1/3 − 0,21·(b/a)(1 − (b/a)⁴/12)] | ≈ 0,229·a·b³ pour a/b = 2 (test 321) |
| Rond plein, tube rond | π·d⁴/32, π(D⁴ − d⁴)/32 | exact (test 322) |
| Tube rectangulaire | Bredt (ligne moyenne) + parois minces = formule EN 10219-2 avec R = 0 | angles arrondis non pris en compte : TSA ne connaît pas le rayon (profil formé à froid ou à chaud) |

Le rayon de congé n'est **pas stocké** dans le fichier `.tsa`, qui utilise un enregistrement binaire fixe des
sections. Il est déduit à chaque calcul de la désignation et des dimensions. J est donc identique avant et
après réouverture (test 332). Une section dont les dimensions ont été modifiées revient aux parois minces.
Des fournisseurs laminent parfois avec un rayon différent de l'EN : par exemple, British Steel lamine le
HE 200 B avec r = 20 mm au lieu de 18 mm. Le J du catalogue d'un fournisseur peut donc différer de quelques
pour cent de celui de TSA.

**Constante de gauchissement** `Section::iw()` :
- profil en I bisymétrique : tf·b³·(h − tf)²/24, écart ≤ 0,57 % aux fiches British Steel ;
- sections pleines et fermées : 0, négligeable ;
- UPN, T, cornière : NaN, non calculée.

## 3. Charges de torsion

| Charge | Transmission | Vérification |
| :--- | :--- | :--- |
| Couple nodal (« Force et couple ») | `load` OpenSees, repère global | θ = TL/GJ (tests 321, 326) |
| Moment réparti sur poutre ou poteau (`MemberMoment`, kNm/m, de m1 à m2 sur [x1, x2]) | forces nodales équivalentes exactes (torsion linéaire, flexion par Hermite) et reconstruction de T(x) et θ(x) | uniforme, linéaire, partiel, bi-encastré, flexion répartie (test 327) ; import TSA3D (test 328) ; barre inclinée (test 329) ; saisie réelle dans la boîte (essai d'interface) |
| Moment réparti sur treillis ou câble | **non transmis** : pas de rigidité en rotation | avertissement avant calcul (tests 334, essai d'interface) |
| Moment réparti sans axe (« gravité ») | **refusé** | message de la boîte ; erreur avant calcul (test 334) |
| Couple concentré en travée | pas de type de charge : placer un nœud et un couple nodal | — |

## 4. Appuis et mécanismes

Les rotations d'un appui articulé ou simple sont libres. En 3D, une barre portée par deux rotules tourne donc
librement autour de son axe. Le réglage d'analyse **« Appuis articulés / simples »** (clé `pinnedSupports`) laisse
le choix à l'utilisateur :

| Mode | Blocages ajoutés | Usage |
| :--- | :--- | :--- |
| Appui à fourche (`fork`, nouveaux projets) | rotation autour de l'axe de la barre si elle est parallèle à un axe global ; structures planes verticales : rotation hors plan, et translation hors plan des appuis simples | hypothèse courante des appuis de poutres |
| Historique (`legacy`, projets enregistrés avant ce réglage) | RX globale, TY des appuis simples | reproduit les calculs antérieurs ; **avertissement** si une barre n'est pas parallèle à X : flexion encastrée et torsion libre |
| Aucun (`none`) | aucun | rotules sphériques réelles ; mécanismes signalés |

Chaque blocage est listé avant le calcul (DDL, nœud, raison). S'il reprend un effort, c'est signalé après le
calcul. Un tel blocage est un appui implicite, à définir explicitement s'il est voulu.

Mécanismes :
- **barres alignées non retenues en torsion** : détectées avant le calcul, en erreur, avec correction proposée ;
- **matrice singulière** signalée par OpenSees : message explicite ;
- **mécanisme quasi singulier**, avec des déplacements plus de 10 fois supérieurs à l'envergure ou une
  rotation de plus de 10 rad : refusé.

Il n'y a pas d'analyse générale du rang de la matrice : un mécanisme d'une autre nature n'est signalé qu'au
calcul (tests 330, 333).

## 5. Résultats et vue 3D

- **Synthèse** : Mx et θx maximaux, affichés dans le panneau des résultats, la boîte de fin de calcul et la
  barre d'état.
- **Vue 3D** :
  - les sections tournent de s·θx(x), s étant le facteur d'affichage. Les valeurs ne sont jamais modifiées ;
  - repères orange : contours et génératrice ;
  - option « Rotation des sections (torsion) » ;
  - échelle automatique limitée à environ 30° de rotation affichée ;
  - plaques, coques et volumes : non transmis au solveur, aucune rotation attribuée.
- **Réouverture d'un projet** : les résultats ne sont pas restaurés. Le panneau indique « Aucun résultat
  disponible » et il faut relancer le calcul (F5). Les réglages d'analyse (cas, mode des appuis) sont, eux,
  enregistrés. Vérifié dans l'application (essais d'interface).

## 6. Gauchissement : étude de faisabilité (non implémenté)

**Pourquoi il compte.** Exemple : console IPE 300, gauchissement empêché à l'encastrement et libre au bout libre.
Données : GJ = 16,09 kNm², EIw = 26,44 kNm⁴, k = √(GJ/EIw) = 0,780 m⁻¹. Comparaison de θ(L) = T/GJ·[L − tanh(kL)/k]
(Vlasov) à TL/GJ (Saint-Venant) :

| L | θ Vlasov / θ Saint-Venant | Bimoment à l'encastrement B(0) = T·tanh(kL)/k |
| :--- | :--- | :--- |
| 1 m | 0,16 | 0,84 m × T |
| 3 m | 0,58 | 1,26 m × T |
| 6 m | 0,79 | 1,28 m × T |

Saint-Venant seul **surestime la rotation** d'un profil ouvert court dont le gauchissement est empêché. Il
**ignore aussi les contraintes normales de gauchissement** σw = B·ω/Iw, qui peuvent dimensionner le profil.

**Solveur livré (OpenSees 3.8.0) : vérifié, pas d'élément 3D à gauchissement.** Une sonde Tcl exécutée sur le
binaire de `thirdparty/OpenSees` donne :
- `model BasicBuilder -ndm 3 -ndf 7` est accepté, mais OpenSees répond « NDM = 3 and NDF = 7 is incompatible with
  available frame elements » ;
- `dispBeamColumnWarping`, `DispBeamColumn3dWarping`, `elasticBeamColumnWarping`, `ElasticWarpingShearBeam3d` :
  « element type not known » ;
- `dispBeamColumnAsym` et `mixedBeamColumnAsym` refusent 7 DDL ;
- seuls existent `forceBeamColumnWarping` et `elasticForceBeamColumnWarping` **en 2D** (`-ndm 2 -ndf 4`), avec les
  sections `ElasticWarpingShear` et `NDFiberWarping` (2D). C'est une formulation de **gauchissement par
  cisaillement dans le plan**, pas la torsion non uniforme : inutilisable ici.

**Architecture proposée** (aucune simulation graphique du gauchissement) :
1. **Formulation** : poutre à parois minces de Vlasov, 7 DDL par nœud (θ' en plus). Pour une section
   bisymétrique, la torsion est découplée et la matrice exacte 4 × 4 de torsion non uniforme (fonctions
   hyperboliques) est connue. Elle s'ajoute à la matrice 12 × 12 de flexion et d'effort normal. Pour une section
   non symétrique (UPN), la formulation est couplée (centre de cisaillement) : hors périmètre initial.
2. **Solveur** : pas d'OpenSees livré. Deux voies :
   - un moteur spécialisé TSA, branché sur l'interface `AnalysisEngine` comme Custom2D, limité à l'élastique
     linéaire 3D ;
   - une autre version d'OpenSees, à vérifier par la même sonde avant toute intégration. Aucune disponibilité
     n'est supposée.
3. **Données** : `Section::iw()` existe déjà, pour les profils en I. Il faudrait aussi le centre de cisaillement
   et la constante ω des sections non symétriques.
4. **Conditions aux limites** : un DDL de gauchissement par nœud. Il est libre ou empêché, au choix de
   l'utilisateur, à chaque appui et à chaque extrémité de barre. Pour les assemblages, la continuité du
   gauchissement entre barres alignées est une hypothèse à choisir explicitement.
5. **Résultats** : θ(x), θ'(x), bimoment B(x) = −EIw·θ'', torsion de Saint-Venant et torsion de gauchissement,
   σw. Le bimoment s'ajoute à `StationForces` et devient un nouveau type de diagramme.
6. **Vue 3D** : la rotation θ(x) passe par `DeformedAxis::twist`, déjà en place. Le gauchissement lui-même
   (déplacements axiaux ω·θ') n'est montré que s'il est calculé.
7. **Tests de référence** :
   - console avec gauchissement empêché : θ(L) et B(0) ci-dessus ;
   - poutre bi-encastrée en gauchissement sous couple central ;
   - cas limite EIw → 0, qui retrouve Saint-Venant ;
   - comparaison à un logiciel validé.

## 7. Charges et éléments non transmis au solveur (inventaire)

| Charge ou élément | Où il peut exister | Représentation | Solveur | Comportement actuel | Correction requise |
| :--- | :--- | :--- | :--- | :--- | :--- |
| Moment réparti sur treillis ou câble | boîte, TSA3D | `MemberLoad` `MemberMoment` | non (pas de rigidité en rotation) | **avertissement** avant calcul | aucune possible : charger un nœud de poutre |
| Moment réparti, moteur 2D (Custom2D) | toutes sources | idem | non | **avertissement** « non transmises au solveur 2D » | composante dans le plan transmissible (non fait) |
| Composantes hors plan (moteur 2D) | toutes | charges nodales et sur barres | non | **avertissement** « hors du plan ignorées » | calcul 3D |
| Charge surfacique TSA3D | import TSA3D | conservée dans les données d'échange | non (aucun élément surfacique calculé) | **avertissement** avant calcul (test 334) et à l'import | maillage de coques transmis au solveur (non fait) |
| Dalles, voiles (rigidité et charges) | modèle | `Slab`, `Wall` | non | **avertissement** avant calcul | idem |
| Poids propre explicite (`SelfWeight` sur barre) | fichier seulement | `MemberLoad` | OpenSees : charge uniforme q1 vers le bas ; 2D : ignoré | **avertissement** (test 334) | à unifier ou à supprimer du format |
| Relâchement N, V ou T en bout de barre | modèle | `EndRelease` | non (seules My et Mz sont transmises) | **avertissement** avant calcul | élément avec relâchements complets |
| Charge nodale « en repère local » | boîte, TSA3D | `NodalLoad` | appliquée selon les axes globaux | **avertissement** | repère d'appui local (non fait) |
| Charge sur barre hors portée ou cible ambiguë | modèle | `MemberLoad` | non | **avertissement** | — |
| Plaques, coques, volumes du maillage Gmsh | maillage | `FeMesh` | non (barres seulement) | **refus ou avertissement** de compatibilité (docs/MESH.md) | adaptateur de coques et volumes |

Aucune de ces charges n'est ignorée en silence. Les avertissements sont présentés avant le calcul : l'utilisateur
doit confirmer pour lancer.

## 8. Validation

- **Tests automatisés** : suite `torsion`, tests 321 à 334, exécutés en Debug et en Release (334 / 334 au
  total). Écarts aux solutions analytiques ≤ 10⁻⁹. Flèches en travée ≤ 10⁻⁴ (intégration numérique de la
  courbure).
- **Essais de l'interface** : `tools/ui_tests/` (README) sur l'application compilée. Ils couvrent la case de
  torsion, le facteur, le changement de cas, le redimensionnement, la réouverture, la saisie du moment réparti,
  les refus, et la taille de la vue selon les panneaux.
- **Non validé pour l'ingénierie** :
  - profils ouverts dont le gauchissement est empêché (§6) ;
  - J des UPN, T et cornières (§2) ;
  - mécanismes autres que la torsion de barres alignées, détectés seulement au calcul.
