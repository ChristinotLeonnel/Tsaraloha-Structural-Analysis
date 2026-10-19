# Comparatif Exhaustif des Fournisseurs d'IA pour le Co-Engineering dans TSA
**Date d'évaluation :** 19 octobre 2026  
**Auteur :** Architecte Logiciel Senior — Calcul des Structures, FEM & IA  
**Projet :** Tsaraloha Structural Analysis (TSA)  
**Cadre Normatif :** ISO/IEC/IEEE 12207, ISO/IEC 25010, IEEE Std 1063, Eurocodes (EN 1990 à EN 1999)

---

## 1. Contexte & Enjeux Spécifiques à TSA et Madagascar

L'intégration d'une intelligence artificielle de **co-ingénierie** dans un progiciel de calcul de structures comme TSA (Tsaraloha Structural Analysis) ne consiste pas à brancher un simple chatbot textuel. Elle exige :
1. **Rigueur scientifique absolue :** Incapacité stricte à halluciner des résultats d'efforts internes ($M, V, N$), des contraintes de Von Mises ou des formules normatives.
2. **Complémentarité Déterministe / Probabiliste :** Les calculs matriciels et la résolution FEM appartiennent au moteur numérique (OpenSees / solveur natif TSA). L'IA est dédiée au raisonnement topologique, au diagnostic d'instabilité, à l'explication physique, à la suggestion d'optimisation et à la rédaction de notes de calcul.
3. **Réalité de Madagascar (Connectivité, Coût & Géographie) :**
   - Coupures d'électricité (délestages) et instabilité récurrente de la bande passante internet.
   - Cartes bancaires internationales malgaches (Mastercard/Visa BNI, Société Générale Madagasikara, MCB, etc.) fréquemment bloquées par les systèmes de paiement Stripe/OpenAI/Anthropic en raison de politiques de prévention des fraudes géographiques.
   - Nécessité vitale d'un **mode 100% gratuit** ou **très faible coût**, sans carte bancaire obligatoire pour démarrer, et d'un repli hors ligne total (**Local Offline**).

---

## 2. Typologie des Accès : Clarification Impérative

Une erreur fréquente consiste à confondre les modèles d'accès. Voici la distinction stricte retenue :

| Type d'accès | Description technique | Modèle financier | Adaptabilité pour TSA |
| :--- | :--- | :--- | :--- |
| **Chatbot Web Gratuit** | Interface web grand public (ex. chatgpt.com, claude.ai, gemini.google.com). | Gratuit avec quotas restreints, perte d'historique. | **Inexploitable directement** dans le code C++/Qt de TSA (aucun endpoint stable sans reverse engineering fragile). |
| **Abonnement Agent / IDE** | Forfait mensuel fixe (ex. Claude Pro/Team 20$/mois, ChatGPT Plus 20$/mois, GitHub Copilot). | 20 à 30 $ / mois / utilisateur. | Destiné au développeur humain (ex. via MCP avec Claude Code ou Cursor), **non redistribuable** aux utilisateurs finaux de TSA. |
| **API Gratuite (Free Tier)** | Endpoint officiel avec clé d'API fournie sans facturation (ex. Google AI Studio, GLM-4-Flash, Groq Free). | 0 $ (limité en requêtes par minute/jour : RPM / RPD). | **Idéal pour l'utilisateur individuel** et pour démarrer sans dépense dans TSA. |
| **API Pay-as-you-go** | Consommation mesurée au million de jetons (input / output / cache) via endpoint HTTP REST (OpenAI, DeepSeek, Anthropic, Gemini Pay). | Facturé au jeton consommé. | **Obligatoire pour les usages intensifs**, garantit la confidentialité des données sur les offres payantes. |
| **Modèles Ouverts Locaux** | Poids ouverts exécutés en local via llama.cpp / GGUF (Qwen 2.5/3, DeepSeek-R1-Distill, Llama 3). | 0 $ de consommation, dépend du matériel local (VRAM GPU / RAM CPU). | **Gouvernance souveraine totale**, résilience aux pannes réseau à Madagascar, confidentialité absolue. |

---

## 3. Matrice d'Évaluation Comparative (10 Critères)

### Grille de notation : 1 (Très faible) à 5 (Excellent)

| Critère d'évaluation | Google Gemini (2.0/1.5 Flash & Pro) | Anthropic Claude (3.5/3.7 Sonnet) | OpenAI (GPT-4o & o1/o3-mini) | DeepSeek (V3 & R1) | Qwen / Alibaba (2.5 Coder & QwQ / Qwen 3) | GLM / Z.ai (GLM-4 Flash / Plus) | Modèles Locaux (llama.cpp TSA) |
| :--- | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| **1. Raisonnement technique & RDM** | 4.8 | 4.9 | 4.9 (o1/o3) | 4.9 (R1) | 4.6 (QwQ) | 4.1 | 3.8 (4B-8B) |
| **2. Codebase C++/Qt complexe** | 4.7 | 5.0 | 4.8 | 4.8 | 4.8 | 4.2 | 3.6 |
| **3. Compréhension RDM / FEM / Matrices** | 4.7 | 4.8 | 4.9 | 4.8 | 4.6 | 4.0 | 3.5 |
| **4. Raisonnement symbolique & numérique** | 4.6 | 4.7 | 4.9 (o1) | 4.9 (R1) | 4.7 (QwQ) | 4.0 | 3.4 |
| **5. Fenêtre de contexte & Projets volumineux** | **5.0 (1M-2M)** | 4.5 (200k) | 4.3 (128k) | 4.3 (128k) | 4.3 (128k) | 4.3 (128k) | 3.0 (8k-32k) |
| **6. Disponibilité API dans TSA** | 5.0 (OpenAI comp.) | 4.5 (Beta OpenAI) | 5.0 (Standard) | 5.0 (Standard) | 5.0 (Standard) | 5.0 (Standard) | 5.0 (Inclus) |
| **7. Outillage (Tools, JSON Schema)** | 4.9 | 4.9 | 5.0 | 4.7 | 4.7 | 4.4 | 4.2 |
| **8. Prix, Quotas Gratuits & Madagascar** | **5.0 (Free Tier mondial)** | 2.5 (Pas de free API, CB striée) | 2.8 (Payant, blocage CB fréquent) | **4.9 (Ultra low-cost)** | **4.8 (Free credits + Local)** | 4.5 (Free tier Flash) | **5.0 (100% gratuit)** |
| **9. Confidentialité code & projets** | 4.2 (Paid) / 3.0 (Free) | 4.8 | 4.7 | 4.0 | 4.5 (DashScope) / **5.0 (Local)** | 4.0 | **5.0 (Air-gap)** |
| **10. Fiabilité, latence & Anti-Lock-in** | 4.8 | 4.6 | 4.7 | 3.8 (Surcharges ponctuelles) | 4.5 | 4.4 | **5.0 (Autonomie)** |
| **SCORE GLOBAL PONDÉRÉ (/5)** | **4.75** | **4.56** | **4.55** | **4.54** | **4.48** | **4.29** | **4.10** |

---

## 4. Analyse Détaillée par Fournisseur

### 4.1. Google Gemini (Recommandation N°1 pour l'accès en ligne direct)

- **Modèles de référence :**
  - **Gemini 2.0 Flash / 1.5 Flash :** Vitesse extrême, raisonnement multimodal, génération quasi-instantanée (~150 tokens/s).
  - **Gemini 2.0 Flash Thinking / Gemini 1.5 Pro :** Raisonnement approfondi étape par étape, fenêtres géantes.
- **Accès API & Quotas Gratuits :**
  - **Google AI Studio (aistudio.google.com) :** Propose un **palier gratuit permanent (Free Tier)** sans saisie de carte bancaire.
  - Quotas gratuits officiels (Gemini 2.0 Flash / 1.5 Flash) : **15 RPM** (requêtes/min), **1 000 000 TPM** (jetons/min), **1 500 RPD** (requêtes/jour).
  - Point d'accès compatible OpenAI disponible : `https://generativelanguage.googleapis.com/v1beta/openai` avec paramètre `model: "gemini-2.0-flash"` ou `"gemini-1.5-flash"`.
- **Atouts majeurs pour TSA :**
  - **Contexte de 1 à 2 millions de jetons :** Capacité unique d'ingérer l'intégralité d'un fichier IFC lourd, les Eurocodes complets (EN 1992, EN 1993), et tout le code source de TSA sans tronquer les données.
  - **Accessibilité à Madagascar :** Accessible sans VPN, sans carte bancaire internationale requise pour obtenir une clé d'API sur Google AI Studio.
  - **Capacités Multimodales :** Peut analyser directement des coupes de plans 2D, des diagrammes de moments fléchissants ou des photos de chantiers / désordres structuraux.
- **Points de vigilance :**
  - Sur le palier gratuit Google AI Studio, Google se réserve le droit d'utiliser les données anonymisées pour entraîner ses modèles. Les projets ultra-confidentiels doivent donc basculer en mode payant Google Cloud Vertex AI ou en mode Local TSA.

### 4.2. DeepSeek (Recommandation N°1 pour le Rapport Performance/Coût)

- **Modèles de référence :**
  - **DeepSeek-V3 (671B MoE, 37B actifs) :** Modèle polyvalent ultra-rapide.
  - **DeepSeek-R1 :** Modèle de raisonnement pur rivalisant avec OpenAI o1 sur MATH (97.3%), AIME et les benchmarks de génie scientifique.
- **Accès API & Tarification :**
  - Point d'accès compatible OpenAI : `https://api.deepseek.com/v1`.
  - Tarification disruptive :
    - DeepSeek-V3 : **0,14 $ / 1M jetons d'entrée** (0,014 $ en cas de cache hit !), **0,28 $ / 1M jetons de sortie**.
    - DeepSeek-R1 : **0,55 $ / 1M jetons d'entrée**, **2,19 $ / 1M jetons de sortie**.
  - 5 millions de jetons offerts à l'inscription.
- **Atouts pour TSA :**
  - Raisonnement mathématique et mécanique hors du commun pour un coût 20 à 50 fois inférieur à OpenAI o1 ou Claude 3.5 Sonnet.
  - Format 100% standard OpenAI : fonctionne sans modification avec `OpenAICompatibleProvider` de TSA.
- **Points de vigilance :**
  - En période de pic mondial, l'API DeepSeek subit des erreurs HTTP 503 / 429 ("Server Busy"). **Le mécanisme de fallback de TSA est impératif.**

### 4.3. Anthropic Claude (Excellence Architecturale & MCP)

- **Modèles de référence :** Claude 3.5 Sonnet, Claude 3.7 Sonnet (Hybrid Reasoning), Claude 3.5 Haiku.
- **Atouts pour TSA :**
  - Meilleure compréhension globale du code source C++/Qt, des design patterns et de l'architecture logicielle.
  - Très grande prudence : Claude refuse spontanément d'extrapoler des données de résistance s'il manque des paramètres géométriques ou de nuance d'acier.
  - Support de prompt caching natif (réduction de 90% du coût sur le contexte structurel statique).
- **Points de vigilance majeurs pour Madagascar :**
  - **Aucune API gratuite permanente** (seul le chat web claude.ai est gratuit avec fortes limites horaires).
  - L'accès API Console (`console.anthropic.com`) requiert un prépaiement par carte bancaire internationale qui échoue régulièrement à Madagascar en raison de contrôles antifraude stricts.
  - En revanche, Claude Code + MCP via l'exécutable local `tsaraloha-mcp.exe` de TSA offre un pont direct pour l'ingénieur disposant déjà d'un compte Claude.

### 4.4. OpenAI (GPT-4o & Famille o1 / o3-mini)

- **Modèles de référence :** GPT-4o, GPT-4o mini, o1, o3-mini.
- **Atouts :**
  - GPT-4o mini est très économique (0,15 $ / 0,60 $ par M jetons) et rapide.
  - o1 / o3-mini excellent dans les résolutions formelles d'équations différentielles de la RDM et les dérivations de matrices de rigidité gauches.
- **Points de vigilance :**
  - Absence de palier d'API gratuit pérenne.
  - Restrictions bancaires et géographiques persistantes pour les comptes basés à Madagascar sans carte virtuelle internationale prépayée.

### 4.5. Qwen (Alibaba Cloud & Open Weights)

- **Modèles de référence :** Qwen 2.5 Coder 32B, QwQ-32B (Reasoning), Qwen 3 (déjà référencé dans TSA).
- **Atouts :**
  - Modèles bilingues/multilingues remarquables en mathématiques et code.
  - **Dualité Local/Cloud :** Les modèles Qwen GGUF tournent déjà directement dans TSA sur CPU ou GPU via `llama-server`.
  - L'API internationale DashScope Alibaba Cloud propose un mode compatible OpenAI avec crédits gratuits généreux.

### 4.6. GLM / Z.ai & Mistral / Groq

- **GLM-4-Flash (Zhipu AI / Z.ai) :** Offre une API **entièrement gratuite** avec support complet des appels d'outils et une latence correcte. Excellente alternative de secours.
- **Groq :** Vitesse stupéfiante (300-500 tokens/sec sur Llama 3.3 70B et Qwen 2.5), palier gratuit par quota horaire/journalier. Idéal pour des validations syntaxiques ou des formats courts.

---

## 5. Synthèse Stratégique pour TSA

Pour garantir **zéro interruption**, **zéro dépense obligatoire** et une **qualité d'ingénierie maximale**, la stratégie optimale pour TSA repose sur un **triptyque hybride** :

```text
┌────────────────────────────────────────────────────────────────────────┐
│                      STRATÉGIE HYBRIDE TSA CO-ENGINEERING              │
└────────────────────────────────────────────────────────────────────────┘
                                    │
         ┌──────────────────────────┼──────────────────────────┐
         ▼                          ▼                          ▼
 1. NIVEAU QUOTIDIEN        2. NIVEAU EXPERT           3. NIVEAU SOUVERAIN
    (Gratuit en ligne)         (Raisonnement lourd)       (Local Hors-ligne)
 ────────────────────       ────────────────────       ────────────────────
 Google Gemini 2.0 Flash    DeepSeek-R1                Qwen 2.5 / Qwen 3
 via Google AI Studio       ou Claude 3.5 Sonnet       via llama-server TSA
 • 100% Gratuit (15 RPM)    • Raisonnement profond     • Fonctionne sans internet
 • 1M-2M tokens contexte    • Vérification formelle    • 100% confidentiel
 • Pas de CB requise        • Coût ultra-faible (R1)   • Résistant aux coupures
 • Accessible à Madagascar  • Fallback automatisé      • Déjà intégré dans src/AI
```
