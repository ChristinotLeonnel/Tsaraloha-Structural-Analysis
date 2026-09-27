# TSALib — Bibliothèque Officielle d'Ingénierie TSA

Bibliothèque officielle d'ingénierie structurale et d'éléments de construction pour **TSA (Tsaraloha Structural Analysis)**.

---

## 📋 Présentation

TSALib fournit l'ensemble des définitions certifiées de matériaux, profilés métalliques, sections de béton armé, câbles de précontrainte, haubans et textures physiques PBR conformes aux normes européennes (**Eurocodes EN 1990 à EN 1993**) et internationales (**ASTM A416**).

Toutes les définitions sont externalisées au format standard JSON et ne nécessitent aucune recompilation de l'application TSA.

---

## 📁 Architecture des Répertoires

```text
Extensions/TSALib/
├── manifest.json              # Métadonnées officielles du package d'extension (v1.0.0)
├── Materials/                 # 16 Fiches matériaux Eurocodes certifiées
│   ├── concrete_c20_25.json ... concrete_c50_60.json (Bétons EC2)
│   ├── steel_s235.json ... steel_s460.json (Aciers de construction EC3)
│   ├── timber_c24.json, timber_d40.json (Bois résineux et feuillus EC5)
│   ├── rebar_b500b.json (Armatures pour béton armé)
│   └── aluminum_6061_t6.json, masonry_m10.json, soil_clay.json, etc.
├── Profiles/                  # 16 Profilés métalliques normalisés Eurocodes
│   ├── ipe100.json ... ipe300.json (Poutrelles IPE)
│   ├── hea100.json ... hea200.json (Poutrelles HEA)
│   ├── heb100.json ... heb200.json (Poutrelles HEB)
│   ├── upn100.json ... upn200.json (Fers U normaux)
│   └── angle_l100x10.json, t_100x10.json (Cornières & Tés)
├── Sections/                  # 6 Sections paramétriques pour poteaux, poutres et voiles
│   ├── rect_300x500.json, rect_400x400.json (Rectangulaires)
│   ├── circ_d300.json, circ_d400.json (Circulaires)
│   └── box_200x200x8.json, pipe_d219x6.json (Caissons & Tubes)
├── Cables/                    # 19 Câbles, torons et haubans certifiés
│   ├── en10138_y1860s7_15_7.json ... (Torons 7 fils pour précontrainte)
│   ├── en10138_bar_y1030_32.json ... (Barres de post-tension filetées)
│   ├── en1993_flc_80.json, en1993_flc_120.json (Câbles clos porteurs EC3)
│   ├── en1993_hanger_30.json (Suspentes de ponts suspendus)
│   ├── stay_pss_19_15_7.json ... stay_pss_61_15_7.json (Haubans Freyssinet)
│   └── astm_a416_gr270_0_5in.json, astm_a416_gr270_0_6in.json (Torons ASTM)
├── Standards/                 # 8 Fiches de référence normative
│   ├── EN1990.json (Bases de calcul)
│   ├── EN1991.json (Actions sur les structures)
│   ├── EN1992.json (Béton armé & précontraint)
│   ├── EN1993.json (Structures en acier)
│   ├── EN1993_1_11.json (Câbles et éléments tendus)
│   ├── EN10138.json (Aciers de précontrainte)
│   └── ASTM_A416.json (Torons acier sans revêtement)
└── Textures/                  # 14 Textures PBR haute résolution sans raccord (PNG)
    ├── concrete.png, reinforced_concrete.png
    ├── steel.png, galvanized.png, rebar.png
    ├── wood.png, brick.png, masonry.png
    ├── glass.png, aluminum.png, rock.png, sand.png, soil.png, gravel.png
    └── textures.json (Catalogue des métadonnées de textures)
```

---

## 🚀 Utilisation dans TSA

1. **Chargement Automatique** : Au lancement de TSA, TSALib est automatiquement détecté dans le répertoire `Extensions/TSALib`.
2. **Gestionnaire Graphique** : Ouvrez le ruban **Structure & Sections** > **Gestionnaire TSALib...** pour consulter, filtrer et valider les bibliothèques.
3. **Rechargement à Chaud** : Toute modification apportée à ces fichiers est prise en compte immédiatement en cliquant sur **Recharger à chaud** dans le dialogue d'extensions, sans redémarrer TSA.
4. **Distribution (.tsalib)** : Utilisez le bouton **Exporter (.tsalib)...** pour empaqueter la bibliothèque sous forme d'une archive unique protégée par SHA-256.

Consultez le guide complet détaillé dans [docs/TSALIB_SYSTEM.md](../../docs/TSALIB_SYSTEM.md).
