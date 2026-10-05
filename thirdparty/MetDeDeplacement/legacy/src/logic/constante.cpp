/**
 * @file constante.cpp
 * @brief Implémentation de la classe ConstanteLine pour le calcul des constantes de rigidité
 * 
 * Ce fichier contient l'implémentation de la classe ConstanteLine qui hérite de Line
 * et ajoute des fonctionnalités pour calculer les constantes de rigidité d'une poutre
 * ou d'une colonne en utilisant le module de Young et l'inertie équivalente.
 * 
 * @example
 * // Exemple d'utilisation de ConstanteLine
 * unordered_map<string, unordered_map<string, unordered_map<string, double>>> data;
 * // ... initialisation des données ...
 * 
 * // Création d'une ligne constante avec ID numérique
 * ConstanteLine line1(1, data, 5, 50, 210000, 0.05022003);
 * 
 * // Création d'une ligne constante avec ID string
 * ConstanteLine line2("ln_2", data, 5, 50, 210000, 0.05022003);
 * 
 * // Accès aux propriétés calculées
 * double module_young = line1.E;
 * double inertie_equiv = line1.I_equivalent;
 * double constante_rigidite = line1.K;
 * 
 * @author Tsaraloha Nomenjanahary Nomenjanahary Christinot Léonnel Calixte
 * @date 02/08/2025
 */

#include "constante.h"

/**
 * @brief Inverse le nom d'un nœud (nd_1 devient nd_2 et vice versa)
 * 
 * @param node Nom du nœud à inverser ("nd_1" ou "nd_2")
 * @return string Nom du nœud inversé
 * 
 * @example
 * string node1 = inverse_node("nd_1"); // Retourne "nd_2"
 * string node2 = inverse_node("nd_2"); // Retourne "nd_1"
 */
static constexpr string inverse_node(string node) {
	if (node == "nd_1") return "nd_2";
	else return "nd_1";
}

/**
 * @brief Trouve les identifiants des nœuds associés à une ligne donnée
 * 
 * Cette fonction recherche dans les données les nœuds qui correspondent
 * aux coordonnées spécifiées dans la ligne avec l'ID numérique.
 * 
 * @param node_number Nombre total de nœuds dans le système
 * @param id_line Identifiant numérique de la ligne
 * @param data Structure de données contenant les informations des nœuds et lignes
 * @return unordered_map<string, string> Mappage des nœuds (nd_1, nd_2) vers leurs identifiants
 * 
 * @example
 * // Supposons que nous ayons 3 nœuds et une ligne avec ID 1
 * unordered_map<string, string> nodes = find_id_node_1(3, 1, data);
 * // nodes["nd_1"] pourrait contenir "nd_0"
 * // nodes["nd_2"] pourrait contenir "nd_2"
 */
static unordered_map<string, string> find_id_node_1(int node_number, int id_line, 
	unordered_map<string, unordered_map<string, unordered_map<string, double>>> data) {

	unordered_map<string, string> Node;
	for (int i = 0; i < node_number; ++i) {
		// Vérifie si les coordonnées du nœud 1 de la ligne correspondent au nœud i
		if (data["ln_" + to_string(id_line)]["nd_1"] == data["noeuds"]["nd_" + to_string(i)]) {
			Node["nd_1"] = "nd_" + to_string(i);
		}
		// Vérifie si les coordonnées du nœud 2 de la ligne correspondent au nœud i
		if (data["ln_" + to_string(id_line)]["nd_2"] == data["noeuds"]["nd_" + to_string(i)]) {
			Node["nd_2"] = "nd_" + to_string(i);
		}
		// Sort de la boucle une fois que les deux nœuds sont trouvés
		if (Node.size() == 2) break; 
	}
	return Node; 
}

/**
 * @brief Trouve les identifiants des nœuds associés à une ligne donnée (version avec ID string)
 * 
 * Cette fonction recherche dans les données les nœuds qui correspondent
 * aux coordonnées spécifiées dans la ligne avec l'ID sous forme de string.
 * 
 * @param node_number Nombre total de nœuds dans le système
 * @param id_line Identifiant string de la ligne
 * @param data Structure de données contenant les informations des nœuds et lignes
 * @return unordered_map<string, string> Mappage des nœuds (nd_1, nd_2) vers leurs identifiants
 * 
 * @example
 * // Utilisation avec un ID string
 * unordered_map<string, string> nodes = find_id_node_1(3, "ln_1", data);
 * // Même résultat que la version avec ID numérique
 */
static unordered_map<string, string> find_id_node_1(int node_number, string id_line,
	unordered_map<string, unordered_map<string, unordered_map<string, double>>> data) {

	unordered_map<string, string> Node;
	for (int i = 0; i < node_number; ++i) {
		// Vérifie si les coordonnées du nœud 1 de la ligne correspondent au nœud i
		if (data[id_line]["nd_1"] == data["noeuds"]["nd_" + to_string(i)]) {
			Node["nd_1"] = "nd_" + to_string(i);
		}
		// Vérifie si les coordonnées du nœud 2 de la ligne correspondent au nœud i
		if (data[id_line]["nd_2"] == data["noeuds"]["nd_" + to_string(i)]) {
			Node["nd_2"] = "nd_" + to_string(i);
		}
		// Sort de la boucle une fois que les deux nœuds sont trouvés
		if (Node.size() == 2) break;
	}
	return Node;
}

/**
 * @brief Constructeur de ConstanteLine avec ID numérique
 * 
 * Initialise une ligne constante avec un ID numérique et calcule les constantes
 * de rigidité basées sur le module de Young et l'inertie équivalente.
 * 
 * @param id Identifiant numérique de la ligne
 * @param data Structure de données contenant les informations du système
 * @param angle_tolerance Tolérance d'angle pour les calculs (défaut: 5)
 * @param scale Échelle pour les calculs (défaut: 50)
 * @param YoungModule Module de Young du matériau (défaut: 1)
 * @param Inertie_equivalent Inertie équivalente (défaut: 0.05022003)
 * 
 * @example
 * // Création d'une poutre en acier (E = 210 GPa)
 * ConstanteLine poutre_acier(1, data, 5, 50, 210000, 0.05022003);
 * 
 * // Création d'une colonne en béton (E = 30 GPa)
 * ConstanteLine colonne_beton(2, data, 5, 50, 30000, 0.08000000);
 */
ConstanteLine::ConstanteLine(int id, unordered_map<string, unordered_map<string, unordered_map<string, double>>> data, 
	double angle_tolerance, int scale, double YoungModule, double Inertie_equivalent) :
	Line(id, data, angle_tolerance, scale), E(YoungModule), I_equivalent(Inertie_equivalent)
{
	init(); 
}

/**
 * @brief Constructeur de ConstanteLine avec ID string
 * 
 * Initialise une ligne constante avec un ID string et calcule les constantes
 * de rigidité basées sur le module de Young et l'inertie équivalente.
 * 
 * @param id Identifiant string de la ligne
 * @param data Structure de données contenant les informations du système
 * @param angle_tolerance Tolérance d'angle pour les calculs (défaut: 5)
 * @param scale Échelle pour les calculs (défaut: 50)
 * @param YoungModule Module de Young du matériau (défaut: 1)
 * @param Inertie_equivalent Inertie équivalente (défaut: 0.05022003)
 * 
 * @example
 * // Création avec ID string
 * ConstanteLine ligne1("ln_1", data, 5, 50, 210000, 0.05022003);
 * 
 * // Création avec paramètres par défaut
 * ConstanteLine ligne2("ln_2", data);
 */
ConstanteLine::ConstanteLine(string id, unordered_map<string, unordered_map<string, unordered_map<string, double>>> data, 
	double angle_tolerance, int scale, double YoungModule, double Inertie_equivalent) :
	Line(id, data, angle_tolerance, scale), E(YoungModule), I_equivalent(Inertie_equivalent)
{
	init();
}

/**
 * @brief Calcule les valeurs de gamma pour toutes les lignes associées
 * 
 * Cette fonction calcule les valeurs de gamma pour chaque ligne consolidée associée.
 * Le gamma représente l'effort appliqué sur la ligne et dépend de la géométrie
 * et des conditions aux limites.
 * 
 * @return vector<Gamma> Vecteur contenant toutes les valeurs de gamma calculées
 * 
 * @example
 * ConstanteLine ligne(1, data, 5, 50, 210000);
 * vector<Gamma> gamma_values = ligne.gamma();
 * 
 * // Parcours des valeurs de gamma
 * for (const auto& gamma : gamma_values) {
 *     cout << "Signe: " << gamma.signe << endl;
 *     cout << "Condition a: " << gamma.condition_a << endl;
 *     cout << "Gamma: " << gamma.gamma << endl;
 *     cout << "Paramètre a: " << gamma.a << endl;
 * }
 */
vector<Gamma> ConstanteLine::gamma()
{
	vector<Gamma> gamma_values;

	// Parcourt toutes les lignes associées
	for (auto& [ligne, nodes] : associated_lines) {
		Line line_cosole = Line(ligne, data);
		
		// Vérifie si la ligne est consolidée
		if (line_cosole.is_consol) {
			vector<string> ndd = line_cosole.associated_lines[line_key];
			unordered_map<string, string> nodes_consol = find_id_node_1(data["nombre"]["propriete"]["neouds"], line_cosole.line_key, data);

			// Crée les objets Node pour les deux nœuds de la ligne consolidée
			Node node_1 = Node(nodes_consol["nd_1"], data);
			Node node_2 = Node(nodes_consol["nd_2"], data);

			bool signe = false;

			// Détermine le signe basé sur la position relative des nœuds
			if (node_1.ligne_associer.size() > 1) {
				if (data["noeuds"][node_1.node_key]["x"] < data["noeuds"][node_2.node_key]["x"]) signe = true;
				else signe = false;
			}
			if (node_2.ligne_associer.size() > 1) {
				if (data["noeuds"][node_2.node_key]["x"] < data["noeuds"][node_1.node_key]["x"]) signe = true;
				else signe = false;
			}
			
			// Détermine la condition géométrique selon le type d'élément
			bool condition_a = data[line_cosole.line_key][ndd[1]]["x"] < data[line_key][inverse_node(ndd[0])]["x"];
			if (is_beam) condition_a = data[line_cosole.line_key][ndd[1]]["y"] <= data[line_key][inverse_node(ndd[0])]["y"];

			double gamma = charge; 
			double a = 0.0;
			
			// Calcul spécifique pour les poutres
			if (is_beam) {
				// Applique le signe au gamma
				if (signe) gamma = -gamma;
				else gamma = gamma;

				// Détermine la valeur de 'a' selon la condition géométrique
				if (condition_a) a = 0.0;
				else a = length;
			}
			
			// Calcul spécifique pour les colonnes
			if (is_column) {
				// Applique le signe au gamma
				if (signe) gamma = -gamma;
				else gamma = gamma;

				// Détermine la valeur de 'a' selon la condition géométrique
				if (condition_a) a = 0.0;
				else a = length;
			}
			
			// Ajoute la valeur de gamma calculée au vecteur
			gamma_values.push_back({ signe, condition_a, gamma, a });
		}
	}
	return gamma_values; 
}

/**
 * @brief Initialise les constantes de rigidité de la ligne
 * 
 * Cette fonction calcule toutes les constantes nécessaires pour les calculs
 * de rigidité : module de Young, inertie, coefficient alpha, constante de rigidité K,
 * et les coefficients a, b, c utilisés dans les matrices de rigidité.
 * 
 * @example
 * // Après création d'un objet ConstanteLine, les constantes sont automatiquement calculées
 * ConstanteLine ligne(1, data, 5, 50, 210000);
 * 
 * // Accès aux constantes calculées
 * cout << "Module de Young: " << ligne.E << endl;
 * cout << "Inertie équivalente: " << ligne.I_equivalent << endl;
 * cout << "Coefficient alpha: " << ligne.alpha << endl;
 * cout << "Constante de rigidité K: " << ligne.K << endl;
 * cout << "Coefficient a: " << ligne.a << endl;
 * cout << "Coefficient b: " << ligne.b << endl;
 * cout << "Coefficient c: " << ligne.c << endl;
 * 
 * // Les valeurs de K dépendent des conditions aux limites :
 * // - K = 4*E*alpha/L pour deux extrémités fixes
 * // - K = 3*E*alpha/L pour une extrémité fixe et une en rotule
 * // - K = 0 pour une extrémité libre
 */
void ConstanteLine::init()
{
	// Validation du module de Young
	if (E <= 0) {
		std::cerr << "Erreur: Le module de Young doit être positif." << std::endl;
		E = 1; // Valeur par défaut
	}
	
	// Validation de l'inertie équivalente
	if (I_equivalent <= 0) {
		std::cerr << "Erreur: L'inertie équivalente doit être positive." << std::endl;
		I_equivalent = 0.05022003; // Valeur par défaut
	}

	// Calcule l'inertie réelle de la section
	I = Inertie(forme.type, forme.dimensions).I; 
	
	// Calcule le coefficient alpha (rapport entre inertie réelle et équivalente)
	alpha = I / I_equivalent;

	// Trouve les nœuds réels associés à cette ligne
	real_nodes = find_id_node_1(data["nombre"]["propriete"]["noeuds"],line_key, data);

	// Détermine les types des nœuds aux extrémités
	vector<string> type_nodes = { Node(real_nodes["nd_1"], data).type , Node(real_nodes["nd_2"], data).type };

	// Calcule la constante de rigidité K selon les conditions aux limites
	if (type_nodes.size() == 2 && type_nodes[0] == "fix" && type_nodes[1] == "fix") {
		// Deux extrémités fixes
		K = 4 * E * alpha / length;
	}
	else if (std::find(type_nodes.begin(), type_nodes.end(), "rotule") != type_nodes.end() and
		std::find(type_nodes.begin(), type_nodes.end(), "free") == type_nodes.end()) {
		// Une extrémité en rotule, l'autre fixe
		K = 3 * E * alpha / length;
	}
	else {
		// Autres cas (extrémité libre)
		K = 0; 
	}

	// Calcule les coefficients de la matrice de rigidité
	a = length / (3 * E * alpha); 
	b = length / (6 * E * alpha);
	c = length / (3 * E * alpha); 

	// Calcule les valeurs de gamma pour toutes les lignes associées
	GAMMA = gamma();
}
