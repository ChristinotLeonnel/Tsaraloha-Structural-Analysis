# Rapport de validation TSA

- Version de TSA : 0.1.0
- Moteurs : Custom2D (TSALab MetDeDeplacement), OpenSees 3.8.0
- Date : 2026-10-31T14:04:31
- Poste : Windows 11 Version 25H2 / x86_64
- Vérifications : 146 réussies, 0 en échec, 0 non disponibles ; 0 cas ignoré(s) (moteur indisponible)

Tolérance mixte : |x_TSA − x_réf| ≤ ε_abs + ε_rel·|x_réf|. Une valeur interpolée entre stations est signalée.

## FEA-01 — Plaque carrée simplement appuyée — quadrangles ShellMITC4 (OpenSees 3.8.0)

| Grandeur | Formule | Référence | TSA | Écart abs. | Écart rel. | Tolérance | Statut | Source | Remarque |
|---|---|---|---|---|---|---|---|---|---|
| flèche au centre (vers le bas) (surface:uz:absmax) | -0.00406*q*a^4/D | -0.0024441724 m | -0.0025090167 | 6.4844292e-05 | 2.65 % | 1e-09 + 0.05·|réf| | RÉUSSI | analytique | convergence : h=1 (36 quadrangles, 0 triangles) → -0.002451 h=0.5 (144 quadrangles, 0 triangles) → -0.002484 h=0.25 (576 quadrangles, 0 triangles) → -0.002509 |
| Σ réactions = q·a² (reactionsum:fz) | q*a^2 | 360 kN | 360 | 1.4028956e-10 | 3.9e-11 % | 1e-09 + 1e-06·|réf| | RÉUSSI | analytique | convergence : h=1 (36 quadrangles, 0 triangles) → 360.000000 h=0.5 (144 quadrangles, 0 triangles) → 360.000000 h=0.25 (576 quadrangles, 0 triangles) → 360.000000 |

Durée : 2.09 s

## FEA-02 — Plaque carrée simplement appuyée — triangles ShellDKGT (OpenSees 3.8.0)

| Grandeur | Formule | Référence | TSA | Écart abs. | Écart rel. | Tolérance | Statut | Source | Remarque |
|---|---|---|---|---|---|---|---|---|---|
| flèche au centre (vers le bas) (surface:uz:absmax) | -0.00406*q*a^4/D | -0.0024441724 m | -0.0024422837 | 1.8886408e-06 | 0.0773 % | 1e-09 + 0.05·|réf| | RÉUSSI | analytique | convergence : h=1 (0 quadrangles, 104 triangles) → -0.002392 h=0.5 (0 quadrangles, 378 triangles) → -0.002429 h=0.25 (0 quadrangles, 1540 triangles) → -0.002442 |
| Σ réactions = q·a² (reactionsum:fz) | q*a^2 | 360 kN | 360 | 1.4373745e-07 | 3.99e-08 % | 1e-09 + 1e-06·|réf| | RÉUSSI | analytique | convergence : h=1 (0 quadrangles, 104 triangles) → 360.000000 h=0.5 (0 quadrangles, 378 triangles) → 360.000000 h=0.25 (0 quadrangles, 1540 triangles) → 360.000000 |

Durée : 0.62 s

## FEA-03 — Bande de coque en console (théorie des poutres, ν = 0) (OpenSees 3.8.0)

| Grandeur | Formule | Référence | TSA | Écart abs. | Écart rel. | Tolérance | Statut | Source | Remarque |
|---|---|---|---|---|---|---|---|---|---|
| flèche en bout (vers le bas) (surface:uz:absmax) | -q*bw*L^4/(8*E*I) | -0.015483871 m | -0.015514839 | 3.0967742e-05 | 0.2 % | 1e-09 + 0.01·|réf| | RÉUSSI | analytique | convergence : h=0.5 (16 quadrangles, 0 triangles) → -0.015515 h=0.25 (64 quadrangles, 0 triangles) → -0.015515 h=0.125 (256 quadrangles, 0 triangles) → -0.015515 |
| Σ réactions (reactionsum:fz) | q*bw*L | 40 kN | 40 | 4.2525983e-11 | 1.06e-10 % | 1e-09 + 1e-06·|réf| | RÉUSSI | analytique | convergence : h=0.5 (16 quadrangles, 0 triangles) → 40.000000 h=0.25 (64 quadrangles, 0 triangles) → 40.000000 h=0.125 (256 quadrangles, 0 triangles) → 40.000000 |

Durée : 0.18 s

## FEA-04 — Dalle avec trémie — équilibre et aire nette (OpenSees 3.8.0)

| Grandeur | Formule | Référence | TSA | Écart abs. | Écart rel. | Tolérance | Statut | Source | Remarque |
|---|---|---|---|---|---|---|---|---|---|
| Σ réactions (reactionsum:fz) | (q+g)*An | 219.891 kN | 219.891 | 6.849632e-12 | 3.12e-12 % | 1e-09 + 1e-06·|réf| | RÉUSSI | analytique |  |

Durée : 0.19 s

## FEA-05 — Mode propre d'une bande de coque en console (OpenSees 3.8.0)

| Grandeur | Formule | Référence | TSA | Écart abs. | Écart rel. | Tolérance | Statut | Source | Remarque |
|---|---|---|---|---|---|---|---|---|---|
| fréquence fondamentale (modal:f1) | 1.875104^2/(2*pi*L^2)*sqrt(E*I/m) | 7.1105155 Hz | 7.1451481 | 0.034632626 | 0.487 % | 1e-09 + 0.02·|réf| | RÉUSSI | analytique |  |
| masse totale (modal:mass:z) | rho*e*L*bw | 2 t | 2 | 0 | 0 % | 1e-09 + 1e-06·|réf| | RÉUSSI | analytique |  |

Durée : 1.15 s

## FEA-06 — Mode propre d'une poutre sur deux appuis (masses réparties) (OpenSees 3.8.0)

| Grandeur | Formule | Référence | TSA | Écart abs. | Écart rel. | Tolérance | Statut | Source | Remarque |
|---|---|---|---|---|---|---|---|---|---|
| mode 1 : flexion latérale (I_z) (modal:f1) | pi/(2*L^2)*sqrt(E*h*b^3/12/m) | 13.029615 Hz | 13.029609 | 5.5411151e-06 | 4.25e-05 % | 1e-09 + 0.005·|réf| | RÉUSSI | analytique |  |
| mode 2 : flexion verticale (I_y) (modal:f2) | pi/(2*L^2)*sqrt(E*I/m) | 26.059229 Hz | 26.059218 | 1.1082229e-05 | 4.25e-05 % | 1e-09 + 0.005·|réf| | RÉUSSI | analytique |  |

Durée : 0.05 s

## FEA-07 — Plaque carrée, appui simple « dur » — ShellMITC4 contre la référence de Mindlin (OpenSees 3.8.0)

| Grandeur | Formule | Référence | TSA | Écart abs. | Écart rel. | Tolérance | Statut | Source | Remarque |
|---|---|---|---|---|---|---|---|---|---|
| flèche au centre (vers le bas), référence de Mindlin (surface:uz:absmax) | -(alpha*q*a^4/D + beta*q*a^2/(k*G*e)) | -0.0024579085 m | -0.0024575492 | 3.5930186e-07 | 0.0146 % | 1e-09 + 0.01·|réf| | RÉUSSI | analytique | convergence : h=1 (36 quadrangles, 0 triangles) → -0.002434 h=0.5 (144 quadrangles, 0 triangles) → -0.002452 h=0.25 (576 quadrangles, 0 triangles) → -0.002456 h=0.125 (2304 quadrangles, 0 triangles) → -0.002458 |
| Σ réactions = q·a² (reactionsum:fz) | q*a^2 | 360 kN | 360 | 8.3383611e-10 | 2.32e-10 % | 1e-09 + 1e-06·|réf| | RÉUSSI | analytique | convergence : h=1 (36 quadrangles, 0 triangles) → 360.000000 h=0.5 (144 quadrangles, 0 triangles) → 360.000000 h=0.25 (576 quadrangles, 0 triangles) → 360.000000 h=0.125 (2304 quadrangles, 0 triangles) → 360.000000 |

Durée : 1.14 s

## RDM-01 — Barre en traction (OpenSees 3.8.0)

| Grandeur | Formule | Référence | TSA | Écart abs. | Écart rel. | Tolérance | Statut | Source | Remarque |
|---|---|---|---|---|---|---|---|---|---|
| allongement δ (node:2:ux) | P*L/(E*A) | 1.7857143e-05 m | 1.7857143e-05 | 6.7762636e-21 | 3.79e-14 % | 1e-09 + 1e-06·|réf| | RÉUSSI | analytique |  |
| effort normal (traction > 0) (member:1:N:start) | P | 100 kN | 100 | 0 | 0 % | 1e-09 + 1e-06·|réf| | RÉUSSI | analytique |  |
| réaction (reaction:1:fx) | -P | -100 kN | -100 | 0 | 0 % | 1e-09 + 1e-06·|réf| | RÉUSSI | analytique |  |
| contrainte normale |σ| = |N|/A (member:1:sigma:absmax) | abs(P)/A | 1250 kPa | 1250 | 0 | 0 % | 1e-09 + 1e-06·|réf| | RÉUSSI | analytique | 21 stations du moteur |

Durée : 0.04 s

## RDM-01 — Barre en traction (Custom2D (TSALab MetDeDeplacement))

| Grandeur | Formule | Référence | TSA | Écart abs. | Écart rel. | Tolérance | Statut | Source | Remarque |
|---|---|---|---|---|---|---|---|---|---|
| allongement δ (node:2:ux) | P*L/(E*A) | 1.7857143e-05 m | 1.7857143e-05 | 3.3881318e-21 | 1.9e-14 % | 1e-09 + 1e-06·|réf| | RÉUSSI | analytique |  |
| effort normal (traction > 0) (member:1:N:start) | P | 100 kN | 100 | 2.8421709e-14 | 2.84e-14 % | 1e-09 + 1e-06·|réf| | RÉUSSI | analytique |  |
| réaction (reaction:1:fx) | -P | -100 kN | -100 | 2.8421709e-14 | 2.84e-14 % | 1e-09 + 1e-06·|réf| | RÉUSSI | analytique |  |
| contrainte normale |σ| = |N|/A (member:1:sigma:absmax) | abs(P)/A | 1250 kPa | 1250 | 4.5474735e-13 | 3.64e-14 % | 1e-09 + 1e-06·|réf| | RÉUSSI | analytique | 43 stations du moteur |

Durée : 0.00 s

## RDM-02 — Barre en compression (OpenSees 3.8.0)

| Grandeur | Formule | Référence | TSA | Écart abs. | Écart rel. | Tolérance | Statut | Source | Remarque |
|---|---|---|---|---|---|---|---|---|---|
| allongement δ (node:2:ux) | P*L/(E*A) | -1.7857143e-05 m | -1.7857143e-05 | 6.7762636e-21 | 3.79e-14 % | 1e-09 + 1e-06·|réf| | RÉUSSI | analytique |  |
| effort normal (traction > 0) (member:1:N:start) | P | -100 kN | -100 | 0 | 0 % | 1e-09 + 1e-06·|réf| | RÉUSSI | analytique |  |
| réaction (reaction:1:fx) | -P | 100 kN | 100 | 0 | 0 % | 1e-09 + 1e-06·|réf| | RÉUSSI | analytique |  |
| contrainte normale |σ| = |N|/A (member:1:sigma:absmax) | abs(P)/A | 1250 kPa | 1250 | 0 | 0 % | 1e-09 + 1e-06·|réf| | RÉUSSI | analytique | 21 stations du moteur |

Durée : 0.03 s

## RDM-02 — Barre en compression (Custom2D (TSALab MetDeDeplacement))

| Grandeur | Formule | Référence | TSA | Écart abs. | Écart rel. | Tolérance | Statut | Source | Remarque |
|---|---|---|---|---|---|---|---|---|---|
| allongement δ (node:2:ux) | P*L/(E*A) | -1.7857143e-05 m | -1.7857143e-05 | 3.3881318e-21 | 1.9e-14 % | 1e-09 + 1e-06·|réf| | RÉUSSI | analytique |  |
| effort normal (traction > 0) (member:1:N:start) | P | -100 kN | -100 | 2.8421709e-14 | 2.84e-14 % | 1e-09 + 1e-06·|réf| | RÉUSSI | analytique |  |
| réaction (reaction:1:fx) | -P | 100 kN | 100 | 2.8421709e-14 | 2.84e-14 % | 1e-09 + 1e-06·|réf| | RÉUSSI | analytique |  |
| contrainte normale |σ| = |N|/A (member:1:sigma:absmax) | abs(P)/A | 1250 kPa | 1250 | 4.5474735e-13 | 3.64e-14 % | 1e-09 + 1e-06·|réf| | RÉUSSI | analytique | 43 stations du moteur |

Durée : 0.00 s

## RDM-03 — Poutre sur deux appuis — charge uniforme (OpenSees 3.8.0)

| Grandeur | Formule | Référence | TSA | Écart abs. | Écart rel. | Tolérance | Statut | Source | Remarque |
|---|---|---|---|---|---|---|---|---|---|
| flèche à mi-portée (node:2:uz) | -5*q*L^4/(384*E*I) | -0.00075334821 m | -0.00075334821 | 2.1684043e-19 | 2.88e-14 % | 1e-09 + 1e-06·|réf| | RÉUSSI | analytique |  |
| réaction gauche (reaction:1:fz) | q*L/2 | 30 kN | 30 | 0 | 0 % | 1e-09 + 1e-06·|réf| | RÉUSSI | analytique |  |
| réaction droite (reaction:3:fz) | q*L/2 | 30 kN | 30 | 0 | 0 % | 1e-09 + 1e-06·|réf| | RÉUSSI | analytique |  |
| moment maximal (member:1:M:absmax) | q*L^2/8 | 45 kN·m | 45 | 7.1054274e-15 | 1.58e-14 % | 1e-09 + 1e-06·|réf| | RÉUSSI | analytique | 21 stations du moteur |
| effort tranchant d'appui (member:1:V:absmax) | q*L/2 | 30 kN | 30 | 0 | 0 % | 1e-09 + 1e-06·|réf| | RÉUSSI | analytique | 21 stations du moteur |
| rotation d'appui (node:1:ry) | q*L^3/(24*E*I) | 0.00040178571 rad | 0.00040178571 | 5.4210109e-20 | 1.35e-14 % | 1e-09 + 1e-06·|réf| | RÉUSSI | analytique |  |

Durée : 0.04 s

## RDM-03 — Poutre sur deux appuis — charge uniforme (Custom2D (TSALab MetDeDeplacement))

| Grandeur | Formule | Référence | TSA | Écart abs. | Écart rel. | Tolérance | Statut | Source | Remarque |
|---|---|---|---|---|---|---|---|---|---|
| flèche à mi-portée (node:2:uz) | -5*q*L^4/(384*E*I) | -0.00075334821 m | -0.00075334821 | 2.1684043e-19 | 2.88e-14 % | 1e-09 + 1e-06·|réf| | RÉUSSI | analytique |  |
| réaction gauche (reaction:1:fz) | q*L/2 | 30 kN | 30 | 1.0658141e-14 | 3.55e-14 % | 1e-09 + 1e-06·|réf| | RÉUSSI | analytique |  |
| réaction droite (reaction:3:fz) | q*L/2 | 30 kN | 30 | 2.1316282e-14 | 7.11e-14 % | 1e-09 + 1e-06·|réf| | RÉUSSI | analytique |  |
| moment maximal (member:1:M:absmax) | q*L^2/8 | 45 kN·m | 45 | 0 | 0 % | 1e-09 + 1e-06·|réf| | RÉUSSI | analytique | 43 stations du moteur |
| effort tranchant d'appui (member:1:V:absmax) | q*L/2 | 30 kN | 30 | 1.0658141e-14 | 3.55e-14 % | 1e-09 + 1e-06·|réf| | RÉUSSI | analytique | 43 stations du moteur |
| rotation d'appui (node:1:ry) | q*L^3/(24*E*I) | 0.00040178571 rad | 0.00040178571 | 2.1684043e-19 | 5.4e-14 % | 1e-09 + 1e-06·|réf| | RÉUSSI | analytique |  |

Durée : 0.00 s

## RDM-03b — Efforts et déformée le long d'une barre unique (stations) (OpenSees 3.8.0)

| Grandeur | Formule | Référence | TSA | Écart abs. | Écart rel. | Tolérance | Statut | Source | Remarque |
|---|---|---|---|---|---|---|---|---|---|
| moment en travée (station) (member:1:M:absmax) | q*L^2/8 | 45 kN·m | 45 | 0 | 0 % | 1e-09 + 1e-06·|réf| | RÉUSSI | analytique | 21 stations du moteur |
| effort tranchant d'appui (member:1:V:absmax) | q*L/2 | 30 kN | 30 | 0 | 0 % | 1e-09 + 1e-06·|réf| | RÉUSSI | analytique | 21 stations du moteur |
| moment au quart de portée (q·x·(L − x)/2) (member:1:M:at=0.25) | q*(L/4)*(L - L/4)/2 | 33.75 kN·m | 33.75 | 0 | 0 % | 1e-09 + 1e-06·|réf| | RÉUSSI | analytique |  |
| flèche maximale (déformée reconstruite, w local, z local vers le haut) (member:1:uz:absmax) | -5*q*L^4/(384*E*I) | -0.00075334821 m | -0.00075334068 | 7.5334821e-09 | 0.001 % | 1e-09 + 0.0001·|réf| | RÉUSSI | analytique | 21 stations du moteur |

Durée : 0.04 s

## RDM-03b — Efforts et déformée le long d'une barre unique (stations) (Custom2D (TSALab MetDeDeplacement))

| Grandeur | Formule | Référence | TSA | Écart abs. | Écart rel. | Tolérance | Statut | Source | Remarque |
|---|---|---|---|---|---|---|---|---|---|
| moment en travée (station) (member:1:M:absmax) | q*L^2/8 | 45 kN·m | 45 | 0 | 0 % | 1e-09 + 1e-06·|réf| | RÉUSSI | analytique | 43 stations du moteur |
| effort tranchant d'appui (member:1:V:absmax) | q*L/2 | 30 kN | 30 | 3.5527137e-15 | 1.18e-14 % | 1e-09 + 1e-06·|réf| | RÉUSSI | analytique | 43 stations du moteur |
| moment au quart de portée (q·x·(L − x)/2) (member:1:M:at=0.25) | q*(L/4)*(L - L/4)/2 | 33.75 kN·m | 33.75 | 7.1054274e-15 | 2.11e-14 % | 1e-09 + 1e-06·|réf| | RÉUSSI | analytique |  |
| flèche maximale (déformée reconstruite, w local, z local vers le haut) (member:1:uz:absmax) | -5*q*L^4/(384*E*I) | -0.00075334821 m | -0.00075334821 | 1.0842022e-19 | 1.44e-14 % | 1e-09 + 0.0001·|réf| | RÉUSSI | analytique | 43 stations du moteur |

Durée : 0.00 s

## RDM-04 — Poutre sur deux appuis — charge ponctuelle centrale (OpenSees 3.8.0)

| Grandeur | Formule | Référence | TSA | Écart abs. | Écart rel. | Tolérance | Statut | Source | Remarque |
|---|---|---|---|---|---|---|---|---|---|
| flèche (node:2:uz) | -P*L^3/(48*E*I) | -0.0010044643 m | -0.0010044643 | 4.3368087e-19 | 4.32e-14 % | 1e-09 + 1e-06·|réf| | RÉUSSI | analytique |  |
| réaction (reaction:1:fz) | P/2 | 25 kN | 25 | 0 | 0 % | 1e-09 + 1e-06·|réf| | RÉUSSI | analytique |  |
| moment maximal (member:1:M:absmax) | P*L/4 | 75 kN·m | 75 | 0 | 0 % | 1e-09 + 1e-06·|réf| | RÉUSSI | analytique | 21 stations du moteur |
| effort tranchant (member:1:V:absmax) | P/2 | 25 kN | 25 | 0 | 0 % | 1e-09 + 1e-06·|réf| | RÉUSSI | analytique | 21 stations du moteur |

Durée : 0.04 s

## RDM-04 — Poutre sur deux appuis — charge ponctuelle centrale (Custom2D (TSALab MetDeDeplacement))

| Grandeur | Formule | Référence | TSA | Écart abs. | Écart rel. | Tolérance | Statut | Source | Remarque |
|---|---|---|---|---|---|---|---|---|---|
| flèche (node:2:uz) | -P*L^3/(48*E*I) | -0.0010044643 m | -0.0010044643 | 4.3368087e-19 | 4.32e-14 % | 1e-09 + 1e-06·|réf| | RÉUSSI | analytique |  |
| réaction (reaction:1:fz) | P/2 | 25 kN | 25 | 3.5527137e-15 | 1.42e-14 % | 1e-09 + 1e-06·|réf| | RÉUSSI | analytique |  |
| moment maximal (member:1:M:absmax) | P*L/4 | 75 kN·m | 75 | 1.4210855e-14 | 1.89e-14 % | 1e-09 + 1e-06·|réf| | RÉUSSI | analytique | 43 stations du moteur |
| effort tranchant (member:1:V:absmax) | P/2 | 25 kN | 25 | 3.5527137e-15 | 1.42e-14 % | 1e-09 + 1e-06·|réf| | RÉUSSI | analytique | 43 stations du moteur |

Durée : 0.00 s

## RDM-04b — Poutre bi-encastrée — charge ponctuelle excentrée sur la barre (OpenSees 3.8.0)

| Grandeur | Formule | Référence | TSA | Écart abs. | Écart rel. | Tolérance | Statut | Source | Remarque |
|---|---|---|---|---|---|---|---|---|---|
| réaction gauche (reaction:1:fz) | P*b^2*(3*a+b)/L^3 | 29.62963 kN | 29.62963 | 0 | 0 % | 1e-09 + 1e-06·|réf| | RÉUSSI | analytique |  |
| réaction droite (reaction:2:fz) | P*a^2*(a+3*b)/L^3 | 10.37037 kN | 10.37037 | 0 | 0 % | 1e-09 + 1e-06·|réf| | RÉUSSI | analytique |  |
| moment d'encastrement gauche (member:1:M:start) | P*a*b^2/L^2 | 35.555556 kN·m | 35.555556 | 0 | 0 % | 1e-09 + 1e-06·|réf| | RÉUSSI | analytique |  |
| moment d'encastrement droit (member:1:M:end) | P*a^2*b/L^2 | 17.777778 kN·m | 17.777778 | 0 | 0 % | 1e-09 + 1e-06·|réf| | RÉUSSI | analytique |  |
| flèche sous la charge (member:1:uz:at=0.3333333333333333) | -P*a^3*b^3/(3*E*I*L^3) | -0.00014109347 m | -0.00014108044 | 1.3031718e-08 | 0.00924 % | 1e-09 + 0.0001·|réf| | RÉUSSI | analytique |  |

Durée : 0.04 s

## RDM-04b — Poutre bi-encastrée — charge ponctuelle excentrée sur la barre (Custom2D (TSALab MetDeDeplacement))

| Grandeur | Formule | Référence | TSA | Écart abs. | Écart rel. | Tolérance | Statut | Source | Remarque |
|---|---|---|---|---|---|---|---|---|---|
| réaction gauche (reaction:1:fz) | P*b^2*(3*a+b)/L^3 | 29.62963 kN | 29.62963 | 0 | 0 % | 1e-09 + 1e-06·|réf| | RÉUSSI | analytique |  |
| réaction droite (reaction:2:fz) | P*a^2*(a+3*b)/L^3 | 10.37037 kN | 10.37037 | 0 | 0 % | 1e-09 + 1e-06·|réf| | RÉUSSI | analytique |  |
| moment d'encastrement gauche (member:1:M:start) | P*a*b^2/L^2 | 35.555556 kN·m | 35.555556 | 7.1054274e-15 | 2e-14 % | 1e-09 + 1e-06·|réf| | RÉUSSI | analytique |  |
| moment d'encastrement droit (member:1:M:end) | P*a^2*b/L^2 | 17.777778 kN·m | 17.777778 | 7.1054274e-15 | 4e-14 % | 1e-09 + 1e-06·|réf| | RÉUSSI | analytique |  |
| flèche sous la charge (member:1:uz:at=0.3333333333333333) | -P*a^3*b^3/(3*E*I*L^3) | -0.00014109347 m | -0.00014109347 | 1.3552527e-19 | 9.61e-14 % | 1e-09 + 0.0001·|réf| | RÉUSSI | analytique | valeur interpolée entre deux stations |

Durée : 0.00 s

## RDM-05a — Console — force en bout (OpenSees 3.8.0)

| Grandeur | Formule | Référence | TSA | Écart abs. | Écart rel. | Tolérance | Statut | Source | Remarque |
|---|---|---|---|---|---|---|---|---|---|
| flèche en bout (node:2:uz) | -P*L^3/(3*E*I) | -0.0019047619 m | -0.0019047619 | 6.505213e-19 | 3.42e-14 % | 1e-09 + 1e-06·|réf| | RÉUSSI | analytique |  |
| rotation en bout (node:2:ry) | P*L^2/(2*E*I) | 0.00071428571 rad | 0.00071428571 | 2.1684043e-19 | 3.04e-14 % | 1e-09 + 1e-06·|réf| | RÉUSSI | analytique |  |
| réaction verticale (reaction:1:fz) | P | 20 kN | 20 | 0 | 0 % | 1e-09 + 1e-06·|réf| | RÉUSSI | analytique |  |
| moment d'encastrement (reaction:1:my) | -P*L | -80 kN·m | -80 | 0 | 0 % | 1e-09 + 1e-06·|réf| | RÉUSSI | analytique |  |
| moment maximal (member:1:M:absmax) | P*L | 80 kN·m | 80 | 0 | 0 % | 1e-09 + 1e-06·|réf| | RÉUSSI | analytique | 21 stations du moteur |

Durée : 0.04 s

## RDM-05a — Console — force en bout (Custom2D (TSALab MetDeDeplacement))

| Grandeur | Formule | Référence | TSA | Écart abs. | Écart rel. | Tolérance | Statut | Source | Remarque |
|---|---|---|---|---|---|---|---|---|---|
| flèche en bout (node:2:uz) | -P*L^3/(3*E*I) | -0.0019047619 m | -0.0019047619 | 4.3368087e-19 | 2.28e-14 % | 1e-09 + 1e-06·|réf| | RÉUSSI | analytique |  |
| rotation en bout (node:2:ry) | P*L^2/(2*E*I) | 0.00071428571 rad | 0.00071428571 | 3.2526065e-19 | 4.55e-14 % | 1e-09 + 1e-06·|réf| | RÉUSSI | analytique |  |
| réaction verticale (reaction:1:fz) | P | 20 kN | 20 | 0 | 0 % | 1e-09 + 1e-06·|réf| | RÉUSSI | analytique |  |
| moment d'encastrement (reaction:1:my) | -P*L | -80 kN·m | -80 | 2.8421709e-14 | 3.55e-14 % | 1e-09 + 1e-06·|réf| | RÉUSSI | analytique |  |
| moment maximal (member:1:M:absmax) | P*L | 80 kN·m | 80 | 2.8421709e-14 | 3.55e-14 % | 1e-09 + 1e-06·|réf| | RÉUSSI | analytique | 43 stations du moteur |

Durée : 0.00 s

## RDM-05b — Console — charge uniforme (OpenSees 3.8.0)

| Grandeur | Formule | Référence | TSA | Écart abs. | Écart rel. | Tolérance | Statut | Source | Remarque |
|---|---|---|---|---|---|---|---|---|---|
| flèche en bout (node:2:uz) | -q*L^4/(8*E*I) | -0.0011428571 m | -0.0011428571 | 4.3368087e-19 | 3.79e-14 % | 1e-09 + 1e-06·|réf| | RÉUSSI | analytique |  |
| rotation en bout (node:2:ry) | q*L^3/(6*E*I) | 0.00038095238 rad | 0.00038095238 | 1.0842022e-19 | 2.85e-14 % | 1e-09 + 1e-06·|réf| | RÉUSSI | analytique |  |
| réaction (reaction:1:fz) | q*L | 32 kN | 32 | 7.1054274e-15 | 2.22e-14 % | 1e-09 + 1e-06·|réf| | RÉUSSI | analytique |  |
| moment d'encastrement (reaction:1:my) | -q*L^2/2 | -64 kN·m | -64 | 2.8421709e-14 | 4.44e-14 % | 1e-09 + 1e-06·|réf| | RÉUSSI | analytique |  |

Durée : 0.03 s

## RDM-05b — Console — charge uniforme (Custom2D (TSALab MetDeDeplacement))

| Grandeur | Formule | Référence | TSA | Écart abs. | Écart rel. | Tolérance | Statut | Source | Remarque |
|---|---|---|---|---|---|---|---|---|---|
| flèche en bout (node:2:uz) | -q*L^4/(8*E*I) | -0.0011428571 m | -0.0011428571 | 4.3368087e-19 | 3.79e-14 % | 1e-09 + 1e-06·|réf| | RÉUSSI | analytique |  |
| rotation en bout (node:2:ry) | q*L^3/(6*E*I) | 0.00038095238 rad | 0.00038095238 | 1.6263033e-19 | 4.27e-14 % | 1e-09 + 1e-06·|réf| | RÉUSSI | analytique |  |
| réaction (reaction:1:fz) | q*L | 32 kN | 32 | 7.1054274e-15 | 2.22e-14 % | 1e-09 + 1e-06·|réf| | RÉUSSI | analytique |  |
| moment d'encastrement (reaction:1:my) | -q*L^2/2 | -64 kN·m | -64 | 2.8421709e-14 | 4.44e-14 % | 1e-09 + 1e-06·|réf| | RÉUSSI | analytique |  |

Durée : 0.00 s

## RDM-06 — Portique encastré — effort horizontal en tête (OpenSees 3.8.0)

| Grandeur | Formule | Référence | TSA | Écart abs. | Écart rel. | Tolérance | Statut | Source | Remarque |
|---|---|---|---|---|---|---|---|---|---|
| déplacement en tête (node:2:ux) | H/K | 0.00043407885 m | 0.00043587557 | 1.7967248e-06 | 0.414 % | 1e-09 + 0.01·|réf| | RÉUSSI | analytique |  |
| déplacement en tête (droite) (node:3:ux) | H/K | 0.00043407885 m | 0.00043397421 | 1.0464358e-07 | 0.0241 % | 1e-09 + 0.01·|réf| | RÉUSSI | analytique |  |
| Σ réactions horizontales (reactionsum:fx) | -H | -20 kN | -20 | 3.5527137e-15 | 1.78e-14 % | 1e-06 + 1e-06·|réf| | RÉUSSI | analytique |  |
| Σ réactions verticales (reactionsum:fz) |  | 0 kN | 0 | 0 | — | 1e-06 + 0·|réf| | RÉUSSI | analytique |  |

Durée : 0.04 s

## RDM-06 — Portique encastré — effort horizontal en tête (Custom2D (TSALab MetDeDeplacement))

| Grandeur | Formule | Référence | TSA | Écart abs. | Écart rel. | Tolérance | Statut | Source | Remarque |
|---|---|---|---|---|---|---|---|---|---|
| déplacement en tête (node:2:ux) | H/K | 0.00043407885 m | 0.00043587557 | 1.7967248e-06 | 0.414 % | 1e-09 + 0.01·|réf| | RÉUSSI | analytique |  |
| déplacement en tête (droite) (node:3:ux) | H/K | 0.00043407885 m | 0.00043397421 | 1.0464358e-07 | 0.0241 % | 1e-09 + 0.01·|réf| | RÉUSSI | analytique |  |
| Σ réactions horizontales (reactionsum:fx) | -H | -20 kN | -20 | 1.3500312e-13 | 6.75e-13 % | 1e-06 + 1e-06·|réf| | RÉUSSI | analytique |  |
| Σ réactions verticales (reactionsum:fz) |  | 0 kN | 0 | 0 | — | 1e-06 + 0·|réf| | RÉUSSI | analytique |  |

Durée : 0.00 s

## RDM-07a — Treillis plan triangulaire (OpenSees 3.8.0)

| Grandeur | Formule | Référence | TSA | Écart abs. | Écart rel. | Tolérance | Statut | Source | Remarque |
|---|---|---|---|---|---|---|---|---|---|
| effort dans une diagonale (compression) (member:2:N:start) | -P/(2*s) | -18.027756 kN | -18.027756 | 3.5527137e-15 | 1.97e-14 % | 1e-09 + 1e-06·|réf| | RÉUSSI | analytique |  |
| effort dans la membrure (traction) (member:1:N:start) | P/(2*t) | 10 kN | 10 | 0 | 0 % | 1e-09 + 1e-06·|réf| | RÉUSSI | analytique |  |
| flèche du sommet (travaux virtuels) (node:3:uz) | -P/(E*A)*(2*Ld/(4*s^2) + Lb/(4*t^2)) | -0.00022179496 m | -0.00022179496 | 2.7105054e-20 | 1.22e-14 % | 1e-09 + 1e-06·|réf| | RÉUSSI | analytique |  |
| réaction (reaction:1:fz) | P/2 | 15 kN | 15 | 0 | 0 % | 1e-09 + 1e-06·|réf| | RÉUSSI | analytique |  |

Durée : 0.04 s

## RDM-07b — Treillis spatial — trépied (OpenSees 3.8.0)

| Grandeur | Formule | Référence | TSA | Écart abs. | Écart rel. | Tolérance | Statut | Source | Remarque |
|---|---|---|---|---|---|---|---|---|---|
| effort dans un pied (member:1:N:start) | -P/(3*c) | -24.037009 kN | -24.037009 | 0 | 0 % | 1e-09 + 1e-06·|réf| | RÉUSSI | analytique |  |
| effort dans un pied (3) (member:3:N:start) | -P/(3*c) | -24.037009 kN | -24.037009 | 3.8724579e-13 | 1.61e-12 % | 1e-09 + 1e-06·|réf| | RÉUSSI | analytique |  |
| déplacement vertical du sommet (node:1:uz) | -P*Ll/(3*E*A*c^2) | -0.00025261162 m | -0.00025261162 | 8.3483567e-18 | 3.3e-12 % | 1e-09 + 1e-06·|réf| | RÉUSSI | analytique |  |
| déplacement horizontal (symétrie) (node:1:ux) |  | 0 m | -1.2431579e-17 | 1.2431579e-17 | — | 1e-12 + 0·|réf| | RÉUSSI | analytique |  |

Durée : 0.03 s

## RDM-08a — Torsion d'une barre circulaire (OpenSees 3.8.0)

| Grandeur | Formule | Référence | TSA | Écart abs. | Écart rel. | Tolérance | Statut | Source | Remarque |
|---|---|---|---|---|---|---|---|---|---|
| rotation de torsion (node:2:rx) | T*L/(G*J) | 0.012611135 rad | 0.012611135 | 1.7347235e-18 | 1.38e-14 % | 1e-09 + 1e-06·|réf| | RÉUSSI | analytique |  |
| moment de torsion (member:1:T:start) | T | 5 kN·m | 5 | 0 | 0 % | 1e-09 + 1e-06·|réf| | RÉUSSI | analytique |  |
| réaction (reaction:1:mx) | -T | -5 kN·m | -5 | 0 | 0 % | 1e-09 + 1e-06·|réf| | RÉUSSI | analytique |  |

Durée : 0.04 s

## RDM-08b — Torsion d'une barre rectangulaire (Saint-Venant) (OpenSees 3.8.0)

| Grandeur | Formule | Référence | TSA | Écart abs. | Écart rel. | Tolérance | Statut | Source | Remarque |
|---|---|---|---|---|---|---|---|---|---|
| rotation de torsion (node:2:rx) | T*L/(G*J) | 0.00016918698 rad | 0.00016904247 | 1.4450868e-07 | 0.0854 % | 1e-09 + 0.002·|réf| | RÉUSSI | analytique |  |

Durée : 0.03 s

## RDM-09 — Flexion composée — console (OpenSees 3.8.0)

| Grandeur | Formule | Référence | TSA | Écart abs. | Écart rel. | Tolérance | Statut | Source | Remarque |
|---|---|---|---|---|---|---|---|---|---|
| contrainte normale maximale (member:1:sigma:absmax) | N/A + F*L/W | 8125 kPa | 8125 | 0 | 0 % | 1e-09 + 1e-06·|réf| | RÉUSSI | analytique | 21 stations du moteur |
| effort normal (member:1:N:start) | N | 200 kN | 200 | 0 | 0 % | 1e-09 + 1e-06·|réf| | RÉUSSI | analytique |  |
| moment à l'encastrement (member:1:M:absmax) | F*L | 30 kN·m | 30 | 0 | 0 % | 1e-09 + 1e-06·|réf| | RÉUSSI | analytique | 21 stations du moteur |

Durée : 0.03 s

## RDM-09 — Flexion composée — console (Custom2D (TSALab MetDeDeplacement))

| Grandeur | Formule | Référence | TSA | Écart abs. | Écart rel. | Tolérance | Statut | Source | Remarque |
|---|---|---|---|---|---|---|---|---|---|
| contrainte normale maximale (member:1:sigma:absmax) | N/A + F*L/W | 8125 kPa | 8125 | 1.8189894e-12 | 2.24e-14 % | 1e-09 + 1e-06·|réf| | RÉUSSI | analytique | 43 stations du moteur |
| effort normal (member:1:N:start) | N | 200 kN | 200 | 5.6843419e-14 | 2.84e-14 % | 1e-09 + 1e-06·|réf| | RÉUSSI | analytique |  |
| moment à l'encastrement (member:1:M:absmax) | F*L | 30 kN·m | 30 | 3.5527137e-15 | 1.18e-14 % | 1e-09 + 1e-06·|réf| | RÉUSSI | analytique | 43 stations du moteur |

Durée : 0.00 s

## RDM-10a — Poutre encastrée-appuyée — charge uniforme (hyperstatique) (OpenSees 3.8.0)

| Grandeur | Formule | Référence | TSA | Écart abs. | Écart rel. | Tolérance | Statut | Source | Remarque |
|---|---|---|---|---|---|---|---|---|---|
| réaction de l'appui simple (reaction:3:fz) | 3*q*L/8 | 36 kN | 36 | 0 | 0 % | 1e-09 + 1e-06·|réf| | RÉUSSI | analytique |  |
| réaction de l'encastrement (reaction:1:fz) | 5*q*L/8 | 60 kN | 60 | 0 | 0 % | 1e-09 + 1e-06·|réf| | RÉUSSI | analytique |  |
| moment d'encastrement (composante globale autour de +Y) (reaction:1:my) | -q*L^2/8 | -96 kN·m | -96 | 0 | 0 % | 1e-09 + 1e-06·|réf| | RÉUSSI | analytique |  |

Durée : 0.04 s

## RDM-10a — Poutre encastrée-appuyée — charge uniforme (hyperstatique) (Custom2D (TSALab MetDeDeplacement))

| Grandeur | Formule | Référence | TSA | Écart abs. | Écart rel. | Tolérance | Statut | Source | Remarque |
|---|---|---|---|---|---|---|---|---|---|
| réaction de l'appui simple (reaction:3:fz) | 3*q*L/8 | 36 kN | 36 | 0 | 0 % | 1e-09 + 1e-06·|réf| | RÉUSSI | analytique |  |
| réaction de l'encastrement (reaction:1:fz) | 5*q*L/8 | 60 kN | 60 | 1.4210855e-14 | 2.37e-14 % | 1e-09 + 1e-06·|réf| | RÉUSSI | analytique |  |
| moment d'encastrement (composante globale autour de +Y) (reaction:1:my) | -q*L^2/8 | -96 kN·m | -96 | 2.8421709e-14 | 2.96e-14 % | 1e-09 + 1e-06·|réf| | RÉUSSI | analytique |  |

Durée : 0.00 s

## RDM-10b — Poutre continue à deux travées égales (OpenSees 3.8.0)

| Grandeur | Formule | Référence | TSA | Écart abs. | Écart rel. | Tolérance | Statut | Source | Remarque |
|---|---|---|---|---|---|---|---|---|---|
| réaction centrale (reaction:2:fz) | 5*q*L/4 | 62.5 kN | 62.5 | 0 | 0 % | 1e-09 + 1e-06·|réf| | RÉUSSI | analytique |  |
| réaction de rive (reaction:1:fz) | 3*q*L/8 | 18.75 kN | 18.75 | 0 | 0 % | 1e-09 + 1e-06·|réf| | RÉUSSI | analytique |  |
| moment sur appui central (valeur absolue) (member:1:M:absmax) | q*L^2/8 | 31.25 kN·m | 31.25 | 0 | 0 % | 1e-09 + 1e-06·|réf| | RÉUSSI | analytique | 21 stations du moteur |

Durée : 0.03 s

## RDM-10b — Poutre continue à deux travées égales (Custom2D (TSALab MetDeDeplacement))

| Grandeur | Formule | Référence | TSA | Écart abs. | Écart rel. | Tolérance | Statut | Source | Remarque |
|---|---|---|---|---|---|---|---|---|---|
| réaction centrale (reaction:2:fz) | 5*q*L/4 | 62.5 kN | 62.5 | 0 | 0 % | 1e-09 + 1e-06·|réf| | RÉUSSI | analytique |  |
| réaction de rive (reaction:1:fz) | 3*q*L/8 | 18.75 kN | 18.75 | 3.5527137e-15 | 1.89e-14 % | 1e-09 + 1e-06·|réf| | RÉUSSI | analytique |  |
| moment sur appui central (valeur absolue) (member:1:M:absmax) | q*L^2/8 | 31.25 kN·m | 31.25 | 1.4210855e-14 | 4.55e-14 % | 1e-09 + 1e-06·|réf| | RÉUSSI | analytique | 43 stations du moteur |

Durée : 0.00 s

## RDM-10c — Poutre bi-encastrée — charge triangulaire (OpenSees 3.8.0)

| Grandeur | Formule | Référence | TSA | Écart abs. | Écart rel. | Tolérance | Statut | Source | Remarque |
|---|---|---|---|---|---|---|---|---|---|
| réaction gauche (reaction:1:fz) | 3*q*L/20 | 13.5 kN | 13.5 | 0 | 0 % | 1e-09 + 1e-06·|réf| | RÉUSSI | analytique |  |
| réaction droite (reaction:2:fz) | 7*q*L/20 | 31.5 kN | 31.5 | 1.0658141e-14 | 3.38e-14 % | 1e-09 + 1e-06·|réf| | RÉUSSI | analytique |  |
| moment d'encastrement gauche (autour de +Y) (reaction:1:my) | -q*L^2/30 | -18 kN·m | -18 | 0 | 0 % | 1e-09 + 1e-06·|réf| | RÉUSSI | analytique |  |
| moment d'encastrement droit (autour de +Y) (reaction:2:my) | q*L^2/20 | 27 kN·m | 27 | 0 | 0 % | 1e-09 + 1e-06·|réf| | RÉUSSI | analytique |  |

Durée : 0.04 s

## RDM-10c — Poutre bi-encastrée — charge triangulaire (Custom2D (TSALab MetDeDeplacement))

| Grandeur | Formule | Référence | TSA | Écart abs. | Écart rel. | Tolérance | Statut | Source | Remarque |
|---|---|---|---|---|---|---|---|---|---|
| réaction gauche (reaction:1:fz) | 3*q*L/20 | 13.5 kN | 13.5 | 7.1054274e-15 | 5.26e-14 % | 1e-09 + 1e-06·|réf| | RÉUSSI | analytique |  |
| réaction droite (reaction:2:fz) | 7*q*L/20 | 31.5 kN | 31.5 | 7.1054274e-15 | 2.26e-14 % | 1e-09 + 1e-06·|réf| | RÉUSSI | analytique |  |
| moment d'encastrement gauche (autour de +Y) (reaction:1:my) | -q*L^2/30 | -18 kN·m | -18 | 1.7763568e-14 | 9.87e-14 % | 1e-09 + 1e-06·|réf| | RÉUSSI | analytique |  |
| moment d'encastrement droit (autour de +Y) (reaction:2:my) | q*L^2/20 | 27 kN·m | 27 | 2.8421709e-14 | 1.05e-13 % | 1e-09 + 1e-06·|réf| | RÉUSSI | analytique |  |

Durée : 0.00 s

## RDM-10d — Poutre sur deux appuis — charge uniforme partielle (OpenSees 3.8.0)

| Grandeur | Formule | Référence | TSA | Écart abs. | Écart rel. | Tolérance | Statut | Source | Remarque |
|---|---|---|---|---|---|---|---|---|---|
| réaction gauche (reaction:1:fz) | q*(bb-a)*(L-(a+bb)/2)/L | 10.833333 kN | 10.833333 | 3.5527137e-15 | 3.28e-14 % | 1e-09 + 1e-06·|réf| | RÉUSSI | analytique |  |
| réaction droite (reaction:2:fz) | q*(bb-a)*((a+bb)/2)/L | 9.1666667 kN | 9.1666667 | 0 | 0 % | 1e-09 + 1e-06·|réf| | RÉUSSI | analytique |  |

Durée : 0.03 s

## RDM-10d — Poutre sur deux appuis — charge uniforme partielle (Custom2D (TSALab MetDeDeplacement))

| Grandeur | Formule | Référence | TSA | Écart abs. | Écart rel. | Tolérance | Statut | Source | Remarque |
|---|---|---|---|---|---|---|---|---|---|
| réaction gauche (reaction:1:fz) | q*(bb-a)*(L-(a+bb)/2)/L | 10.833333 kN | 10.833333 | 1.7763568e-15 | 1.64e-14 % | 1e-09 + 1e-06·|réf| | RÉUSSI | analytique |  |
| réaction droite (reaction:2:fz) | q*(bb-a)*((a+bb)/2)/L | 9.1666667 kN | 9.1666667 | 0 | 0 % | 1e-09 + 1e-06·|réf| | RÉUSSI | analytique |  |

Durée : 0.00 s

## RDM-11a — Solveur non linéaire en géométrie linéaire — charge complète atteinte (OpenSees 3.8.0)

| Grandeur | Formule | Référence | TSA | Écart abs. | Écart rel. | Tolérance | Statut | Source | Remarque |
|---|---|---|---|---|---|---|---|---|---|
| flèche en bout (charge complète) (node:2:uz) | -P*L^3/(3*E*I) | -0.0019047619 m | -0.0019047619 | 4.3368087e-19 | 2.28e-14 % | 1e-09 + 1e-06·|réf| | RÉUSSI | analytique |  |
| réaction (charge complète) (reaction:1:fz) | P | 20 kN | 20 | 1.0658141e-14 | 5.33e-14 % | 1e-09 + 1e-06·|réf| | RÉUSSI | analytique |  |

Durée : 0.04 s

## RDM-11b — Effet du second ordre (P-Δ) — console comprimée avec effort transversal (OpenSees 3.8.0)

| Grandeur | Formule | Référence | TSA | Écart abs. | Écart rel. | Tolérance | Statut | Source | Remarque |
|---|---|---|---|---|---|---|---|---|---|
| déplacement en tête (second ordre) (node:2:ux) | H*(tan(k*L) - k*L)/(P*k) | 0.0023619565 m | 0.0023612584 | 6.980369e-07 | 0.0296 % | 1e-09 + 0.005·|réf| | RÉUSSI | analytique |  |
| réaction horizontale (reaction:1:fx) | -H | -10 kN | -10 | 9.0594199e-14 | 9.06e-13 % | 1e-06 + 1e-06·|réf| | RÉUSSI | analytique |  |
| moment d'encastrement (second ordre : H·L + P·δ) (reaction:1:my) | -(H*L + P*H*(tan(k*L) - k*L)/(P*k)) | -58.895652 kN·m | -58.890067 | 0.0055842952 | 0.00948 % | 1e-06 + 0.005·|réf| | RÉUSSI | analytique |  |

Durée : 0.04 s

## RDM-12 — Poids propre automatique des barres (OpenSees 3.8.0)

| Grandeur | Formule | Référence | TSA | Écart abs. | Écart rel. | Tolérance | Statut | Source | Remarque |
|---|---|---|---|---|---|---|---|---|---|
| réaction (poids propre) (reaction:1:fz) | w*L/2 | 1.657531 kN | 1.6575931 | 6.2115962e-05 | 0.00375 % | 1e-09 + 0.001·|réf| | RÉUSSI | analytique |  |
| Σ réactions = poids de la barre (reactionsum:fz) | w*L | 3.3150619 kN | 3.3151861 | 0.00012423192 | 0.00375 % | 1e-09 + 0.001·|réf| | RÉUSSI | analytique |  |
| moment en travée (member:1:M:absmax) | w*L^2/8 | 3.3150619 kN·m | 3.3151861 | 0.00012423192 | 0.00375 % | 1e-09 + 0.001·|réf| | RÉUSSI | analytique | 21 stations du moteur |

Durée : 0.04 s

## RDM-12 — Poids propre automatique des barres (Custom2D (TSALab MetDeDeplacement))

| Grandeur | Formule | Référence | TSA | Écart abs. | Écart rel. | Tolérance | Statut | Source | Remarque |
|---|---|---|---|---|---|---|---|---|---|
| réaction (poids propre) (reaction:1:fz) | w*L/2 | 1.657531 kN | 1.6575931 | 6.2115962e-05 | 0.00375 % | 1e-09 + 0.001·|réf| | RÉUSSI | analytique |  |
| Σ réactions = poids de la barre (reactionsum:fz) | w*L | 3.3150619 kN | 3.3151861 | 0.00012423192 | 0.00375 % | 1e-09 + 0.001·|réf| | RÉUSSI | analytique |  |
| moment en travée (member:1:M:absmax) | w*L^2/8 | 3.3150619 kN·m | 3.3151861 | 0.00012423192 | 0.00375 % | 1e-09 + 0.001·|réf| | RÉUSSI | analytique | 43 stations du moteur |

Durée : 0.00 s

