# Réglementations Techniques & Données Locales de Madagascar

## 1. Contexte Géophysique & Cadre Institutionnel

À Madagascar, les études de structures et d'infrastructures de génie civil font intervenir des organismes publics techniques et de recherche majeurs :

* **IOGA (Institut et Observatoire de Géophysique d'Antananarivo) :** Rattaché à l'Université d'Antananarivo (Faculté des Sciences), son **Laboratoire de Sismologie et d'Infrasons (LSI)** gère le réseau national de surveillance sismique, cartographie les failles actives et produit les évaluations de l'aléa sismique.
* **LNTPB (Laboratoire National des Travaux Publics et du Bâtiment) :** Organisme national sous tutelle du Ministère des Travaux Publics, chargé des essais géotechniques des sols, du contrôle de conformité des granulats, bétons, aciers et des expertises de fondations.
* **BNGRC (Bureau National de Gestion des Risques et des Catastrophes) :** Coordonne la prévention et les directives de construction face aux aléas cycloniques, inondations et séismes.
* **Direction Générale de la Météorologie (Météo Madagascar) :** Fournit les statistiques climatiques, régimes des vents cycloniques et pluviométries centennales indispensables au dimensionnement des ponts et dalots.

---

## 2. Aléa Sismique à Madagascar & Pratique Parasismique

Bien qu'appartenant à la plaque tectonique africaine, Madagascar présente une sismicité intra-plaque significative liée à l'extension du rift est-africain et à des structures volcaniques récentes.

### 2.1 Zonage de Sismicité Usuel
1. **Zone des Hautes Terres Centrales (Analamanga, Vakinankaratra, Itasy) :**
   - Sismicité modérée à fréquente, liée aux failles de graben et au volcanisme quaternaire de l'Itasy et de l'Ankaratra.
   - Accélération de calcul recommandée au rocher : $a_{gR} \approx 0.08\text{g à }0.12\text{g}$ ($0.8\text{ à }1.2\text{ m/s}^2$).
2. **Zone du Graben du Lac Alaotra :**
   - Zone tectoniquement active avec failles normales majeures.
   - Accélération de calcul recommandée : $a_{gR} \approx 0.10\text{g à }0.15\text{g}$.
3. **Zone Côtière Ouest et Canal de Mozambique :**
   - Sismicité faible à diffuse.
4. **Zone Sud (Bordure de Tolagnaro / Fort-Dauphin) :**
   - Événements historiques notables au large et en bordure de marge continentale.

### 2.2 Application Normative dans TSA
En l'absence d'annexe nationale formellement publiée au journal officiel pour l'Eurocode 8, la pratique de l'ingénierie malgache prescrit :
* L'application de l'**Eurocode 8 (EN 1998-1)** avec un spectre élastique de Type 2 (séismes de magnitude modérée à faible profondeur).
* Paramètres de sol $S$ selon la classification du LNTPB (Sols latéritiques fermes : Classe B ou C ; sables alluvionnaires ou remblais récents : Classe D ou E nécessitant vérification du risque de liquéfaction).
* Coefficients de comportement : $q = 1.5$ (DCL - Ductilité faible), $q = 3.0$ à $3.9$ (DCM - Ductilité moyenne avec ossatures poteaux-poutres bien contreventées).

---

## 3. Aléa Cyclonique & Sollicitations de Vent Extrême

Madagascar est l'une des nations du bassin Sud-Ouest de l'Océan Indien les plus exposées aux cyclones tropicaux intenses (rafales dépassant fréquemment $200\text{ km/h}$).

### 3.1 Carte des Vitesses de Vent de Référence ($v_{b,0}$)

| Zone Géographique | Régions Conviviales | Vitesse de Base $v_{b,0}$ | Pression Dynamique de Base $q_{b,0}$ |
| :--- | :--- | :---: | :---: |
| **Zone I (Côte Est Cyclonique Majeure)** | Toamasina, Sainte-Marie, Sava, Analanjirofo, Vatovavy-Fitovinany | **$45\text{ à }52\text{ m/s}$** ($162\text{ à }187\text{ km/h}$) | **$1.24\text{ à }1.65\text{ kN/m}^2$** |
| **Zone II (Nord & Nord-Ouest)** | Antsiranana (Diego-Suarez), Nosy Be, Mahajanga | **$38\text{ à }44\text{ m/s}$** ($137\text{ à }158\text{ km/h}$) | **$0.88\text{ à }1.19\text{ kN/m}^2$** |
| **Zone III (Hautes Terres Centrales)** | Antananarivo, Antsirabe, Moramanga, Fianarantsoa | **$34\text{ à }40\text{ m/s}$** ($122\text{ à }144\text{ km/h}$) | **$0.71\text{ à }0.98\text{ kN/m}^2$** |
| **Zone IV (Sud & Sud-Ouest Semi-Aride)** | Toliara, Morondava, Tolagnaro | **$34\text{ à }38\text{ m/s}$** ($122\text{ à }137\text{ km/h}$) | **$0.71\text{ à }0.88\text{ kN/m}^2$** |

*Note sur les rafales de crête :* Pour les toitures légères industrielles et hangars métalliques sur la côte Est, la pression de pointe $q_p(z)$ selon l'Eurocode 1-4 dépasse souvent **$2.50\text{ kN/m}^2$**, nécessitant des ancrages stricts des pannes et liernes.

---

## 4. Matériaux Locaux et Pratiques des Bureaux d'Études Malgaches

Pour le paramétrage réaliste du modèle dans TSA ([`src/Model/MaterialLibrary.h`](file:///d:/Tsaraloha%20Platforme/TSA/src/Model/MaterialLibrary.h)) :

1. **Bétons de Structure :**
   - Dosages courants : $350\text{ kg/m}^3$ de ciment Portland CPJ-CEM II/32.5 ou 42.5 (produit localement par Holcim/Lafarge Madagascar).
   - Classes caractéristiques réelles obtenues aux essais LNTPB : $C20/25$ ($f_{ck} = 20\text{ MPa}$) à $C25/30$ ($f_{ck} = 25\text{ MPa}$).
2. **Aciers pour Béton Armé :**
   - Barres à Haute Adhérence (HA) : Nuance $FeE400$ ($f_{yk} = 400\text{ MPa}$) ou $FeE500$ ($f_{yk} = 500\text{ MPa}$).
3. **Maçonneries & Remplissages :**
   - Briques de terre cuite artisanales ou semi-industrielles : Poids volumique $\gamma \approx 16\text{ à }18\text{ kN/m}^3$, résistance moyenne en compression $f_b \approx 3\text{ à }5\text{ MPa}$.
4. **Sols et Géotechnique (Sols Tropicaux Latéritiques) :**
   - Sols rouges latéritiques altérés des Hautes Terres : Cohésion effective $c' \approx 15\text{ à }30\text{ kPa}$, angle de frottement interne $\phi' \approx 20^\circ\text{ à }26^\circ$, contrainte admissible sous semelle filante courante $q_{adm} \approx 0.15\text{ à }0.25\text{ MPa}$ ($1.5\text{ à }2.5\text{ bars}$).
   - Terrains compressibles de rizières et bas-fonds alluvionnaires (plaine du Betsimitatatra à Antananarivo) : Très faibles caractéristiques nécessitant remblais de préchargement ou fondations sur pieux/puits en béton armé.

---

## 5. Références Techniques & Publications

1. **Institut et Observatoire de Géophysique d'Antananarivo (IOGA)** — *Rapports annuels de sismicité et cartes d'aléa sismique de Madagascar*.
2. **BNGRC** — *Directives nationales d'habitat et de construction résistante aux cyclones et inondations*.
3. **Ministère des Travaux Publics / LNTPB** — *Cahier des Prescriptions Techniques Générales (CPTG) applicables aux travaux routiers et ouvrages d'art*.
4. **Andrianaivo, L. et al.** — *Seismotectonics and Seismic Hazard Assessment in Central Madagascar*, Journal of African Earth Sciences.
