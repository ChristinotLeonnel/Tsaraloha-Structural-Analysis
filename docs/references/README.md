# Bibliothèque de Références Techniques & Scientifiques — TSA

> Ce répertoire rassemble le corpus complet de références documentaires, normatives, scientifiques et techniques constitué pour le développement et la validation du logiciel **Tsaraloha Structural Analysis (TSA)**.

---

## 1. Organisation Thématique de la Bibliothèque

```text
docs/references/
├── BIM_IFC/                  # Standards buildingSMART (IFC4.3, IFC4, IFC2x3), IDS, BCF, domaines d'analyse
├── FEM_Theory/               # Formulation des éléments finis (Euler-Bernoulli, Timoshenko, Vlasov, DKT/Mindlin)
├── Structural_Analysis/      # Méthodes de calcul, solveurs, rigidités et matrices de transformation
├── Eurocodes/                # Normes européennes de conception (EN 1990 à EN 1999) et guides JRC
├── Reinforced_Concrete/      # Béton armé et précontraint, bielles et tirants, états limites
├── Steel_Structures/         # Structures métalliques, classification de sections, instabilités
├── Geotechnics_Foundations/  # Mécanique des sols, semelles, radiers, pieux, tassements (EC7)
├── Bridges_Infrastructure/   # Typologies de ponts (VIP, ponts-dalles, mixtes, haubans) et charges LM1/LM2
├── Hydraulic_Structures/     # Dalots cadres fermés, portiques ouverts, murs de soutènement (SETRA)
├── Loads_Dynamics_Seismic/   # Actions climatiques (vent, neige), dynamique des structures, spectres
├── Geometry_Meshing/         # Noyau CAO OpenCASCADE (B-Rep) et générateur de maillage Gmsh SDK
├── OpenSource_Libraries/     # Licences et gouvernance des dépendances (LGPL, BSD, GPL, MIT, MPL)
├── Validation_Benchmarks/    # Cas tests de référence vérifiables et reproductibles avec solutions exactes
├── Madagascar_Regulations/   # Données géophysiques, IOGA (sismicité), BNGRC, vents cycloniques et LNTPB
│
├── CATALOGUE_REFERENCES.md   # [Livrable 1] Catalogue général détaillé de l'ensemble des références
├── PRIORITES_TSA.md          # [Livrable 2] Feuille de route d'implémentation par composant logiciel
├── VALIDATION_BENCHMARKS.md  # [Livrable 3] Définition complète des cas tests de référence analytiques & FEA
└── RAPPORT_RECHERCHE.md      # [Livrable 4] Rapport de synthèse de la recherche documentaire
```

---

## 2. Principes Directeurs de Constitution

1. **Sources Officielles Primaires :** Priorité absolue accordée aux organismes de normalisation (CEN, ISO, buildingSMART), aux instituts scientifiques publics (JRC, Cerema, SETRA, PEER) et aux universités reconnues.
2. **Légalité et Propriété Intellectuelle :** Respect strict des droits d'auteur ; les normes payantes font l'objet d'un recensement méticuleux de leurs métadonnées officielles sans téléchargement illégal. Les publications ouvertes et spécifications libres sont intégrées directement.
3. **Traçabilité pour le Code :** Chaque formule, matrice ou hypothèse utilisée dans `src/` doit être rattachable à une référence explicite présente dans ce corpus.
