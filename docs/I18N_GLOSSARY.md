# Glossaire de traduction — français, anglais, malgache

Ce glossaire est la référence pour traduire l'interface de TSA ([I18N.md](I18N.md)) : un même objet porte le même nom
dans toute l'interface, et la recherche globale le retrouve dans les trois langues.

**Statut des termes malgaches.** Ils ont été choisis pour leur sens courant, en privilégiant les mots malgaches
existants (*andry*, *rindrina*, *enta*, *harato*…) plutôt que des emprunts. Ils **n'ont pas encore été relus par un
ingénieur malgachophone**. La colonne « Statut » indique le degré de confiance :

| Statut | Signification |
|---|---|
| **courant** | Mot usuel dont le sens correspond au terme technique. |
| **proposé** | Terme composé ou adapté pour l'ingénierie : à confirmer avec des professionnels. |
| **emprunt** | Emprunt adapté au malgache, faute de terme établi. |

Les symboles, unités, formules, normes, noms d'éléments finis et identifiants ne sont jamais traduits : `N`, `Mz`,
`σ`, `kN`, `kN·m`, `ELU`, `EN 1992`, `ElasticBeam3d`, `IFC`, `Ctrl+S`, `model.create_node`.

## 1. Structure et modélisation

| Français | English | Malagasy | Statut |
|---|---|---|---|
| structure | structure | rafitra | courant |
| nœud | node | fihaonana | proposé |
| barre | member | tsorakazo | courant |
| poutre | beam | andry mitsivalana | proposé |
| poteau | column | andry | courant |
| dalle | slab | takelaka | courant |
| voile (mur) | wall | rindrina | courant |
| fondation, semelle | foundation, footing | fototra | courant |
| treillis | truss | rafitra makarakara | proposé |
| câble, hauban | cable, stay | tariby, tariby fihazonana | courant |
| section (profil) | section | fizarana | proposé |
| matériau | material | akora | courant |
| béton armé | reinforced concrete | betôna misy vy | emprunt |
| acier | steel | vy | courant |
| appui | support | fanohanana | courant |
| encastrement | fixed support | fanohanana raikitra | proposé |
| articulation (rotule) | pinned support (hinge) | fanohanana mihodina (tonona) | proposé |
| appui simple (rouleau) | roller support | fanohanana tsotra (mihodikodina) | proposé |
| niveau, étage | level, storey | rihana | courant |
| grille d'axes | axis grid | makarakaran'ny tsipika fototra | proposé |
| axe de grille | grid axis | tsipika fototra | proposé |
| plan de travail | work plane | planina fiasana | proposé |
| intersection | intersection | fifanapahana | courant |
| origine | origin | fiaviana | courant |
| accrochage (snap) | snap | fipetahana | proposé |
| cotation | dimension | refy | courant |
| modèle | model | modely | emprunt |

## 2. Charges et calcul

| Français | English | Malagasy | Statut |
|---|---|---|---|
| charge | load | enta | courant |
| charge ponctuelle | point load | enta tokana | proposé |
| charge répartie | distributed load | enta miparitaka | proposé |
| charge surfacique | surface load | enta mivelatra | proposé |
| poids propre | self-weight | lanja manokana | proposé |
| cas de charge | load case | tranga enta | proposé |
| combinaison de charges | load combination | fitambaran'enta | proposé |
| maillage | mesh | harato | proposé |
| élément fini | finite element | singa voafetra | proposé |
| calcul, analyse | analysis | kajy | courant |
| calcul linéaire | linear analysis | kajy tsipika | proposé |
| calcul non linéaire | nonlinear analysis | kajy tsy tsipika | proposé |
| solveur | solver | motera kajy | proposé |
| résultats | results | vokatra | courant |
| déplacement | displacement | fifindrana | courant |
| rotation | rotation | fihodinana | courant |
| déformée | deformed shape | endrika miova | proposé |
| déformation | strain | fiovan'endrika | proposé |
| contrainte | stress | tsindry anatiny | proposé |
| effort normal | axial force | hery mahitsy | proposé |
| effort tranchant | shear force | hery manapaka | proposé |
| moment fléchissant | bending moment | momenta fiforetana | proposé |
| torsion | torsion | fiolonana | proposé |
| flèche | deflection | fiondrika | proposé |
| réaction d'appui | support reaction | hery mamalin'ny fanohanana | proposé |
| mode propre | eigenmode | fihetsika voajanahary | proposé |
| fréquence propre | natural frequency | matetika voajanahary | proposé |
| flambement | buckling | fiforetana noho ny tsindry | proposé |
| note de calcul, rapport | calculation report | tatitra momba ny kajy | proposé |
| diagramme | diagram | kisary | courant |

## 3. Interface

| Français | English | Malagasy | Statut |
|---|---|---|---|
| projet | project | tetikasa | courant |
| nouveau | new | vaovao | courant |
| ouvrir | open | sokafy | courant |
| enregistrer | save | tehirizo | courant |
| fermer | close | hidio | courant |
| quitter | exit | hivoaka | courant |
| annuler (défaire) | undo | foano | courant |
| annuler (abandonner) | cancel | aoka ihany | courant |
| rétablir | redo | averina | courant |
| copier, coller, couper | copy, paste, cut | adikao, apetaho, hetezo | courant |
| supprimer | delete | fafao | courant |
| sélection, sélectionner | selection, select | fifantenana, fidio | courant |
| afficher, masquer | show, hide | asehoy, afeno | courant |
| isoler | isolate | atokana | courant |
| déplacer | move | afindrao | courant |
| symétrie (miroir) | mirror | taratra | proposé |
| vue | view | fijery | courant |
| vue de dessus, de face | top view, front view | fijery avy ambony, avy eo anoloana | courant |
| zoom avant, arrière | zoom in, out | hanakaiky, hanalavitra | proposé |
| paramètres | settings | fikirana | courant |
| outils | tools | fitaovana | courant |
| panneau | panel | tontonana | proposé |
| fenêtre | window | varavarankely | courant |
| rechercher | search | hikaroka | courant |
| langue de l'interface | interface language | fitenin'ny fifandraisana | proposé |
| importer, exporter | import, export | ampidiro, avoahy | courant |
| aide | help | fanampiana | courant |
| raccourci clavier | keyboard shortcut | lalana fohy amin'ny fafana | proposé |
| thème sombre, clair | dark, light theme | endrika maizina, mazava | courant |
| bibliothèque (de sections) | library | tahiry | proposé |
| extension | extension | fanitarana | proposé |
| module | module | môdely | emprunt |
