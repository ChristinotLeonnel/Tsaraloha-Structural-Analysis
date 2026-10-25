// Locale numérique « C » pour tout le processus (docs/LINUX_BUILD.md).
//
// Les conversions de la bibliothèque C (std::stod, strtod, std::to_string, printf %f) servent aux fichiers
// d'échange (.tsa, scripts OpenSees et leurs résultats, STEP/IFC, scripts de commandes) et supposent le point
// décimal. Sous Unix, le constructeur de QCoreApplication applique la locale du système (setlocale(LC_ALL, "")) :
// avec fr_FR, « 3.5 » serait lu 3. Windows ne change pas la locale du CRT : ce rétablissement aligne les deux.
// L'affichage (QLocale) n'en dépend pas.

#include <QCoreApplication>

#include <clocale>

namespace
{

void useCNumericLocale()
{
    std::setlocale(LC_NUMERIC, "C");
}

} // namespace

// Appelée par le constructeur de chaque QCoreApplication, après l'initialisation de la locale par Qt.
Q_COREAPP_STARTUP_FUNCTION(useCNumericLocale)
