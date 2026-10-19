# Architecture Modulaire du Co-Engineering dans TSA
**Date :** 19 octobre 2026  
**Statut :** Spécification d'Architecture Logicielle (ISO/IEC/IEEE 42010 / IEEE Std 1063)  
**Projet :** Tsaraloha Structural Analysis (TSA)  
**Modules concernés :** `src/AI/`, `src/UI/AI/`, `src/Model/`, `src/Analysis/`, `src/Commands/`, `src/Automation/`

---

## 1. Principes Directeurs Fondamentaux

L'architecture du module de Co-Engineering de TSA est régie par cinq invariants intangibles :

1. **Le Modèle Structural est l'unique source de vérité (`Single Source of Truth`) :**
   Aucune représentation intermédiaire, aucun modèle géométrique OCCT, ni aucun texte généré par l'IA ne peut détenir l'état de la structure. Seul `TSA::Model::Model` fait foi.
2. **Séparation Déterministe / Probabiliste :**
   - **Calculs numériques & matriciels :** Confiés exclusivement aux moteurs déterministes vérifiables : le solveur FEM (OpenSees / solveur natif TSA), les contrôles topologiques (`StructuralChecker`) et les calculs analytiques de RDM.
   - **Raisonnement, Synthèse & Conseils :** Confiés au modèle de langage (explications physiques, détection d'anomalies qualitatives, propositions d'optimisation de profilés, rédaction de rapports).
3. **Contrôle Humain Intégral (`Human-in-the-Loop`) :**
   L'IA ne modifie JAMAIS le modèle actif de manière silencieuse ou autonome. Toute action modifiante prend la forme d'une `ActionProposal` typée, examinée et validée explicitement par l'ingénieur, et s'exécute via le système de commandes transactionnel (`ICommand` + `UndoManager`).
4. **Indépendance Vis-à-Vis des Fournisseurs (`Vendor Neutrality`) :**
   L'architecture repose sur une couche d'abstraction unifiée (`IAIProvider`). Le basculement entre un modèle local (Qwen sous llama-server), un modèle gratuit (Google Gemini 2.0 Flash) ou un modèle à coût ultra-faible (DeepSeek-R1) se fait sans aucune modification du code métier.
5. **Résilience et Tolérance aux Pannes Réseau :**
   Gestion native des coupures internet et des limites de quota grâce à un routeur multi-fournisseurs avec repli automatique (`fallback mechanism`) et un mode hors-ligne complet.

---

## 2. Diagramme d'Architecture Globale

```text
┌────────────────────────────────────────────────────────────────────────────────────────┐
│                                   INTERFACE UTILISATEUR                                │
│   AICoEngineeringDock (Ruban / Dock)  ──►  AIRuntimeDialog (Config & Quotas)           │
│   (Historique, Variantes, Propositions, Cartes d'Impact, Indicateur de Statut)          │
└───────────────────────────────────────────┬────────────────────────────────────────────┘
                                            │ Signals / Slots Qt
                                            ▼
┌────────────────────────────────────────────────────────────────────────────────────────┐
│                              CHEF D'ORCHESTRE CO-ENGINEERING                            │
│                               TSA::AI::AIOrchestrator                                  │
│   ┌────────────────────────┬─────────────────────────┬─────────────────────────────┐   │
│   │ Gestionnaire de Tâches │ Routeur Multi-Fournisseur│ Registre Human-in-the-Loop │   │
│   │ (Chat/Analyze/Check)   │ (Fallback & Retries)    │ (ActionProposal & Undo)     │   │
│   └────────────────────────┴─────────────────────────┴─────────────────────────────┘   │
└────────────┬──────────────────────────────┬─────────────────────────────┬──────────────┘
             │                              │                             │
             ▼                              ▼                             ▼
┌─────────────────────────┐   ┌─────────────────────────┐   ┌────────────────────────────┐
│ CONTEXTE D'INGÉNIERIE   │   │  OUTILS D'INGÉNIERIE    │   │  FOURNISSEURS D'INFÉRENCE  │
│ EngineeringContext      │   │     AIToolRegistry      │   │       IAIProvider          │
├─────────────────────────┤   ├─────────────────────────┤   ├────────────────────────────┤
│ • Nœuds & Connectivité  │   │ • Lecture seule :       │   │ • OpenAICompatibleProvider │
│ • Profilés & Sections   │   │   - get_object()        │   │   (Gemini, DeepSeek,       │
│ • Matériaux (E, nu, rho)│   │   - check_model()       │   │    Claude, OpenAI, Groq)   │
│ • Appuis (6 DDLs)       │   │   - list_members()      │   │ • LocalLlamaServer         │
│ • Charges & Combinaisons│   │ • Propositions :        │   │   (Moteur local Qwen GGUF) │
│ • Résultats OpenSees    │   │   - propose_section()   │   │ • MockAIProvider           │
│ • RAG Normatif (BM25)   │   │   - propose_analysis()  │   │   (Tests & CI sans clé)    │
│ • IFC & Données BIM     │   │ • Contrôleur Déterministe│  │ • AIProviderRouter         │
│ • Unités explicites SI  │   │   (StructuralChecker)   │   │   (Fallback automatique)   │
└─────────────────────────┘   └─────────────┬───────────┘   └────────────────────────────┘
                                            │
                                            ▼ Exécution contrôlée
                              ┌───────────────────────────┐
                              │     MOTEUR DE CALCUL      │
                              │ • OpenSees (Async Worker) │
                              │ • Solveur Déterministe    │
                              │ • CommandManager (Undo)   │
                              └───────────────────────────┘
```

---

## 3. Détail des Sous-Systèmes

### 3.1. Interface Co-Engineering (`src/UI/AI/`)

- **`AICoEngineeringDock` :**
  - Panneau ancrable et rétractable (Raccourci par défaut : `Ctrl+Maj+I`).
  - Zone de discussion interactive avec support du Markdown, des formules et des tableaux.
  - Cartes d'action interactives (`Proposal Cards`) affichant l'impact direct d'une proposition :
    - Avant : Profilé actuel (ex. `IPE 300`, poids, contraintes).
    - Après : Profilé proposé (ex. `IPE 360`, delta de matière, flèche recalculée).
    - Boutons explicites : **« Appliquer au modèle »** ou **« Rejeter »**.
  - Comparateur de variantes structurelles : mémorisation de scénarios géométriques sans écraser le fichier actif.
- **`AIRuntimeDialog` :**
  - Sélecteur de mode : `LOCAL`, `AUTO`, `CLOUD`.
  - Configuration sécurisée des clés API avec chiffrement Windows DPAPI (`SecretStore`).
  - Sélection du profil matériel (CPU, GPU Vulkan/CUDA).
  - Monitoring des quotas (jetons consommés, temps de réponse, coût estimé).

### 3.2. Couche Fournisseurs d'IA (`src/AI/Providers/`)

L'interface commune `IAIProvider` normalise les interactions asynchrones :

```cpp
class IAIProvider : public QObject {
    Q_OBJECT
public:
    virtual QString providerId() const = 0;
    virtual QString displayName() const = 0;
    virtual ProviderLocality locality() const = 0; // Local ou Cloud
    virtual bool isAvailable() const = 0;
    virtual AICapabilities capabilities() const = 0;
    virtual quint64 chat(const ChatRequest& request) = 0;
    virtual void cancel(quint64 requestId) = 0;
signals:
    void contentDelta(quint64 requestId, const QString& text);
    void reasoningDelta(quint64 requestId, const QString& text);
    void finished(quint64 requestId, const ChatResult& result);
    void failed(quint64 requestId, const QString& error);
};
```

#### Routeur Multi-Fournisseurs avec Repli (`Fallback Router`)
Pour pallier les défaillances de connexion ou les limitations de débit (HTTP 429 / 503) :
1. **Fournisseur Primaire :** Modèle sélectionné (ex. `Google Gemini 2.0 Flash` gratuit ou `DeepSeek-R1`).
2. **Fournisseur Secondaire (Secours) :** En cas d'échec ou de timeout après 8 secondes, basculement transparent vers le fournisseur secondaire (ex. `DeepSeek-V3` ou `Qwen Local`).
3. **Mode Déconnecté Souverain :** Si aucune connexion internet n'est active, repli automatique sur le moteur `llama-server` local sans bloquer l'ingénieur.

### 3.3. Contexte d'Ingénierie Structuré (`src/AI/Context/`)

Pour éviter l'engorgement de la fenêtre de contexte et éliminer les hallucinations d'unités, `EngineeringContextBuilder` génère un extrait JSON compact, typé et unifié :

- **Système d'unités SI explicite :**
  - Longueurs : Mètres ($\text{m}$) et millimètres ($\text{mm}$).
  - Forces : Kilonewtons ($\text{kN}$).
  - Moments : Kilonewtons-mètres ($\text{kN}\cdot\text{m}$).
  - Contraintes & Modules : Mégapascals ($\text{MPa}$) et Gigapascals ($\text{GPa}$).
- **Données topologiques et physiques :**
  - Répertoire des nœuds avec coordonnées $(X, Y, Z)$ et matrice de blocage des 6 DDLs.
  - Inventaire des éléments linéaires (Poutres, Poteaux, Barres de treillis, Câbles).
  - Propriétés des sections (Nom, $A$, $I_y$, $I_z$, $J_t$) et nuances de matériaux ($S235$, $S355$, $C25/30$, etc.).
  - Cas de charges (Permanentes $G$, Variables $Q$, Neige $S$, Vent $W$) et combinaisons ELU / ELS.
  - Résumé des résultats de calcul : Déplacements extrêmes, moments fléchissants maximaux, réactions d'appui.
  - Statut de fraîcheur : Balise explicite `upToDate: true/false` empêchant l'IA d'analyser des résultats désynchronisés après une modification de géométrie.

### 3.4. Outils d'Ingénierie & Liste Blanche (`src/AI/Tools/`)

Le modèle d'IA accède au modèle TSA exclusivement à travers des outils déclarés selon la spécification JSON Schema :

| Nom de l'outil | Nature | Description technique |
| :--- | :---: | :--- |
| `get_project_info` | Lecture | Résumé global des effectifs du modèle, unités et hypothèses. |
| `list_members` | Lecture | Filtrage paginé des barres selon le type (poutre, poteau, treillis). |
| `list_nodes` | Lecture | Coordonnées et conditions d'appuis des nœuds. |
| `get_object` | Lecture | Inspection exhaustive d'un composant (dimensions, charges associées, efforts internes). |
| `list_loads` | Lecture | Inventaire des charges nodales, linéiques réparties et surfaciques. |
| `get_results_summary` | Lecture | Synthèse des enveloppes d'efforts $N, V_y, V_z, M_y, M_z$ et flèches. |
| `check_model` | Déterministe | Exécution du diagnostic topologique et cinématique sans IA. |
| `search_knowledge` | RAG Local | Recherche BM25 dans la documentation technique et les Eurocodes locaux. |
| `propose_section_change` | Proposition | Génération d'une `ActionProposal` pour modifier une section sans l'appliquer. |
| `propose_run_analysis` | Proposition | Demande à l'ingénieur de lancer le calcul OpenSees. |

### 3.5. Contrôles Déterministes (`StructuralChecker`)

Le `StructuralChecker` exécute des algorithmes de théorie des graphes et de statique graphique avant toute consultation de l'IA :
- Nœuds géométriquement isolés (non rattachés à un élément).
- Nœuds doublons (distance relative $< 1\,\text{mm}$).
- Barres dégénérées de longueur nulle ($L < 10^{-4}\,\text{m}$) ou barres superposées.
- Absence d'appuis (mécanisme cinématique évident à 6 DDLs libres).
- Sous-structures isolées formant des mécanismes locaux.
- Appui ponctuel articulé unique sur structure spatiale (rotation d'ensemble non bloquée).
- Dépassement indicatif du critère de flèche réglementaire Eurocode ($L / 250$).

---

## 4. Rôle de MCP (Model Context Protocol)

TSA dispose déjà d'un serveur MCP dédié (`tsaraloha-mcp.exe` dans `tools/mcp/`) relié à `AutomationServer` via un tube nommé sécurisé (`Windows Named Pipe`).

### Positionnement d'Architecture :
- **Dock Co-Engineering interne :** TSA est le **client** qui interroge les modèles d'IA via l'`AIOrchestrator`. C'est le mode privilégié pour l'utilisateur final de TSA.
- **Claude Code / Agents autonomes externes via MCP :** TSA agit comme le **serveur**, exposant ses commandes (`model.create_node`, `analysis.run`, `results.summary`) à un agent externe piloté par l'ingénieur depuis son terminal ou son IDE.

**Conclusion :** Le protocole MCP est un atout d'ouverture majeur pour les workflows avancés des développeurs et ingénieurs équipés d'un abonnement Claude Code, mais **ne constitue pas une dépendance bloquante** pour le fonctionnement autonome du Co-Engineering dans TSA.
