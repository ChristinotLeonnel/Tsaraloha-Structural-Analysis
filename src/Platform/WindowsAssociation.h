#pragma once

#include <QString>

namespace TSA::Platform
{

/**
 * @brief Gestionnaire de l'association des fichiers projet du produit (<ProductIdentity.h>) avec Windows
 *
 * Enregistre l'extension native du produit dans le registre Windows (HKCU/Software/Classes) :
 * - Extension native associée au ProgID du produit (TSA : .tsa / "TSA.Project")
 * - Description (TSA : "TSA Project File")
 * - Icône par défaut (DefaultIcon) pointant vers l'exécutable
 * - Commande d'ouverture (shell/open/command) avec passage d'argument "%1"
 * - Rafraîchissement automatique du Shell Windows (SHChangeNotify)
 */
class WindowsAssociation
{
public:
    /**
     * @brief Enregistre l'extension native pour l'exécutable actuel
     * @param executablePath Chemin absolu vers l'exécutable (détecté automatiquement si vide)
     * @return true si l'enregistrement a réussi
     */
    static bool registerFileAssociation(const QString& executablePath = QString());

    /**
     * @brief Supprime l'association de l'extension native du registre pour l'utilisateur actuel
     */
    static bool unregisterFileAssociation();

    /**
     * @brief Vérifie si l'extension native est déjà associée au produit
     */
    static bool isFileAssociationRegistered();

    /**
     * @brief Enregistre / retire l'extension Explorateur <Produit>ThumbnailProvider.dll (HKCU) placée à
     *        côté de l'exécutable : miniature du dernier état du modèle pour les fichiers projet.
     */
    static bool registerThumbnailProvider();
    static bool unregisterThumbnailProvider();

    /**
     * @brief Signale à l'Explorateur qu'un fichier a changé (invalidation de sa miniature en cache).
     */
    static void notifyFileUpdated(const QString& filePath);
};

} // namespace TSA::Platform
