#pragma once

// Icônes SVG adaptées au thème (docs/ICONS.md). Les icônes de resources/icons sont dessinées pour le thème sombre ;
// au rendu, les couleurs sous 3:1 de contraste avec le fond du thème actif sont remplacées par celles de
// :/icons/palette.json (tools/icons/make_palette.py). L'état désactivé est rendu en gris atténué (forme lisible) ;
// un changement de thème s'applique au prochain affichage, sans recréer les icônes.

#include <QByteArray>
#include <QHash>
#include <QIcon>
#include <QString>

namespace TSA::UI
{

/// Icône d'une ressource SVG (« :/icons/… ») suivant le thème. Autre format (PNG…) : QIcon ordinaire.
QIcon themedIcon(const QString& resourcePath);

/// Table de remplacement des couleurs (« #RRGGBB » → « #RRGGBB ») du thème.
const QHash<QString, QString>& iconPalette(bool dark);

/// Contenu SVG de la ressource avec les couleurs du thème (vide si la ressource est absente).
QByteArray themedSvg(const QString& resourcePath, bool dark);

/// Image de l'icône pour un thème et un état, indépendamment du thème actif (tests de contraste).
QImage renderThemedIcon(const QString& resourcePath, int size, bool dark, QIcon::Mode mode = QIcon::Normal);
/// Même rendu dans un rectangle : l'icône (carrée) est centrée, sans déformation.
QImage renderThemedIcon(const QString& resourcePath, const QSize& size, bool dark, QIcon::Mode mode = QIcon::Normal);

/// Logo du produit lisible sur le fond du thème (barre de titre, accueil, barre d'état). Thème sombre : icône officielle
/// (Product::kIconSvg). Thème clair : glyphe du produit (Product::kGlyphSvg) en aplat de la couleur du texte, sans fond —
/// le carré sombre et le T gris fin de l'icône officielle se lisaient mal sur un fond clair.
QByteArray productLogoSvg(bool dark);
QIcon productLogoIcon(bool dark);

} // namespace TSA::UI
