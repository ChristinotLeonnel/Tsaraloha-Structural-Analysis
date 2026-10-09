#pragma once

// Chemins de fichiers transportés en std::string UTF-8 (QString::toStdString) → std::filesystem::path.
// Sous Windows, std::fstream construit avec une chaîne étroite l'interprète dans la page de code ANSI :
// un chemin UTF-8 contenant « é », « â »… ne s'ouvre pas. Toute ouverture d'un tel chemin par la
// bibliothèque standard passe donc par cette fonction.

#include <filesystem>
#include <string>

namespace TSA::Core
{

inline std::filesystem::path utf8Path(const std::string& utf8)
{
    return std::filesystem::path(std::u8string(utf8.begin(), utf8.end()));
}

} // namespace TSA::Core
