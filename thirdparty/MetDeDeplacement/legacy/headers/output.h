#pragma once
#ifndef OUTPUT
#define OUTPUT

#include "curves.h"

#include <fstream>
#include <iostream>
#include <cmath>
#include <limits>

#include <regex>
#include <sstream>

#include <json.hpp>

#include <sys/stat.h> // Nécessaire pour la création de dossiers

#ifdef _WIN32
#include <direct.h> 
#define MKDIR(dir) _mkdir(dir) // Macro pour la création de dossier sous Windows
#else
#define MKDIR(dir) mkdir(dir, 0755) // Macro pour la création de dossier sous Unix/Linux avec permissions 0755 (rwxr-xr-x)
#endif 


class Output : public Curves
{
	using Curves::Curves; 

public:

	Output(unordered_map<string, unordered_map<string, unordered_map<string, double>>> data,
		bool Wind = true, string W_Directino = "right", int scale = 50, double angle_tolerance = 5,
		double YoungModule = 1, double Inertie_equivalent = 0.05022003, double precision = 0.001, 
		string CheminDuDossier = "C:/Users/tsara/OneDrive/Desktop/MatriceOne/Methode de rotation/output", string folder_name = "Chistinot");

private:
    /**
	 * @brief Sauvegarde un objet JSON dans un fichier
	 * @param data L'objet json à sauvegarder
	 * @param filename Le chemin du fichier de destination
	 * @throws std::runtime_error Si le fichier ne peut pas être ouvert
	 */
	void saveToFile(const nlohmann::json& data, const string& filename);

	/**
	 * @brief Sauvegarde des données structurées dans un fichier JSON
	 * @param data Une map contenant des vecteurs de doubles
	 * @param filename Le chemin du fichier de destination
	 *
	 * Cette fonction est optimisée pour sauvegarder des données numériques
	 * organisées en paires clé-valeur, où chaque valeur est un vecteur de doubles.
	 */
	void saveData(const map<string, vector<double>>& data, const string& filename);

	template<typename T>
	void saveData(const map<string, map<string, T>>& data, const string& filename); 

	template<typename T>
	void saveData(const vector<vector<T>>& vect, const std::string& filename);

	template<typename T, typename U>
	void saveData(const map<T, U>& data, const string& filename);

	template <typename T>
	void saveData(const map<string, vector<T>>& data, const string& filename);

	static bool dossierExiste(const std::string& chemin); 
	void creeDossier(const std::string& chemin); 

	map<string, map<string, double>> j;
	map<string, double> jj;
};


#endif // !OUTPUT
