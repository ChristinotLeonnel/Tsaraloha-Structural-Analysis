# Journal d'audit (notes de travail)

## Contexte
- HEAD 5403ed6 feature/diagram-multi-curves ; 2 .tsa d'exemple modifiés (30/10 15:55), non commités. Aucun autre processus TSA.
- Binaires build-windows-validation (Release, 31/10 04:09) plus récents que toutes les sources.
- Outils : MSVC 14.51 (vcvars), clang-tidy VS (VC/Tools/Llvm/x64/bin), pas de WSL distro, pas de gcc, python 3.15 + numpy (pas scipy/sympy).
- Pas de sous-agents (consigne outil) : audit fait directement.

## Architecture
- 179 570 lignes src/, 763 fichiers. 28/36 modules forment une SCC au niveau répertoire (notes/map.txt).
- check_layers : 0 violation mais seulement 4 couches et includes guillemets.

## Fem3D lu intégralement (éléments, condensation, assemblage, solveur, repères, sections, réponse, modèle)
- EB/Timoshenko/transformation/réactions/conventions : corrects à la lecture, rederivés à la main.
- Points à vérifier : (1) LDLT non protégé dans la condensation (mais adaptateur refuse relâchements N,V,T) ;
  (2) Fem3DError depuis convert() hors try (addNode non fini) ; (3) Uniform partiel ; (4) moteur toujours EB, Timoshenko inatteignable
  (5) fromLoadCase O(charges) par élément => O(E·L), 3 fois par cas ; (6) addElement O(n) => O(n²) ; addLoadCase...

## CONFIRMÉ (bench-hinge) : OpenSees + rotule + charge Linear/Couple/Point => FAUX silencieux
- HINGE-02 point : M_j 13,33 au lieu de 26,67 (-50 %), M rotule 26,67 au lieu de 0. HINGE-03 : M_j 36 au lieu de 54, R_i 36 au lieu de 27.
- GERBER-02 : M_clamp 162 au lieu de 108, travée 37,44 au lieu de 54. Fem3D + Custom2D corrects. Uniforme (beamUniform) correct.
- Cause : OpenSeesAnalysisBuilder.cpp:824-842 + OpenSeesResultsReader.cpp:380-390 (FEF bi-encastré), et beamPoint d'OpenSees 3.8.0 sans gestion du release.

## Validation différentielle (frame3d.py indépendant, autovalidé sur 8 solutions fermées)
- 15 portiques × 2 moteurs, 12 678 comparaisons : A (sans rotule) et B (rotule + uniforme) : écart rel max ~1e-10 OpenSees et Fem3D.
- C (rotule + ponctuelle/trapèze) : OpenSees 0/5, 1770 échecs, pire member:6:M:end 24,35 vs 1,23 (×19,8). Fem3D 5/5.
- Aucun cas de tests/benchmarks n'a de release. Équilibre global passe (forces nodales statiquement équivalentes) : garde-fou aveugle.

## Numérique suite
- selfWeightFactor : lu par Custom2D (Custom2DAdapter:234), NDC (NDCProjectNote:548,809), IA ; PAS par OpenSeesAnalysisBuilder / Fem3D. À confirmer par essai.
- En mode cas unique : flag du cas ignoré, params.includeSelfWeight (défaut true) ; en combinaison : flag du cas, params ignoré (loadPatterns).
- iyz() jamais utilisé hors Section : cornières calculées sur axes géométriques.
- EC3 : alpha, tableau 6.2, classes OK. EC2 : fctm, lambda, eta, epsCu3, Asmin, aciers comprimés OK. Pas de déversement ni interaction M-N (vérifier NDC). fy indépendant de l'épaisseur.
- Gravité : direction Gravity prend -|q| (signe ignoré).

## Harnais numérique (harness-run/numerics.txt)
- selfWeightFactor=1.5 ignoré par OpenSees, Fem3D, Custom2D (ΣRz=3.315 = 1×wL). Cas seul : drapeau du cas ignoré, réglage décide ; combinaison : réglage ignoré, drapeau décide (mêmes 3 moteurs).
- Câble comprimé : N=-20,2 kN, 0 avertissement, succès.
- Cornière L100x100x10 : TSA uz=-23,8 mm, exact flexion déviée -36,7 mm, uy exact ±21,7 mm ignoré. Iyz=-107 cm4 ignoré.
- NDCGenerator Legacy (défaut) axe fort : IPE 200 L=5 m : 809 kN au lieu de 105 (×7,68).
- NaN + Fem3D : Fem3DError échappe à AnalysisManager::prepare (non contenue). OpenSees refuse proprement.

## bench-more
- PIN2-trap (deux rotules) OpenSees : M travée 36 au lieu de 54 (-33 %), M extrémités 36 au lieu de 0. PIN2-point : 26,67 au lieu de 40. Témoin uniforme OK. Fem3D/Custom2D OK.
- PDELTA-LIN : geomTransf PDelta + statique linéaire : ux 1,505 mm (1er ordre) au lieu de 2,362 ; M 40 au lieu de 58,9 ; Rx -9,778 au lieu de -10 (équilibre faux). Aucun refus ni avertissement. Traçabilité : "OpenSees (version non lue)" pour ce 1er cas.
- Subdivision 3 : RND-A*-sub3 OK.

## Fuzz .tsa (scripts/fuzz_tsa.py) : 3000 mutations (2 semences), CRC recalculé, cibles tsa-ifc export + tsa-mesh : 0 plantage, 0 délai. ~60 % acceptés (code 0) : pas de contrôle référentiel. GUI (OccView) et analyse non couverts par ce fuzz.

## Perf (perf-api.txt, perffem.txt)
486n/1125b : enr 92ms, ouv 17, édit 3.9, annul 3.8, Fem3D 72, OS 145. 3179n/8330b : enr 450, ouv 160, édit 40, annul 43, Fem3D 4650, OS 1432. 10625n/29200b : enr 1927, ouv 499, édit 133, annul 128, Fem3D 117301, OS 18252.
Fem3D 29200b : conversion 0,85 s ; assemblage+factorisation 81-98 s ; cas 0,75 s (sans PP) / 4,8 s (avec PP : fromLoadCase O(E²)) ; remappage 0,4 s.
saveToFile sans QGuiApplication : 0xC0000409 (TSAPreviewGenerator police) reproduit.
OpenSees code retour 0 sur erreur Tcl ; TSA_OPS_SUCCESS jamais lu ; tout vide intercepté.
Pas d'autosave ; CrashHandler minidump seul ; pas de QApplication::notify. SecretStore Linux en clair (isProtected false).

## Non linéaire (cases-nl) : défaut réglages (numSteps 10, stepSize 0,05)
NLIN-uniforme : M mi-portée 81 (réf 54), absmax 107,73 ; NLIN-trapeze : 45 (réf 54). Témoin linéaire OK. Réactions OK. Cause : OpenSeesResultsReader.cpp:551 loadStep=stepSize>0?stepSize:1/numSteps alors que builder:905 Δλ=1/numSteps.
Banc officiel : 146/146 en 7 s (bench-official).

## INCIDENT 14:12-14:16 (à déclarer dans le rapport)
- J'ai lancé 3DDesplacement/build/fem3d_opensees_benchmarks.exe SANS lire son code. Effets : fenêtres visibles (std::system, opensees_benchmarks.cpp:620), 41 *_opensees.txt suivis supprimés (fs::remove :618), synthese.md/.csv remplacés par une synthèse vide, 26 journaux .log ignorés remis à 0 octet (non restaurables).
- Utilisateur : "il ouvre plusieurs fenêtres opensees". Arrêt immédiat (TaskStop, processus OpenSees orphelin 3612 tué).
- Utilisateur : "je viens de remplacer les fichiers de opensees" : thirdparty/OpenSees recréé à 14:15 ; 851/851 fichiers identiques à engines/OpenSees ; 3.8.0 OK (essai masqué).
- Réparation : 43 chemins restaurés depuis l'index (--pathspec-from-file), sauvegarde préalable notes/synthese.*, statut avant dans notes/3dd-status-avant-restauration.txt.
- "cas en écart : 0" = tout NON EXÉCUTÉ : ne vaut pas validation. fem3d_tests 49/49 valable. TSALab_TestSuite NON exécuté. fem3dpost_tests délai 300 s, sortie vide : non concluant. tsalab_science_tests : arrêt à S6 (5 passés avant), OpenSees figé 120 s, code 259, orphelin : non concluant ; défaut : pas de TerminateProcess après délai, 259 présenté comme code de sortie.
- Pas de mesure GUI TSA.exe : aucune fenêtre ne doit plus s'ouvrir sur le bureau de l'utilisateur.
- TSA.code-workspace modifié pendant l'audit par l'utilisateur (ajout ../3DDesplacement), pas par moi.

## Fuzz voie d'analyse (harnais fuzzload, Fem3D, jamais OpenSees) : 1200 mutations, 23 plantages (15 AV + 8 FAILFAST)
Tri (notes/triage.txt) : 15 ACCESS_VIOLATION dans ModelValidator (références pendantes : lecteur sans contrôle) ; 4 Standard_ConstructionError gp_Vec::Normalize dans prepare ; 4 Fem3DError coordonnées non finies dans prepare.
Unités N : 583 écarts tous ×1000 (banc compare N à kN) : calcul correct.
Exemples : 0 rotule dans 9 projets. Modifs .tsa : 1.10→2.0, SECP+SETT ajoutés, THMB réduit, modèle inchangé.
CI : OpenSees absent, actions non épinglées, Qt 6.10.3 + MSVC 19.44.
