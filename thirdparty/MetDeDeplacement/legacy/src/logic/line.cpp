/**
 * @file line.cpp
 * @brief Implémentation de la classe Line pour la gestion des lignes géométriques
 * 
 * Cette classe représente une ligne dans un système de coordonnées 3D et fournit
 * des méthodes pour analyser ses propriétés géométriques, détecter les connexions
 * avec d'autres lignes, et calculer les forces et sections associées.
 * 
 * @author TSARALOHA Nomenjanahary Christinot Léonnel Calixtes
 * @date 2024
 */

#include "line.h"
#include <iostream>

/**
 * @brief Constructeur de Line avec ID numérique
 * @param id Identifiant numérique de la ligne
 * @param data Structure de données contenant toutes les informations géométriques
 * @param angle_tolerance Tolérance angulaire en degrés pour la classification des lignes
 * @param scale Facteur d'échelle pour les calculs de forces
 */
Line::Line(int id, unordered_map<string, unordered_map<string, unordered_map<string, double>>> data,
    double angle_tolerance, int scale)
    : scale(scale), data(data), tolerance(angle_tolerance), id(id)
{
    line_key = "ln_" + to_string(id);
    initialisation(); 
}

/**
 * @brief Constructeur de Line avec ID sous forme de chaîne
 * @param id Identifiant de la ligne sous forme de chaîne
 * @param data Structure de données contenant toutes les informations géométriques
 * @param angle_tolerance Tolérance angulaire en degrés pour la classification des lignes
 * @param scale Facteur d'échelle pour les calculs de forces
 */
Line::Line(string id, unordered_map<string, unordered_map<string, unordered_map<string, double>>> data, double angle_tolerance, int scale)
    : scale(scale), data(data), tolerance(angle_tolerance), id_str(id)
{
    line_key = id_str; 
    initialisation(); 
}

/**
 * @brief Trouve toutes les lignes connectées à la ligne courante
 * 
 * Cette méthode parcourt toutes les lignes du système et identifie celles qui
 * partagent des nœuds communs avec la ligne courante. Elle utilise une tolérance
 * de 0.01 pour la comparaison des coordonnées des nœuds.
 * 
 * @return Map associant les clés des lignes connectées aux paires de nœuds correspondants
 * eg : { "ln_0": ["nd_1", "nd_2"], "ln_1": ["nd_1", "nd_2"] }
 */
unordered_map<string, vector<string>> Line::find_associated_lines()
{
    unordered_map<string, vector<string>> connected_lines;

    int total_lines = 0;
    // Vérification robuste de l'existence des clés
    if (data.count("nombre") && data["nombre"].count("propriete") && data["nombre"]["propriete"].count("ligne")) {
        total_lines = static_cast<int>(data["nombre"]["propriete"]["ligne"]);
    }

    string other_line_key;

    for (int i = 0; i < total_lines; ++i) {
        other_line_key = "ln_" + to_string(i);

        // Ne pas comparer la ligne à elle-même
        if (other_line_key == line_key) {
            continue;
        }

        // Vérification de l'existence des nœuds pour la ligne courante
        if (!data.count(other_line_key) ||
            !data[other_line_key].count("nd_1") ||
            !data[other_line_key].count("nd_2")) {
            continue;
        }

        // On vérifie toutes les combinaisons d'extrémités
        if (nodes_approx_equal(node_1, data[other_line_key]["nd_1"], 0.01)) {
            connected_lines[other_line_key] = { "nd_1", "nd_1" };
        }
        else if (nodes_approx_equal(node_1, data[other_line_key]["nd_2"], 0.01)) {
            connected_lines[other_line_key] = { "nd_1", "nd_2" };
        }
        else if (nodes_approx_equal(node_2, data[other_line_key]["nd_1"], 0.01)) {
            connected_lines[other_line_key] = { "nd_2", "nd_1" };
        }
        else if (nodes_approx_equal(node_2, data[other_line_key]["nd_2"], 0.01)) {
            connected_lines[other_line_key] = { "nd_2", "nd_2" };
        }
    }

    return connected_lines;
}

/**
 * @brief Compare deux nœuds pour déterminer s'ils sont approximativement égaux
 * 
 * Calcule la distance euclidienne entre deux nœuds 3D et retourne true si cette
 * distance est inférieure ou égale à la tolérance spécifiée.
 * 
 * @param node_1 Premier nœud avec coordonnées {x, y, z}
 * @param node_2 Deuxième nœud avec coordonnées {x, y, z}
 * @param tolerance Distance maximale admissible entre les nœuds
 * @return true si les nœuds sont considérés comme égaux, false sinon
 * eg : nodes_approx_equal({x: 1.0, y: 2.0, z: 0.0}, {x: 1.01, y: 2.01, z: 0.0}, 0.02) = true
 */
bool Line::nodes_approx_equal(const unordered_map<string, double>& node_1, const unordered_map<string, double>& node_2, double tolerance)
{
    // Vérifie que les deux nœuds possèdent bien les clés nécessaires
    static const vector<string> keys = {"x", "y", "z"};
    for (const auto& key : keys) {
        if (node_1.find(key) == node_1.end() || node_2.find(key) == node_2.end()) {
            return false;
        }
    }

    // Calcul de la distance euclidienne entre les deux nœuds
    double dx = node_1.at("x") - node_2.at("x");
    double dy = node_1.at("y") - node_2.at("y");
    double dz = node_1.at("z") - node_2.at("z");
    double distance = sqrt(dx * dx + dy * dy + dz * dz);

    // On considère les nœuds égaux si la distance est inférieure à la tolérance
    return distance <= tolerance;
}

/**
 * @brief Détermine si la ligne est une console (poutre en porte-à-faux)
 * 
 * Une console est une poutre qui n'est connectée à aucune colonne à l'une de ses
 * extrémités. Cette méthode vérifie les connexions avec les colonnes pour déterminer
 * si la ligne courante est une console.
 * 
 * @return true si la ligne est une console, false sinon
 */
bool Line::check_is_consol() const
{
    // Amélioration : On considère une poutre comme console si elle n'est connectée à aucune colonne à l'une de ses extrémités.
    if (is_column || is_inclined)  
        return false;

    bool extremity1_connected_to_column = false;
    bool extremity2_connected_to_column = false;

    for (const auto& [key, connection] : associated_lines) {
        // Récupérer les données de la ligne associée
        const auto& line_data = const_cast<Line*>(this)->data.at(key);

        // Vérifier si la ligne associée est une colonne
        if (is_column_type(line_data.at("nd_1"), line_data.at("nd_2"))) {
            // connection[0] : extrémité de la ligne courante ("nd_1" ou "nd_2")
            // connection[1] : extrémité de la ligne associée
            if (!connection.empty()) {
                if (connection[0] == "nd_1") {
                    extremity1_connected_to_column = true;
                }
                if (connection[0] == "nd_2") {
                    extremity2_connected_to_column = true;
                }
            }
        }
    }

    // Si une extrémité n'est connectée à aucune colonne, c'est une console
    return !extremity1_connected_to_column || !extremity2_connected_to_column;
}

/**
 * @brief Détermine si une ligne est de type colonne basée sur ses nœuds
 * 
 * Une colonne est caractérisée par une variation négligeable en x (verticalité)
 * et une variation significative en y (pour éviter les points confondus).
 * 
 * @param node_1 Premier nœud de la ligne
 * @param node_2 Deuxième nœud de la ligne
 * @return true si la ligne est une colonne, false sinon
 */
bool Line::is_column_type(unordered_map<string, double> node_1, unordered_map<string, double> node_2) const
{
    // Amélioration : Vérifie si la différence en x est négligeable (verticalité), et que la différence en y est significative (pour éviter les points confondus)
    double dx = abs(node_1.at("x") - node_2.at("x"));
    double dy = abs(node_1.at("y") - node_2.at("y"));
    // On considère une colonne si la variation en x est très faible (verticale) et la variation en y est significative
    return dx <= tolerance && dy > tolerance;
}

/**
 * @brief Vérifie si un point donné se trouve sur la ligne
 * 
 * Cette méthode utilise des calculs géométriques pour déterminer si un point
 * se trouve exactement sur la ligne définie par ses deux extrémités.
 * 
 * @param coordonner Vecteur contenant les coordonnées [x, y, z] du point
 * @return true si le point est sur la ligne, false sinon
 * eg : is_point_on_line([1.5, 2.0, 0.0]) = true si le point est sur la ligne
 */
bool Line::is_point_on_line(vector<double> coordonner)
{
    // Vérification de la taille du vecteur
    if (coordonner.size() < 2) return false;

    constexpr double epsilon = 1e-10;

    // Vérification si le point est dans le même plan z (si pertinent)
    if (node_1.count("z") && node_2.count("z") && coordonner.size() >= 3) {
        if (abs(node_1.at("z") - coordonner[2]) > epsilon && abs(node_2.at("z") - coordonner[2]) > epsilon) {
            return false;
        }
    }

    double x1 = node_1.at("x");
    double y1 = node_1.at("y");
    double x2 = node_2.at("x");
    double y2 = node_2.at("y");
    double px = coordonner[0];
    double py = coordonner[1];

    // Cas vertical (x1 == x2)
    if (abs(x1 - x2) < epsilon) {
        // Le point doit avoir le même x et être entre y1 et y2
        return abs(px - x1) < epsilon &&
               ((py >= std::min(y1, y2) - epsilon && py <= std::max(y1, y2) + epsilon));
    }

    // Cas horizontal (y1 == y2)
    if (abs(y1 - y2) < epsilon) {
        return abs(py - y1) < epsilon &&
               ((px >= std::min(x1, x2) - epsilon && px <= std::max(x1, x2) + epsilon));
    }

    // Calcul de la colinéarité (produit vectoriel nul)
    double cross = (px - x1) * (y2 - y1) - (py - y1) * (x2 - x1);
    if (abs(cross) > epsilon) {
        return false;
    }

    // Vérifier si le point est entre les deux extrémités de la ligne
    bool between_x = (px >= std::min(x1, x2) - epsilon && px <= std::max(x1, x2) + epsilon);
    bool between_y = (py >= std::min(y1, y2) - epsilon && py <= std::max(y1, y2) + epsilon);

    return between_x && between_y;
}

/**
 * @brief Vérifie si un point correspond exactement à l'un des nœuds de la ligne
 * 
 * Compare les coordonnées du point avec celles des deux nœuds de la ligne
 * en utilisant une tolérance très faible (1e-8).
 * 
 * @param coord_point Vecteur contenant les coordonnées [x, y, z] du point
 * @return true si le point correspond à l'un des nœuds, false sinon
 * eg : is_point_one_off_nd_line([1.0, 2.0, 0.0]) = true si le point correspond à node_1 ou node_2
 */
bool Line::is_point_one_off_nd_line(vector<double> coord_point)
{
    // Vérifie si coord_point correspond exactement à node_1 ou node_2 (x, y, z)
    if (coord_point.size() != 3) return false;

    // Comparaison directe avec les coordonnées des nœuds
    bool match_node1 = 
        abs(coord_point[0] - node_1.at("x")) < 1e-8 &&
        abs(coord_point[1] - node_1.at("y")) < 1e-8 &&
        abs(coord_point[2] - node_1.at("z")) < 1e-8;

    bool match_node2 = 
        abs(coord_point[0] - node_2.at("x")) < 1e-8 &&
        abs(coord_point[1] - node_2.at("y")) < 1e-8 &&
        abs(coord_point[2] - node_2.at("z")) < 1e-8;

	// Retourne vrai si le point correspond à l'un des nœuds
    return match_node1 || match_node2;
}

/**
 * @brief Vérifie si un nœud d'un rectangle se trouve sur la ligne
 * 
 * Cette méthode vérifie deux conditions :
 * 1. Le nœud du rectangle correspond exactement à l'un des nœuds de la ligne
 * 2. Le nœud se trouve dans la boîte englobante de la ligne
 * 
 * @param id_node_recte Identifiant du nœud du rectangle (1-4)
 * @param id_rectangle Identifiant du rectangle
 * @return true si le nœud est sur la ligne, false sinon
 * eg : is_rectangle_node_in_line(1, 0) = true si le nœud 1 du rectangle 0 est sur la ligne
 */
bool Line::is_rectangle_node_in_line(int id_node_recte, int id_rectangle)
{
	unordered_map<string, double> coords = data["rectangle_" + to_string(id_rectangle)]["nd_" + to_string(id_node_recte)];
	vector<double> coord_point = { coords.at("x"), coords.at("y"), coords.at("z") };
  
	bool condition_1 = is_point_one_off_nd_line(coord_point);
   
	double x_min = std::min(node_1.at("x"), node_2.at("x"));
	double x_max = std::max(node_1.at("x"), node_2.at("x"));
	double y_min = std::min(node_1.at("y"), node_2.at("y"));
	double y_max = std::max(node_1.at("y"), node_2.at("y"));
	double z_min = std::min(node_1.at("z"), node_2.at("z"));
	double z_max = std::max(node_1.at("z"), node_2.at("z"));

    // condition_2 = (x_min <= node[0] <= x_max and y_min <= node[1] <= y_max)

    bool condition_2 = (coord_point[0] >= x_min && coord_point[0] <= x_max &&
                        coord_point[1] >= y_min && coord_point[1] <= y_max &&
		                coord_point[2] >= z_min && coord_point[2] <= z_max); 

    return condition_1 and condition_2;
}

/**
 * @brief Détermine si un rectangle intersecte la ligne et calcule l'orientation
 * 
 * Cette méthode vérifie quels nœuds du rectangle se trouvent sur la ligne
 * et détermine si cette intersection est valide selon le type de ligne
 * (colonne ou poutre). Elle calcule également l'orientation (positive/négative)
 * pour les calculs de forces.
 * 
 * @param id_rectangle Identifiant du rectangle à analyser
 * @return Structure RectangleLineResult contenant la validité, l'orientation et les nœuds
 * eg : RectangleLineResult{is_valid: true, positive: true, nodes_on_line: [1, 4]}
 */
RectangleLineResult Line::is_rectangle_in_line(int id_rectangle)
{

    vector<int> nodes_on_line;
    for (int i = 1; i <= 4; ++i) {
        if (is_rectangle_node_in_line(i, id_rectangle)) {
            nodes_on_line.push_back(i); 
        }
    }

    bool is_valid = false;
    
    // Determine is_valid based on is_column (poteau) or is_beam (poutre)
    if (is_column) {
        is_valid = (nodes_on_line == vector<int>{1, 4} || nodes_on_line == vector<int>{4, 1} ||
                    nodes_on_line == vector<int>{2, 3} || nodes_on_line == vector<int>{3, 2});
        /*if (is_valid) {
            std::cout << "\n" << line_key << " : \n";
            for (const auto& i : nodes_on_line) {
                std::cout << i << " , ";
            }
            std::cout << "\n";
        }*/
    } else if (is_beam) {
        is_valid = (nodes_on_line == vector<int>{1, 2} || nodes_on_line == vector<int>{2, 1} ||
                    nodes_on_line == vector<int>{3, 4} || nodes_on_line == vector<int>{4, 3});
        /*if (is_valid and ! is_consol) {
            std::cout << "\n" << line_key << " : \n";
            for (const auto& i : nodes_on_line) {
                std::cout << i << " , ";
            }
            std::cout << "\n";
        }*/
    } else {
        is_valid = false;
    }

    // Prepare to compute positive
    optional<bool> positive = std::nullopt;

    if (is_valid && is_column) {
        // Get rectangle node coordinates
        auto& rect = data["rectangle_" + to_string(id_rectangle)];
        bool val = rect.at("nd_1").at("x") <= rect.at("nd_2").at("x");
       
        if (std::find(nodes_on_line.begin(), nodes_on_line.end(), 1) != nodes_on_line.end()) {
            positive = !val;
        } else if (std::find(nodes_on_line.begin(), nodes_on_line.end(), 2) != nodes_on_line.end()) {
            positive = val;
        }
    } else if (is_valid && is_beam) {
        auto& rect = data["rectangle_" + to_string(id_rectangle)];
        bool val = rect.at("nd_1").at("y") < rect.at("nd_4").at("y");
        if (std::find(nodes_on_line.begin(), nodes_on_line.end(), 1) != nodes_on_line.end()) {
            positive = !val;
        } else if (std::find(nodes_on_line.begin(), nodes_on_line.end(), 4) != nodes_on_line.end()) {
            positive = val;
        }
    } else {
        positive = std::nullopt;
    }

    // Return all three values in the struct
    return RectangleLineResult{is_valid, positive, nodes_on_line};
}

/**
 * @brief Vérifie si un rectangle est positionné au milieu de la ligne
 * 
 * Compare le centre de gravité du rectangle avec le milieu de la ligne
 * en utilisant la tolérance définie pour la classe.
 * 
 * @param id_rectangle Identifiant du rectangle à analyser
 * @return Structure SectionLineResult contenant la validité et les dimensions
 * eg : SectionLineResult{is_in_middle: true, dimensions: "25.0x30.0"}
 */
SectionLineResult Line::is_rectangle_in_middle_of_line(int id_rectangle)
{
    // Verifie si un rectangle est au milieu de la ligne
    // Récupérer le rectangle et son centre de gravité G
    auto& rectangle = data["rectangle_" + to_string(id_rectangle)];
    auto& G_vec = rectangle["G"]; // G est un vecteur [x, y, z]
    double Gx = G_vec.at("x");
    double Gy = G_vec.at("y"); 
    double Gz = G_vec.at("z");
    double largeur = rectangle["largeur"]["valeur"]; 
    double hauteur = rectangle["hauteur"]["valeur"];

    // Récupérer les deux noeuds de la ligne
    auto& nd1 = node_1;
    auto& nd2 = node_2;

    // Calculer le milieu de la ligne
    double dx = (nd1.at("x") + nd2.at("x")) / 2.0;
    double dy = (nd1.at("y") + nd2.at("y")) / 2.0;
    double dz = (nd1.at("z") + nd2.at("z")) / 2.0;

    // Définir l'erreur admissible (tolérance)
    double err_adm = tolerance;

    // Vérifier si le centre du rectangle est au milieu de la ligne dans les 3 dimensions
    bool is_in_middle = (abs(dx - Gx) <= err_adm) &&
                        (abs(dy - Gy) <= err_adm) &&
                        (abs(dz - Gz) <= err_adm);

    // Calculer largeur et hauteur en cm (en supposant qu'elles sont en m)
    double largeur_cm = round(abs(largeur) * 100.0 * 100.0) / 100.0;
    double hauteur_cm = round(abs(hauteur) * 100.0 * 100.0) / 100.0;

    // Créer la chaîne formatée
    string dims = to_string(largeur_cm) + "x" + to_string(hauteur_cm);

    // Retourner la structure avec le booléen et les dimensions
    return SectionLineResult{is_in_middle, dims};
}

/**
 * @brief Vérifie si un cercle est positionné au milieu de la ligne
 * 
 * Compare le centre du cercle avec le milieu de la ligne en utilisant
 * la tolérance définie pour la classe.
 * 
 * @param id_circle Identifiant du cercle à analyser
 * @return Structure SectionLineResult contenant la validité et le rayon
 * eg : SectionLineResult{is_in_middle: true, dimensions: "Rx15.0"}
 */
SectionLineResult Line::is_circle_in_middle_of_line(int id_circle)
{
    // Vérifie si le centre d'un cercle est au milieu de la ligne
    // Récupérer le cercle et son centre
    auto& cercle = data["circle_" + to_string(id_circle)];
    auto& G_vec = cercle["center"]; // G est un vecteur [x, y, z]
    double Gx = G_vec.at("x");
    double Gy = G_vec.at("y");
    double Gz = G_vec.at("z");
    double rayon = cercle["propriete"]["radius"];

    // Récupérer les deux noeuds de la ligne
    auto& nd1 = node_1;
    auto& nd2 = node_2;

    // Calculer le milieu de la ligne
    double dx = (nd1.at("x") + nd2.at("x")) / 2.0;
    double dy = (nd1.at("y") + nd2.at("y")) / 2.0;
    double dz = (nd1.at("z") + nd2.at("z")) / 2.0;

    // Définir l'erreur admissible (tolérance)
    double err_adm = tolerance;

    // Vérifier si le centre du cercle est au milieu de la ligne dans les 3 dimensions
    bool is_in_middle = (abs(dx - Gx) <= err_adm) &&
                        (abs(dy - Gy) <= err_adm) &&
                        (abs(dz - Gz) <= err_adm);

    // Calculer le rayon en cm et arrondir à deux décimales
    double rayon_cm = round(abs(rayon) * 100.0 * 100.0) / 100.0;

    // Créer la chaîne formatée
    string dims = "Rx" + to_string(rayon_cm);

    // Retourner la structure avec le booléen et la dimension
    return SectionLineResult{is_in_middle, dims};
}

/**
 * @brief Détermine la section géométrique associée à la ligne
 * 
 * Cette méthode recherche parmi tous les rectangles et cercles du système
 * celui qui est positionné au milieu de la ligne. Elle priorise les rectangles
 * par rapport aux cercles.
 * 
 * @return Structure SectionLine contenant le type, les dimensions et l'identifiant
 * @throws std::runtime_error si aucune section n'est trouvée
 * eg : SectionLine{type: "rectangular", dimensions: "25.0x30.0", id: 0}
 */
SectionLine Line::section()
{
    // Determines the type and index of the geometric section (rectangle or circle) intersected by the mid-line.

    // On suppose que le nombre de rectangles et de cercles sont stockés dans data
    // sous les clés "nb_rect" et "nb_ce" respectivement.
    int nb_rect = 0;
    int nb_ce = 0;
    if (data.find("nombre") != data.end() && data["nombre"].find("propriete") != data["nombre"].end()) {
        nb_rect = static_cast<int>(data["nombre"]["propriete"]["rectangle"]);
    }
    if (data.find("nombre") != data.end() && data["nombre"].find("propriete") != data["nombre"].end()) {
        nb_ce = static_cast<int>(data["nombre"]["propriete"]["cercle"]);
    }

    // Chercher un rectangle dont le centre est au milieu de la ligne
    vector<bool> found_rects;
    SectionLineResult rect_result;

    for (int rect_id = 0; rect_id < nb_rect; ++rect_id) {
        rect_result = is_rectangle_in_middle_of_line(rect_id);
        found_rects.push_back(rect_result.is_in_middle);
        if (rect_result.is_in_middle) {
            // On retourne le type et l'indice
            return SectionLine{"rectangular", rect_result.dimensions, rect_id }; 
        }
    }

    // Si aucun rectangle trouvé, chercher un cercle
    if (std::find(found_rects.begin(), found_rects.end(), true) == found_rects.end()) {
        for (int ce_id = 0; ce_id < nb_ce; ++ce_id) {
            SectionLineResult ce_result = is_circle_in_middle_of_line(ce_id);
            if (ce_result.is_in_middle) {
                return SectionLine{"circular", ce_result.dimensions, ce_id};
            }
        }
        // Si aucun rectangle ni cercle trouvé, lever une exception
        std::cout << "EREUR DE SECTION :: " << line_key << "\n";
        throw std::runtime_error("Ligne pas de section ou Section pas rect, ce ou Section Introuvable");
    }
    
    return SectionLine{"", "", 0};  
}

/**
 * @brief Calcule la force associée à la ligne
 * 
 * Cette méthode calcule l'intensité de la force en fonction du type de ligne
 * (colonne, poutre, console) et des rectangles qui l'intersectent. Les calculs
 * diffèrent selon le type de ligne et prennent en compte l'orientation des sections.
 * 
 * @return Structure Force contenant l'intensité et le type de section
 * eg : Force{Intensiter: 25.0, type: "rectangular"}
 */
Force Line::force() 
{   
    if (data["nombre"]["propriete"]["rectangle"] == 0) return Force(0, ""); 

    double Intensite = 0.0;

    for (int id_rectangle = 0 ; id_rectangle < data["nombre"]["propriete"]["rectangle"] ; ++id_rectangle) {

        RectangleLineResult rect = is_rectangle_in_line(id_rectangle);

        if (is_column and rect.is_valid) {
            Intensite = abs(data["rectangle_" + to_string(id_rectangle)]["largeur"]["valeur"]);
            if (rect.positive) Intensite = Intensite;
            else Intensite = -Intensite;
            break; 
        }
        else if (is_beam and not is_consol and rect.is_valid) {
            Intensite = abs(data["rectangle_" + to_string(id_rectangle)]["hauteur"]["valeur"]);
            if (rect.positive) Intensite = -Intensite;
            else Intensite = Intensite;
            break;
        }
        else if (is_consol and rect.is_valid) {
            Intensite = abs(data["rectangle_" + to_string(id_rectangle)]["hauteur"]["valeur"]) * length * length / 2 ;
            if (rect.positive) Intensite = -Intensite;
            else Intensite = Intensite;
            break;
        }
    }
    return Force(Intensite * scale, "rectangular"); 
}

/**
 * @brief Initialise toutes les propriétés de la ligne
 * 
 * Cette méthode privée est appelée par les constructeurs pour :
 * - Extraire les coordonnées des nœuds
 * - Calculer la longueur de la ligne
 * - Déterminer le type de ligne (colonne, poutre, inclinée)
 * - Calculer l'angle d'inclinaison
 * - Trouver les lignes associées
 * - Calculer la charge et la forme de section
 */
void Line::initialisation()
{
    // Vérification robuste de l'existence des clés pour éviter les erreurs
    if (data.count(line_key) && data[line_key].count("nd_1") && data[line_key].count("nd_2")) {
        node_1 = data[line_key]["nd_1"];
        node_2 = data[line_key]["nd_2"];
    }
    else {
        std::cerr << "Erreur: Impossible de trouver les noeuds pour la ligne " << id << std::endl;
        node_1 = { {"x", 0.0}, {"y", 0.0}, {"z", 0.0} };
        node_2 = { {"x", 0.0}, {"y", 0.0}, {"z", 0.0} };
    }

    // Calcul de la longueur de la ligne
    double dx = node_1["x"] - node_2["x"];
    double dy = node_1["y"] - node_2["y"];
    double dz = node_1["z"] - node_2["z"];
    length = sqrt(dx * dx + dy * dy + dz * dz);

    // Calcul de la tolérance angulaire en coordonnées
    double angle_rad = tolerance * pi / 180.0;
    double coord_tolerance = length * sin(angle_rad);

    // Détermination du type de ligne
    is_column = abs(node_1["x"] - node_2["x"]) <= coord_tolerance;
    is_beam = abs(node_1["y"] - node_2["y"]) <= coord_tolerance;
    is_inclined = !is_column && !is_beam;

    // Calcul de l'angle d'inclinaison
    if (is_inclined) {
        double deltaY = node_2["y"] - node_1["y"];
        double deltaX = node_2["x"] - node_1["x"];
        inclination_angle = atan2(deltaY, deltaX) * 180.0 / pi;
    }
    else if (is_column) {
        inclination_angle = 90.0;
    }
    else {
        inclination_angle = 0.0;
    }

    // Mise à jour de la tolérance pour les autres méthodes si besoin
    tolerance = coord_tolerance;

    // Recherche des lignes associées
    associated_lines = find_associated_lines();
    is_consol = check_is_consol(); 

	Force a = force(); // Calcul de la force associée
    charge = a.Intensiter;
    type_force = a.type;
    forme = section();  
}

