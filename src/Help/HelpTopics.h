#pragma once

// Aide contextuelle : registre central des identifiants d'aide de TSA et construction des adresses
// de la documentation en ligne (TSA Web).
//
// Un bouton Aide ne connaît qu'un identifiant stable (« model.nodes », « loading.distributed »…) ;
// la page correspondante est donnée par la table de HelpTopics.cpp, seule source de vérité côté
// logiciel. Le site vérifie à sa construction que chaque identifiant mène à une page existante
// (scripts/check-docs.mjs du dépôt Tsaraloha-Web).
//
// Sécurité :
//   - destination unique : l'adresse officielle de product/ProductIdentity.h (kDocsBaseUrl), HTTPS ;
//   - aucune donnée de projet dans l'adresse : seuls la langue (fr / en) et la version de TSA sont
//     transmises ;
//   - identifiant inconnu : repli sur l'accueil de la documentation (signalé).
//
// Qt Core uniquement (testable sans interface) ; l'ouverture du navigateur est dans
// src/UI/HelpLauncher.

#include <QString>
#include <QUrl>

#include <span>

namespace TSA::Help
{

struct HelpTopic
{
    const char* id;   ///< identifiant stable, utilisé par les boutons Aide
    const char* path; ///< chemin de la page, relatif à l'adresse de la documentation (« docs/… »)
};

/// Identifiant de l'accueil de la documentation (repli des identifiants inconnus).
inline constexpr char kOverviewTopic[] = "general.overview";

/// Registre complet (ordre de la table).
std::span<const HelpTopic> topics();

/// Chemin de la page d'un identifiant ; vide si l'identifiant est inconnu.
QString pathForTopic(const QString& topicId);

/// L'identifiant figure-t-il dans le registre ?
bool isKnownTopic(const QString& topicId);

/// Adresse de base acceptable : HTTPS, hôte non vide, ni identifiants, ni requête, ni fragment.
bool isValidBaseUrl(const QUrl& baseUrl);

/**
 * L'adresse reste-t-elle dans la documentation officielle ? HTTPS, même hôte et même port que la
 * base, chemin sous celui de la base, sans identifiants ni fragment, paramètres limités à
 * « lang » et « v ».
 */
bool isAllowedHelpUrl(const QUrl& url, const QUrl& baseUrl);

struct HelpUrl
{
    QUrl url;               ///< adresse à ouvrir (vide si error n'est pas vide)
    bool knownTopic = true; ///< false : identifiant inconnu, accueil de la documentation utilisé
    QString error;          ///< raison du refus (base absente ou invalide)
};

/**
 * Adresse de la page d'un identifiant. language : « fr » ou « en » (autre valeur : ignorée) ;
 * version : « X.Y.Z » (autre valeur : ignorée).
 */
HelpUrl buildHelpUrl(const QString& baseUrl, const QString& topicId, const QString& language, const QString& version);

/// Même chose avec l'adresse officielle et la version du produit compilé (ProductIdentity.h).
HelpUrl officialHelpUrl(const QString& topicId, const QString& language = QStringLiteral("fr"));

} // namespace TSA::Help
