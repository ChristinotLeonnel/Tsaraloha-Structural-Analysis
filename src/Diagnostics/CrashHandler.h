#pragma once

#include <string>

namespace TSA::Diagnostics
{

/**
 * @brief Gestionnaire de crash natif Windows et C++ pour TSA.
 * Intercepte les exceptions non gérées, génère un rapport de crash textuel
 * (avec pile d'appels et 100 derniers événements) et produit un minidump .dmp.
 */
class CrashHandler
{
public:
    /**
     * @brief Installe le filtre d'exception non gérée et le gestionnaire de terminaison.
     */
    static void install();

    /**
     * @brief Désinstalle le filtre d'exception.
     */
    static void uninstall();

    /**
     * @brief Indique si le gestionnaire de crash est actuellement installé.
     */
    static bool isInstalled();

    /**
     * @brief Déclenche manuellement la génération d'un rapport de crash d'urgence.
     */
    static void writeCrashReport(const std::string& reason, void* exceptionPointers = nullptr);
};

} // namespace TSA::Diagnostics
