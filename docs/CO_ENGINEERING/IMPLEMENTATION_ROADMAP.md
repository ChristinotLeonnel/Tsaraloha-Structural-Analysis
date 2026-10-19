# Feuille de Route d'Implémentation du Co-Engineering (TSA)
**Date :** 19 octobre 2026  
**Cycle de Développement :** 2026 – 2027  
**Projet :** Tsaraloha Structural Analysis (TSA)  
**Conformité :** ISO/IEC/IEEE 12207 (Processus du Cycle de Vie Logiciel)

---

## 1. Vue d'Ensemble des Phases de Développement

Le déploiement du Co-Engineering dans TSA est organisé en **quatre phases incrémentales et maîtrisées**, garantissant à chaque étape la stabilité du moteur FEM, la sécurité des données et le contrôle budgétaire :

```text
┌────────────────────────────────────────────────────────────────────────┐
│               FEUILLE DE ROUTE CO-ENGINEERING TSA (4 PHASES)           │
└────────────────────────────────────────────────────────────────────────┘
                                    │
    ┌───────────────────────────────┼───────────────────────────────┐
    ▼                               ▼                               ▼
 PHASE 1 (Immédiat)              PHASE 2 (Court terme)           PHASE 3 (Moyen terme)
 • Fournisseur Gratuit Gemini   • Validation Analytique RDM     • Génération Note de Calcul
 • Routeur avec Fallback        • Comparateur OpenSees/TSA      • RAG Eurocodes Enrichi
 • Fournisseur Simulé (Mock)    • Cartes d'Impact Avancées      • Export PDF/HTML IEEE 1063
 • Support DeepSeek R1/V3       • Diagnostic de Flambement      • Vérification ELS/ELU
    │                               │                               │
    └───────────────────────────────┼───────────────────────────────┘
                                    │
                                    ▼
                             PHASE 4 (Long terme)
                             • Co-Conception BIM / IFC 4.3
                             • Optimisation Topologique & Variantes
                             • Automatisation via Scripts Blueprint
```

---

## 2. Description Détaillée des Jalons

### Phase 1 : Couche Fournisseurs Résiliente & Accès Gratuit (En cours)
- **Objectif :** Permettre l'accès immédiat à une IA performante sans frais d'infrastructure, avec tolérance aux pannes réseau.
- **Livrables :**
  1. `MockAIProvider` : Fournisseur simulé sans clé API pour tests unitaires, validation continue (CI/CD) et démonstrations hors-ligne.
  2. Intégration directe des préréglages Cloud :
     - `Google Gemini 2.0 Flash` (Point d'accès compatible OpenAI, palier gratuit 15 RPM).
     - `DeepSeek-R1` et `DeepSeek-V3` (Point d'accès standard OpenAI, ultra-faible coût).
     - `Qwen DashScope` (Alibaba Cloud) et `Groq`.
  3. `AIProviderRouter` : Mécanisme de basculement automatique (`fallback`) en cas de timeout, d'erreur 429 (quota dépassé) ou d'indisponibilité réseau.
  4. Suite de tests automatisés `test_ai.cpp` étendue (Tests 124+ validant le mock, le routeur et les cas d'omission).
- **Critère de sortie :** Compilation sans avertissement, tests automatisés 100% passants.

### Phase 2 : Vérification Analytique Multi-Moteurs & Diagnostic RDM
- **Objectif :** Équiper l'assistant des outils pour confronter les calculs OpenSees aux solutions fermées de la RDM.
- **Livrables :**
  1. Outils d'ingénierie étendus dans `AIToolRegistry` :
     - `verify_beam_analytical` : calcul déterministe des flèches et moments de poutres isostatiques et continues.
     - `compare_opensees_result` : calcul automatique des écarts relatifs entre calcul numérique et formule fermée.
     - `check_slenderness` : calcul automatique des élancements d'Euler $\lambda = L_{cr} / i$ et vérification du risque de flambement selon l'EN 1993-1-1.
  2. Amélioration du dock `AICoEngineeringDock` : affichage des courbes d'efforts $M, V, N$ directement dans les réponses de l'IA via le moteur graphique de TSA.
- **Critère de sortie :** Validation des cas T01 à T07 du protocole d'essai.

### Phase 3 : Génération Automatisée de Notes de Calcul (NDC)
- **Objectif :** Rédiger des notes de calcul complètes, justifiées et conformes aux Eurocodes et à la norme IEEE Std 1063.
- **Livrables :**
  1. Couplage avec le module `src/NDC/` (Note de Calcul) de TSA :
     - L'IA prépare le sommaire et le corps technique de la note en injectant les tableaux d'efforts réels issus d'OpenSees.
     - Citation des articles exacts des Eurocodes récupérés par le moteur RAG local.
  2. Moteur d'exportation vers PDF et HTML avec mise en page professionnelle (logos, cartouches, annexes mathématiques).
- **Critère de sortie :** Génération sans hallucination d'une note de calcul complète sur un bâtiment R+2 testé dans TSA.

### Phase 4 : Co-Conception BIM / IFC & Optimisation Topologique
- **Objectif :** Assister l'architecte et l'ingénieur dans la synthèse structurelle et l'éco-conception.
- **Livrables :**
  1. Outils d'inspection IFC 4.3 : corrélation entre les objets architecturaux (murs, ouvertures) et le modèle filaire FEM de TSA.
  2. Optimisation multi-critères : recherche automatique du profilé le plus léger garantissant $\sigma \le f_y / \gamma_{M0}$ et $v \le L / 300$.
  3. Générateur de scripts Blueprint : l'IA propose un script de modélisation paramétrique complet validable d'un clic.
- **Critère de sortie :** Réduction mesurée du volume d'acier sur un cas d'étude réel sans violation des états limites.

---

## 3. Matrice de Gestion des Risques

| Risque identifié | Probabilité | Impact | Stratégie d'atténuation |
| :--- | :---: | :---: | :--- |
| **Coupure réseau Internet à Madagascar** | Élevée | Fort | Moteur local `llama-server` avec modèles Qwen GGUF préinstallés ; repli transparent sans blocage de l'UI. |
| **Dépassement de quota gratuit (HTTP 429)** | Moyenne | Moyen | Basculement immédiat par le routeur vers le fournisseur secondaire ou le moteur local. |
| **Modification erronée de profilé par l'IA** | Moyenne | Élevé | Principe du Human-in-the-loop : aucune application directe, confirmation obligatoire, Undo instantané (Ctrl+Z). |
| **Changement d'API côté fournisseur** | Faible | Moyen | Standardisation stricte sur le protocole Chat Completions d'OpenAI utilisé par 95% des fournisseurs mondiaux. |
| **Obsolescence des clés secrètes** | Moyenne | Faible | Notification explicite dans l'interface et invite de reconfiguration sécurisée. |
