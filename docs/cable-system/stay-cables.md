# SYSTÈMES DE HAUBANS (STAY CABLES)

Les ponts à haubans et structures haubanées nécessitent une modélisation précise de la géométrie des faisceaux, de la transmission des efforts aux pylônes et au tablier, ainsi que de la raideur apparente sous chargement.

---

## 1. Dispositions Géométriques Normalisées

Le module `TSA::Model::StayCable` prend en charge les trois familles classiques de disposition de nappes :

### A. Éventail (Fan System)
- Tous les haubans convergent vers une zone d'ancrage compacte au sommet du pylône.
- **Avantage structurel** : Inclinaison maximale des haubans, offrant la meilleure efficacité axiale pour reprendre les charges verticales du tablier.
- Angle d'inclinaison par rapport à l'horizontale :
  $$\theta_i = \arctan\left(\frac{z_{sommet} - z_{tablier,i}}{|x_{sommet} - x_{tablier,i}|}\right)$$

### B. Harpe (Harp System)
- Les haubans sont tous parallèles les uns aux autres ($\theta_i \approx \text{cste}$).
- Les ancrages s'échelonnent régulièrement le long du mât du pylône et du tablier.
- **Avantage esthétique** : Parallélisme visuel très pur.
- **Contrainte mécanique** : Efforts de flexion accrus dans le pylône.

### C. Semi-Éventail (Semi-Fan / Modified Fan)
- Compromis moderne (le plus utilisé au monde, ex: Viaduc de Millau, Pont de Normandie).
- Les ancrages en tête de pylône sont espacés d'une distance minimale (ex: 1.0 à 1.5 m) pour loger les trompettes d'ancrage et les vérins de mise en tension, tout en maintenant une forte inclinaison.

---

## 2. Système Multi-Torons Parallèles (PSS - Parallel Strand System)

Chaque hauban est constitué d'un faisceau de torons $T15S$ ($\varnothing 15.7\text{ mm}$, $A_p = 150\text{ mm}^2$, $f_{pk} = 1860\text{ MPa}$) sous triple protection anti-corrosion :
1. Galvanisation à chaud de chaque fil élémentaire.
2. Cire ou graisse de blocage pétrolière.
3. Gaine individuelle en polyéthylène haute densité (PEHD).
4. Gaine générale extérieure aérodynamique à double filet hélicoïdal anti-vibrations pluie-vent (*rain-wind induced vibrations*).

### Unités Standards :
- **PSS 7** : Section $1050\text{ mm}^2$, Capacité $F_{pk} = 1953\text{ kN}$
- **PSS 12** : Section $1800\text{ mm}^2$, Capacité $F_{pk} = 3348\text{ kN}$
- **PSS 19** : Section $2850\text{ mm}^2$, Capacité $F_{pk} = 5301\text{ kN}$
- **PSS 37** : Section $5550\text{ mm}^2$, Capacité $F_{pk} = 10323\text{ kN}$
- **PSS 61** : Section $9150\text{ mm}^2$, Capacité $F_{pk} = 17019\text{ kN}$
- **PSS 91** : Section $13650\text{ mm}^2$, Capacité $F_{pk} = 25389\text{ kN}$

---

## 3. Dispositifs d'Amortissement & Ancrages

- **Ancrages Réglables / Fixes** : Têtes d'ancrage cylindriques logées dans des trompettes intégrées aux entretoises de pylône et de tablier.
- **Amortisseurs Internes (IAD)** et **Amortisseurs Externes à Fluide Visqueux (VFD)** modélisés pour la dynamique et les vérifications au vent.
