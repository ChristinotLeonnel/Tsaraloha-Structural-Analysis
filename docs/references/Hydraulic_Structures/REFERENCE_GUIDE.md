# Ouvrages Hydrauliques, Dalots Cadres & Murs de Soutènement

## 1. Dalots Cadres & Portiques en Béton Armé

Les dalots et ponts-cadres sont des ouvrages d'art enterrés sous remblai assurant l'écoulement hydraulique des cours d'eau ou le passage de canalisations et voies sous voies de circulation.

### 1.1 Typologies selon le Guide SETRA « Ponts-Cadres et Portiques »
* **Pont-cadre fermé (PICF) :** Structure monolithique rectangulaire fermée comprenant :
  - La **traverse supérieure** (tablier recevant les charges de remblai et de trafic).
  - Deux **piédroits verticaux** (murs latéraux reprenant les poussées des terres et de l'eau).
  - Le **radier général inférieur** (répartissant la charge totale sur le sol de fondation).
* **Portique ouvert (PIPO) :** Structure en forme de $U$ inversé sans radier fermé, reposant sur deux semelles filantes indépendantes.

### 1.2 Actions Prises en Compte
1. **Poids propre de la structure en béton armé :** $\gamma_c = 25\text{ kN/m}^3$.
2. **Poids des terres de couverture (remblai supérieur d'épaisseur $h_r$) :**
   $$q_r = \gamma_r \cdot h_r \quad (\gamma_r \approx 18\text{ à }20\text{ kN/m}^3)$$
3. **Poussée latérale des terres sur les piédroits :**
   En raison de la grande rigidité des angles du cadre, le sol n'atteint pas l'état actif complet de Rankine ; on applique le **coefficient de poussée des terres au repos $K_0$** (formule de Jaky) :
   $$K_0 = 1 - \sin\phi' \quad (\text{soit } K_0 \approx 0.50 \text{ pour } \phi' = 30^\circ)$$
   $$\sigma_h(z) = K_0 \left( q + \gamma_r z \right)$$
4. **Pression hydrostatique de l'eau :**
   $$\sigma_w(z) = \gamma_w \cdot z_w \quad (\gamma_w = 9.81\text{ kN/m}^3)$$
5. **Diffusion des charges de trafic dans le remblai :**
   Les charges concentrées des essieux de camions (Tandem LM1 de $150\text{ kN}$ par roue) se diffusent en profondeur selon un angle standard de dispersion $\alpha = 30^\circ$ ou une pente de $2:1$ (Norme EN 1991-2) :
   $$A_{diff}(h_r) = (a + 2 h_r \tan 30^\circ) \times (b + 2 h_r \tan 30^\circ)$$

### 1.3 Formules Analytiques des Moments d'Encastrement aux Coins du Cadre
Pour un cadre fermé de portée entre nus $L$, de hauteur entre axes $H$, d'épaisseur de traverse $e_t$ (inertie $I_t$) et d'épaisseur de piédroit $e_p$ (inertie $I_p$) soumis à une charge verticale uniforme $q$ sur la traverse :

$$M_{coin} = -\frac{q L^2}{12} \frac{1}{1 + k_r} \quad \text{avec} \quad k_r = \frac{I_t \cdot H}{I_p \cdot L}$$

* **Moment fléchissant en travée de la traverse supérieure :**
  $$M_{max,traverse} = \frac{q L^2}{8} - |M_{coin}|$$

---

## 2. Murs de Soutènement

Les murs de soutènement stabilisent des dénivellations de terrain. TSA traite les murs cantilever en béton armé (profil en Té inversé) et les murs poids en maçonnerie/béton cyclopéen.

### 2.1 Théories de Poussée des Terres (Rankine & Coulomb)
* **Poussée active de Rankine (sol sans cohésion, frottement mur-sol négligé) :**
  $$K_a = \tan^2\left(45^\circ - \frac{\phi'}{2}\right) = \frac{1 - \sin\phi'}{1 + \sin\phi'}$$
  $$P_a = \frac{1}{2} K_a \gamma H^2$$
  Le point d'application se situe au tiers inférieur ($H/3$).
* **Poussée de Coulomb (prenant en compte le frottement mur-sol $\delta \approx 2/3 \phi'$) :**
  $$K_a = \frac{\cos^2(\phi' - \alpha)}{\cos^2\alpha \cos(\alpha + \delta) \left[ 1 + \sqrt{\frac{\sin(\phi' + \delta)\sin(\phi' - \beta)}{\cos(\alpha + \delta)\cos(\alpha - \beta)}} \right]^2}$$

### 2.2 Justifications de Stabilité d'Ensemble
Pour valider un mur de soutènement sous combinaisons ELU/ELS :

1. **Sécurité au Renversement ($FS_r$) :**
   $$FS_r = \frac{\sum M_{stabilisateurs}}{\sum M_{renversants}} \ge 1.50 \quad \text{(ou vérification excentricité ELS } e \le B/6 \text{)}$$
2. **Sécurité au Glissement sur la Base ($FS_g$) :**
   $$FS_g = \frac{N \tan\delta_b + c_b B'}{T_h} \ge 1.50$$
3. **Portance du Sol de Fondation (Capacité portante Meyerhof) :**
   $$q_{ref} = \frac{N}{B - 2e} \le q_{adm}$$
   où $e = \frac{B}{2} - \frac{\sum M_{net}}{N}$ est l'excentricité de la résultante des charges sur la semelle.

---

## 3. Documents de Référence Officiels

1. **SETRA** (1998) — *Ponts-cadres et portiques : Guide de conception technique*.
2. **Cerema** (2015) — *Recommandations pour la conception et le dimensionnement des dalots et passages d'eau*.
3. **Ministère des Transports (France)** — *Fascicule 61 titre II : Conception des ponts et dalots routiers*.
4. **Eurocode 7 (EN 1997-1)** — *Section 9 : Ouvrages de soutènement*.
