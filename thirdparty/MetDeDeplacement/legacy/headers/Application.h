#pragma 
#ifndef Application
#define Application

#include <unordered_map>
#include <json.hpp>
#include <string>
#include <iostream>

using json = nlohmann::json;
// Inclure le fichier d'en-tête de la classe Ligne
// Fonction utilitaire pour convertir un json en unordered_map imbriqué
static std::unordered_map<std::string, std::unordered_map<std::string, std::unordered_map<std::string, double>>>
jsonToNestedMap(const json& data) {
    std::unordered_map<std::string, std::unordered_map<std::string, std::unordered_map<std::string, double>>> result;

    for (auto it = data.begin(); it != data.end(); ++it) {
        const std::string& key1 = it.key();
        if (!it.value().is_object()) continue;
        for (auto it2 = it.value().begin(); it2 != it.value().end(); ++it2) {
            const std::string& key2 = it2.key();
            if (!it2.value().is_object()) continue;
            for (auto it3 = it2.value().begin(); it3 != it2.value().end(); ++it3) {
                const std::string& key3 = it3.key();
                if (it3.value().is_number_float() || it3.value().is_number_integer()) {
                    result[key1][key2][key3] = it3.value().get<double>();
                }
            }
        }
    }

    return result;
}

#endif // !Application
