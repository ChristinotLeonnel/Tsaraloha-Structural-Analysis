#include "output.h"

// Cette unité gère l'export des résultats de calculs:
// - Création du dossier de sortie
// - Sérialisation en JSON des courbes, types de lignes et paramètres
// - Export en texte lisible des inconnues et de la matrice de rigidité

Output::Output(unordered_map<string, unordered_map<string, unordered_map<string, double>>> data, 
	bool Wind, string W_Directino, int scale, double angle_tolerance, 
	double YoungModule, double Inertie_equivalent, double precision,
    string CheminDuDossier, string folder_name) :

	Curves(data, Wind, W_Directino, scale, angle_tolerance, YoungModule, Inertie_equivalent, precision)
{
    // Chemin complet du dossier de sortie pour cette exécution
    string OutputPath = CheminDuDossier + "\\" + folder_name; 
    creeDossier(OutputPath);

    // Sauvegarde des courbes de moments fléchissants
    saveData(MomentsCurves, OutputPath + "/MomentFlechissant.json"); 

    // Sauvegarde des abscisses associées aux moments fléchissants
    saveData(AbscisseMomentsCurves, OutputPath + "/X_MF.json");

    // Sauvegarde des abscisses associées aux efforts tranchants
    saveData(AbscisseShearsCurves, OutputPath + "/X_ET.json");

    // Sauvegarde des courbes d'efforts tranchants
    saveData(ShearsCurves, OutputPath + "/EffortTranchant.json");

    // Typage des lignes: flags booléens et propriétés supplémentaires pour poutre/colonne/console
    j.clear(); 
    for (int i = 0; i < total_lignes; ++i) {
        auto& ln = (*LINE_Mij).at("ln_" + to_string(i));
        jj["is_beam"] = ln->is_beam;
        jj["is_column"] = ln->is_column; 
        jj["is_consol"] = ln->is_consol; 
        jj["noeud_i"] = ln->i;
        jj["noeud_j"] = ln->j;
        jj["charge"] = ln->charge;
        jj["I"] = ln->I;
        jj["L"] = ln->length;
        jj["alpha"] = ln->alpha;
        jj["K"] = ln->K;
        jj["charge"] = ln->charge; 
        jj["Rapport"] = ln->alpha; 
        jj["alpha"] = ln->alpha; 
        jj["a"] = ln->a; 
        jj["b"] = ln->b;
        jj["c"] = ln->c;
        jj["K"] = ln->K;
        jj["E"] = ln->E;
        
        if (ln->forme.type == "rectangular") {
            jj["Base"] = abs(data["rectangle_" + to_string((ln->forme.id))]["largeur"]["valeur"]) * 100;
            jj["Hauteur"] = abs(data["rectangle_" + to_string((ln->forme.id))]["hauteur"]["valeur"]) * 100;

        } else if (ln->forme.type == "circular") {
            jj["Diametre"] = abs(data["circle_" + to_string((ln->forme.id))]["largeur"]["valeur"]) * 100;
        } 
        
        for (const auto& [k, v] : ln->Mij) {
            jj[k] = v;
        }
        for (const auto& [k, v] : ln->M) {
            jj["m_" + k.substr(k.find("_") + 1)] = v;
        }

        for (const auto& [k, v] : AnalyseCourbe[ln->line_key]) { 
            jj[k] = v; 
        }

        j["ln_" + to_string(i)] = jj;

        jj.clear();
    }
    saveData(j, OutputPath + "/line_properties.json");

    // Inconnues du système (vecteur solution)
    saveData(Solution, OutputPath + "/solution.json"); 

    // Somme des moment sur chaque noeud (bilan)
    saveData(VERDICTE, OutputPath + "/verdicte.json");

    // Sauvergarde de Second Membre
	saveData(Second_Membres_Key_Val, OutputPath + "/SecondMembre.json");

    // Matrice de rigidité exportée en JSON (format tableau 2D)
    map<string, vector<double>> MAP;
    int s = 0;
    for (const auto& i : Matrice_de_rigidite) {
        MAP[Inconue[s]] = i;
        ++s; 
    }
    saveData(MAP, OutputPath + "/MatriceDeRigidite.json"); 

    // Enregistrement des inconnues et de leur valeur dans un fichier texte lisible
    {
        std::ofstream txtFile(OutputPath + "/solution.txt");
        if (txtFile.is_open()) {
            txtFile << "Inconnue\tValeur\n";
            for (const auto& [key, value] : Solution) {
                txtFile << key << "\t" << value << "\n";
            }
            txtFile.close();
        } else {
            throw std::runtime_error("Impossible d'ouvrir le fichier: " + OutputPath + "/solution.txt");
        }
    }

    // Enregistrement de la matrice de rigidité alignée avec les inconnues dans un fichier texte
    {
        std::ofstream matFile(OutputPath + "/MatriceDeRigidite.txt");
        if (matFile.is_open()) {
            // Affichage similaire à la console : chaque valeur sur 10 caractères, précision 7 décimales
            matFile << "Matrice de rigidité :\n";

            // Afficher les noms des inconnues en haut comme dans pandas
            matFile << std::setw(12) << " ";
            int compteur = 0; 
            for (const auto& i : Inconue) {
                matFile << std::setw(12) << Inconue[compteur];
                compteur++; 
            }
            matFile << "\n";

            // Afficher chaque ligne avec le nom de l'inconnue en début de ligne
            int idx = 0;
            for (const auto& row : Matrice_de_rigidite) {
                // Récupérer le nom de l'inconnue correspondant à la ligne
                auto it = Solution.begin();
                std::advance(it, idx);
                if (it != Solution.end()) {
                    matFile << std::setw(12) << Inconue[idx]; 
                } else {
                    matFile << std::setw(12) << " ";
                }
                for (double val : row) {
                    matFile << std::setw(12) << std::fixed << std::setprecision(7) << val;
                }
                matFile << "\n";
                ++idx;
            }
            matFile.close();
        } else {
            throw std::runtime_error("Impossible d'ouvrir le fichier: " + OutputPath + "/MatriceDeRigidite.txt");
        }
    }

    // Correction : Initialiser M_MAX à une très petite valeur et M_MIN à une très grande valeur
    double M_MAX = std::numeric_limits<double>::lowest();
    double M_MIN = std::numeric_limits<double>::max();

    double M_TRAVEE_MAX = std::numeric_limits<double>::lowest();
    double M_TRAVEE_MIN = std::numeric_limits<double>::max();

    string ligne_max;
    string ligne_min;

    string ligne_travee_max;
    string ligne_travee_min;

    string key_min;
    string key_max;

    for (int i = 0; i < total_lignes; ++i) {
        auto& ln = (*LINE_Mij).at("ln_" + to_string(i));
        if (ln->is_beam) {
            for (const auto& [key, value] : ln->Mij) {
                if (std::abs(value) > M_MAX) {
                    M_MAX = std::abs(value);
                    ligne_max = "ln_" + to_string(i);
                    key_max = key;
                }
                if (std::abs(value) < M_MIN) {
                    M_MIN = std::abs(value);
                    ligne_min = "ln_" + to_string(i);
                    key_min = key;
                }
            }
            // Correction : ne pas parcourir deux fois, et bien distinguer max/min
            auto it_courbe = AnalyseCourbe.find(ln->line_key);
            if (it_courbe != AnalyseCourbe.end()) {
                auto it_max = it_courbe->second.find("M_en_travee_max");
                if (it_max != it_courbe->second.end()) {
                    double val = it_max->second;
                    if (std::abs(val) > M_TRAVEE_MAX) {
                        M_TRAVEE_MAX = std::abs(val);
                        ligne_travee_max = "ln_" + to_string(i);
                    }
                    if (std::abs(val) < M_TRAVEE_MIN) {
                        M_TRAVEE_MIN = std::abs(val);
                        ligne_travee_min = "ln_" + to_string(i);
                    }
                }
            }
        }
    }

    // Ajout dans un json ces trois données (après la boucle, pas à chaque itération)
    {
        nlohmann::json j_max;
        j_max["M_MAX"] = M_MAX;
        j_max["ligne_max"] = ligne_max;
        j_max["key_max"] = key_max;

        j_max["M_MIN"] = M_MIN;
        j_max["ligne_min"] = ligne_min;
        j_max["key_min"] = key_min;

		j_max["M_TRAVEE_MAX"] = M_TRAVEE_MAX;
		j_max["ligne_travee_max"] = ligne_travee_max;
		j_max["M_TRAVEE_MIN"] = M_TRAVEE_MIN;
		j_max["ligne_travee_min"] = ligne_travee_min;

        std::ofstream file_max(OutputPath + "/MomentMax.json");
        if (file_max.is_open()) {
            file_max << j_max.dump(4);
            file_max.close();
        }
    }


    // Maintenant, ligne_max contient l'indice de la ligne avec le moment max,
    // et key_max la clé correspondante dans Mij.

	std::cout << "Les resultats ont ete sauvegardes dans le dossier : " << OutputPath << std::endl;
}

// Écrit le JSON sur disque avec indentation pour faciliter la lecture
void Output::saveToFile(const nlohmann::json& data, const std::string& filename)
{
    std::ofstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Impossible d'ouvrir le fichier: " + filename);
    }

    file << data.dump(4); // 4 est le nombre d'espaces pour l'indentation
}

// Sérialise une collection {nom_serie -> valeurs} en JSON
void Output::saveData(const map<string, vector<double>>& data, const string& filename) {
    nlohmann::json j;
    for (const auto& [key, values] : data) {
        j[key] = values;
    }
    saveToFile(j, filename);
}

// Sérialise un dictionnaire de dictionnaires: j[cle1][cle2] = valeur
template<typename T>
void Output::saveData(const map<string, map<string, T>>& data, const string& filename) {
    nlohmann::json j;
    for (const auto& [key_1, value] : data) {
        j[key_1] = {}; 
        for (const auto& [key_2, val] : value) {
            j[key_1][key_2] = val; 
        }
    }
    saveToFile(j, filename);
}

// Sérialise une matrice générique (vector<vector<T>>)
template<typename T>
void Output::saveData(const vector<vector<T>>& vect, const std::string& filename)
{
    nlohmann::json j = nlohmann::json::array();
    for (const auto& row : vect) {
        j.push_back(row);
    }
    saveToFile(j, filename);
}

// Sérialise une table de correspondance simple (clé -> valeur)
template<typename T, typename U>
void Output::saveData(const map<T, U>& data, const string& filename) {
    nlohmann::json j; 
    for (const auto& [k, v] : data) {
        j[k] = v; 
    }
    saveToFile(j, filename); 
}

template<typename T>
void Output::saveData(const map<string, vector<T>>& data, const string& filename) {
    nlohmann::json j;
    for (const auto& l : data) {
        for (const auto& [k, v] : l) {
            j[k] = v; 
        }
    }
    saveToFile(j , filename); 
}

/**
 * @brief V�rifie si un dossier existe
 * @param chemin Chemin du dossier
 * @return true si le dossier existe, false sinon
 */
bool Output::dossierExiste(const std::string& chemin)
{
    struct stat info;
    if (stat(chemin.c_str(), &info) != 0) {
        return false;
    }
    return (info.st_mode & S_IFDIR) != 0;
};

/**
 * @brief Cr�e un dossier s'il n'existe pas
 * @param chemin Chemin du dossier � cr�er
 */
void Output::creeDossier(const std::string& chemin)
{
    if (!dossierExiste(chemin)) {
        int status = MKDIR(chemin.c_str());
        if (status == 0) {
            std::cout << "Le dossier \"" << chemin << "\" a ete cree avec succes." << std::endl;
        }
        else {
            std::cerr << "Erreur lors de la creation du dossier \"" << chemin << "\"." << std::endl;
        }
    }
    else {
        std::cout << "Le dossier \"" << chemin << "\" existe deja." << std::endl;
    }
};