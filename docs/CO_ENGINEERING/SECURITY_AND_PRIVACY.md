# Sécurité, Confidentialité & Responsabilité de l'Ingénieur (Co-Engineering TSA)
**Date :** 19 octobre 2026  
**Statut :** Politique de Sécurité et Cadre Déontologique  
**Projet :** Tsaraloha Structural Analysis (TSA)  
**Normes de Référence :** ISO/IEC 27001, ISO/IEC 25010 (Attribut « Sécurité »), RGPD/GDPR, Eurocodes

---

## 1. Cadre Juridique et Déontologique en Génie Civil

Dans le domaine du calcul des structures, des ouvrages d'art et du bâtiment :
1. **Responsabilité Légale Non Déléguable :**
   Aucune directive, note de calcul, dimensionnement de ferraillage ou vérification de stabilité généré par une intelligence artificielle ne peut se substituer à la signature et à la responsabilité décennale / civile professionnelle de l'ingénieur qualifié.
2. **Statut d'Aide à la Décision :**
   Le module Co-Engineering de TSA est formellement classifié comme un **système interactif d'aide à la décision technique**. Il formule des hypothèses, guide le diagnostic et prépare des scénarios ; il ne valide jamais une structure de façon unilatérale.
3. **Interdiction de l'Approximation Silencieuse :**
   Si une grandeur géométrique ou mécanique est indéterminée (ex. module d'élasticité $E$ inconnu, coefficient de Poisson $\nu$ manquant, condition d'appui ambiguë), l'IA doit expressément formuler son incertitude ou refuser de conclure, plutôt que d'injecter une valeur moyenne par défaut sans préavis.

---

## 2. Modèle de Menaces & Mesures d'Atténuation

| Vecteur de Menace | Risque associé | Mesure d'atténuation dans TSA |
| :--- | :--- | :--- |
| **Fuite de données de projet (Propriété Intellectuelle)** | Envoi involontaire de plans confidentiels, géométries brevetées ou données financières vers des serveurs tiers. | • Mode **LOCAL par défaut** (air-gap total).<br>• Les fichiers binaires `.tsa` ne sont **JAMAIS** transmis sur le réseau.<br>• Seules des extractions géométriques anonymisées sont projetées dans le contexte JSON. |
| **Modification destructive du modèle** | Altération silencieuse d'un appui, suppression d'un contreventement ou changement de section non consenti. | • **Garantie par conception :** aucun outil d'écriture directe.<br>• Seules des `ActionProposal` sont générées.<br>• L'ingénieur doit cliquer sur « Appliquer ».<br>• Chaque application génère une transaction `Undo/Redo`. |
| **Injection d'instructions malveillantes (Prompt Injection)** | Données de projet corrompues (ex. nom d'un matériau contenant une directive LLM de contournement). | • Découpage strict des rôles : `system`, `user`, `tool`.<br>• Le contexte structurel est injecté dans un bloc JSON rigide, jamais interprété comme une consigne système. |
| **Vol de clés d'API (Credential Theft)** | Compromission de la clé OpenAI, Gemini ou Anthropic stockée sur le poste utilisateur. | • Chiffrement au repos via l'API cryptographique native du système d'exploitation : **Windows DPAPI** (`CryptProtectData`).<br>• Aucune clé en clair dans le registre ou les fichiers `.ini`. |
| **Hallucination de conformité normative** | Citation de clauses imaginaires de l'Eurocode ou formules inventées. | • Règle d'or du prompt système : interdiction absolue de citer une norme sans preuve issue du RAG local (`search_knowledge`).<br>• Si la base locale ne contient pas l'Eurocode requis, l'IA indique explicitement que le contrôle reste à la charge de l'ingénieur. |

---

## 3. Gestion Cryptographique des Clés d'API (`SecretStore`)

Sous Windows, le stockage des secrets dans TSA est confié au module `TSA::AI::SecretStore` qui exploite la Data Protection API (**DPAPI**) :

```text
Clé d'API en clair (ex. sk-proj-...)
        │
        ▼ CryptProtectData (Chiffrement lié au compte Windows actif)
Vecteur d'octets chiffrés (Blob binaire incompréhensible)
        │
        ▼ Stockage dans QSettings ("AI/cloudApiKeyProtected")
```

### Avantages de sécurité :
1. Même si un tiers copie le fichier de configuration de l'application, la clé ne peut pas être déchiffrée sur un autre poste ou sous une autre session Windows.
2. La clé en clair n'existe en mémoire RAM que le temps de la construction de la requête réseau HTTPS sécurisée (`TLS 1.3`).
3. Lorsque l'utilisateur efface sa clé depuis l'interface `AIRuntimeDialog`, la mémoire est purgée.

---

## 4. Politique de Flux et Sortie de Données (`Data Egress`)

TSA applique une politique d'isolation stricte des flux sortants :

```text
┌─────────────────────────────────────────────────────────────┐
│                    POSTE DE TRAVAIL LOCAL                   │
│                                                             │
│   Fichier projet confidentiel (.tsa)                        │
│   ├── Maillage volumique OCCT                               │
│   ├── Notes internes du bureau d'études                     │
│   └── Historique complet des versions                       │
│                                                             │
│                [ BARRIÈRE D'ISOLATION LOCALE ]              │
│                              │                              │
│                              ▼ Filtre EngineeringContext    │
│   JSON Anonymisé Uniquement :                               │
│   - Nœuds : [ID, X, Y, Z]                                   │
│   - Barres : [ID, Profil, Matériau générique]               │
│   - Résultats agrégés : [DispMax, MMax]                     │
└──────────────────────────────┬──────────────────────────────┘
                               │
                Requête HTTPS TLS 1.3 sortante
                (Soumise à accord préalable explicite)
                               ▼
┌─────────────────────────────────────────────────────────────┐
│                  FOURNISSEUR CLOUD SÉLECTIONNÉ              │
│         (Google Gemini / DeepSeek / Anthropic Claude)       │
└─────────────────────────────────────────────────────────────┘
```

### Règle du Consentement Éclairé :
Lorsque l'utilisateur est en mode `AUTO` ou bascule sur le `CLOUD` :
1. Une boîte de dialogue modale avertit l'ingénieur avant la toute première transmission externe.
2. Le message indique précisément quel fournisseur sera contacté et la nature des données envoyées (géométrie et efforts, sans métadonnées confidentielles de projet).
3. L'ingénieur peut refuser : dans ce cas, TSA effectue un repli immédiat sur le moteur `LOCAL` (si installé) ou interrompt la demande sans envoyer le moindre octet.

---

## 5. Journalisation et Traçabilité d'Audit

Conformément à la norme **ISO/IEC/IEEE 12207**, toutes les interactions du Co-Engineering sont consignées dans un journal d'audit local (`<AppData>/Tsaraloha/ai/logs/audit.jsonl`).

### Données journalisées (Audit technique) :
- Horodatage précis (ISO 8601).
- Identifiant du fournisseur et du modèle utilisé.
- Événements système : `request`, `consent_granted`, `consent_denied`, `proposal_accepted`, `proposal_rejected`.
- Métriques d'exécution : latence globale, temps de premier jeton, nombre de jetons d'entrée et de sortie, vitesse (jetons/s).
- Identifiant des outils exécutés et propositions validées.

### Données STRICTEMENT EXCLUES de la journalisation :
- Le contenu textuel intégral des invites (prompts) utilisateur.
- Les clés d'API ou jetons d'autorisation.
- Les données structurelles confidentielles.
