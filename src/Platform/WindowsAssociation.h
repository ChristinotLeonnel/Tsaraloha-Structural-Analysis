#pragma once

#include <QString>

namespace TSA::Platform
{

/**
 * @brief Gestionnaire de l'association de fichiers .tsa avec le système Windows
 *
 * Enregistre .tsa dans le registre Windows (HKCU/Software/Classes) :
 * - Extension .tsa associée au ProgID "TSA.Project"
 * - Description "TSA Project File"
 * - Icône par défaut (DefaultIcon) pointant vers l'exécutable TSA.exe
 * - Commande d'ouverture (shell/open/command) avec passage d'argument "%1"
 * - Rafraîchissement automatique du Shell Windows (SHChangeNotify)
 */
class WindowsAssociation
{
public:
    /**
     * @brief Enregistre l'extension .tsa pour l'exécutable TSA actuel
     * @param executablePath Chemin absolu vers TSA.exe (détecté automatiquement si vide)
     * @return true si l'enregistrement a réussi
     */
    static bool registerFileAssociation(const QString& executablePath = QString());

    /**
     * @brief Supprime l'association .tsa du registre pour l'utilisateur actuel
     */
    static bool unregisterFileAssociation();

    /**
     * @brief Vérifie si .tsa est déjà associé à TSA.exe
     */
    static bool isFileAssociationRegistered();
};

} // namespace TSA::Platform
