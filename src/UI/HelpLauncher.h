#pragma once

// Ouverture de la documentation en ligne depuis un bouton ou une commande Aide.
// Adresse construite par TSA::Help (src/Help/HelpTopics) : domaine officiel HTTPS uniquement, aucune
// donnée de projet transmise. Ouverture par QDesktopServices::openUrl (navigateur par défaut) : aucun
// appel bloquant, aucune attente de réseau dans TSA.

#include <QString>
#include <QUrl>

#include <functional>

class QPushButton;
class QWidget;

namespace TSA::UI
{

enum class HelpOpenResult
{
    Opened,        ///< adresse transmise au navigateur
    NotConfigured, ///< aucune documentation en ligne pour ce produit : l'appelant garde son aide locale
    Failed         ///< adresse refusée ou navigateur indisponible : message affiché (adresse copiable)
};

/**
 * Ouvre la page de l'identifiant (repli sur l'accueil de la documentation s'il est inconnu).
 * En cas d'échec, affiche un message non bloquant avec l'adresse sélectionnable.
 */
HelpOpenResult openHelpTopic(QWidget* parent, const QString& topicId);

/// Une documentation en ligne est-elle configurée pour ce produit (ProductIdentity.h : kDocsBaseUrl) ?
bool isOnlineHelpAvailable();

/**
 * Bouton « Aide » relié à l'identifiant (objectName « helpButton », propriété « helpTopic »).
 * nullptr si aucune documentation en ligne n'est configurée : la fenêtre n'affiche pas de bouton inerte.
 */
QPushButton* createHelpButton(QWidget* parent, const QString& topicId);

/// Tests : remplace l'ouverture du navigateur (nullptr : QDesktopServices::openUrl).
void setHelpUrlOpenerForTesting(std::function<bool(const QUrl&)> opener);

} // namespace TSA::UI
